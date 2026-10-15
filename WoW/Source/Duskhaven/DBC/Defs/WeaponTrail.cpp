#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrail.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackWeaponTrail(WeaponTrailRow& row, const uint32_t* cols, const char* str)
    {
        row.ID = cols[0];
        row.FilePath = const_cast<char*>(str ? (str + cols[1]) : "");
        row.Roll = *reinterpret_cast<const float*>(&cols[2]);
        row.Pitch = *reinterpret_cast<const float*>(&cols[3]);
        row.Yaw = *reinterpret_cast<const float*>(&cols[4]);
        row.TexturePath[0] = const_cast<char*>(str ? (str + cols[5]) : "");
        row.TexturePath[1] = const_cast<char*>(str ? (str + cols[6]) : "");
        row.TexturePath[2] = const_cast<char*>(str ? (str + cols[7]) : "");
        row.TextureScrollRateU[0] = *reinterpret_cast<const float*>(&cols[8]);
        row.TextureScrollRateU[1] = *reinterpret_cast<const float*>(&cols[9]);
        row.TextureScrollRateU[2] = *reinterpret_cast<const float*>(&cols[10]);
        row.TextureScrollRateV[0] = *reinterpret_cast<const float*>(&cols[11]);
        row.TextureScrollRateV[1] = *reinterpret_cast<const float*>(&cols[12]);
        row.TextureScrollRateV[2] = *reinterpret_cast<const float*>(&cols[13]);
        row.TextureScaleU[0] = *reinterpret_cast<const float*>(&cols[14]);
        row.TextureScaleU[1] = *reinterpret_cast<const float*>(&cols[15]);
        row.TextureScaleU[2] = *reinterpret_cast<const float*>(&cols[16]);
        row.TextureScaleV[0] = *reinterpret_cast<const float*>(&cols[17]);
        row.TextureScaleV[1] = *reinterpret_cast<const float*>(&cols[18]);
        row.TextureScaleV[2] = *reinterpret_cast<const float*>(&cols[19]);
    }

    CDBCTable<WeaponTrailRow> s_table(
        "WeaponTrail",
        [](WeaponTrailRow const& row) { return row.ID; },
        UnpackWeaponTrail);
} // namespace

WeaponTrailRow* WeaponTrailDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t WeaponTrailDB::GetNumRows()
{
    return s_table.GetNumRows();
}

WeaponTrailRow* WeaponTrailDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int WeaponTrailDB::LuaGet(lua_State* L, int id)
{
    WeaponTrailRow* row = Get(static_cast<uint32_t>(id));
    if (!row)
        return 0;

    ClientLua::PushNumber(L, row->ID);
    ClientLua::PushString(L, row->FilePath ? row->FilePath : "");
    ClientLua::PushNumber(L, row->Roll);
    ClientLua::PushNumber(L, row->Pitch);
    ClientLua::PushNumber(L, row->Yaw);
    for (char const* texturePath : row->TexturePath)
        ClientLua::PushString(L, texturePath ? texturePath : "");
    for (float value : row->TextureScrollRateU)
        ClientLua::PushNumber(L, value);
    for (float value : row->TextureScrollRateV)
        ClientLua::PushNumber(L, value);
    for (float value : row->TextureScaleU)
        ClientLua::PushNumber(L, value);
    for (float value : row->TextureScaleV)
        ClientLua::PushNumber(L, value);
    return 20;
}

REGISTER_CDBC_TABLE(WeaponTrail, s_table, WeaponTrailDB::LuaGet);
