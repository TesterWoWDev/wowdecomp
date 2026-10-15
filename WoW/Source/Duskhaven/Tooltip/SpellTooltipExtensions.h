#pragma once

#include <cstdint>
#include <string>

class SpellRec;
class CGPlayer_C;
class CGUnit_C;
class PowerDisplayRec;
struct SpellEffectCacheRow;

namespace Duskhaven
{
    namespace Tooltip
    {
        float GetAttackPowerBonusForSpell(CGPlayer_C* player, const SpellRec* spell);
        float GetSpellPowerBonusForSpell(CGPlayer_C* player, const SpellRec* spell, bool healing);
        float ApplyScalarValuesForPlayer(CGPlayer_C* player, const SpellRec* spell, float sp, float ap, float bv, int mode, bool healing);
        float GetEffectBonusValue(CGPlayer_C* player, const SpellRec* spell, uint32_t effectIndex, int32_t spellLevel, int32_t parsePet, int32_t parseUnit, int32_t noModifiers);
        bool IsEffectHealing(const SpellRec* spell, uint32_t effectIndex);

        bool OwnedTooltipDescriptionHasValueMacro(const char* description);
        bool ParseOwnedSpellTooltipText(const SpellRec* spell, const char* source, char* dest, size_t destSize, uint32_t auraStackCount);

        std::string ExpandTooltipText(const SpellRec* spell, const char* text);

        int32_t FormatSpellPowerCost(char* dest, uint32_t destSize, const SpellRec* spell, const CGUnit_C* caster, int32_t powerCost, const PowerDisplayRec* display);
        void FormatSpellCooldown(const SpellRec* spell, int32_t cooldown, char* recastTime, uint32_t recastSize);
        uint32_t GetSpellRemainingCooldown(uint32_t spellId, uint32_t currentCooldown);
        void ProjectEffectIntoFirstSlot(const SpellRec* spell, const ::SpellEffectCacheRow& effect, SpellRec& projected);
    } // namespace Tooltip
} // namespace Duskhaven
