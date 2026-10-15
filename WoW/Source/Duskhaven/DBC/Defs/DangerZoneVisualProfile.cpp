#include "WoW/Source/Duskhaven/DBC/Defs/DangerZoneVisualProfile.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackDangerZoneVisualProfile(DangerZoneVisualProfileRow& row, const uint32_t* cols, const char* str)
    {
        row.id = static_cast<int>(cols[0]);
        row.schoolMask = static_cast<int>(cols[1]);
        row.shapeMask = static_cast<int>(cols[2]);
        row.flags = static_cast<int>(cols[3]);
        row.modelPath = const_cast<char*>(str ? (str + cols[4]) : "");
        row.modelRadius = *reinterpret_cast<const float*>(&cols[5]);
        row.radiusScale = *reinterpret_cast<const float*>(&cols[6]);
        row.zOffset = *reinterpret_cast<const float*>(&cols[7]);
        row.minDurationMs = static_cast<int>(cols[8]);
        row.maxDurationMs = static_cast<int>(cols[9]);
        row.red = *reinterpret_cast<const float*>(&cols[10]);
        row.green = *reinterpret_cast<const float*>(&cols[11]);
        row.blue = *reinterpret_cast<const float*>(&cols[12]);
        row.alpha = *reinterpret_cast<const float*>(&cols[13]);
        row.debugSpellId = static_cast<int>(cols[14]);
    }

    CDBCTable<DangerZoneVisualProfileRow> s_table(
        "DangerZoneVisualProfile",
        [](DangerZoneVisualProfileRow const& row) { return static_cast<uint32_t>(row.id); },
        UnpackDangerZoneVisualProfile);
} // namespace

DangerZoneVisualProfileRow* DangerZoneVisualProfileDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t DangerZoneVisualProfileDB::GetNumRows()
{
    return s_table.GetNumRows();
}

DangerZoneVisualProfileRow* DangerZoneVisualProfileDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int DangerZoneVisualProfileDB::LuaGet(lua_State* L, int id)
{
    DangerZoneVisualProfileRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->id);
    ClientLua::PushNumber(L, r->schoolMask);
    ClientLua::PushNumber(L, r->shapeMask);
    ClientLua::PushNumber(L, r->flags);
    ClientLua::PushString(L, r->modelPath);
    ClientLua::PushNumber(L, r->modelRadius);
    ClientLua::PushNumber(L, r->radiusScale);
    ClientLua::PushNumber(L, r->zOffset);
    ClientLua::PushNumber(L, r->minDurationMs);
    ClientLua::PushNumber(L, r->maxDurationMs);
    ClientLua::PushNumber(L, r->red);
    ClientLua::PushNumber(L, r->green);
    ClientLua::PushNumber(L, r->blue);
    ClientLua::PushNumber(L, r->alpha);
    ClientLua::PushNumber(L, r->debugSpellId);
    return 15;
}

REGISTER_CDBC_TABLE(DangerZoneVisualProfile, s_table, DangerZoneVisualProfileDB::LuaGet);
