#include "WoW/Source/Duskhaven/Config/DHConfig.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    static const std::pair<char const*, char const*> kBuiltInDefaults[] = {
        {"allowDiscord", "1"},
        {"terrainReplacementEnabled", "0"},
    };

    struct DHConfigState
    {
        bool initialized = false;
        std::filesystem::path path;
        std::unordered_map<std::string, std::string> values;
        std::unordered_map<std::string, std::string> defaults;
        std::mutex mutex;
    };

    DHConfigState& State()
    {
        static DHConfigState state;
        return state;
    }

    std::filesystem::path ResolveDHConfigPath()
    {
        char buffer[MAX_PATH] = {};
        DWORD length          = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
            return std::filesystem::path(buffer).parent_path() / "WTF/DHConfig";

        return std::filesystem::current_path() / "WTF/DHConfig";
    }

    bool ParseLine(std::string const& line, std::string& key, std::string& value)
    {
        key.clear();
        value.clear();

        bool escape          = false;
        bool sawSeparator    = false;
        std::string* current = &key;

        for (char ch : line)
        {
            if (escape)
            {
                switch (ch)
                {
                    case 'n':
                        current->push_back('\n');
                        break;
                    case 'r':
                        current->push_back('\r');
                        break;
                    default:
                        current->push_back(ch);
                        break;
                }
                escape = false;
                continue;
            }

            if (ch == '\\')
            {
                escape = true;
                continue;
            }

            if (!sawSeparator && ch == '=')
            {
                sawSeparator = true;
                current      = &value;
                continue;
            }

            current->push_back(ch);
        }

        if (escape)
            current->push_back('\\');

        return sawSeparator;
    }

    void LoadUnlocked()
    {
        DHConfigState& state = State();
        state.values.clear();
        state.path = ResolveDHConfigPath();

        std::ifstream input(state.path);
        if (!input.is_open())
        {
            return;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                continue;

            std::string key;
            std::string value;
            if (!ParseLine(line, key, value))
                continue;

            state.values[key] = value;
        }
    }

    void EnsureInitializedUnlocked()
    {
        DHConfigState& state = State();
        if (state.initialized)
            return;

        LoadUnlocked();
        state.initialized = true;
    }

    void PersistUnlocked()
    {
        DHConfigState& state = State();

        std::error_code createError;
        std::filesystem::create_directories(state.path.parent_path(), createError);
        if (createError)
            return;

        std::ofstream output(state.path, std::ios::out | std::ios::trunc);
        if (!output.is_open())
            return;

        std::vector<std::pair<std::string, std::string>> entries(state.values.begin(), state.values.end());
        std::sort(entries.begin(), entries.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });

        for (auto const& [key, value] : entries)
        {
            output << key << "=" << value << "\n";
        }
    }
} // namespace

void DHConfig::Initialize()
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    for (auto const& [key, value] : kBuiltInDefaults)
    {
        State().defaults[key] = value;
    }
}

std::string DHConfig::Read(std::string const& key, std::string const& defaultValue)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    auto it = State().values.find(key);
    if (it != State().values.end())
        return it->second;

    auto defaultIt = State().defaults.find(key);
    if (defaultIt != State().defaults.end())
        return defaultIt->second;

    return defaultValue;
}

std::string DHConfig::ReadString(std::string const& key, std::string const& defaultValue)
{
    return Read(key, defaultValue);
}

int32_t DHConfig::ReadInt(std::string const& key, int32_t defaultValue)
{
    std::string value = Read(key, "");
    if (value.empty())
        return defaultValue;

    char* end = nullptr;
    long result = std::strtol(value.c_str(), &end, 10);
    return end != value.c_str() ? static_cast<int32_t>(result) : defaultValue;
}

float DHConfig::ReadFloat(std::string const& key, float defaultValue)
{
    std::string value = Read(key, "");
    if (value.empty())
        return defaultValue;

    char* end = nullptr;
    float result = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? result : defaultValue;
}

void DHConfig::Write(std::string const& key, std::string const& value)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    State().values[key] = value;
    PersistUnlocked();
}

void DHConfig::AddDefault(std::string const& key, std::string const& value)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    State().defaults[key] = value;
}

void DHConfig::WriteDefaults()
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    bool changed = false;
    for (auto const& [key, value] : State().defaults)
    {
        if (State().values.find(key) == State().values.end())
        {
            State().values[key] = value;
            changed = true;
        }
    }

    if (changed)
        PersistUnlocked();
}

bool DHConfig::Has(std::string const& key)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    return State().values.find(key) != State().values.end() ||
           State().defaults.find(key) != State().defaults.end();
}

static int Script_DHConfigReadString(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    const char* def = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    std::string val = DHConfig::ReadString(key, def);
    lua_pushstring(L, val.c_str());
    return 1;
}

static int Script_DHConfigReadInt(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    int32_t def     = (int32_t)lua_tonumber(L, 2);
    lua_pushnumber(L, DHConfig::ReadInt(key, def));
    return 1;
}

static int Script_DHConfigReadFloat(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    float def       = (float)lua_tonumber(L, 2);
    lua_pushnumber(L, DHConfig::ReadFloat(key, def));
    return 1;
}

static int Script_DHConfigWrite(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    const char* val = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    DHConfig::Write(key, val);
    return 0;
}

static int Script_DHConfigWriteTable(lua_State* L)
{
    if (lua_gettop(L) < 2)
    {
        lua_pushstring(L, "DHConfigWriteTable(key, table) requires 2 arguments");
        return 1;
    }

    if (lua_type(L, 1) != LUA_TSTRING)
    {
        lua_pushstring(L, "DHConfigWriteTable: arg1 must be a string key");
        return 1;
    }

    if (lua_type(L, 2) != LUA_TTABLE)
    {
        lua_pushstring(L, "DHConfigWriteTable: arg2 must be a table");
        return 1;
    }

    std::string key = lua_tostring(L, 1);
    LuaValue root   = LuaCppBridge::ReadTable(L, 2);
    std::string serialized = LuaCppBridge::ToLuaSource(root);
    DHConfig::Write(key, serialized);
    return 1;
}

static int Script_DHConfigReadTable(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TSTRING)
    {
        lua_pushstring(L, "DHConfigReadTable: arg1 must be a string key");
        return 1;
    }

    std::string key        = lua_tostring(L, 1);
    std::string serialized = DHConfig::ReadString(key, "");

    if (serialized.empty())
    {
        if (lua_gettop(L) >= 2)
            lua_pushvalue(L, 2);
        else
            lua_pushnil(L);
        return 1;
    }

    std::string chunk = "return " + serialized;
    if (luaL_loadbuffer(L, chunk.c_str(), chunk.size(), key.c_str()) != 0)
        return 1;

    lua_pcall(L, 0, 1, 0);
    return 1;
}

static int Script_DHConfigAddDefault(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    const char* def = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    DHConfig::AddDefault(key, def);
    return 0;
}

static int Script_DHConfigWriteDefaults(lua_State* L)
{
    DHConfig::WriteDefaults();
    return 0;
}

static int Script_DHConfigHas(lua_State* L)
{
    const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    lua_pushboolean(L, DHConfig::Has(key));
    return 1;
}

void DHConfig::RegisterLuaFunctions()
{
    FrameScript_RegisterFunction("DHConfigReadString", Script_DHConfigReadString);
    FrameScript_RegisterFunction("DHConfigReadInt", Script_DHConfigReadInt);
    FrameScript_RegisterFunction("DHConfigReadFloat", Script_DHConfigReadFloat);
    FrameScript_RegisterFunction("DHConfigWrite", Script_DHConfigWrite);
    FrameScript_RegisterFunction("DHConfigWriteTable", Script_DHConfigWriteTable);
    FrameScript_RegisterFunction("DHConfigReadTable", Script_DHConfigReadTable);
    FrameScript_RegisterFunction("DHConfigAddDefault", Script_DHConfigAddDefault);
    FrameScript_RegisterFunction("DHConfigWriteDefaults", Script_DHConfigWriteDefaults);
    FrameScript_RegisterFunction("DHConfigHas", Script_DHConfigHas);
}
