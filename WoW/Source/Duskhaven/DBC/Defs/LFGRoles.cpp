#include "WoW/Source/Duskhaven/DBC/Defs/LFGRoles.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<LFGRolesRow> s_table("LFGRoles", [](LFGRolesRow const& row) { return row.ClassID; });
} // namespace

LFGRolesRow* LFGRolesDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

int LFGRolesDB::LuaGet(lua_State* L, int id)
{
    LFGRolesRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->ClassID);
    ClientLua::PushNumber(L, r->Roles);
    return 2;
}

REGISTER_CDBC_TABLE(LFGRoles, s_table, LFGRolesDB::LuaGet);
