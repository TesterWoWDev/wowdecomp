#pragma once

#include <Windows.h>
#include <filesystem>
#include <string>

namespace ContentStreaming
{
    // A handle lock, rather than a mutex owned by the download worker thread,
    // survives worker completion and protects mounted archives until exit.
    class UpdateLock
    {
    public:
        ~UpdateLock() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
        UpdateLock() = default;
        UpdateLock(const UpdateLock&) = delete;
        UpdateLock& operator=(const UpdateLock&) = delete;

        bool Acquire(const std::filesystem::path& gameRoot, std::string& error, DWORD timeoutMs = 0)
        {
            if (handle_ != INVALID_HANDLE_VALUE)
                return true;
            const auto path = gameRoot / ".duskhaven-streaming.lock";
            const ULONGLONG started = GetTickCount64();
            do
            {
                handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE |
                                          FILE_FLAG_OPEN_REPARSE_POINT,
                                      nullptr);
                if (handle_ != INVALID_HANDLE_VALUE)
                {
                    BY_HANDLE_FILE_INFORMATION info{};
                    if (GetFileInformationByHandle(handle_, &info) && info.nNumberOfLinks == 1 &&
                        !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                        return true;
                    CloseHandle(handle_);
                    handle_ = INVALID_HANDLE_VALUE;
                    error = "The game update lock is not a regular file.";
                    return false;
                }
                const DWORD code = GetLastError();
                if ((code != ERROR_SHARING_VIOLATION && code != ERROR_ACCESS_DENIED) ||
                    GetTickCount64() - started >= timeoutMs)
                    break;
                Sleep(50);
            } while (true);
            error = "Another client or updater is using this game folder. Close it and restart this client.";
            return false;
        }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };
}
