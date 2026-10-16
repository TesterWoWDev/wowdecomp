#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct lua_State;

namespace ModCore
{
    class AutoExport
    {
    public:
        static AutoExport& Get();

        void Initialize(const std::filesystem::path& apiDir);
        void ExportAll(lua_State* L);

    private:
        void GenerateEmmyLua(const std::filesystem::path& outputPath);
        void GenerateJsonSchema(const std::filesystem::path& outputPath);
        void GenerateAiGuide(const std::filesystem::path& outputPath);

        std::filesystem::path m_apiDir;
    };
}
