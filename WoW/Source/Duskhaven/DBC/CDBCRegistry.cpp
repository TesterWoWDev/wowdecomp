#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct RegisteredCDBC
    {
        CDBCEntry definition;
        bool loaded = false;
    };

    std::vector<RegisteredCDBC>& Entries()
    {
        static std::vector<RegisteredCDBC> entries;
        return entries;
    }

    std::unordered_map<std::string, std::size_t>& NameMap()
    {
        static std::unordered_map<std::string, std::size_t> names;
        return names;
    }

    bool& RegistryLoaded()
    {
        static bool loaded = false;
        return loaded;
    }

    std::string ToLower(const std::string& str)
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }
} // namespace

int CDBCRegistry::Register(CDBCEntry entry)
{
    Entries().push_back({entry, false});
    return 0;
}

void CDBCRegistry::LoadAll()
{
    if (RegistryLoaded())
        return;

    auto& entries = Entries();
    auto& names = NameMap();
    names.reserve(entries.size());

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        RegisteredCDBC& registered = entries[i];
        const CDBCEntry& def = registered.definition;
        if (!def.name || !def.name[0] || !def.load || !def.reload || !def.luaGet)
            continue;

        names[ToLower(def.name)] = i;
        registered.loaded = def.load();
    }

    RegisterLuaApi();
    RegistryLoaded() = true;
}

bool CDBCRegistry::Reload(const char* name)
{
    if (!name || !name[0] || !RegistryLoaded())
        return false;

    std::string lower = ToLower(name);
    auto it = NameMap().find(lower);
    if (it == NameMap().end())
        return false;

    RegisteredCDBC& reg = Entries()[it->second];
    reg.loaded = reg.definition.reload();
    return reg.loaded;
}

int CDBCRegistry::LuaDispatch(lua_State* L)
{
    std::string const name = ClientLua::GetString(L, 1, "");
    int const id = static_cast<int>(ClientLua::GetNumber(L, 2, 0));

    auto it = NameMap().find(ToLower(name));
    if (it == NameMap().end() || !Entries()[it->second].loaded)
        return 0;

    return Entries()[it->second].definition.luaGet(L, id);
}

bool CDBCRegistry::IsLoaded(const char* name)
{
    if (!name || !name[0])
        return false;

    auto it = NameMap().find(ToLower(name));
    return (it != NameMap().end()) && Entries()[it->second].loaded;
}

std::size_t CDBCRegistry::Count()
{
    return Entries().size();
}

void CDBCRegistry::RegisterLuaApi()
{
    ClientLua::AddFunction("_CDBC_GET", CDBCRegistry::LuaDispatch);
    ClientLua::AddFunction("GetCDBCRow", CDBCRegistry::LuaDispatch);
}
