#include "SpellTooltipExtensions.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/PowerDisplayRec.h"
#include "WoW/Source/DB/Rec/SpellDurationRec.h"
#include "WoW/Source/DB/Rec/SpellIconRec.h"
#include "WoW/Source/DB/Rec/SpellRadiusRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Duskhaven/Character/CharacterDefines.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalAttributes.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalCostData.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffectScalars.h"
#include "WoW/Source/Duskhaven/Spells/SpellCustomAttributes.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Tooltip.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Duskhaven
{
    namespace Tooltip
    {
        float GetAttackPowerBonusForSpell(CGPlayer_C* player, const SpellRec* spell)
        {
            if (!player || !player->m_unit || !spell)
                return 0.0f;

            uint8_t attType = (spell->m_equippedItemClass == 2 && (spell->m_equippedItemSubclass & 262156) && spell->m_defenseType != 2) ? 2 :
                              (spell->m_attributesExC & 0x00000004 /* SPELL_ATTR3_MAIN_HAND */) ? 0 : 1;

            CGUnitData* data = player->m_unit;
            float ap = 0.0f;
            if (attType == 2)
            {
                ap = static_cast<float>(data->m_unitFieldRangedAttackPower + data->m_unitFieldRangedAttackPowerMods[0] + data->m_unitFieldRangedAttackPowerMods[1]) *
                     (data->m_unitFieldRangedAttackPowerMultiplier + 1.0f);
            }
            else
            {
                ap = static_cast<float>(data->m_unitFieldAttackPower + data->m_unitFieldAttackPowerMods[0] + data->m_unitFieldAttackPowerMods[1]) *
                     (data->m_unitFieldAttackPowerMultiplier + 1.0f);
            }
            return ap > 0.0f ? ap : 0.0f;
        }

        float GetSpellPowerBonusForSpell(CGPlayer_C* player, const SpellRec* spell, bool healing)
        {
            if (!player || !spell)
                return 0.0f;

            if (healing)
                return static_cast<float>(player->m_player->m_playerFieldModHealingDonePos);

            int32_t spBonus = 0;
            bool hasSchool = false;
            uint32_t schoolMask = spell->m_schoolMask;
            for (uint32_t school = 1; school <= 6; ++school)
            {
                if (schoolMask & (1 << school))
                {
                    int32_t temp = player->GetModDamageDonePos(school) + player->GetModDamageDoneNeg(school);
                    if (!hasSchool || temp > spBonus)
                        spBonus = temp;
                    hasSchool = true;
                }
            }
            return hasSchool ? static_cast<float>(spBonus) : 0.0f;
        }

        float ApplyScalarValuesForPlayer(CGPlayer_C* player, const SpellRec* spell, float sp, float ap, float bv, int mode, bool healing)
        {
            if (!player || !spell)
                return 0.0f;

            float apVal = ap * GetAttackPowerBonusForSpell(player, spell);
            float spVal = sp * GetSpellPowerBonusForSpell(player, spell, healing);
            float bvVal = bv * static_cast<float>(player->m_player->m_playerShieldBlock);

            if (mode == 1)
                return (std::max)(apVal, spVal) + bvVal;

            return apVal + spVal + bvVal;
        }

        bool IsEffectHealing(const SpellRec* spell, uint32_t effectIndex)
        {
            if (!spell)
                return false;

            SpellEffectCacheRow* customEffect = SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIndex));
            uint32_t effType = customEffect ? customEffect->effect : (effectIndex < 3 ? spell->m_effect[effectIndex] : 0);
            uint32_t auraType = customEffect ? customEffect->effectApplyAuraName : (effectIndex < 3 ? spell->m_effectAura[effectIndex] : 0);

            return (effType == 10 /* Heal */ || effType == 67 /* HealMaxHealth */ || effType == 75 /* HealMechanical */ || effType == 136 /* HealPct */ ||
                    ((effType == 6 /* ApplyAura */ || effType == 27 /* PersistentAreaAura */) && auraType == 8 /* PeriodicHeal */));
        }

        float GetEffectBonusValue(CGPlayer_C* player, const SpellRec* spell, uint32_t effectIndex, int32_t spellLevel, int32_t parsePet, int32_t parseUnit, int32_t noModifiers)
        {
            if (!player || !spell || effectIndex >= 32)
                return 0.0f;

            bool healing = IsEffectHealing(spell, effectIndex);
            float value = 0.0f;

            SpellEffectCacheRow* customEffect = SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIndex));
            if (customEffect)
            {
                value = ApplyScalarValuesForPlayer(player, spell, customEffect->effectSpellPowerBonus,
                                                   customEffect->effectAttackPowerBonus, customEffect->effectBlockValueBonus,
                                                   customEffect->effectScalingMode, healing);
                value += static_cast<float>(customEffect->effectBasePoints + 1);
                return value;
            }

            SpellEffectScalarsRow* scalarRow = SpellEffectScalarsDB::Find(spell->m_id, effectIndex);
            if (scalarRow)
            {
                value = ApplyScalarValuesForPlayer(player, spell, scalarRow->sp, scalarRow->ap, scalarRow->bv, scalarRow->mode, healing);
            }

            if (effectIndex < 3)
            {
                float minP = 0.0f, maxP = 0.0f;
                Spell_C_GetMinMaxPoints(spell, effectIndex, &minP, &maxP, spellLevel, parsePet, parseUnit, noModifiers);
                value += minP;
            }

            return value;
        }

        static bool IsVariableBoundary(char c)
        {
            unsigned char uc = static_cast<unsigned char>(c);
            return c == '\0' || (!std::isalnum(uc) && c != '_');
        }

        static bool TryAppendSpellNameToken(const char* text, size_t& i, std::string& expanded)
        {
            if (text[i] != '$')
                return false;

            const char* idStart = text + i + 1;
            bool bracketed = false;
            if (*idStart == '<')
            {
                bracketed = true;
                ++idStart;
            }

            if (!std::isdigit(static_cast<unsigned char>(*idStart)))
                return false;

            char* idEnd = nullptr;
            unsigned long spellId = std::strtoul(idStart, &idEnd, 10);
            if (!idEnd || idEnd == idStart || spellId == 0)
                return false;

            if (bracketed)
            {
                if (*idEnd != '>')
                    return false;
                ++idEnd;
            }

            if (std::strncmp(idEnd, "name", 4) != 0 || !IsVariableBoundary(idEnd[4]))
                return false;

            SpellRec targetSpell;
            if (!g_spellDB.GetRecord(static_cast<int32_t>(spellId), &targetSpell) || !targetSpell.m_name || !*targetSpell.m_name)
                return false;

            expanded.append("|cffffffff");
            expanded.append(targetSpell.m_name);
            expanded.append("|r");
            i = static_cast<size_t>((idEnd + 4) - text);
            return true;
        }

        static bool TryAppendDirectEffectToken(const SpellRec* spell, const char* text, size_t& i, std::string& expanded)
        {
            if (text[i] != '$' || !spell)
                return false;

            char token = text[i + 1];
            switch (token)
            {
                case 'd': case 'D':
                case 'g': case 'G':
                case 'h': case 'H':
                case 's': case 'S':
                case 'v': case 'V':
                case 'z': case 'Z':
                    break;
                default:
                    return false;
            }

            const char* indexStart = text + i + 2;
            if (!std::isdigit(static_cast<unsigned char>(*indexStart)))
                return false;

            char* indexEnd = nullptr;
            unsigned long ordinal = std::strtoul(indexStart, &indexEnd, 10);
            if (!indexEnd || indexEnd == indexStart || !IsVariableBoundary(*indexEnd))
                return false;

            if (ordinal < 1 || ordinal > 32)
            {
                expanded.push_back('0');
                i = static_cast<size_t>(indexEnd - text);
                return true;
            }

            uint32_t effectIndex = static_cast<uint32_t>(ordinal - 1);
            SpellEffectCacheRow* customEffect = SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIndex));

            float val = 0.0f;
            bool isInt = true;

            switch (token)
            {
                case 'd': case 'D':
                    val = customEffect ? customEffect->effectPointsPerCombo : (effectIndex < 3 ? spell->m_effectPointsPerCombo[effectIndex] : 0.0f);
                    isInt = false;
                    break;
                case 'g': case 'G':
                    val = customEffect ? customEffect->effectMultipleValue : (effectIndex < 3 ? spell->m_effectAmplitude[effectIndex] : 0.0f);
                    isInt = false;
                    break;
                case 'h': case 'H':
                    val = customEffect ? customEffect->effectChainAmplitude : (effectIndex < 3 ? spell->m_effectChainAmplitude[effectIndex] : 0.0f);
                    isInt = (token == 'H');
                    break;
                case 's': case 'S':
                    val = static_cast<float>(customEffect ? customEffect->effectMiscValue : (effectIndex < 3 ? spell->m_effectMiscValue[effectIndex] : 0));
                    break;
                case 'v': case 'V':
                    val = static_cast<float>((spell->m_procTypeMask & 1) ? 5000 : (customEffect ? customEffect->effectAmplitude : (effectIndex < 3 ? spell->m_effectAuraPeriod[effectIndex] : 0)));
                    break;
                case 'z': case 'Z':
                    val = static_cast<float>(customEffect ? customEffect->effectChainTargets : (effectIndex < 3 ? spell->m_effectChainTargets[effectIndex] : 0));
                    break;
            }

            char numBuf[64] = {};
            if (isInt)
                std::snprintf(numBuf, sizeof(numBuf), "%d", static_cast<int>(val));
            else
                std::snprintf(numBuf, sizeof(numBuf), "%.1f", val);

            expanded.append(numBuf);
            i = static_cast<size_t>(indexEnd - text);
            return true;
        }

        std::string ExpandTooltipText(const SpellRec* spell, const char* text)
        {
            if (!text || !*text)
                return text ? std::string(text) : std::string();

            std::string expanded;
            expanded.reserve(std::strlen(text));

            for (size_t i = 0; text[i] != '\0';)
            {
                if (TryAppendSpellNameToken(text, i, expanded))
                    continue;

                // Only perform direct effect token replacement for effects 4..32 so stock effects 1..3
                // retain native modifiers (talents, spell mods, glyphs).
                if (text[i] == '$' && (text[i + 1] == 's' || text[i + 1] == 'S' || text[i + 1] == 'd' || text[i + 1] == 'D' ||
                                      text[i + 1] == 'g' || text[i + 1] == 'G' || text[i + 1] == 'h' || text[i + 1] == 'H' ||
                                      text[i + 1] == 'v' || text[i + 1] == 'V' || text[i + 1] == 'z' || text[i + 1] == 'Z'))
                {
                    char* idxEnd = nullptr;
                    unsigned long ord = std::strtoul(text + i + 2, &idxEnd, 10);
                    if (idxEnd && idxEnd != text + i + 2 && ord >= 4 && ord <= 32 && IsVariableBoundary(*idxEnd))
                    {
                        if (TryAppendDirectEffectToken(spell, text, i, expanded))
                            continue;
                    }
                }

                if (text[i] != '$' || text[i + 1] == '{')
                {
                    expanded.push_back(text[i++]);
                    continue;
                }

                // Check for unbraced extended variables (e.g. $hp, $bon1, $power1, $mastery1, etc.)
                const char* const fixedNames[] = {
                    "hp", "HP", "dpct", "ppct", "MASTERY",
                    "mastery1", "mastery2", "mastery3", "mastery4",
                    "power1", "power2", "power3", "power4", "power5", "power6", "power7",
                    "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7"
                };

                size_t bestLen = 0;
                for (const char* fn : fixedNames)
                {
                    size_t len = std::strlen(fn);
                    if (std::strncmp(text + i + 1, fn, len) == 0 && IsVariableBoundary(text[i + 1 + len]))
                    {
                        if (len > bestLen)
                            bestLen = len;
                    }
                }

                // Check indexed prefixes: ppl1..32, PPL1..32, bon1..32
                if (bestLen == 0)
                {
                    const char* const pfx3[] = { "ppl", "PPL", "bon" };
                    for (const char* pfx : pfx3)
                    {
                        size_t pLen = std::strlen(pfx);
                        if (std::strncmp(text + i + 1, pfx, pLen) == 0)
                        {
                            char* pEnd = nullptr;
                            unsigned long num = std::strtoul(text + i + 1 + pLen, &pEnd, 10);
                            if (pEnd && pEnd != text + i + 1 + pLen && num >= 1 && num <= 32 && IsVariableBoundary(*pEnd))
                            {
                                bestLen = static_cast<size_t>(pEnd - (text + i + 1));
                                break;
                            }
                        }
                    }
                }

                // Check 2-letter prefix: bc, BC (indices 1..32)
                if (bestLen == 0)
                {
                    if (std::strncmp(text + i + 1, "bc", 2) == 0 || std::strncmp(text + i + 1, "BC", 2) == 0)
                    {
                        char* pEnd = nullptr;
                        unsigned long num = std::strtoul(text + i + 1 + 2, &pEnd, 10);
                        if (pEnd && pEnd != text + i + 1 + 2 && num >= 1 && num <= 32 && IsVariableBoundary(*pEnd))
                        {
                            bestLen = static_cast<size_t>(pEnd - (text + i + 1));
                        }
                    }
                }

                if (bestLen > 0)
                {
                    expanded.append("${");
                    expanded.append(text + i + 1, bestLen);
                    expanded.push_back('}');
                    i += bestLen + 1;
                }
                else
                {
                    expanded.push_back(text[i++]);
                }
            }

            return expanded;
        }

        int32_t FormatSpellPowerCost(char* dest, uint32_t destSize, const SpellRec* spell, const CGUnit_C* caster, int32_t powerCost, const PowerDisplayRec* display)
        {
            if (!spell || !dest || destSize == 0)
                return powerCost;

            SpellAdditionalAttributesRow* customAttr = Duskhaven::SpellAdditionalAttributesDB::Get(spell->m_id);
            if (customAttr && (customAttr->customAttr2 & SPELL_ATTR2_CU_DO_NOT_DISPLAY_POWER_COST))
            {
                dest[0] = '\0';
                return 0;
            }

            SpellAdditionalCostDataRow* addCost = SpellAdditionalCostDataDB::Get(spell->m_id);
            int32_t costPerSec = Spell_C_GetPowerCostPerSecond(spell, caster);

            if (powerCost > 0 && addCost && addCost->maximumAdditionalSpend > 0 && costPerSec <= 0)
            {
                uint32_t maxSpend = static_cast<uint32_t>(powerCost) + static_cast<uint32_t>(addCost->maximumAdditionalSpend);
                const char* rName = addCost->resourceName ? addCost->resourceName : "";
                if (maxSpend != 1 && addCost->resourceNamePlural && addCost->resourceNamePlural[0])
                    rName = addCost->resourceNamePlural;

                SStrPrintf(dest, destSize, "%d to %u %s", powerCost, maxSpend, rName);
                if (maxSpend != 1 && (!addCost->resourceNamePlural || !addCost->resourceNamePlural[0]) && (addCost->flags & SpellAdditionalCostFlags::PluralizeResourceName))
                {
                    SStrPack(dest, "s", destSize);
                }
                return powerCost;
            }

            if (addCost && addCost->cost > 0)
            {
                if (dest[0] != '\0')
                    SStrPack(dest, " + ", destSize);

                const char* rName = addCost->resourceName ? addCost->resourceName : "";
                if (addCost->cost != 1 && addCost->resourceNamePlural && addCost->resourceNamePlural[0])
                    rName = addCost->resourceNamePlural;

                char addBuf[128];
                SStrPrintf(addBuf, sizeof(addBuf), "%d %s", addCost->cost, rName);
                SStrPack(dest, addBuf, destSize);
                if (addCost->cost != 1 && (!addCost->resourceNamePlural || !addCost->resourceNamePlural[0]) && (addCost->flags & SpellAdditionalCostFlags::PluralizeResourceName))
                {
                    SStrPack(dest, "s", destSize);
                }
            }

            return powerCost;
        }

        void FormatSpellCooldown(const SpellRec* spell, int32_t cooldown, char* recastTime, uint32_t recastSize)
        {
            if (!spell || !recastTime || recastSize == 0)
                return;

            bool isCharged = false;
            int32_t recoveryTime = cooldown;

            auto it = Duskhaven::Character::s_spellChargeMap.find(spell->m_id);
            if (it != Duskhaven::Character::s_spellChargeMap.end())
            {
                const auto& charge = it->second;
                if (charge.cooldown)
                    recoveryTime = static_cast<int32_t>(charge.cooldown);
                else if (charge.remainingCooldown)
                    recoveryTime = static_cast<int32_t>(charge.remainingCooldown);
                isCharged = charge.maxCharges > 1;
            }

            if (recoveryTime > 0)
            {
                int32_t recastMinutes = recoveryTime >= 60000;
                char format[128];
                FormatString(recastMinutes ? "SPELL_RECAST_TIME_MIN" : "SPELL_RECAST_TIME_SEC", -1, GENDER_NOT_APPLICABLE, format, sizeof(format));
                if (isCharged)
                    SStrPack(format, FrameScript_GetText("SPELL_RECAST_RECHARGE", -1, GENDER_NOT_APPLICABLE), sizeof(format));
                else
                    SStrPack(format, FrameScript_GetText("SPELL_RECAST_COOLDOWN", -1, GENDER_NOT_APPLICABLE), sizeof(format));

                SStrPrintf(recastTime, recastSize, format, recoveryTime / (recastMinutes ? 60000.0f : 1000.0f));
            }
            else
            {
                recastTime[0] = '\0';
            }
        }

        uint32_t GetSpellRemainingCooldown(uint32_t spellId, uint32_t currentCooldown)
        {
            auto it = Duskhaven::Character::s_spellChargeMap.find(spellId);
            if (it == Duskhaven::Character::s_spellChargeMap.end())
                return currentCooldown;

            auto& charge = it->second;
            if (charge.maxCharges > 1 && charge.currentCharges > 0)
                return 0;

            uint32_t remaining = currentCooldown;
            if (charge.remainingCooldown >= currentCooldown)
            {
                uint32_t currAsync = OsGetAsyncTimeMs();
                if (charge.remainingCooldown > (currAsync - charge.async))
                    remaining = charge.remainingCooldown + (charge.async - currAsync);
                else
                    remaining = 0;

                charge.remainingCooldown = remaining;
                charge.async = currAsync;
            }
            return remaining;
        }

        void ProjectEffectIntoFirstSlot(const SpellRec* spell, const ::SpellEffectCacheRow& effect, SpellRec& projected)
        {
            if (spell)
                projected = *spell;
            else
                std::memset(&projected, 0, sizeof(projected));

            projected.m_effect[0]                   = effect.effect;
            projected.m_effectDieSides[0]           = static_cast<uint32_t>(effect.effectDieSides);
            projected.m_effectRealPointsPerLevel[0] = effect.effectRealPointsPerLevel;
            projected.m_effectBasePoints[0]         = static_cast<uint32_t>(effect.effectBasePoints);
            projected.m_effectMechanic[0]           = effect.effectMechanic;
            projected.m_implicitTargetA[0]          = effect.effectImplicitTargetA;
            projected.m_implicitTargetB[0]          = effect.effectImplicitTargetB;
            projected.m_effectRadiusIndex[0]        = effect.effectRadiusIndex;
            projected.m_effectAura[0]               = effect.effectApplyAuraName;
            projected.m_effectAuraPeriod[0]         = effect.effectAmplitude;
            projected.m_effectAmplitude[0]          = effect.effectMultipleValue;
            projected.m_effectChainTargets[0]       = effect.effectChainTargets;
            projected.m_effectItemType[0]           = static_cast<uint32_t>(effect.effectItemType);
            projected.m_effectMiscValue[0]          = static_cast<uint32_t>(effect.effectMiscValue);
            projected.m_effectMiscValueB[0]         = static_cast<uint32_t>(effect.effectMiscValueB);
            projected.m_effectTriggerSpell[0]       = effect.effectTriggerSpell;
            projected.m_effectPointsPerCombo[0]     = effect.effectPointsPerCombo;
            projected.m_effectSpellClassMaskA[0]    = effect.effectSpellClassMaskA;
            projected.m_effectSpellClassMaskB[0]    = effect.effectSpellClassMaskB;
            projected.m_effectSpellClassMaskC[0]    = effect.effectSpellClassMaskC;
            projected.m_effectChainAmplitude[0]     = effect.effectChainAmplitude;
            projected.m_effectBonusCoefficient[0]   = effect.effectBonusMultiplier;

            for (size_t slot = 1; slot < 3; ++slot)
            {
                projected.m_effect[slot] = 0;
                projected.m_effectDieSides[slot] = 0;
                projected.m_effectRealPointsPerLevel[slot] = 0.0f;
                projected.m_effectBasePoints[slot] = 0;
                projected.m_effectMechanic[slot] = 0;
                projected.m_implicitTargetA[slot] = 0;
                projected.m_implicitTargetB[slot] = 0;
                projected.m_effectRadiusIndex[slot] = 0;
                projected.m_effectAura[slot] = 0;
                projected.m_effectAuraPeriod[slot] = 0;
                projected.m_effectAmplitude[slot] = 0;
                projected.m_effectChainTargets[slot] = 0;
                projected.m_effectItemType[slot] = 0;
                projected.m_effectMiscValue[slot] = 0;
                projected.m_effectMiscValueB[slot] = 0;
                projected.m_effectTriggerSpell[slot] = 0;
                projected.m_effectPointsPerCombo[slot] = 0.0f;
                projected.m_effectSpellClassMaskA[slot] = 0;
                projected.m_effectSpellClassMaskB[slot] = 0;
                projected.m_effectSpellClassMaskC[slot] = 0;
                projected.m_effectChainAmplitude[slot] = 0.0f;
                projected.m_effectBonusCoefficient[slot] = 0.0f;
            }
        }

    } // namespace Tooltip
} // namespace Duskhaven
