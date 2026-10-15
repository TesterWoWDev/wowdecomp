#include "WoW/Source/Duskhaven/DBC/Defs/ScriptedMissileMotion.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackScriptedMissileMotion(ScriptedMissileMotionRow& row, const uint32_t* cols, const char* str)
    {
        row.id = static_cast<int>(cols[0]);
        row.motion = static_cast<int>(cols[1]);
        row.count = static_cast<int>(cols[2]);
        row.durationMs = static_cast<int>(cols[3]);
        row.tickMs = static_cast<int>(cols[4]);
        row.flags = static_cast<int>(cols[5]);
        row.collisionRadius = *reinterpret_cast<const float*>(&cols[6]);
        row.radius = *reinterpret_cast<const float*>(&cols[7]);
        row.radiusVelocity = *reinterpret_cast<const float*>(&cols[8]);
        row.startDistance = *reinterpret_cast<const float*>(&cols[9]);
        row.height = *reinterpret_cast<const float*>(&cols[10]);
        row.verticalVelocity = *reinterpret_cast<const float*>(&cols[11]);
        row.forwardSpeed = *reinterpret_cast<const float*>(&cols[12]);
        row.angularSpeed = *reinterpret_cast<const float*>(&cols[13]);
        row.sineAmplitude = *reinterpret_cast<const float*>(&cols[14]);
        row.sineFrequency = *reinterpret_cast<const float*>(&cols[15]);
        row.modelPath = const_cast<char*>(str ? (str + cols[16]) : "");
        row.visualFlags = static_cast<int>(cols[17]);
        row.attachMode = static_cast<int>(cols[18]);
        row.attachPoint = static_cast<int>(cols[19]);
        row.visualScale = *reinterpret_cast<const float*>(&cols[20]);
    }

    CDBCTable<ScriptedMissileMotionRow> s_table(
        "ScriptedMissileMotion",
        [](ScriptedMissileMotionRow const& row) { return static_cast<uint32_t>(row.id); },
        UnpackScriptedMissileMotion);
} // namespace

ScriptedMissileMotionRow* ScriptedMissileMotionDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

int ScriptedMissileMotionDB::LuaGet(lua_State* L, int id)
{
    ScriptedMissileMotionRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->id);
    ClientLua::PushNumber(L, r->motion);
    ClientLua::PushNumber(L, r->count);
    ClientLua::PushNumber(L, r->durationMs);
    ClientLua::PushNumber(L, r->tickMs);
    ClientLua::PushNumber(L, r->flags);
    ClientLua::PushNumber(L, r->collisionRadius);
    ClientLua::PushNumber(L, r->radius);
    ClientLua::PushNumber(L, r->radiusVelocity);
    ClientLua::PushNumber(L, r->startDistance);
    ClientLua::PushNumber(L, r->height);
    ClientLua::PushNumber(L, r->verticalVelocity);
    ClientLua::PushNumber(L, r->forwardSpeed);
    ClientLua::PushNumber(L, r->angularSpeed);
    ClientLua::PushNumber(L, r->sineAmplitude);
    ClientLua::PushNumber(L, r->sineFrequency);
    ClientLua::PushString(L, r->modelPath);
    ClientLua::PushNumber(L, r->visualFlags);
    ClientLua::PushNumber(L, r->attachMode);
    ClientLua::PushNumber(L, r->attachPoint);
    ClientLua::PushNumber(L, r->visualScale);
    return 21;
}

REGISTER_CDBC_TABLE(ScriptedMissileMotion, s_table, ScriptedMissileMotionDB::LuaGet);
