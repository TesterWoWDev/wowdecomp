#pragma once

#include <cstdint>
#include <string>

namespace Patcher::Filesystem
{
    bool ReadTextFile(const std::string& path, std::string& out, std::string& error);
    bool WriteTextFile(const std::string& path, const std::string& text, std::string& error);
    bool HashFileMd5(const std::string& path, std::string& outHash, uint64_t& outSize, std::string& error);
    bool HashFileSha256(const std::string& path, std::string& outHash, uint64_t& outSize, std::string& error);
    std::string GetExeDirectory();
    std::string LastErrorString(const char* prefix);
    bool EqualsIgnoreCase(const std::string& a, const std::string& b);
} // namespace Patcher::Filesystem
