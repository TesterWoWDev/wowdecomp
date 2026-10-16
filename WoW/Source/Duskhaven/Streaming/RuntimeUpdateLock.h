#pragma once

#include <Windows.h>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace RuntimeUpdate
{
    inline bool FlushFile(const std::filesystem::path& filename)
    {
        HANDLE file = CreateFileW(filename.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        BY_HANDLE_FILE_INFORMATION info{};
        const bool flushed = GetFileInformationByHandle(file, &info) && info.nNumberOfLinks == 1 &&
            !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            FlushFileBuffers(file) != FALSE;
        CloseHandle(file);
        return flushed;
    }

    inline bool ReadSelectedRuntime(const std::filesystem::path& updateRoot, std::wstring& version)
    {
        const std::filesystem::path marker = updateRoot / L"runtime.current";
        const DWORD attributes = GetFileAttributesW(marker.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
            return false;
        std::error_code error;
        if (std::filesystem::file_size(marker, error) != 84 || error)
            return false;
        std::ifstream stream(marker, std::ios::binary);
        std::string header;
        std::string manifest;
        std::string extra;
        if (!std::getline(stream, header) || header != "version=1" || !std::getline(stream, manifest) ||
            manifest.size() != 73 || manifest.rfind("manifest=", 0) != 0 || std::getline(stream, extra))
            return false;
        for (size_t index = 9; index < manifest.size(); ++index)
        {
            const char character = manifest[index];
            if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F')))
                return false;
        }
        version.assign(manifest.begin() + 9, manifest.end());
        return stream.eof();
    }

    // Both the running client and the detached installer mutate the transaction.
    // Scope their mutex to a normalized client path so separate installations
    // do not block one another while downloads are in progress.
    class TransactionLock
    {
    public:
        TransactionLock() = default;
        TransactionLock(const TransactionLock&) = delete;
        TransactionLock& operator=(const TransactionLock&) = delete;

        ~TransactionLock()
        {
            if (_owned)
                ReleaseMutex(_handle);
            if (_handle)
                CloseHandle(_handle);
        }

        bool Acquire(const std::filesystem::path& gameRoot, DWORD timeout = 120000)
        {
            if (_handle || !gameRoot.is_absolute())
                return false;

            std::vector<wchar_t> path(32768);
            const DWORD length = GetFullPathNameW(gameRoot.c_str(), static_cast<DWORD>(path.size()),
                                                   path.data(), nullptr);
            if (length == 0 || length >= path.size())
                return false;

            std::wstring normalized(path.data(), length);
            while (normalized.size() > 3 && (normalized.back() == L'\\' || normalized.back() == L'/'))
                normalized.pop_back();
            uint64_t key = 14695981039346656037ull;
            for (const wchar_t character : normalized)
            {
                const wchar_t lower = character == L'/' ? L'\\' : static_cast<wchar_t>(std::towlower(character));
                key ^= static_cast<uint16_t>(lower);
                key *= 1099511628211ull;
            }

            const std::wstring name = L"Local\\DuskhavenClientRuntimeInstall-v2-" + std::to_wstring(key);
            _handle = CreateMutexW(nullptr, FALSE, name.c_str());
            if (!_handle)
                return false;
            const DWORD wait = WaitForSingleObject(_handle, timeout);
            _owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
            return _owned;
        }

    private:
        HANDLE _handle = nullptr;
        bool _owned = false;
    };
}
