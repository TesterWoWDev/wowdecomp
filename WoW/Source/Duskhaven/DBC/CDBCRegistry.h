#pragma once

#include <cstddef>
#include <string>

struct lua_State;

struct CDBCEntry
{
    const char* name;
    bool (*load)();
    bool (*reload)();
    int (*luaGet)(lua_State* L, int id);
};

class CDBCRegistry
{
public:
    static int Register(CDBCEntry entry);
    static void LoadAll();
    static bool Reload(const char* name);
    static int LuaDispatch(lua_State* L);

    static bool IsLoaded(const char* name);
    static std::size_t Count();
    static std::string PendingCompatibilityError() { return ""; }

private:
    static void RegisterLuaApi();
};

#define REGISTER_CDBC(name_, load_fn, reload_fn, lua_fn) \
    static int _cdbc_##name_##_reg = CDBCRegistry::Register({#name_, (load_fn), (reload_fn), (lua_fn)})

#define REGISTER_CDBC_TABLE(name_, table_, lua_fn) \
    namespace \
    { \
        bool _cdbc_##name_##_load() \
        { \
            return (table_).Load(); \
        } \
        bool _cdbc_##name_##_reload() \
        { \
            return (table_).Reload(); \
        } \
    } \
    REGISTER_CDBC(name_, _cdbc_##name_##_load, _cdbc_##name_##_reload, lua_fn)
