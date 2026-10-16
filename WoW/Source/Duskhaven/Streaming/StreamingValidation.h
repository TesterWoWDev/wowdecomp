#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace StreamingValidation
{
    inline bool ParseUnsigned(const std::string& text, uint64_t& value, bool allowZero = false)
    {
        if (text.empty())
            return false;
        uint64_t parsed = 0;
        for (unsigned char character : text)
        {
            if (character < '0' || character > '9')
                return false;
            const uint64_t digit = character - '0';
            if (parsed > ((std::numeric_limits<uint64_t>::max)() - digit) / 10)
                return false;
            parsed = parsed * 10 + digit;
        }
        if (!allowZero && parsed == 0)
            return false;
        value = parsed;
        return true;
    }

    inline bool IsSafeRelativePath(const std::string& path)
    {
        if (path.empty() || path.front() == '/' || path.front() == '\\' ||
            path.back() == '/' || path.back() == '\\')
            return false;
        size_t begin = 0;
        while (begin < path.size())
        {
            const size_t slash = path.find_first_of("/\\", begin);
            const size_t end = slash == std::string::npos ? path.size() : slash;
            const std::string segment = path.substr(begin, end - begin);
            if (segment.empty() || segment == "." || segment == ".." ||
                segment.back() == '.' || segment.back() == ' ')
                return false;
            for (unsigned char character : segment)
            {
                if (character < 0x20 || character >= 0x7F || character == ':' || character == ';' ||
                    character == '=' || character == '?' || character == '#' || character == '%' ||
                    character == '"' || character == '<' || character == '>' || character == '|' || character == '*')
                    return false;
            }
            std::string stem = segment.substr(0, segment.find('.'));
            std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c)
                           { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : static_cast<char>(c); });
            if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
                (stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0) &&
                 stem[3] >= '1' && stem[3] <= '9'))
                return false;
            begin = end + 1;
        }
        return true;
    }

    inline bool IsSafeArchivePath(std::string path)
    {
        // Published archives are direct children of Data. Base game archives
        // and nested/locale paths must never become manifest write targets.
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c)
                       { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c); });
        return path.size() == 16 && path.compare(0, 11, "data/patch-") == 0 &&
               path[11] >= 'a' && path[11] <= 'z' && path.compare(12, 4, ".mpq") == 0;
    }
}
