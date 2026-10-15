#pragma once
#include <cstdint>
struct lua_State;

struct LFGRolesRow
{
    uint32_t ClassID;
    uint32_t Roles;
};

namespace LFGRolesDB
{
    LFGRolesRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
} // namespace LFGRolesDB

namespace Duskhaven
{
    namespace LFGRolesDB
    {
        using ::LFGRolesDB::Get;
        using ::LFGRolesDB::LuaGet;
    }
}
