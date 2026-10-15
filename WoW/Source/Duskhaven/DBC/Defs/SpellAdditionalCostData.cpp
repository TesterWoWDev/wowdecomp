#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalCostData.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackSpellAdditionalCostData(SpellAdditionalCostDataRow& row, const uint32_t* cols, const char* str)
    {
        row.spellID = static_cast<int>(cols[0]);
        row.resourceName = const_cast<char*>(str ? (str + cols[1]) : "");
        row.cost = static_cast<int>(cols[2]);
        row.flags = static_cast<int>(cols[3]);
        row.resourceNamePlural = const_cast<char*>(str ? (str + cols[4]) : "");
        row.maximumAdditionalSpend = static_cast<int>(cols[5]);
        row.scalarPerPower = *reinterpret_cast<const float*>(&cols[6]);
        row.damageSpellID = static_cast<int>(cols[7]);
        row.powerType = static_cast<int>(cols[8]);
        row.powerUnitMultiplier = static_cast<int>(cols[9]);
        row.requiredAuraID = static_cast<int>(cols[10]);
        row.excludedAuraID = static_cast<int>(cols[11]);
        row.freeMaxSpendAuraID = static_cast<int>(cols[12]);
        row.freeMaxSpendConditionAuraID = static_cast<int>(cols[13]);
        row.generatedPowerAuraID = static_cast<int>(cols[14]);
        row.generatedPower = static_cast<int>(cols[15]);
    }

    CDBCTable<SpellAdditionalCostDataRow> s_table(
        "SpellAdditionalCostData",
        [](SpellAdditionalCostDataRow const& row) { return static_cast<uint32_t>(row.spellID); },
        UnpackSpellAdditionalCostData);
} // namespace

SpellAdditionalCostDataRow* SpellAdditionalCostDataDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

int SpellAdditionalCostDataDB::LuaGet(lua_State* L, int id)
{
    SpellAdditionalCostDataRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->spellID);
    ClientLua::PushString(L, r->resourceName);
    ClientLua::PushNumber(L, r->cost);
    ClientLua::PushNumber(L, r->flags);
    ClientLua::PushString(L, r->resourceNamePlural);
    ClientLua::PushNumber(L, r->maximumAdditionalSpend);
    ClientLua::PushNumber(L, r->scalarPerPower);
    ClientLua::PushNumber(L, r->damageSpellID);
    ClientLua::PushNumber(L, r->powerType);
    ClientLua::PushNumber(L, r->powerUnitMultiplier);
    ClientLua::PushNumber(L, r->requiredAuraID);
    ClientLua::PushNumber(L, r->excludedAuraID);
    ClientLua::PushNumber(L, r->freeMaxSpendAuraID);
    ClientLua::PushNumber(L, r->freeMaxSpendConditionAuraID);
    ClientLua::PushNumber(L, r->generatedPowerAuraID);
    ClientLua::PushNumber(L, r->generatedPower);
    return 16;
}

REGISTER_CDBC_TABLE(SpellAdditionalCostData, s_table, SpellAdditionalCostDataDB::LuaGet);
