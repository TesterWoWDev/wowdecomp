#include "WoW/Source/Duskhaven/DBC/Defs/ZoneLightPoint.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<ZoneLightPointRow> s_table(
        "ZoneLightPoint",
        [](ZoneLightPointRow const& row) { return static_cast<uint32_t>(row.ID); });
} // namespace

ZoneLightPointRow* ZoneLightPointDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t ZoneLightPointDB::GetNumRows()
{
    return s_table.GetNumRows();
}

ZoneLightPointRow* ZoneLightPointDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int ZoneLightPointDB::LuaGet(lua_State* L, int id)
{
    ZoneLightPointRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->ID);
    ClientLua::PushNumber(L, r->zoneLightID);
    ClientLua::PushNumber(L, r->positionX);
    ClientLua::PushNumber(L, r->positionY);
    ClientLua::PushNumber(L, r->pointOrder);
    return 5;
}

REGISTER_CDBC_TABLE(ZoneLightPoint, s_table, ZoneLightPointDB::LuaGet);
