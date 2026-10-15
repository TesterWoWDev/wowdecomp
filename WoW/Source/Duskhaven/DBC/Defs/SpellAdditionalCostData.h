#pragma once
#include <cstdint>
struct lua_State;

struct SpellAdditionalCostDataRow
{
    int spellID;
    char* resourceName;
    int cost;
    int flags;
    char* resourceNamePlural;
    int maximumAdditionalSpend;
    float scalarPerPower;
    int damageSpellID;
    int powerType;
    int powerUnitMultiplier;
    int requiredAuraID;
    int excludedAuraID;
    int freeMaxSpendAuraID;
    int freeMaxSpendConditionAuraID;
    int generatedPowerAuraID;
    int generatedPower;
};

namespace SpellAdditionalCostFlags
{
    constexpr uint32_t PluralizeResourceName = 1u << 0;
}

namespace SpellAdditionalCostDataDB
{
    SpellAdditionalCostDataRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
} // namespace SpellAdditionalCostDataDB
