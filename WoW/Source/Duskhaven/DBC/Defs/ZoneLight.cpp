#include "WoW/Source/Duskhaven/DBC/Defs/ZoneLight.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackZoneLight(ZoneLightRow& row, const uint32_t* cols, const char* str)
    {
        row.ID = static_cast<int32_t>(cols[0]);
        row.name = const_cast<char*>(str ? (str + cols[1]) : "");
        row.mapID = static_cast<int32_t>(cols[2]);
        row.lightID = static_cast<int32_t>(cols[3]);
    }

    CDBCTable<ZoneLightRow> s_table(
        "ZoneLight",
        [](ZoneLightRow const& row) { return static_cast<uint32_t>(row.ID); },
        UnpackZoneLight);
} // namespace

ZoneLightRow* ZoneLightDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t ZoneLightDB::GetNumRows()
{
    return s_table.GetNumRows();
}

ZoneLightRow* ZoneLightDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int ZoneLightDB::LuaGet(lua_State* L, int id)
{
    ZoneLightRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->ID);
    ClientLua::PushString(L, r->name);
    ClientLua::PushNumber(L, r->mapID);
    ClientLua::PushNumber(L, r->lightID);
    return 4;
}

REGISTER_CDBC_TABLE(ZoneLight, s_table, ZoneLightDB::LuaGet);
