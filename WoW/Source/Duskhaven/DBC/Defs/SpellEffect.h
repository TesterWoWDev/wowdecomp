#pragma once
#include <cstdint>
#include <functional>
struct lua_State;

inline int SpellEffectCacheKey(uint32_t spellId, uint32_t effectIndex)
{
    return int((spellId << 5) | (effectIndex & 0x1F));
}

struct SpellEffectCacheRow
{
    uint32_t spellID;
    uint32_t effectIndex;
    uint32_t effect;
    int32_t effectDieSides;
    float effectRealPointsPerLevel;
    int32_t effectBasePoints;
    uint32_t effectMechanic;
    uint32_t effectImplicitTargetA;
    uint32_t effectImplicitTargetB;
    uint32_t effectRadiusIndex;
    uint32_t effectApplyAuraName;
    uint32_t effectAmplitude;
    float effectMultipleValue;
    uint32_t effectChainTargets;
    int32_t effectItemType;
    int32_t effectMiscValue;
    int32_t effectMiscValueB;
    uint32_t effectTriggerSpell;
    float effectPointsPerCombo;
    uint32_t effectSpellClassMaskA;
    uint32_t effectSpellClassMaskB;
    uint32_t effectSpellClassMaskC;
    float effectChainAmplitude;
    float effectBonusMultiplier;
    float effectSpellPowerBonus;
    float effectAttackPowerBonus;
    float effectBlockValueBonus;
    int32_t effectScalingMode;
};

namespace SpellEffectCacheDB
{
    SpellEffectCacheRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
    void AddRow(uint32_t id, const SpellEffectCacheRow& row);
    void EraseRow(uint32_t id);
    void ForEachRow(const std::function<void(SpellEffectCacheRow const*)>& callback);
} // namespace SpellEffectCacheDB
