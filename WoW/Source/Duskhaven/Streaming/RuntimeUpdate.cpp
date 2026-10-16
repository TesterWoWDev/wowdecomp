#include "RuntimeUpdate.h"
#include "RuntimeUpdateLock.h"

#include <Patcher/PatcherFilesystem.h>
#include <Patcher/PatcherHash.h>
#include <Patcher/PatcherHttp.h>
#include <Patcher/PatcherJson.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace
{
    constexpr size_t RuntimeManifestLimit = 256 * 1024;
    constexpr uint64_t MaximumRuntimeFileSize = 256ull * 1024 * 1024;
    constexpr uint64_t MaximumRuntimeSize = 512ull * 1024 * 1024;
    constexpr std::array<const char*, 5> RuntimeNames = {
        "ClientExtensions.dll",
        "ClientExtensions64.exe",
        "DuskhavenBackgroundDownloader.exe",
        "d3d9.dll",
        "discord_partner_sdk.dll",
    };

    struct RuntimeFile
    {
        std::string name;
        std::string objectPath;
        uint64_t size = 0;
        std::string sha256;
    };

    bool IsHexSha256(const std::string& value)
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char character)
        {
            return std::isxdigit(character) != 0;
        });
    }

    bool IsAllowedName(const std::string& value)
    {
        return std::any_of(RuntimeNames.begin(), RuntimeNames.end(), [&](const char* name) { return value == name; });
    }

    bool JsonString(const Patcher::Json::Value& object, const char* key, std::string& value)
    {
        const Patcher::Json::Value* field = object.Find(key);
        if (!field || field->type != Patcher::Json::Type::String)
            return false;
        value = field->string;
        return true;
    }

    bool JsonUnsigned(const Patcher::Json::Value& object, const char* key, uint64_t& value)
    {
        const Patcher::Json::Value* field = object.Find(key);
        if (!field || field->type != Patcher::Json::Type::Number || field->number.empty() ||
            !std::all_of(field->number.begin(), field->number.end(),
                         [](unsigned char character) { return std::isdigit(character) != 0; }))
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            value = std::stoull(field->number, &consumed, 10);
            return consumed == field->number.size();
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseManifest(const std::string& text, std::vector<RuntimeFile>& files, std::string& error)
    {
        Patcher::Json::Value root;
        if (!Patcher::Json::Parse(text, root, error) || root.type != Patcher::Json::Type::Object ||
            root.object.size() != 2)
        {
            error = "runtime.json must contain one supported object";
            return false;
        }
        uint64_t schema = 0;
        if (!JsonUnsigned(root, "schema", schema) || schema != 1)
        {
            error = "runtime.json has an unsupported schema";
            return false;
        }
        const Patcher::Json::Value* list = root.Find("files");
        if (!list || list->type != Patcher::Json::Type::Array || list->array.size() != RuntimeNames.size())
        {
            error = "runtime.json must contain exactly five files";
            return false;
        }

        std::set<std::string> names;
        uint64_t totalSize = 0;
        for (const Patcher::Json::Value& value : list->array)
        {
            RuntimeFile file;
            if (value.type != Patcher::Json::Type::Object || value.object.size() != 4 ||
                !JsonString(value, "name", file.name) || !JsonUnsigned(value, "size", file.size) ||
                !JsonString(value, "sha256", file.sha256) || !JsonString(value, "object", file.objectPath) ||
                !IsAllowedName(file.name) || !names.insert(file.name).second || file.size == 0 ||
                file.size > MaximumRuntimeFileSize || !IsHexSha256(file.sha256))
            {
                error = "runtime.json contains an invalid runtime file";
                return false;
            }
            const std::string expectedObject =
                "objects/" + file.sha256 + "/runtime/" + file.name;
            if (file.objectPath != expectedObject || totalSize > MaximumRuntimeSize - file.size)
            {
                error = "runtime.json contains an invalid runtime object path or size";
                return false;
            }
            totalSize += file.size;
            files.push_back(std::move(file));
        }
        if (totalSize > MaximumRuntimeSize)
        {
            error = "runtime.json exceeds the runtime size limit";
            return false;
        }
        for (const char* name : RuntimeNames)
        {
            if (names.count(name) == 0)
            {
                error = std::string("runtime.json is missing ") + name;
                return false;
            }
        }
        return true;
    }

    bool HashText(const std::string& text, std::string& hash, std::string& error)
    {
        Patcher::Hash::Sha256 sha256;
        return sha256.Begin(error) && sha256.Update(text.data(), static_cast<uint32_t>(text.size()), error) &&
               sha256.Finish(hash, error);
    }

    bool IsPlainFile(const std::filesystem::path& filename)
    {
        const DWORD attributes = GetFileAttributesW(filename.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
               (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    bool IsPlainDirectory(const std::filesystem::path& directory)
    {
        const DWORD attributes = GetFileAttributesW(directory.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
               (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    bool EnsurePlainDirectory(const std::filesystem::path& directory, std::string& error)
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(directory, filesystemError);
        if (filesystemError || !IsPlainDirectory(directory))
        {
            error = "could not create a safe runtime staging directory";
            return false;
        }
        return true;
    }

    bool FileMatches(const std::filesystem::path& filename, const RuntimeFile& file)
    {
        if (!IsPlainFile(filename))
            return false;
        uint64_t size = 0;
        std::string hash;
        std::string error;
        return Patcher::Filesystem::HashFileSha256(filename.string(), hash, size, error) && size == file.size &&
               Patcher::Filesystem::EqualsIgnoreCase(hash, file.sha256);
    }

    bool IsPeFile(const std::filesystem::path& filename)
    {
        std::ifstream stream(filename, std::ios::binary);
        char magic[2] = {};
        return stream.read(magic, sizeof(magic)) && magic[0] == 'M' && magic[1] == 'Z';
    }

    // The five runtime files are bootstrap artifacts that content streaming
    // refreshes on its own schedule; they only have to be launchable. Judging an
    // installed file by presence, expected size, and a valid image keeps a
    // locally rebuilt binary of identical size from forcing a self-update.
    // Every downloaded, staged, or promoted byte is still verified with the
    // exact SHA-256 from runtime.json.
    bool InstalledRuntimeFileUsable(const std::filesystem::path& filename, const RuntimeFile& file)
    {
        if (!IsPlainFile(filename) || !IsPeFile(filename))
            return false;
        std::error_code sizeError;
        const uint64_t size = std::filesystem::file_size(filename, sizeError);
        return !sizeError && size == file.size;
    }

    bool WriteAtomically(const std::filesystem::path& filename, const std::string& contents, std::string& error)
    {
        const std::filesystem::path temporary(filename.wstring() + L".tmp");
        const DWORD attributes = GetFileAttributesW(temporary.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (!IsPlainFile(temporary) || !DeleteFileW(temporary.c_str())))
        {
            error = "runtime staging contains unsafe or locked transaction metadata";
            return false;
        }
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream || !(stream << contents) || !stream.flush())
            {
                error = "could not write runtime transaction metadata";
                DeleteFileW(temporary.c_str());
                return false;
            }
        }
        if (!RuntimeUpdate::FlushFile(temporary) ||
            !MoveFileExW(temporary.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            error = Patcher::Filesystem::LastErrorString("could not commit runtime transaction metadata");
            DeleteFileW(temporary.c_str());
            return false;
        }
        return true;
    }

    std::wstring QuoteCommandLineArgument(const std::wstring& value)
    {
        std::wstring quoted;
        quoted.reserve(value.size() + 2);
        quoted.push_back(L'"');

        size_t backslashes = 0;
        for (const wchar_t character : value)
        {
            if (character == L'\\')
            {
                ++backslashes;
                continue;
            }
            if (character == L'"')
            {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'"');
                backslashes = 0;
                continue;
            }
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(character);
        }

        // Backslashes immediately before the closing quote must be doubled or
        // CommandLineToArgvW treats the quote as escaped and consumes the next
        // option into this value.
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;
    }

    bool AllRuntimeFilesUsable(const std::filesystem::path& gameRoot, const std::vector<RuntimeFile>& files)
    {
        return std::all_of(files.begin(), files.end(), [&](const RuntimeFile& file)
        {
            return InstalledRuntimeFileUsable(gameRoot / file.name, file);
        });
    }

    bool RuntimePathIsAbsent(const std::filesystem::path& path)
    {
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            return false;
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    bool RemovePlainRuntimeFile(const std::filesystem::path& path)
    {
        if (RuntimePathIsAbsent(path))
            return true;
        if (!IsPlainFile(path))
            return false;
        return DeleteFileW(path.c_str()) != FALSE || RuntimePathIsAbsent(path);
    }

    bool RemoveEmptyRuntimeDirectory(const std::filesystem::path& path)
    {
        if (RuntimePathIsAbsent(path))
            return true;
        return IsPlainDirectory(path) && RemoveDirectoryW(path.c_str()) != FALSE;
    }

    bool RemoveRuntimePayloadDirectory(const std::filesystem::path& directory)
    {
        if (RuntimePathIsAbsent(directory))
            return true;
        if (!IsPlainDirectory(directory))
            return false;

        std::error_code filesystemError;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(directory, filesystemError))
        {
            if (filesystemError || !IsAllowedName(entry.path().filename().string()) ||
                !RemovePlainRuntimeFile(entry.path()))
                return false;
        }
        return !filesystemError && RemoveEmptyRuntimeDirectory(directory);
    }

    void RetryCompletedTransactionCleanup(const std::filesystem::path& gameRoot,
                                          const std::string& manifestSha256,
                                          const std::vector<RuntimeFile>& files)
    {
        const std::filesystem::path updateRoot = gameRoot / ".dhupdate";
        if (RuntimePathIsAbsent(updateRoot) || !IsPlainDirectory(updateRoot))
            return;

        const std::filesystem::path releasesRoot = updateRoot / "releases";
        if (RuntimePathIsAbsent(releasesRoot) || !IsPlainDirectory(releasesRoot))
            return;

        std::wstring selectedVersion;
        if (!RuntimePathIsAbsent(updateRoot / "runtime.current") &&
            (!RuntimeUpdate::ReadSelectedRuntime(updateRoot, selectedVersion) ||
             selectedVersion != std::wstring(manifestSha256.begin(), manifestSha256.end())))
            return;

        // The caller has independently verified that the complete installed
        // runtime matches the selected manifest. Superseded transactions can
        // also be discarded; no other helper can use them while this lock is
        // held, and its selection check prevents reinstalling them afterward.
        std::error_code filesystemError;
        std::vector<std::filesystem::path> completedRoots;
        for (std::filesystem::directory_iterator entry(releasesRoot, filesystemError), end;
             !filesystemError && entry != end; entry.increment(filesystemError))
        {
            if (IsHexSha256(entry->path().filename().string()) && IsPlainDirectory(entry->path()))
                completedRoots.push_back(entry->path());
        }
        for (const std::filesystem::path& releaseRoot : completedRoots)
        {
            RemoveRuntimePayloadDirectory(releaseRoot / "backup.pending");
            RemoveRuntimePayloadDirectory(releaseRoot / "backup");
            for (const RuntimeFile& file : files)
            {
                RemovePlainRuntimeFile(releaseRoot / file.name);
                RemovePlainRuntimeFile(releaseRoot / (file.name + ".part"));
            }
            RemovePlainRuntimeFile(releaseRoot / "runtime.plan.tmp");
            RemovePlainRuntimeFile(releaseRoot / "runtime.plan");
            RemovePlainRuntimeFile(releaseRoot / "runtime.installing.tmp");
            RemovePlainRuntimeFile(releaseRoot / "runtime.installing");
            RemovePlainRuntimeFile(releaseRoot / (L"RuntimeUpdater-" +
                releaseRoot.filename().wstring().substr(0, 12) + L".exe"));
            RemoveEmptyRuntimeDirectory(releaseRoot);
        }
        RemovePlainRuntimeFile(updateRoot / "runtime.current.tmp");
        RemovePlainRuntimeFile(updateRoot / "runtime.current");
        RemoveEmptyRuntimeDirectory(releasesRoot);
        RemoveEmptyRuntimeDirectory(updateRoot);
    }

    bool PrepareTemporaryRunner(const std::filesystem::path& source, const std::string& manifestSha256,
                                const RuntimeFile& promoter, std::filesystem::path& runner,
                                std::string& error)
    {
        std::vector<wchar_t> temporaryDirectory(32768);
        const DWORD temporaryLength = GetTempPathW(static_cast<DWORD>(temporaryDirectory.size()),
                                                    temporaryDirectory.data());
        if (temporaryLength == 0 || temporaryLength >= temporaryDirectory.size())
        {
            error = Patcher::Filesystem::LastErrorString("could not locate the runtime installer temp directory");
            return false;
        }

        const std::filesystem::path temporaryRoot(std::wstring(temporaryDirectory.data(), temporaryLength));
        const std::wstring prefix = L"DuskhavenRuntimeUpdater-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" +
            std::wstring(manifestSha256.begin(), manifestSha256.begin() + 12);
        for (unsigned attempt = 0; attempt < 32; ++attempt)
        {
            runner = temporaryRoot / (prefix + L"-" + std::to_wstring(attempt) + L".exe");
            if (CopyFileW(source.c_str(), runner.c_str(), TRUE))
            {
                if (FileMatches(runner, promoter))
                    return true;
                DeleteFileW(runner.c_str());
                error = "the temporary runtime installer failed verification";
                return false;
            }

            const DWORD copyError = GetLastError();
            if (copyError != ERROR_FILE_EXISTS && copyError != ERROR_ALREADY_EXISTS)
            {
                SetLastError(copyError);
                error = Patcher::Filesystem::LastErrorString("could not prepare the runtime installer");
                return false;
            }
        }

        SetLastError(ERROR_FILE_EXISTS);
        error = Patcher::Filesystem::LastErrorString("could not reserve a temporary runtime installer");
        return false;
    }

    void DiscardTemporaryRunner(const std::filesystem::path& runner)
    {
        if (!runner.empty() && !DeleteFileW(runner.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
            MoveFileExW(runner.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    bool ResumeDetachedProcess(PROCESS_INFORMATION& process, HANDLE readyEvent,
                               DWORD& failure, DWORD& childExitCode)
    {
        BOOL inJob = TRUE;
        if (!IsProcessInJob(process.hProcess, nullptr, &inJob))
        {
            failure = GetLastError();
        }
        else if (inJob)
        {
            failure = ERROR_ACCESS_DENIED;
        }
        else if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
        {
            failure = GetLastError();
        }
        else
        {
            // The helper signals only after it has parsed and validated its
            // arguments and opened the WoW process it will wait for. This avoids
            // a fixed startup delay for every staged archive while still
            // rejecting loader and initialization failures.
            HANDLE startupHandles[] = { readyEvent, process.hProcess };
            const DWORD startupState = WaitForMultipleObjects(2, startupHandles, FALSE, 5000);
            if (startupState == WAIT_OBJECT_0)
            {
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                process = {};
                return true;
            }
            if (startupState == WAIT_FAILED)
                failure = GetLastError();
            else if (startupState == WAIT_OBJECT_0 + 1)
            {
                if (!GetExitCodeProcess(process.hProcess, &childExitCode))
                    childExitCode = STILL_ACTIVE;
                failure = ERROR_PROCESS_ABORTED;
            }
            else
                failure = ERROR_TIMEOUT;
        }

        // Never let an updater that is still tied to WoW's job begin waiting.
        // A kill-on-close launcher job would terminate it alongside the client
        // and leave the same update staged forever.
        const DWORD processState = WaitForSingleObject(process.hProcess, 0);
        if (processState == WAIT_TIMEOUT || processState == WAIT_FAILED)
        {
            if (!TerminateProcess(process.hProcess, failure))
                failure = GetLastError();
            else if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0)
                failure = ERROR_TIMEOUT;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        process = {};
        return false;
    }

    bool SameSecurityContext(HANDLE otherProcess, DWORD& failure)
    {
        HANDLE currentToken = nullptr;
        HANDLE otherToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &currentToken) ||
            !OpenProcessToken(otherProcess, TOKEN_QUERY, &otherToken))
        {
            failure = GetLastError();
            if (currentToken)
                CloseHandle(currentToken);
            if (otherToken)
                CloseHandle(otherToken);
            return false;
        }

        TOKEN_ELEVATION_TYPE currentElevation = TokenElevationTypeDefault;
        TOKEN_ELEVATION_TYPE otherElevation = TokenElevationTypeDefault;
        TOKEN_STATISTICS currentStatistics{};
        TOKEN_STATISTICS otherStatistics{};
        DWORD returned = 0;
        const bool tokenInfoRead =
            GetTokenInformation(currentToken, TokenElevationType, &currentElevation,
                                sizeof(currentElevation), &returned) != FALSE &&
            GetTokenInformation(otherToken, TokenElevationType, &otherElevation,
                                sizeof(otherElevation), &returned) != FALSE &&
            GetTokenInformation(currentToken, TokenStatistics, &currentStatistics,
                                sizeof(currentStatistics), &returned) != FALSE &&
            GetTokenInformation(otherToken, TokenStatistics, &otherStatistics,
                                sizeof(otherStatistics), &returned) != FALSE;
        if (!tokenInfoRead)
        {
            failure = GetLastError();
            CloseHandle(otherToken);
            CloseHandle(currentToken);
            return false;
        }

        CloseHandle(otherToken);
        CloseHandle(currentToken);
        if (currentElevation != otherElevation ||
            currentStatistics.AuthenticationId.HighPart != otherStatistics.AuthenticationId.HighPart ||
            currentStatistics.AuthenticationId.LowPart != otherStatistics.AuthenticationId.LowPart)
        {
            // PROC_THREAD_ATTRIBUTE_PARENT_PROCESS also inherits the selected
            // parent's process token. Refuse to silently downgrade an elevated
            // client or cross into another logon session.
            failure = ERROR_ELEVATION_REQUIRED;
            return false;
        }
        return true;
    }

    bool LaunchThroughShellParent(const std::filesystem::path& runner, const std::wstring& command,
                                  const std::filesystem::path& gameRoot, HANDLE readyEvent,
                                  DWORD& failure, DWORD& childExitCode)
    {
        const HWND shellWindow = GetShellWindow();
        if (!shellWindow)
        {
            failure = ERROR_NOT_FOUND;
            return false;
        }

        DWORD shellProcessId = 0;
        GetWindowThreadProcessId(shellWindow, &shellProcessId);
        if (shellProcessId == 0)
        {
            failure = ERROR_NOT_FOUND;
            return false;
        }

        HANDLE shellProcess = OpenProcess(PROCESS_CREATE_PROCESS | PROCESS_QUERY_LIMITED_INFORMATION,
                                          FALSE, shellProcessId);
        if (!shellProcess)
        {
            failure = GetLastError();
            return false;
        }

        BOOL shellInJob = TRUE;
        if (!IsProcessInJob(shellProcess, nullptr, &shellInJob))
        {
            failure = GetLastError();
            CloseHandle(shellProcess);
            return false;
        }
        if (shellInJob)
        {
            failure = ERROR_ACCESS_DENIED;
            CloseHandle(shellProcess);
            return false;
        }
        if (!SameSecurityContext(shellProcess, failure))
        {
            CloseHandle(shellProcess);
            return false;
        }

        SIZE_T attributeBytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
        if (attributeBytes == 0)
        {
            failure = GetLastError();
            CloseHandle(shellProcess);
            return false;
        }

        std::vector<unsigned char> attributeStorage(attributeBytes);
        auto* attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeBytes))
        {
            failure = GetLastError();
            CloseHandle(shellProcess);
            return false;
        }

        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                                       &shellProcess, sizeof(shellProcess), nullptr, nullptr))
        {
            failure = GetLastError();
            DeleteProcThreadAttributeList(attributes);
            CloseHandle(shellProcess);
            return false;
        }

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.lpAttributeList = attributes;

        std::vector<wchar_t> commandBuffer(command.begin(), command.end());
        commandBuffer.push_back(L'\0');
        PROCESS_INFORMATION process{};
        const bool started = CreateProcessW(
            runner.c_str(), commandBuffer.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, gameRoot.c_str(),
            &startup.StartupInfo, &process) != FALSE;
        failure = started ? ERROR_SUCCESS : GetLastError();

        DeleteProcThreadAttributeList(attributes);
        CloseHandle(shellProcess);
        if (!started)
            return false;
        return ResumeDetachedProcess(process, readyEvent, failure, childExitCode);
    }

    bool ScheduleInstaller(const std::filesystem::path& gameRoot, const std::filesystem::path& releaseRoot,
                           const std::filesystem::path& plan, const std::string& manifestSha256,
                           const RuntimeFile& promoter, std::string& error)
    {
        const std::filesystem::path stagedPromoter = releaseRoot / promoter.name;
        const std::filesystem::path installedPromoter = gameRoot / promoter.name;
        const std::filesystem::path promoterSource = IsPlainFile(stagedPromoter) ? stagedPromoter : installedPromoter;
        if (!FileMatches(promoterSource, promoter))
        {
            error = "the runtime installer is missing or does not match runtime.json";
            return false;
        }

        // The updater must not execute from inside .dhupdate: Windows keeps a
        // running executable locked, which otherwise prevents removing the
        // completed transaction directory. A unique verified copy in the
        // user's temp directory can clean .dhupdate after installation.
        const std::filesystem::path legacyRunner = releaseRoot / (L"RuntimeUpdater-" +
            std::wstring(manifestSha256.begin(), manifestSha256.begin() + 12) + L".exe");
        if (!RemovePlainRuntimeFile(legacyRunner))
        {
            error = "a previous runtime installer is still using the staged transaction";
            return false;
        }
        std::filesystem::path runner;
        if (!PrepareTemporaryRunner(promoterSource, manifestSha256, promoter, runner, error))
            return false;

        const std::wstring command = QuoteCommandLineArgument(runner.wstring()) + L" --wait-pid " +
            std::to_wstring(GetCurrentProcessId()) + L" --runtime-plan " +
            QuoteCommandLineArgument(plan.wstring()) + L" --game-root " +
            QuoteCommandLineArgument(gameRoot.wstring()) + L" --runtime-version " +
            std::wstring(manifestSha256.begin(), manifestSha256.end());
        if (RuntimeUpdate::LaunchDetachedHelper(runner, command, gameRoot, error))
            return true;

        DiscardTemporaryRunner(runner);
        return false;
    }
} // namespace

namespace RuntimeUpdate
{
    bool LaunchDetachedHelper(const std::filesystem::path& executable, const std::wstring& command,
                              const std::filesystem::path& workingDirectory, std::string& error)
    {
        std::error_code pathError;
        if (std::filesystem::equivalent(executable.parent_path(), workingDirectory, pathError))
        {
            // Archive helpers wait alongside the runtime helper. Executing the
            // installed EXE would keep it locked while the runtime helper owns
            // the content lock and tries to replace it. Run a verified copy for
            // every detached helper, not only runtime self-updates.
            RuntimeFile helper;
            helper.name = executable.filename().string();
            if (helper.name != "DuskhavenBackgroundDownloader.exe" || !IsPlainFile(executable) ||
                !Patcher::Filesystem::HashFileSha256(executable.string(), helper.sha256, helper.size, error) ||
                helper.size == 0 || helper.size > MaximumRuntimeFileSize)
            {
                error = "could not verify the detached archive helper";
                return false;
            }
            std::filesystem::path runner;
            if (!PrepareTemporaryRunner(executable, helper.sha256, helper, runner, error))
                return false;
            // CreateProcessW selects executable explicitly; argv[0] in command
            // remains informational and all transaction arguments stay intact.
            if (LaunchDetachedHelper(runner, command, workingDirectory, error))
                return true;
            DiscardTemporaryRunner(runner);
            return false;
        }
        if (pathError)
        {
            error = "could not validate the detached helper directory";
            return false;
        }
        static std::atomic_uint64_t readySequence{ 0 };
        const std::wstring readyEventName = L"Local\\DuskhavenHelperReady-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" +
            std::to_wstring(readySequence.fetch_add(1, std::memory_order_relaxed));
        HANDLE readyEvent = CreateEventW(nullptr, TRUE, FALSE, readyEventName.c_str());
        if (!readyEvent)
        {
            error = Patcher::Filesystem::LastErrorString("could not create the detached helper handshake");
            return false;
        }

        const std::wstring launchCommand = command + L" --ready-event " +
            QuoteCommandLineArgument(readyEventName);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;

        std::vector<wchar_t> commandBuffer(launchCommand.begin(), launchCommand.end());
        commandBuffer.push_back(L'\0');
        PROCESS_INFORMATION process{};
        DWORD failure = ERROR_GEN_FAILURE;
        DWORD childExitCode = STILL_ACTIVE;
        ResetEvent(readyEvent);
        if (CreateProcessW(executable.c_str(), commandBuffer.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                           nullptr, workingDirectory.c_str(), &startup, &process) != FALSE)
        {
            if (ResumeDetachedProcess(process, readyEvent, failure, childExitCode))
            {
                CloseHandle(readyEvent);
                return true;
            }
        }
        else
        {
            failure = GetLastError();
        }

        // Some launchers place WoW in a kill-on-close job that forbids
        // CREATE_BREAKAWAY_FROM_JOB. Inherit the ordinary desktop shell's job
        // association instead, then verify the helper is actually detached
        // before allowing it to run.
        childExitCode = STILL_ACTIVE;
        ResetEvent(readyEvent);
        if (LaunchThroughShellParent(executable, launchCommand, workingDirectory, readyEvent,
                                     failure, childExitCode))
        {
            CloseHandle(readyEvent);
            return true;
        }

        CloseHandle(readyEvent);

        if (failure == ERROR_PROCESS_ABORTED && childExitCode != STILL_ACTIVE)
        {
            error = "detached helper exited before startup (exit code " + std::to_string(childExitCode) + ")";
            return false;
        }
        SetLastError(failure);
        error = Patcher::Filesystem::LastErrorString("could not start a detached helper");
        return false;
    }

    bool Prepare(const ReleasePointer& release, const std::string& gameRootValue, bool forceRepair,
                 const ProgressCallbacks& progress, Result& result, std::string& error)
    {
        result = Result::Current;
        if (release.indexUrl.empty() && release.manifestUrl.empty() && release.manifestSha256.empty())
            return true;
        if (!release.Available() || !IsHexSha256(release.manifestSha256) ||
            !Patcher::Http::IsSecureOrLoopbackUrl(release.indexUrl) ||
            !Patcher::Http::IsSecureOrLoopbackUrl(release.manifestUrl) ||
            !Patcher::Http::HaveSameOrigin(release.indexUrl, release.manifestUrl))
        {
            error = "runtime manifest escaped the trusted update origin or has invalid metadata";
            return false;
        }

        std::filesystem::path gameRoot = std::filesystem::path(gameRootValue).lexically_normal();
        if (!gameRoot.has_filename() && gameRoot != gameRoot.root_path())
            gameRoot = gameRoot.parent_path();
        if (!gameRoot.is_absolute() || !IsPlainDirectory(gameRoot))
        {
            error = "the client runtime directory is unsafe or unavailable";
            return false;
        }
        TransactionLock transactionLock;
        if (!transactionLock.Acquire(gameRoot, 5000))
        {
            error = "another client is preparing or installing this runtime update; close it and retry";
            return false;
        }
        const std::filesystem::path updateRoot = gameRoot / ".dhupdate";

        std::string manifestText;
        if (!Patcher::Http::DownloadString(release.manifestUrl, RuntimeManifestLimit, manifestText, error))
            return false;
        std::string manifestHash;
        if (!HashText(manifestText, manifestHash, error) ||
            !Patcher::Filesystem::EqualsIgnoreCase(manifestHash, release.manifestSha256))
        {
            error = "runtime.json failed SHA-256 verification";
            return false;
        }

        std::vector<RuntimeFile> files;
        if (!ParseManifest(manifestText, files, error))
            return false;
        if (!forceRepair && AllRuntimeFilesUsable(gameRoot, files))
        {
            RetryCompletedTransactionCleanup(gameRoot, release.manifestSha256, files);
            return true;
        }

        std::vector<const RuntimeFile*> changed;
        for (const RuntimeFile& file : files)
        {
            if (forceRepair || !FileMatches(gameRoot / file.name, file))
                changed.push_back(&file);
        }
        if (changed.empty())
            return true;

        const std::filesystem::path releasesRoot = updateRoot / "releases";
        const std::filesystem::path releaseRoot = releasesRoot / release.manifestSha256;
        if (!EnsurePlainDirectory(updateRoot, error) || !EnsurePlainDirectory(releasesRoot, error) ||
            !EnsurePlainDirectory(releaseRoot, error))
            return false;

        uint64_t downloadBytes = 0;
        uint32_t downloadFiles = 0;
        for (const RuntimeFile* file : changed)
        {
            if (!FileMatches(releaseRoot / file->name, *file))
            {
                downloadBytes += file->size;
                ++downloadFiles;
            }
        }
        if (progress.beginDownloads)
            progress.beginDownloads(downloadBytes, downloadFiles);

        for (const RuntimeFile* file : changed)
        {
            const std::filesystem::path staged = releaseRoot / file->name;
            if (FileMatches(staged, *file))
                continue;

            const std::filesystem::path partial(staged.wstring() + L".part");
            uint64_t resumeFrom = 0;
            std::error_code filesystemError;
            const DWORD partialAttributes = GetFileAttributesW(partial.c_str());
            if (partialAttributes != INVALID_FILE_ATTRIBUTES &&
                ((partialAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
                 (partialAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0))
            {
                error = "runtime staging contains an unsafe partial file";
                return false;
            }
            if (std::filesystem::is_regular_file(partial, filesystemError))
            {
                resumeFrom = std::filesystem::file_size(partial, filesystemError);
                if (filesystemError || resumeFrom > file->size)
                {
                    DeleteFileW(partial.c_str());
                    resumeFrom = 0;
                }
            }
            if (progress.beginFile)
                progress.beginFile(file->name, resumeFrom);

            std::string objectUrl;
            if (!Patcher::Http::ResolveUrl(release.indexUrl, file->objectPath, objectUrl, error) ||
                !Patcher::Http::IsSecureOrLoopbackUrl(objectUrl) ||
                !Patcher::Http::HaveSameOrigin(release.indexUrl, objectUrl))
            {
                error = "runtime object URL escaped the trusted update origin";
                return false;
            }
            Patcher::Http::ResumeOptions options;
            options.resumeFrom = resumeFrom;
            options.expectedSize = file->size;
            options.onProgress = progress.updateFile;
            if (!Patcher::Http::DownloadFileResumable(objectUrl, partial.string(), options, error))
                return false;
            if (!FileMatches(partial, *file) || !IsPeFile(partial))
            {
                DeleteFileW(partial.c_str());
                error = "downloaded runtime file failed verification: " + file->name;
                return false;
            }
            if (!MoveFileExW(partial.c_str(), staged.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                error = Patcher::Filesystem::LastErrorString("could not stage downloaded runtime file");
                return false;
            }
            if (progress.completeFile)
                progress.completeFile(file->size);
        }

        // The post-exit installer applies one complete, verified runtime set. Files
        // that are already current are copied locally into the transaction so a
        // partial update can never stamp the release marker over a mixed set.
        for (const RuntimeFile& file : files)
        {
            const std::filesystem::path staged = releaseRoot / file.name;
            if (FileMatches(staged, file))
            {
                if (!FlushFile(staged))
                {
                    error = "could not persist the staged runtime file: " + file.name;
                    return false;
                }
                continue;
            }

            const DWORD stagedAttributes = GetFileAttributesW(staged.c_str());
            if (stagedAttributes != INVALID_FILE_ATTRIBUTES &&
                ((stagedAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
                 (stagedAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0))
            {
                error = "runtime staging contains an unsafe file";
                return false;
            }
            DeleteFileW(staged.c_str());
            const std::filesystem::path installed = gameRoot / file.name;
            if (!FileMatches(installed, file) || !CopyFileW(installed.c_str(), staged.c_str(), FALSE) ||
                !FileMatches(staged, file) || !FlushFile(staged))
            {
                DeleteFileW(staged.c_str());
                error = "could not stage the current runtime file: " + file.name;
                return false;
            }
        }

        std::ostringstream planText;
        planText << "version=1\n";
        planText << "manifest=" << release.manifestSha256 << '\n';
        for (const RuntimeFile& file : files)
            planText << file.name << '\t' << file.size << '\t' << file.sha256 << '\n';
        const std::filesystem::path plan = releaseRoot / "runtime.plan";
        if (!WriteAtomically(plan, planText.str(), error))
            return false;
        // A previously launched helper may still be waiting for another client
        // to exit. Only the selected transaction may install after that wait.
        if (!WriteAtomically(updateRoot / "runtime.current",
                             "version=1\nmanifest=" + release.manifestSha256 + "\n", error))
            return false;

        const auto installer = std::find_if(files.begin(), files.end(), [](const RuntimeFile& file)
        {
            return file.name == "DuskhavenBackgroundDownloader.exe";
        });
        if (installer == files.end() ||
            !ScheduleInstaller(gameRoot, releaseRoot, plan, release.manifestSha256, *installer, error))
            return false;

        result = Result::RestartRequired;
        return true;
    }
} // namespace RuntimeUpdate
