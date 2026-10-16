#include "PatcherMpq.h"
#include "PatcherFilesystem.h"
#include "PatcherHash.h"
#include "WoW/Source/Duskhaven/Streaming/ClientArchive.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace Patcher::Mpq
{
    namespace
    {
        std::string ToClientPath(std::string path)
        {
            std::replace(path.begin(), path.end(), '/', '\\');
            return path;
        }

        std::string StormError(const char* action)
        {
            std::ostringstream stream;
            stream << action << " failed (win32=" << GetLastError() << ")";
            return stream.str();
        }

        template <typename THash>
        bool HashArchiveRecord(System_SFile::ARCHIVEREC* archive, const std::string& archivePath,
                              const std::string& logicalPath, std::string& outHash, uint64_t& outSize,
                              std::string& error, const uint64_t* expectedSize = nullptr)
        {
            const std::string clientPath = ToClientPath(logicalPath);
            IOMpqUnit* file = nullptr;
            if (Blizzard::Mopaq::SFileOpenFileEx(archive, clientPath.c_str(), 0, &file) == 0 || !file)
            {
                error = "MPQ member not found: " + archivePath + " :: " + logicalPath;
                return false;
            }

            uint32_t highSize = 0;
            const uint32_t lowSize = Blizzard::Mopaq::SFileGetFileSize(file, &highSize);
            if (lowSize == 0xFFFFFFFFu)
            {
                error = "failed to read MPQ member size for " + logicalPath;
                Blizzard::Mopaq::SFileCloseFile(file);
                return false;
            }
            outSize = (static_cast<uint64_t>(highSize) << 32) | lowSize;

            if (expectedSize && outSize != *expectedSize)
            {
                Blizzard::Mopaq::SFileCloseFile(file);
                outHash.clear();
                return true;
            }

            THash hash;
            if (!hash.Begin(error))
            {
                Blizzard::Mopaq::SFileCloseFile(file);
                return false;
            }

            constexpr uint32_t BufferSize = 64 * 1024;
            std::vector<unsigned char> buffer(BufferSize);
            uint64_t remaining = outSize;
            while (remaining > 0)
            {
                const uint32_t requested = static_cast<uint32_t>((std::min<uint64_t>)(remaining, BufferSize));
                uint32_t read = 0;
                if (!Blizzard::Mopaq::SFileReadFile(file, buffer.data(), requested, &read) || read == 0)
                {
                    error = "failed to read MPQ member: " + archivePath + " :: " + logicalPath;
                    Blizzard::Mopaq::SFileCloseFile(file);
                    return false;
                }
                if (!hash.Update(buffer.data(), read, error))
                {
                    Blizzard::Mopaq::SFileCloseFile(file);
                    return false;
                }
                remaining -= read;
            }

            Blizzard::Mopaq::SFileCloseFile(file);
            return hash.Finish(outHash, error);
        }

        bool VerifyDownloadedFile(const FileReplacement& file, std::string& error)
        {
            std::string actualHash;
            uint64_t actualSize = 0;
            const bool useMd5   = !file.md5.empty();
            const bool hashed   = useMd5 ? Filesystem::HashFileMd5(file.downloadedPath, actualHash, actualSize, error) :
                                           Filesystem::HashFileSha256(file.downloadedPath, actualHash, actualSize, error);
            if (!hashed)
                return false;

            if (actualSize != file.size)
            {
                std::ostringstream stream;
                stream << "downloaded file size mismatch for " << file.logicalPath << ": expected " << file.size
                       << " got " << actualSize;
                error = stream.str();
                return false;
            }

            const std::string& expectedHash = useMd5 ? file.md5 : file.sha256;
            if (!expectedHash.empty() && !Filesystem::EqualsIgnoreCase(actualHash, expectedHash))
            {
                error = "downloaded file hash mismatch for " + file.logicalPath;
                return false;
            }

            return true;
        }

        class ArchiveCriticalSection final
        {
          public:
            explicit ArchiveCriticalSection(System_SFile::ARCHIVEREC* archive)
                : mutex_(archive ? &archive->m_mutex.m_mutex : nullptr)
            {
                if (mutex_)
                    EnterCriticalSection(mutex_);
            }

            ~ArchiveCriticalSection()
            {
                if (mutex_)
                    LeaveCriticalSection(mutex_);
            }

            ArchiveCriticalSection(const ArchiveCriticalSection&)            = delete;
            ArchiveCriticalSection& operator=(const ArchiveCriticalSection&) = delete;

          private:
            CRITICAL_SECTION* mutex_;
        };

        class PayloadPins final
        {
          public:
            ~PayloadPins()
            {
                for (HANDLE handle : handles_)
                    CloseHandle(handle);
            }

            bool Pin(const std::string& path, std::string& error)
            {
                if (path.empty() || !paths_.insert(path).second)
                    return true;

                // Prevent payload deletion while the archive transaction is active.
                HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle == INVALID_HANDLE_VALUE)
                {
                    error = Filesystem::LastErrorString(
                        ("downloaded update data disappeared before apply: " + path).c_str());
                    return false;
                }
                handles_.push_back(handle);
                return true;
            }

          private:
            std::set<std::string> paths_;
            std::vector<HANDLE> handles_;
        };

        class BundleArchiveCache final
        {
          public:
            ~BundleArchiveCache()
            {
                for (const auto& entry : archives_)
                {
                    if (entry.second)
                        Blizzard::Mopaq::SFileCloseArchive(entry.second);
                }
            }

            bool Open(const std::string& path, std::string& error)
            {
                if (archives_.find(path) != archives_.end())
                    return true;

                System_SFile::ARCHIVEREC* archive = nullptr;
                if (!Blizzard::Mopaq::SFileOpenArchive(path.c_str(), 0, 0, &archive) || !archive)
                {
                    error = StormError(("failed to open update bundle " + path).c_str());
                    return false;
                }
                archives_.emplace(path, archive);
                return true;
            }

            System_SFile::ARCHIVEREC* Get(const std::string& path) const
            {
                const auto archive = archives_.find(path);
                return archive == archives_.end() ? nullptr : archive->second;
            }

          private:
            std::map<std::string, System_SFile::ARCHIVEREC*> archives_;
        };

        bool WriteMountedMember(System_SFile::ARCHIVEREC* archiveRecord, const FileReplacement& file,
                                System_SFile::ARCHIVEREC* bundleArchive,
                                std::vector<unsigned char>& buffer, std::string& error)
        {
            if (file.size > (std::numeric_limits<uint32_t>::max)())
            {
                error = "MPQ member is too large for the client writer: " + file.logicalPath;
                return false;
            }

            std::ifstream input;
            IOMpqUnit* bundleMember = nullptr;
            if (!file.bundlePath.empty())
            {
                const std::string sourceMember = ToClientPath(file.logicalPath);
                if (!bundleArchive ||
                    Blizzard::Mopaq::SFileOpenFileEx(bundleArchive, sourceMember.c_str(), 0, &bundleMember) == 0 ||
                    !bundleMember)
                {
                    if (bundleMember)
                        Blizzard::Mopaq::SFileCloseFile(bundleMember);
                    error = "update bundle member could not be opened: " + file.logicalPath;
                    return false;
                }
            }
            else
            {
                input.open(file.downloadedPath, std::ios::binary);
            }
            if (file.bundlePath.empty() && !input)
            {
                error = "downloaded MPQ member could not be opened: " + file.logicalPath;
                return false;
            }

            const std::string memberPath = ToClientPath(file.logicalPath);
            constexpr uint32_t ClientReplaceExisting = 0x00000001;
            constexpr uint32_t MpqFileCompress       = 0x00000200;
            constexpr uint32_t FileFlags             = ClientReplaceExisting | MpqFileCompress;
            IOMpqUnit* output                        = nullptr;
            if (!Blizzard::Mopaq::SFileCreateFile(archiveRecord, memberPath.c_str(), static_cast<uint32_t>(file.size),
                                                  FileFlags, &output) ||
                !output)
            {
                if (bundleMember)
                    Blizzard::Mopaq::SFileCloseFile(bundleMember);
                error = "the mounted MPQ rejected " + file.logicalPath;
                return false;
            }

            bool success       = true;
            uint64_t remaining = file.size;
            while (remaining > 0)
            {
                const size_t requested = static_cast<size_t>((std::min<uint64_t>)(remaining, buffer.size()));
                size_t read            = 0;
                if (bundleMember)
                {
                    uint32_t bundleRead = 0;
                    if (!Blizzard::Mopaq::SFileReadFile(bundleMember, buffer.data(), static_cast<uint32_t>(requested),
                                                        &bundleRead))
                    {
                        const DWORD readError = GetLastError();
                        std::ostringstream stream;
                        stream << "reading bundled MPQ member failed for " << file.logicalPath
                               << " (win32=" << readError << ")";
                        error   = stream.str();
                        success = false;
                        break;
                    }
                    read = bundleRead;
                }
                else
                {
                    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(requested));
                    read = static_cast<size_t>(input.gcount());
                }
                if (read != requested ||
                    !Blizzard::Mopaq::SFileWriteFile(output, buffer.data(), static_cast<uint32_t>(read)))
                {
                    success = false;
                    break;
                }
                remaining -= read;
            }

            if (!Blizzard::Mopaq::SFileCloseFile(output))
                success = false;
            if (bundleMember)
                Blizzard::Mopaq::SFileCloseFile(bundleMember);
            if (!success && error.empty())
                error = "writing the mounted MPQ failed for " + file.logicalPath;
            return success;
        }

        bool WriteMountedDeleteMarker(System_SFile::ARCHIVEREC* archiveRecord, const std::string& logicalPath, std::string& error)
        {
            const std::string memberPath             = ToClientPath(logicalPath);
            constexpr uint32_t ClientReplaceExisting = 0x00000001;
            constexpr uint32_t MpqFileDeleteMarker   = 0x02000000;
            constexpr uint32_t DeleteFlags           = ClientReplaceExisting | MpqFileDeleteMarker;
            IOMpqUnit* output                        = nullptr;
            if (!Blizzard::Mopaq::SFileCreateFile(archiveRecord, memberPath.c_str(), 0, DeleteFlags, &output) || !output)
            {
                error = "the mounted MPQ rejected deletion of " + logicalPath;
                return false;
            }

            if (!Blizzard::Mopaq::SFileCloseFile(output))
            {
                error = "writing the MPQ deletion marker failed for " + logicalPath;
                return false;
            }
            return true;
        }
    } // namespace

    bool ArchiveContainsFile(const std::string& archivePath, const std::string& logicalPath)
    {
        ClientArchive::MountedArchive mounted;
        if (ClientArchive::FindMounted(archivePath, mounted))
        {
            IOMpqUnit* file = nullptr;
            const bool found =
                mounted.archiveRecord &&
                Blizzard::Mopaq::SFileOpenFileEx(mounted.archiveRecord, ToClientPath(logicalPath).c_str(), 0, &file) != 0 &&
                file;
            if (file)
                Blizzard::Mopaq::SFileCloseFile(file);
            return found;
        }

        System_SFile::ARCHIVEREC* archive = nullptr;
        if (!Blizzard::Mopaq::SFileOpenArchive(archivePath.c_str(), 0, 0, &archive) || !archive)
            return false;
        IOMpqUnit* file = nullptr;
        const bool found =
            Blizzard::Mopaq::SFileOpenFileEx(archive, ToClientPath(logicalPath).c_str(), 0, &file) != 0 &&
            file != nullptr;
        if (file)
            Blizzard::Mopaq::SFileCloseFile(file);
        Blizzard::Mopaq::SFileCloseArchive(archive);
        return found;
    }

    bool MatchArchiveFilesMd5(const std::string& archivePath, const std::vector<FileExpectation>& files,
                              std::vector<bool>& matches, std::string& error, const MatchProgressCallback& onProgress)
    {
        matches.assign(files.size(), false);
        if (files.empty())
            return true;

        ClientArchive::MountedArchive mounted;
        const bool useMountedArchive = ClientArchive::FindMounted(archivePath, mounted);
        System_SFile::ARCHIVEREC* validationArchive = nullptr;
        if (!useMountedArchive &&
            (!Blizzard::Mopaq::SFileOpenArchive(archivePath.c_str(), 0, 0, &validationArchive) ||
             !validationArchive))
        {
            error = StormError(("failed to open MPQ " + archivePath).c_str());
            return false;
        }
        if (validationArchive)
            Blizzard::Mopaq::SFileCloseArchive(validationArchive);

        const uint64_t totalBytes =
            std::accumulate(files.begin(), files.end(), uint64_t{0},
                            [](uint64_t total, const FileExpectation& file) { return total + file.size; });
        if (onProgress)
            onProgress(0, totalBytes, 0, files.size());

        const unsigned hardwareThreads = (std::max)(1u, std::thread::hardware_concurrency());
        size_t workerCount = 1;
        if (!useMountedArchive)
            workerCount = (std::min)(files.size(), static_cast<size_t>((std::min)(hardwareThreads, 4u)));
        std::vector<uint8_t> results(files.size(), 0);
        std::atomic<uint64_t> completedBytes{0};
        std::atomic<size_t> completedFiles{0};
        std::atomic<bool> scanFailed{false};
        std::mutex errorMutex;
        const auto scanRange = [&](size_t begin, size_t end)
        {
            System_SFile::ARCHIVEREC* archive = nullptr;
            if (!useMountedArchive &&
                (!Blizzard::Mopaq::SFileOpenArchive(archivePath.c_str(), 0, 0, &archive) || !archive))
            {
                scanFailed = true;
                std::lock_guard<std::mutex> lock(errorMutex);
                if (error.empty())
                    error = StormError(("failed to open MPQ " + archivePath).c_str());
                return;
            }

            for (size_t index = begin; index < end && !scanFailed.load(); ++index)
            {
                std::string hash;
                uint64_t size = 0;
                std::string memberError;
                const bool hashed =
                    HashArchiveRecord<Hash::Md5>(useMountedArchive ? mounted.archiveRecord : archive,
                                                 archivePath, files[index].logicalPath, hash, size,
                                                 memberError, &files[index].size);
                if (hashed)
                {
                    results[index] =
                        size == files[index].size && Filesystem::EqualsIgnoreCase(hash, files[index].md5) ? 1 : 0;
                }
                else if (memberError.rfind("MPQ member not found:", 0) != 0)
                {
                    scanFailed = true;
                    std::lock_guard<std::mutex> lock(errorMutex);
                    if (error.empty())
                        error = std::move(memberError);
                    break;
                }

                const uint64_t bytes = completedBytes.fetch_add(files[index].size) + files[index].size;
                const size_t count   = completedFiles.fetch_add(1) + 1;
                if (onProgress && (count == files.size() || count % 64 == 0))
                    onProgress(bytes, totalBytes, count, files.size());
            }

            if (archive)
                Blizzard::Mopaq::SFileCloseArchive(archive);
        };

        std::vector<std::thread> workers;
        if (workerCount == 1)
        {
            scanRange(0, files.size());
        }
        else
        {
            workers.reserve(workerCount);
            for (size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                const size_t begin = files.size() * workerIndex / workerCount;
                const size_t end   = files.size() * (workerIndex + 1) / workerCount;
                workers.emplace_back(scanRange, begin, end);
            }
        }

        for (std::thread& worker : workers)
            worker.join();
        if (scanFailed)
            return false;

        for (size_t index = 0; index < results.size(); ++index)
            matches[index] = results[index] != 0;
        if (onProgress)
            onProgress(totalBytes, totalBytes, files.size(), files.size());
        return true;
    }

    bool InstallChangesInMountedArchive(const std::string& archivePath, const std::vector<FileReplacement>& files,
                                        const std::vector<std::string>& deletedFiles,
                                        const ProgressCallback& onProgress, std::string& error)
    {
        ClientArchive::MountedArchive mounted;
        if (!ClientArchive::EnsureMountedWritable(archivePath, mounted, error))
            return false;

        // Glue code rereads (listfile), making it a useful canary for stale mounts.
        std::vector<uint8_t> listfileBefore;
        const bool validateListfileRedirect =
            mounted.originalArchiveRecord != nullptr &&
            ClientArchive::ReadWholeFile(mounted.archiveRecord, "(listfile)", listfileBefore);

        const uint64_t replacementBytes =
            std::accumulate(files.begin(), files.end(), uint64_t{0},
                            [](uint64_t total, const FileReplacement& file) { return total + file.size; });

        // Reserve the uncompressed replacement size before mutating the archive.
        ULARGE_INTEGER freeBytes{};
        constexpr uint64_t FlushReserve    = 64ull * 1024ull * 1024ull;
        const std::string archiveDirectory = std::filesystem::path(archivePath).parent_path().string();
        if (!GetDiskFreeSpaceExA(archiveDirectory.c_str(), &freeBytes, nullptr, nullptr))
        {
            error = Filesystem::LastErrorString(("failed to check free disk space for " + archivePath).c_str());
            return false;
        }
        if (replacementBytes > (std::numeric_limits<uint64_t>::max)() - FlushReserve ||
            freeBytes.QuadPart < replacementBytes + FlushReserve)
        {
            error = "not enough free disk space to update " + std::filesystem::path(archivePath).filename().string();
            return false;
        }

        // Pin every payload before the first permanent MPQ mutation.
        PayloadPins payloadPins;
        BundleArchiveCache bundleArchives;
        for (const FileReplacement& file : files)
        {
            const std::string& sourcePath = file.bundlePath.empty() ? file.downloadedPath : file.bundlePath;
            if (!payloadPins.Pin(sourcePath, error))
                return false;

            // Bundles are verified after download. Open each once for all member reads.
            if (!file.bundlePath.empty())
            {
                if (!bundleArchives.Open(file.bundlePath, error))
                    return false;
            }
            else if (!VerifyDownloadedFile(file, error))
                return false;
        }

        const size_t totalFiles = files.size() + deletedFiles.size();
        uint64_t completedBytes = 0;
        size_t completedFiles   = 0;
        if (onProgress)
            onProgress(0, replacementBytes, 0, totalFiles);

        // Keep the archive locked until each new hash entry has a complete payload.
        std::vector<unsigned char> writeBuffer(1024 * 1024);
        for (const FileReplacement& file : files)
        {
            ArchiveCriticalSection lock(mounted.archiveRecord);
            if (!WriteMountedMember(mounted.archiveRecord, file, bundleArchives.Get(file.bundlePath), writeBuffer,
                                    error))
            {
                System_SFile::ARCHIVEREC::WriteArchiveChanges(mounted.archiveRecord);
                return false;
            }

            completedBytes += file.size;
            ++completedFiles;
            if (onProgress)
                onProgress(completedBytes, replacementBytes, completedFiles, totalFiles);
        }

        for (const std::string& logicalPath : deletedFiles)
        {
            ArchiveCriticalSection lock(mounted.archiveRecord);
            if (!WriteMountedDeleteMarker(mounted.archiveRecord, logicalPath, error))
            {
                System_SFile::ARCHIVEREC::WriteArchiveChanges(mounted.archiveRecord);
                return false;
            }

            ++completedFiles;
            if (onProgress)
                onProgress(completedBytes, replacementBytes, completedFiles, totalFiles);
        }

        {
            ArchiveCriticalSection lock(mounted.archiveRecord);
            if (!System_SFile::ARCHIVEREC::WriteArchiveChanges(mounted.archiveRecord))
            {
                error = "the mounted MPQ could not be flushed to disk";
                return false;
            }
        }

        // Rebuild internal hash table so updated files are discovered immediately
        SFile::RebuildHash();

        if (validateListfileRedirect)
        {
            std::vector<uint8_t> currentListfile;
            std::vector<uint8_t> redirectedListfile;
            if (!ClientArchive::ReadWholeFile(mounted.archiveRecord, "(listfile)", currentListfile) ||
                !ClientArchive::ReadWholeFile(mounted.originalArchiveRecord, "(listfile)", redirectedListfile) ||
                currentListfile != redirectedListfile)
            {
                error = "the updated game file could not refresh its live mount";
                return false;
            }
        }
        return true;
    }
} // namespace Patcher::Mpq
