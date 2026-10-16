#include "ModCore.h"
#include "BytecodeCache.h"
#include "AutoExport.h"
#include "FileWatcher.h"

#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/FrameScript/FrameScript.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ModCore
{
    namespace
    {
        int Script_ModCoreLog(lua_State* L)
        {
            const char* level = lua_tostring(L, 1);
            const char* msg = lua_tostring(L, 2);
            if (!msg) return 0;

            std::cout << "[LUA " << (level ? level : "INFO") << "] " << msg << std::endl;
            return 0;
        }

        std::string ReadSmallFile(const std::filesystem::path& path)
        {
            std::ifstream file(path);
            if (!file.is_open()) return "";
            std::stringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        // Minimal JSON parser for simple key-value config
        bool ParseSimpleBool(const std::string& json, const std::string& key, bool defaultVal)
        {
            size_t pos = json.find("\"" + key + "\"");
            if (pos == std::string::npos) return defaultVal;
            size_t colon = json.find(':', pos);
            if (colon == std::string::npos) return defaultVal;
            size_t truePos = json.find("true", colon);
            size_t falsePos = json.find("false", colon);
            size_t comma = json.find(',', colon);
            size_t brace = json.find('}', colon);
            size_t endPos = std::min(comma, brace);

            if (truePos != std::string::npos && truePos < endPos) return true;
            if (falsePos != std::string::npos && falsePos < endPos) return false;
            return defaultVal;
        }
    }

    ModManager& ModManager::Get()
    {
        static ModManager instance;
        return instance;
    }

    ModManager::~ModManager()
    {
        Shutdown();
    }

    std::filesystem::path ModManager::ResolveModsDirectory()
    {
        // 1. Try directory containing current executable
        wchar_t exePathBuffer[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePathBuffer, MAX_PATH))
        {
            std::filesystem::path exeDir = std::filesystem::path(exePathBuffer).parent_path();
            if (std::filesystem::exists(exeDir / "mods"))
                return exeDir / "mods";
        }

        // 2. Try current working directory
        std::filesystem::path cwd = std::filesystem::current_path();
        if (std::filesystem::exists(cwd / "mods"))
            return cwd / "mods";

        // 3. Try bin/Win/mods
        if (std::filesystem::exists(cwd / "bin" / "Win" / "mods"))
            return cwd / "bin" / "Win" / "mods";

        // Fallback default
        return cwd / "mods";
    }

    void ModManager::LoadConfig()
    {
        std::filesystem::path configFile = m_modsPath / "config.json";
        if (!std::filesystem::exists(configFile))
            return;

        std::string json = ReadSmallFile(configFile);
        m_devMode = ParseSimpleBool(json, "devMode", true);
        m_cacheEnabled = ParseSimpleBool(json, "cacheEnabled", true);
        m_hotReloadEnabled = ParseSimpleBool(json, "hotReload", true);
        m_aiExportOnBoot = ParseSimpleBool(json, "aiExportOnBoot", true);
    }

    void ModManager::Initialize()
    {
        if (m_initialized)
            return;

        m_modsPath = ResolveModsDirectory();
        std::cout << "[ModCore] Initializing ModManager. Mods Root: " << m_modsPath.string() << std::endl;

        LoadConfig();

        BytecodeCache::Get().Initialize(m_modsPath / "cache", m_cacheEnabled);
        AutoExport::Get().Initialize(m_modsPath / "api");

        m_initialized = true;
    }

    void ModManager::Shutdown()
    {
        if (!m_initialized)
            return;

        FileWatcher::Get().Stop();
        m_initialized = false;
        std::cout << "[ModCore] ModManager shut down." << std::endl;
    }

    void ModManager::RegisterLuaBindings(lua_State* L)
    {
        if (!L)
            L = FrameScript_GetContext();
        if (!L)
            return;

        m_L = L;
        std::cout << "[ModCore] Registering Lua bindings and executing bootstrap..." << std::endl;

        // Register internal logging helper
        FrameScript_RegisterFunction("_MODCORE_LOG", Script_ModCoreLog);

        // Set BasePath in global table
        std::string setBasePathLua = "ModCore = ModCore or {}; ModCore.BasePath = [[" + m_modsPath.generic_string() + "/]];";
        FrameScript_Execute(setBasePathLua.c_str(), "ModCore_BasePath", nullptr);

        // Load and execute mods/system/bootstrap.lua via BytecodeCache
        std::filesystem::path bootstrapPath = m_modsPath / "system" / "bootstrap.lua";
        if (BytecodeCache::Get().LoadScript(m_L, bootstrapPath, "bootstrap.lua"))
        {
            if (lua_pcall(m_L, 0, LUA_MULTRET, 0) != 0)
            {
                const char* err = lua_tostring(m_L, -1);
                std::cerr << "[ModCore] Error executing bootstrap.lua: " << (err ? err : "") << std::endl;
                lua_pop(m_L, 1);
            }
            else
            {
                std::cout << "[ModCore] Bootstrap executed successfully." << std::endl;
            }
        }

        // Mount all mods
        DiscoverAndMountMods();

        // Run AI Auto-Export
        if (m_aiExportOnBoot)
        {
            AutoExport::Get().ExportAll(m_L);
        }

        // Start file watcher if hot-reload is enabled
        if (m_devMode && m_hotReloadEnabled)
        {
            FileWatcher::Get().Start(m_modsPath);
            std::cout << "[ModCore] FileWatcher active on " << m_modsPath.string() << std::endl;
        }
    }

    void ModManager::DiscoverAndMountMods()
    {
        if (!std::filesystem::exists(m_modsPath))
            return;

        // Mount core_duskhaven first
        std::filesystem::path coreMod = m_modsPath / "core_duskhaven";
        if (std::filesystem::exists(coreMod / "mod.json"))
        {
            MountMod(coreMod);
        }

        // Mount user_mods
        std::filesystem::path userMods = m_modsPath / "user_mods";
        if (std::filesystem::exists(userMods))
        {
            for (const auto& entry : std::filesystem::directory_iterator(userMods))
            {
                if (entry.is_directory() && std::filesystem::exists(entry.path() / "mod.json"))
                {
                    MountMod(entry.path());
                }
            }
        }
    }

    bool ModManager::MountMod(const std::filesystem::path& modDir)
    {
        if (!m_L)
            return false;

        std::filesystem::path manifestPath = modDir / "mod.json";
        std::string manifestContent = ReadSmallFile(manifestPath);
        if (manifestContent.empty())
            return false;

        // Call ModCore.MountMod in Lua
        std::string mountLua = "if ModCore and ModCore.MountMod then "
                               "  ModCore.MountMod([[" + modDir.generic_string() + "]], { "
                               "    id = [[" + modDir.filename().generic_string() + "]], "
                               "    dir = [[" + modDir.generic_string() + "]] "
                               "  }) "
                               "end";
        FrameScript_Execute(mountLua.c_str(), "MountMod", nullptr);
        return true;
    }

    bool ModManager::ReloadScript(const std::filesystem::path& scriptPath)
    {
        if (!m_L || !std::filesystem::exists(scriptPath))
            return false;

        std::cout << "[ModCore] Hot-reloading script: " << scriptPath.filename().string() << std::endl;
        if (BytecodeCache::Get().LoadScript(m_L, scriptPath, scriptPath.filename().string()))
        {
            if (lua_pcall(m_L, 0, LUA_MULTRET, 0) != 0)
            {
                const char* err = lua_tostring(m_L, -1);
                std::cerr << "[ModCore] Error hot-reloading " << scriptPath.string() << ": " << (err ? err : "") << std::endl;
                lua_pop(m_L, 1);
                return false;
            }
            std::cout << "[ModCore] Successfully reloaded: " << scriptPath.filename().string() << std::endl;
            return true;
        }
        return false;
    }

    void ModManager::OnEngineInit()
    {
        if (!m_L) return;
        FrameScript_Execute("if ModCore and ModCore.OnEngineInit then ModCore.OnEngineInit() end", "ModCore_OnEngineInit", nullptr);
    }

    void ModManager::OnWorldUpdate(float dt)
    {
        // Process hot-reloaded files
        if (m_hotReloadEnabled)
        {
            auto modifiedFiles = FileWatcher::Get().PopModifiedFiles();
            for (const auto& path : modifiedFiles)
            {
                ReloadScript(path);
            }
        }

        if (!m_L) return;

        // Dispatch frame update to Lua
        lua_getglobal(m_L, "ModCore");
        if (lua_istable(m_L, -1))
        {
            lua_getfield(m_L, -1, "OnWorldUpdate");
            if (lua_isfunction(m_L, -1))
            {
                lua_pushnumber(m_L, dt);
                if (lua_pcall(m_L, 1, 0, 0) != 0)
                {
                    lua_pop(m_L, 1); // pop error
                }
            }
            else
            {
                lua_pop(m_L, 1);
            }
        }
        lua_pop(m_L, 1);
    }

    void ModManager::OnWorldRender(void* worldFrame)
    {
        if (!m_L) return;

        lua_getglobal(m_L, "ModCore");
        if (lua_istable(m_L, -1))
        {
            lua_getfield(m_L, -1, "OnWorldRender");
            if (lua_isfunction(m_L, -1))
            {
                lua_pushlightuserdata(m_L, worldFrame);
                if (lua_pcall(m_L, 1, 0, 0) != 0)
                {
                    lua_pop(m_L, 1);
                }
            }
            else
            {
                lua_pop(m_L, 1);
            }
        }
        lua_pop(m_L, 1);
    }

    bool ModManager::OnPacketReceived(uint32_t opcode, uint32_t time, void* dataStore)
    {
        if (!m_L) return false;

        bool handled = false;
        lua_getglobal(m_L, "ModCore");
        if (lua_istable(m_L, -1))
        {
            lua_getfield(m_L, -1, "OnPacketReceived");
            if (lua_isfunction(m_L, -1))
            {
                lua_pushinteger(m_L, opcode);
                lua_pushinteger(m_L, time);
                lua_pushlightuserdata(m_L, dataStore);
                if (lua_pcall(m_L, 3, 1, 0) == 0)
                {
                    if (lua_isboolean(m_L, -1))
                    {
                        handled = (lua_toboolean(m_L, -1) != 0);
                    }
                    lua_pop(m_L, 1);
                }
                else
                {
                    lua_pop(m_L, 1);
                }
            }
            else
            {
                lua_pop(m_L, 1);
            }
        }
        lua_pop(m_L, 1);
        return handled;
    }

    bool ModManager::OnSpellQuery(const char* queryType, uint32_t spellId, int32_t* outResult)
    {
        if (!m_L || !queryType) return false;

        bool handled = false;
        lua_getglobal(m_L, "ModCore");
        if (lua_istable(m_L, -1))
        {
            lua_getfield(m_L, -1, "OnSpellQuery");
            if (lua_isfunction(m_L, -1))
            {
                lua_pushstring(m_L, queryType);
                lua_pushinteger(m_L, spellId);
                if (lua_pcall(m_L, 2, 1, 0) == 0)
                {
                    if (lua_isnumber(m_L, -1))
                    {
                        if (outResult)
                            *outResult = static_cast<int32_t>(lua_tointeger(m_L, -1));
                        handled = true;
                    }
                    lua_pop(m_L, 1);
                }
                else
                {
                    lua_pop(m_L, 1);
                }
            }
            else
            {
                lua_pop(m_L, 1);
            }
        }
        lua_pop(m_L, 1);
        return handled;
    }
}
