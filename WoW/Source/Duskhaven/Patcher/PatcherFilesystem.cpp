#include "PatcherFilesystem.h"
#include "PatcherHash.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace Patcher::Filesystem
{
    namespace
    {
        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

    } // namespace

    std::string LastErrorString(const char* prefix)
    {
        std::ostringstream stream;
        stream << prefix << " (win32=" << GetLastError() << ")";
        return stream.str();
    }

    bool EqualsIgnoreCase(const std::string& a, const std::string& b)
    {
        return ToLowerAscii(a) == ToLowerAscii(b);
    }

    std::string GetExeDirectory()
    {
        char path[MAX_PATH] = {};
        const DWORD length  = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return "";

        for (int i = static_cast<int>(length); i > 0; --i)
        {
            if (path[i - 1] == '\\' || path[i - 1] == '/')
            {
                path[i] = '\0';
                return path;
            }
        }
        return "";
    }

    bool ReadTextFile(const std::string& path, std::string& out, std::string& error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            error = "failed to open " + path;
            return false;
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        if (!file.good() && !file.eof())
        {
            error = "failed to read " + path;
            return false;
        }

        out = stream.str();
        return true;
    }

    bool WriteTextFile(const std::string& path, const std::string& text, std::string& error)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = "failed to open " + path;
            return false;
        }

        file << text;
        if (!file)
        {
            error = "failed to write " + path;
            return false;
        }

        return true;
    }

    bool HashFileSha256(const std::string& path, std::string& outHash, uint64_t& outSize, std::string& error)
    {
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = LastErrorString(("failed to open " + path).c_str());
            return false;
        }

        LARGE_INTEGER fileSize = {};
        if (!GetFileSizeEx(file, &fileSize))
        {
            error = LastErrorString(("failed to get file size for " + path).c_str());
            CloseHandle(file);
            return false;
        }
        if (fileSize.QuadPart < 0)
        {
            error = "invalid negative file size for " + path;
            CloseHandle(file);
            return false;
        }
        outSize = static_cast<uint64_t>(fileSize.QuadPart);

        Hash::Sha256 hash;
        if (!hash.Begin(error))
        {
            CloseHandle(file);
            return false;
        }

        constexpr DWORD BufferSize = 64 * 1024;
        std::vector<unsigned char> buffer(BufferSize);
        while (true)
        {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), BufferSize, &read, nullptr))
            {
                error = LastErrorString(("failed to read " + path).c_str());
                CloseHandle(file);
                return false;
            }

            if (read == 0)
                break;

            if (!hash.Update(buffer.data(), read, error))
            {
                CloseHandle(file);
                return false;
            }
        }

        CloseHandle(file);
        return hash.Finish(outHash, error);
    }

    bool HashFileMd5(const std::string& path, std::string& outHash, uint64_t& outSize, std::string& error)
    {
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = LastErrorString(("failed to open " + path).c_str());
            return false;
        }

        LARGE_INTEGER fileSize = {};
        if (!GetFileSizeEx(file, &fileSize))
        {
            error = LastErrorString(("failed to get file size for " + path).c_str());
            CloseHandle(file);
            return false;
        }
        if (fileSize.QuadPart < 0)
        {
            error = "invalid negative file size for " + path;
            CloseHandle(file);
            return false;
        }
        outSize = static_cast<uint64_t>(fileSize.QuadPart);

        Hash::Md5 hash;
        if (!hash.Begin(error))
        {
            CloseHandle(file);
            return false;
        }

        constexpr DWORD BufferSize = 64 * 1024;
        std::vector<unsigned char> buffer(BufferSize);
        while (true)
        {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), BufferSize, &read, nullptr))
            {
                error = LastErrorString(("failed to read " + path).c_str());
                CloseHandle(file);
                return false;
            }
            if (read == 0)
                break;
            if (!hash.Update(buffer.data(), read, error))
            {
                CloseHandle(file);
                return false;
            }
        }

        CloseHandle(file);
        return hash.Finish(outHash, error);
    }

} // namespace Patcher::Filesystem
