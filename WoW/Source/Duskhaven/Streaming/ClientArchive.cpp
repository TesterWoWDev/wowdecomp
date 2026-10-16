#include "ClientArchive.h"
#include "StreamingLogger.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQGlobals.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOFileUnit.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr int WritableArchiveFlag = 0x10;

    std::mutex g_archiveMutex;
    std::unordered_map<std::string, ClientArchive::MountedArchive> g_mountedArchives;
    std::unordered_set<System_SFile::ARCHIVEREC*> g_pinnedOriginalArchives;

    struct WritableRequest
    {
        std::string path;
        ClientArchive::MountedArchive mounted;
        std::string error;
        bool complete = false;
        bool started  = false;
        std::condition_variable changed;
    };
    std::mutex g_writableRequestMutex;
    std::vector<std::shared_ptr<WritableRequest>> g_writableRequests;

    std::string NormalizePath(std::string path)
    {
        std::replace(path.begin(), path.end(), '/', '\\');
        while (path.size() >= 2 && path[0] == '.' && path[1] == '\\')
            path.erase(0, 2);
        std::transform(path.begin(), path.end(), path.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return path;
    }

    std::string AbsolutePath(const char* path)
    {
        if (!path || !*path)
            return {};

        std::error_code error;
        const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), error);
        return error ? std::string{} : NormalizePath(absolute.lexically_normal().string());
    }

    bool ResolveWritableRedirect(ClientArchive::MountedArchive& mounted)
    {
        if (!mounted.archiveRecord)
            return false;

        System_SFile::ARCHIVEREC* original = mounted.archiveRecord;
        if ((original->m_flags & WritableArchiveFlag) != 0)
        {
            mounted.writable = true;
            return true;
        }

        System_SFile::ARCHIVEREC* redirected = original->m_parent;
        if (!redirected)
            return false;

        if ((redirected->m_flags & WritableArchiveFlag) == 0)
            return false;

        mounted.originalArchiveRecord = original;
        mounted.archiveRecord         = redirected;
        mounted.writable              = true;
        return true;
    }

    System_SFile::ARCHIVEREC* FindArchiveRecord(const std::string& path)
    {
        std::vector<std::string> candidates{path};
        std::string slashed = path;
        std::replace(slashed.begin(), slashed.end(), '/', '\\');
        if (slashed != path)
            candidates.push_back(slashed);

        std::string backslashed = path;
        std::replace(backslashed.begin(), backslashed.end(), '\\', '/');
        if (backslashed != path)
            candidates.push_back(backslashed);

        std::error_code error;
        const std::filesystem::path relative =
            std::filesystem::relative(std::filesystem::path(path), std::filesystem::current_path(error), error);
        if (!error && !relative.empty())
        {
            std::string relativeText = relative.lexically_normal().string();
            std::replace(relativeText.begin(), relativeText.end(), '/', '\\');
            candidates.push_back(relativeText);
        }

        System_SFile::ARCHIVEREC* record = nullptr;
        for (const std::string& candidate : candidates)
        {
            record = System_MPQGlobals::FindArchiveByName(candidate.c_str());
            if (record)
                break;
        }
        return record;
    }

    bool RememberDiscoveredArchive(const std::string& path, System_SFile::ARCHIVEREC* record, ClientArchive::MountedArchive& out)
    {
        if (!record)
            return false;

        std::lock_guard<std::mutex> lock(g_archiveMutex);
        out = {record, record, (record->m_flags & WritableArchiveFlag) != 0, nullptr};
        ResolveWritableRedirect(out);
        const std::string normalized = NormalizePath(path);
        const std::string absolute   = AbsolutePath(path.c_str());
        if (!normalized.empty())
            g_mountedArchives[normalized] = out;
        if (!absolute.empty())
            g_mountedArchives[absolute] = out;
        return true;
    }

    bool UpgradeMountedArchive(const std::string& path, ClientArchive::MountedArchive& mounted, std::string& error)
    {
        System_SFile::ARCHIVEREC* archive = mounted.archiveRecord;
        if (!archive || !archive->m_file)
        {
            error = std::filesystem::path(path).stem().string() + " is not a writable packed game file.";
            return false;
        }

        IOFileUnit* fileUnit = dynamic_cast<IOFileUnit*>(archive->m_file);
        if (!fileUnit)
        {
            fileUnit = reinterpret_cast<IOFileUnit*>(archive->m_file);
        }

        Blizzard::Lock::ScopedMutex lock(archive->m_mutex.m_mutex);

        bool writable = false;
        if (fileUnit->m_stream && Blizzard::File::Close(fileUnit->m_stream))
        {
            if (Blizzard::File::Open(archive->m_name, 0x1007, fileUnit->m_stream) && fileUnit->m_stream)
            {
                writable = true;
            }
            else
            {
                Blizzard::File::Open(archive->m_name, 0x1005, fileUnit->m_stream);
            }
        }

        if (!writable)
        {
            error = std::filesystem::path(path).stem().string() +
                    " is being locked by another program. Please close that program and restart the client.";
            return false;
        }

        System_SFile::ARCHIVEREC* writableRecord = nullptr;
        if (!Blizzard::Mopaq::SFileOpenArchive(archive->m_file, archive->m_name, archive->m_priority + 1,
                                                archive->m_flags | WritableArchiveFlag, &writableRecord) ||
            !writableRecord)
        {
            error = "the mounted game file could not be prepared for updating";
            return false;
        }

        System_SFile::GetArchivePtr(writableRecord, true);

        if (archive->m_parent == nullptr)
            archive->m_parent = writableRecord;

        mounted.originalArchiveRecord = archive;
        mounted.archiveRecord         = writableRecord;
        mounted.writable              = true;

        {
            std::lock_guard<std::mutex> mapLock(g_archiveMutex);
            g_pinnedOriginalArchives.insert(archive);
            for (auto& [key, known] : g_mountedArchives)
            {
                if (known.archiveRecord == archive)
                {
                    known.publicHandle          = writableRecord;
                    known.archiveRecord         = writableRecord;
                    known.writable              = true;
                    known.originalArchiveRecord = archive;
                }
            }
        }

        LOG_INFO << "ClientArchive: mounted writable view over existing archive: " << archive->m_name;
        return true;
    }
} // namespace

namespace ClientArchive
{
    bool FindMounted(const std::string& path, MountedArchive& out)
    {
        const std::string normalized = NormalizePath(path);
        const std::string absolute   = AbsolutePath(path.c_str());

        {
            std::lock_guard<std::mutex> lock(g_archiveMutex);
            auto found = !absolute.empty() ? g_mountedArchives.find(absolute) : g_mountedArchives.end();
            if (found == g_mountedArchives.end())
                found = g_mountedArchives.find(normalized);
            if (found != g_mountedArchives.end())
            {
                out = found->second;
                if (!out.writable && ResolveWritableRedirect(out))
                    found->second = out;
                return out.archiveRecord != nullptr;
            }
        }

        return RememberDiscoveredArchive(path, FindArchiveRecord(path), out);
    }

    bool EnsureMountedWritable(const std::string& path, MountedArchive& out, std::string& error)
    {
        if (!FindMounted(path, out))
        {
            error = "the game did not mount " + std::filesystem::path(path).filename().string();
            return false;
        }
        if (out.writable)
            return true;

        auto request     = std::make_shared<WritableRequest>();
        request->path    = path;
        request->mounted = out;
        std::unique_lock<std::mutex> lock(g_writableRequestMutex);
        g_writableRequests.push_back(request);
        if (!request->changed.wait_for(lock, std::chrono::seconds(30), [&request] { return request->complete; }))
        {
            if (!request->started)
            {
                g_writableRequests.erase(std::remove(g_writableRequests.begin(), g_writableRequests.end(), request),
                                         g_writableRequests.end());
                error = "The client could not prepare the update while busy. Please restart the client.";
                return false;
            }
            request->changed.wait(lock, [&request] { return request->complete; });
        }
        out   = request->mounted;
        error = request->error;
        return error.empty() && out.writable;
    }

    void PumpWritableRequests()
    {
        std::vector<std::shared_ptr<WritableRequest>> requests;
        {
            std::lock_guard<std::mutex> lock(g_writableRequestMutex);
            requests.swap(g_writableRequests);
            for (const auto& request : requests)
                request->started = true;
        }

        for (const auto& request : requests)
        {
            UpgradeMountedArchive(request->path, request->mounted, request->error);
            {
                std::lock_guard<std::mutex> lock(g_writableRequestMutex);
                request->complete = true;
            }
            request->changed.notify_one();
        }
    }
} // namespace ClientArchive
