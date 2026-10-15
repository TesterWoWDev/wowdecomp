#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrailParam.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<WeaponTrailParamRow> s_table(
        "WeaponTrailParam",
        [](WeaponTrailParamRow const& row) { return row.ID; });
} // namespace

WeaponTrailParamRow* WeaponTrailParamDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t WeaponTrailParamDB::GetNumRows()
{
    return s_table.GetNumRows();
}

WeaponTrailParamRow* WeaponTrailParamDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int WeaponTrailParamDB::LuaGet(lua_State* L, int id)
{
    WeaponTrailParamRow* row = Get(static_cast<uint32_t>(id));
    if (!row)
        return 0;

    ClientLua::PushNumber(L, row->ID);
    ClientLua::PushNumber(L, row->Hand);
    ClientLua::PushNumber(L, row->Duration);
    ClientLua::PushNumber(L, row->FadeOutTime);
    ClientLua::PushNumber(L, row->EdgeLifeSpan);
    ClientLua::PushNumber(L, row->InitialDelay);
    ClientLua::PushNumber(L, row->SmoothSampleAngle);
    ClientLua::PushNumber(L, row->OverrideAttachTop);
    ClientLua::PushNumber(L, row->OverrideAttachBot);
    ClientLua::PushNumber(L, row->Flags);
    ClientLua::PushNumber(L, row->WeaponTrailID);
    return 11;
}

REGISTER_CDBC_TABLE(WeaponTrailParam, s_table, WeaponTrailParamDB::LuaGet);
