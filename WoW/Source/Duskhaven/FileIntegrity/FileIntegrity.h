#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Duskhaven
{
    namespace FileIntegrity
    {
        struct FileInfo
        {
            uint64_t size;
            std::string hashHex;
        };

        void RunStartupScan();
        const std::unordered_map<std::string, FileInfo>& GetLoadedMpqEntries();
        void RegisterLuaFunctions();
    }
}
