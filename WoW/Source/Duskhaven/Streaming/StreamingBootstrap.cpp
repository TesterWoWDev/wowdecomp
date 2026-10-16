#include "StreamingBootstrap.h"
#include <Patcher/PatcherHttp.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    constexpr size_t MAX_BOOTSTRAP_SIZE = 64 * 1024;

    std::string Trim(std::string value)
    {
        auto isWhitespace = [](unsigned char character) { return std::isspace(character) != 0; };
        value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isWhitespace));
        value.erase(std::find_if_not(value.rbegin(), value.rend(), isWhitespace).base(), value.end());
        return value;
    }

    std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }

    std::string GetClientRootFilePath(const char* filename)
    {
        if (!filename || !*filename)
            return {};

        char executablePath[MAX_PATH] = {};
        const DWORD length            = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return {};

        for (DWORD index = length; index > 0; --index)
        {
            if (executablePath[index - 1] == '\\' || executablePath[index - 1] == '/')
            {
                executablePath[index] = '\0';
                return std::string(executablePath) + filename;
            }
        }

        return {};
    }

    bool IsValidManifestUrl(const std::string& value)
    {
        if (value.size() > 2048)
            return false;

        const size_t schemeLength = value.compare(0, 7, "http://") == 0  ? 7 :
                                    value.compare(0, 8, "https://") == 0 ? 8 :
                                                                           0;
        if (schemeLength == 0 || value.size() <= schemeLength)
            return false;

        for (unsigned char character : value)
        {
            if (character <= 0x20 || character >= 0x7F)
                return false;
        }

        if (value.find('\\') != std::string::npos || value.find('@') != std::string::npos ||
            value.find('?') != std::string::npos || value.find('#') != std::string::npos ||
            value.find('%') != std::string::npos)
        {
            return false;
        }

        const size_t pathStart = value.find('/', schemeLength);
        if (pathStart == std::string::npos || pathStart == schemeLength)
            return false;

        constexpr const char* manifestSuffix = "/streaming.mfil";
        constexpr const char* latestSuffix   = "/latest.json";
        const size_t manifestLength          = std::char_traits<char>::length(manifestSuffix);
        const size_t latestLength            = std::char_traits<char>::length(latestSuffix);
        const bool supportedPath =
            (value.size() >= manifestLength &&
             value.compare(value.size() - manifestLength, manifestLength, manifestSuffix) == 0) ||
            (value.size() >= latestLength &&
             value.compare(value.size() - latestLength, latestLength, latestSuffix) == 0);
        return supportedPath && Patcher::Http::IsSecureOrLoopbackUrl(value);
    }
} // namespace

namespace StreamingBootstrap
{
    Status Inspect(const char* filename)
    {
        Status status;
        const std::string displayName = filename ? filename : "bootstrap";
        const std::string path        = GetClientRootFilePath(filename);
        if (path.empty())
        {
            status.detail = "client root unavailable";
            return status;
        }

        WIN32_FILE_ATTRIBUTE_DATA attributes = {};
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes))
        {
            status.detail = displayName + " not present";
            return status;
        }

        status.exists = true;
        if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            status.detail = displayName + " is a directory";
            return status;
        }
        if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            status.detail = displayName + " must not be a symbolic link or reparse point";
            return status;
        }

        ULARGE_INTEGER size = {};
        size.HighPart       = attributes.nFileSizeHigh;
        size.LowPart        = attributes.nFileSizeLow;
        if (size.QuadPart == 0 || size.QuadPart > MAX_BOOTSTRAP_SIZE)
        {
            status.detail = displayName + " has an invalid size";
            return status;
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            status.detail = displayName + " could not be opened";
            return status;
        }

        std::string contents(static_cast<size_t>(size.QuadPart), '\0');
        stream.read(&contents[0], static_cast<std::streamsize>(contents.size()));
        if (!stream || contents.find('\0') != std::string::npos)
        {
            status.detail = displayName + " could not be read as text";
            return status;
        }

        bool hasVersion   = false;
        bool validVersion = false;
        bool hasManifest  = false;
        std::istringstream lines(contents);
        for (std::string line; std::getline(lines, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            line = Trim(line);
            if (line.empty())
                continue;

            const size_t separator = line.find('=');
            if (separator == std::string::npos)
            {
                status.detail = displayName + " contains a malformed line";
                return status;
            }

            const std::string key   = Lowercase(Trim(line.substr(0, separator)));
            const std::string value = Trim(line.substr(separator + 1));
            if (key == "version")
            {
                if (hasVersion)
                {
                    status.detail = displayName + " contains duplicate version keys";
                    return status;
                }
                hasVersion   = true;
                validVersion = value == "1";
            }
            else if (key == "manifest")
            {
                if (hasManifest)
                {
                    status.detail = displayName + " contains duplicate manifest keys";
                    return status;
                }
                hasManifest        = true;
                status.manifestUrl = value;
            }
            else
            {
                status.detail = displayName + " contains an unsupported key";
                return status;
            }
        }

        if (!hasVersion || !validVersion)
        {
            status.detail = displayName + " must contain version=1";
            return status;
        }
        if (!IsValidManifestUrl(status.manifestUrl))
        {
            status.detail = displayName +
                            " manifest must be an HTTPS URL ending in /latest.json or /streaming.mfil "
                            "(HTTP is allowed only for localhost testing)";
            return status;
        }

        status.valid  = true;
        status.detail = displayName + " ready";
        return status;
    }
} // namespace StreamingBootstrap
