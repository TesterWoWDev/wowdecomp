#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

struct lua_State;

namespace ModCore
{
    class ModManager
    {
    public:
        static ModManager& Get();

        void Initialize();
        void Shutdown();

        // Lifecycle dispatches
        void OnEngineInit();
        void OnWorldUpdate(float dt);
        void OnWorldRender(void* worldFrame);
        bool OnPacketReceived(uint32_t opcode, uint32_t time, void* dataStore);
        bool OnSpellQuery(const char* queryType, uint32_t spellId, int32_t* outResult);

        // Native script registration
        void RegisterLuaBindings(lua_State* L);

        // Mod discovery and mounting
        void DiscoverAndMountMods();
        bool MountMod(const std::filesystem::path& modDir);
        bool ReloadScript(const std::filesystem::path& scriptPath);

        const std::filesystem::path& GetBasePath() const { return m_modsPath; }
        lua_State* GetLuaState() const { return m_L; }

    private:
        ModManager() = default;
        ~ModManager();

        void LoadConfig();
        std::filesystem::path ResolveModsDirectory();

        std::filesystem::path m_modsPath;
        lua_State* m_L = nullptr;
        bool m_initialized = false;
        bool m_devMode = true;
        bool m_cacheEnabled = true;
        bool m_hotReloadEnabled = true;
        bool m_aiExportOnBoot = true;
    };
}
