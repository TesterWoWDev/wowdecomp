#include "TooltipEvaluator.h"
#include "ModCore.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellDurationRec.h"
#include "WoW/Source/DB/Rec/SpellIconRec.h"
#include "WoW/Source/DB/Rec/SpellRadiusRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ModCore
{
    static constexpr uint32_t SPELLVARIABLE_EFFECT_SLOT_COUNT = 32;
    static constexpr uint32_t kTooltipDamageTypeBleed = 0x80000000;

    enum SpellEffectType : uint32_t {
        SpellEffectSchoolDamage       = 2,
        SpellEffectApplyAura          = 6,
        SpellEffectHealthLeech        = 9,
        SpellEffectHeal               = 10,
        SpellEffectPersistentAreaAura = 27,
        SpellEffectHealMaxHealth      = 67,
        SpellEffectHealMechanical     = 75,
        SpellEffectHealPct            = 136,
    };

    enum AuraType : uint32_t {
        AuraPeriodicDamage                = 3,
        AuraPeriodicHeal                  = 8,
        AuraProcTriggerDamage             = 43,
        AuraPeriodicLeech                 = 53,
        AuraPeriodicHealthFunnel          = 62,
        AuraPeriodicManaLeech             = 64,
        AuraPeriodicDamagePct             = 89,
        AuraPeriodicDummy                 = 226,
        AuraPeriodicTriggerSpellWithValue = 227,
    };

    enum SpellMechanicType : uint32_t {
        SpellMechanicBleed = 15,
    };

    enum SpellModType : int32_t {
        SpellModDamage       = 0,
        SpellModDuration     = 1,
        SpellModRadius       = 6,
        SpellModAllEffects   = 8,
        SpellModCooldown     = 11,
        SpellModTargetCount  = 17,
        SpellModProcChance   = 18,
        SpellModPeriod       = 19,
        SpellModScalingRatio = 24,
        SpellModValueMult    = 27,
        SpellModHealing      = 29,
    };

    namespace SpellAttr5 {
        constexpr uint32_t StartPeriodicAtApply = 0x200;
    }

    static constexpr uint32_t kSchoolPhysical = 0x01;
    static constexpr uint32_t kSchoolHoly     = 0x02;
    static constexpr uint32_t kSchoolFire     = 0x04;
    static constexpr uint32_t kSchoolNature   = 0x08;
    static constexpr uint32_t kSchoolFrost    = 0x10;
    static constexpr uint32_t kSchoolShadow   = 0x20;
    static constexpr uint32_t kSchoolArcane   = 0x40;

    static uint32_t OwnedTooltipPrimarySchoolMask(uint32_t schoolMask)
    {
        if (schoolMask & kSchoolFire)
            return kSchoolFire;
        if (schoolMask & kSchoolFrost)
            return kSchoolFrost;
        if (schoolMask & kSchoolNature)
            return kSchoolNature;
        if (schoolMask & kSchoolShadow)
            return kSchoolShadow;
        if (schoolMask & kSchoolArcane)
            return kSchoolArcane;
        if (schoolMask & kSchoolHoly)
            return kSchoolHoly;
        if (schoolMask & kSchoolPhysical)
            return kSchoolPhysical;
        return 0;
    }

    static uint32_t OwnedTooltipSpellSchoolMask(const SpellRec* spell)
    {
        if (!spell)
            return kSchoolPhysical;
        uint32_t mask = OwnedTooltipPrimarySchoolMask(spell->m_schoolMask);
        return mask ? mask : kSchoolPhysical;
    }

    static const char* OwnedTooltipSchoolName(uint32_t schoolMask)
    {
        schoolMask = OwnedTooltipPrimarySchoolMask(schoolMask);
        if (schoolMask == kSchoolFire)
            return "Fire";
        if (schoolMask == kSchoolFrost)
            return "Frost";
        if (schoolMask == kSchoolNature)
            return "Nature";
        if (schoolMask == kSchoolShadow)
            return "Shadow";
        if (schoolMask == kSchoolArcane)
            return "Arcane";
        if (schoolMask == kSchoolHoly)
            return "Holy";
        if (schoolMask == kSchoolPhysical)
            return "Physical";
        return nullptr;
    }

    static const char* OwnedTooltipSchoolColor(uint32_t schoolMask)
    {
        schoolMask = OwnedTooltipPrimarySchoolMask(schoolMask);
        if (schoolMask == kSchoolFire)
            return "|cffff8000";
        if (schoolMask == kSchoolFrost)
            return "|cff80c0ff";
        if (schoolMask == kSchoolNature)
            return "|cff40c040";
        if (schoolMask == kSchoolShadow)
            return "|cff805080";
        if (schoolMask == kSchoolArcane)
            return "|cffff80ff";
        if (schoolMask == kSchoolHoly)
            return "|cffffe080";
        if (schoolMask == kSchoolPhysical)
            return "|cffffffff";
        return nullptr;
    }

    static const char* OwnedTooltipDamageTypeColor(uint32_t damageType)
    {
        if (damageType == kTooltipDamageTypeBleed)
            return "|cffb03030";
        return OwnedTooltipSchoolColor(damageType);
    }

    static const char* OwnedTooltipDamageTypeName(uint32_t damageType)
    {
        if (damageType == kTooltipDamageTypeBleed)
            return "Bleed";
        return OwnedTooltipSchoolName(damageType);
    }

    static int s_ownedTooltipExpressionEvaluationDepth = 0;
    static uint32_t s_ownedTooltipTextParseDepth = 0;
    static bool s_ownedTooltipHasLastPluralValue = false;
    static float s_ownedTooltipLastPluralValue = 0.0f;

    struct OwnedTooltipExpressionColorState
    {
        bool hasValue       = false;
        bool hasDamage      = false;
        bool conflict       = false;
        uint32_t schoolMask = 0;
    };
    static OwnedTooltipExpressionColorState s_ownedTooltipExpressionColor;

    static void RecordOwnedTooltipPluralValue(float value)
    {
        s_ownedTooltipHasLastPluralValue = true;
        s_ownedTooltipLastPluralValue = value;
    }

    static void AppendOwnedTooltipNumber(std::string& out, float value, bool integerValue)
    {
        RecordOwnedTooltipPluralValue(value);
        char buffer[64] = {};
        if (integerValue)
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int32_t>(value));
        else
            std::snprintf(buffer, sizeof(buffer), "%.1f", value);
        out.append(buffer);
    }

    static void AppendOwnedTooltipDuration(std::string& out, uint32_t milliseconds)
    {
        if (!milliseconds)
        {
            out.push_back('0');
            return;
        }

        char buffer[128] = {};
        uint32_t value = milliseconds;
        const char* unit = "ms";
        if (milliseconds >= 3600000 && milliseconds % 3600000 == 0)
        {
            value = milliseconds / 3600000;
            unit = value == 1 ? "hour" : "hours";
        }
        else if (milliseconds >= 60000 && milliseconds % 60000 == 0)
        {
            value = milliseconds / 60000;
            unit = "min";
        }
        else if (milliseconds >= 1000)
        {
            value = milliseconds / 1000;
            unit = "sec";
        }

        std::snprintf(buffer, sizeof(buffer), "%u %s", value, unit);
        out.append(buffer);
    }

    static void AppendOwnedTooltipTimeValue(std::string& out, float milliseconds)
    {
        if (milliseconds <= 0.0f)
        {
            out.append("0 sec");
            return;
        }

        float divisor = 1000.0f;
        const char* unit = "sec";
        if (milliseconds >= 3600000.0f)
        {
            divisor = 3600000.0f;
            unit = "hour";
        }
        else if (milliseconds >= 60000.0f)
        {
            divisor = 60000.0f;
            unit = "min";
        }

        double scaled = static_cast<double>(milliseconds) / static_cast<double>(divisor);

        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "%.2f", scaled);
        size_t len = std::strlen(buffer);
        while (len > 0 && buffer[len - 1] == '0')
            buffer[--len] = '\0';
        if (len > 0 && buffer[len - 1] == '.')
            buffer[--len] = '\0';

        out.append(buffer);
        out.push_back(' ');
        out.append(unit);
        if (std::strcmp(unit, "hour") == 0 && std::strcmp(buffer, "1") != 0)
            out.push_back('s');
    }

    static void AppendOwnedTooltipExpressionNumber(std::string& out, double value)
    {
        RecordOwnedTooltipPluralValue(static_cast<float>(value));
        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "%.3f", value);
        size_t len = std::strlen(buffer);
        while (len > 0 && buffer[len - 1] == '0')
            buffer[--len] = '\0';
        if (len > 0 && buffer[len - 1] == '.')
            buffer[--len] = '\0';
        out.append(buffer);
    }

    static void AppendOwnedTooltipError(std::string& out)
    {
        out.append("|cffff0000ERROR|r");
    }

    struct OwnedTooltipMacroContext
    {
        const SpellRec* currentSpell = nullptr;
        SpellRec targetStorage       = {};
        const SpellRec* targetSpell  = nullptr;
        std::vector<uint32_t> args;
        uint32_t effectIndex         = 0;
        uint32_t auraStackCount      = 0;
    };

    static void ApplyOwnedTooltipSpellMod(const SpellRec* spell, int modOp, float& value)
    {
        if (spell)
            Spell_C_ModifySpellValue(spell, value, modOp);
    }

    static uint32_t OwnedTooltipEffectType(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effect[ctx.effectIndex];
        return 0;
    }

    static uint32_t OwnedTooltipEffectAuraType(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effectAura[ctx.effectIndex];
        return 0;
    }

    static uint32_t OwnedTooltipEffectMechanic(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.targetSpell && ctx.effectIndex < 3)
            return ctx.targetSpell->m_effectMechanic[ctx.effectIndex];
        return 0;
    }

    static bool IsOwnedTooltipAuraEffect(uint32_t effectType)
    {
        return effectType == SpellEffectApplyAura || effectType == SpellEffectPersistentAreaAura;
    }

    static bool IsOwnedTooltipPeriodicAura(uint32_t auraType)
    {
        return auraType == AuraPeriodicDamage || auraType == AuraPeriodicHeal ||
               auraType == AuraPeriodicLeech || auraType == AuraPeriodicHealthFunnel ||
               auraType == AuraPeriodicManaLeech || auraType == AuraPeriodicDamagePct ||
               auraType == AuraPeriodicDummy || auraType == AuraPeriodicTriggerSpellWithValue;
    }

    static bool OwnedTooltipEffectIsPeriodic(OwnedTooltipMacroContext& ctx)
    {
        return IsOwnedTooltipAuraEffect(OwnedTooltipEffectType(ctx)) &&
               IsOwnedTooltipPeriodicAura(OwnedTooltipEffectAuraType(ctx));
    }

    static bool IsOwnedTooltipDamageEffect(uint32_t effectType)
    {
        return effectType == SpellEffectSchoolDamage || effectType == SpellEffectHealthLeech;
    }

    static bool IsOwnedTooltipHealingEffect(uint32_t effectType)
    {
        return effectType == SpellEffectHeal || effectType == SpellEffectHealMaxHealth ||
               effectType == SpellEffectHealMechanical || effectType == SpellEffectHealPct;
    }

    static bool IsOwnedTooltipHealAmountEffect(uint32_t effectType)
    {
        return effectType == SpellEffectHeal || effectType == SpellEffectHealMechanical;
    }

    static bool OwnedTooltipValueIsDamage(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipDamageEffect(effectType))
            return true;
        if (!IsOwnedTooltipAuraEffect(effectType))
            return false;
        uint32_t const auraType = OwnedTooltipEffectAuraType(ctx);
        return auraType == AuraPeriodicDamage || auraType == AuraProcTriggerDamage;
    }

    static bool OwnedTooltipValueIsHealing(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipHealingEffect(effectType))
            return true;
        return IsOwnedTooltipAuraEffect(effectType) && OwnedTooltipEffectAuraType(ctx) == AuraPeriodicHeal;
    }

    static bool OwnedTooltipValueIsHealAmount(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (IsOwnedTooltipHealAmountEffect(effectType))
            return true;
        return IsOwnedTooltipAuraEffect(effectType) && OwnedTooltipEffectAuraType(ctx) == AuraPeriodicHeal;
    }

    static bool OwnedTooltipValueIsBleed(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (!OwnedTooltipValueIsDamage(ctx, effectType))
            return false;
        uint32_t const effectMechanic = OwnedTooltipEffectMechanic(ctx);
        if (effectMechanic == SpellMechanicBleed)
            return true;
        return ctx.targetSpell && ctx.targetSpell->m_mechanic == SpellMechanicBleed;
    }

    static uint32_t OwnedTooltipValueDamageType(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (OwnedTooltipValueIsBleed(ctx, effectType))
            return kTooltipDamageTypeBleed;
        return ctx.targetSpell ? OwnedTooltipSpellSchoolMask(ctx.targetSpell) : 0;
    }

    static const char* OwnedTooltipValueColor(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth != 0)
            return nullptr;
        if (OwnedTooltipValueIsHealing(ctx, effectType))
            return "|cff9dff9d";
        if (OwnedTooltipValueIsDamage(ctx, effectType) && ctx.targetSpell)
            return OwnedTooltipDamageTypeColor(OwnedTooltipValueDamageType(ctx, effectType));
        return nullptr;
    }

    static std::string OwnedTooltipValueSuffix(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth != 0)
            return "";
        if (OwnedTooltipValueIsHealAmount(ctx, effectType))
            return " Health";
        if (!OwnedTooltipValueIsDamage(ctx, effectType) || !ctx.targetSpell)
            return "";
        uint32_t const damageType = OwnedTooltipValueDamageType(ctx, effectType);
        const char* damageName    = OwnedTooltipDamageTypeName(damageType);
        if (!damageName)
            return "";
        return std::string(" ") + damageName + " damage";
    }

    static void RecordOwnedTooltipExpressionValue(OwnedTooltipMacroContext& ctx, uint32_t effectType)
    {
        if (s_ownedTooltipExpressionEvaluationDepth == 0)
            return;
        s_ownedTooltipExpressionColor.hasValue = true;
        if (!OwnedTooltipValueIsDamage(ctx, effectType) || !ctx.targetSpell)
        {
            s_ownedTooltipExpressionColor.conflict = true;
            return;
        }
        uint32_t const damageType = OwnedTooltipValueDamageType(ctx, effectType);
        if (!damageType)
        {
            s_ownedTooltipExpressionColor.conflict = true;
            return;
        }
        if (!s_ownedTooltipExpressionColor.hasDamage)
        {
            s_ownedTooltipExpressionColor.hasDamage  = true;
            s_ownedTooltipExpressionColor.schoolMask = damageType;
            return;
        }
        if (s_ownedTooltipExpressionColor.schoolMask != damageType)
            s_ownedTooltipExpressionColor.conflict = true;
    }

    static float OwnedTooltipRadiusValue(uint32_t radiusIndex)
    {
        if (!radiusIndex)
            return 0.0f;
        SpellRadiusRec* rec = g_spellRadiusDB.GetRecord(static_cast<int32_t>(radiusIndex));
        return rec ? rec->m_radius : static_cast<float>(radiusIndex);
    }

    static uint32_t OwnedTooltipDurationMs(const SpellRec* spell)
    {
        if (!spell || !spell->m_durationIndex)
            return 0;
        SpellDurationRec* rec = g_spellDurationDB.GetRecord(spell->m_durationIndex);
        return (rec && rec->m_duration > 0) ? static_cast<uint32_t>(rec->m_duration) : 0;
    }

    static uint32_t OwnedTooltipCooldownMs(const SpellRec* spell)
    {
        if (!spell)
            return 0;
        float cooldown = static_cast<float>((std::max)(spell->m_recoveryTime, spell->m_categoryRecoveryTime));
        ApplyOwnedTooltipSpellMod(spell, SpellModCooldown, cooldown);
        return cooldown > 0.0f ? static_cast<uint32_t>(std::round(cooldown)) : 0;
    }

    static uint32_t OwnedTooltipEffectPeriodMs(OwnedTooltipMacroContext& ctx)
    {
        float period = 0.0f;
        if (ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1))
            period = 5000.0f;
        else if (ctx.targetSpell && ctx.effectIndex < 3)
            period = static_cast<float>(ctx.targetSpell->m_effectAuraPeriod[ctx.effectIndex]);

        if (period <= 0.0f)
            return 0;

        if (!(ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1)))
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModPeriod, period);

        return period > 0.0f ? static_cast<uint32_t>(period) : 0;
    }

    static uint32_t OwnedTooltipEffectDurationMs(OwnedTooltipMacroContext& ctx)
    {
        if (!ctx.targetSpell)
            return 0;

        float duration = static_cast<float>(OwnedTooltipDurationMs(ctx.targetSpell));
        if (duration <= 0.0f)
            return 0;

        ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModDuration, duration);
        if (duration <= 0.0f)
            return 0;

        if (duration > 30000.0f)
            duration = 30000.0f;

        return static_cast<uint32_t>(duration);
    }

    static float OwnedTooltipEffectHastedPeriodMs(uint32_t period)
    {
        if (!period)
            return 0.0f;

        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        if (!activePlayer || !activePlayer->m_unit)
            return static_cast<float>(period);

        float castSpeed = activePlayer->m_unit->m_unitModCastSpeed;
        if (castSpeed <= 0.0f)
            return static_cast<float>(period);

        return static_cast<float>(period) * castSpeed;
    }

    struct OwnedTooltipTickInfo
    {
        uint32_t wholeTicks    = 1;
        float amountMultiplier = 1.0f;
    };

    static OwnedTooltipTickInfo OwnedTooltipEffectTickInfo(OwnedTooltipMacroContext& ctx)
    {
        OwnedTooltipTickInfo info;
        if (!ctx.targetSpell || !OwnedTooltipEffectIsPeriodic(ctx))
            return info;

        uint32_t const duration = OwnedTooltipEffectDurationMs(ctx);
        uint32_t const period   = OwnedTooltipEffectPeriodMs(ctx);
        if (!duration || !period)
            return info;

        float const hastedPeriod     = OwnedTooltipEffectHastedPeriodMs(period);
        uint32_t const runtimePeriod = hastedPeriod > 0.0f ? static_cast<uint32_t>(hastedPeriod) : period;
        if (!runtimePeriod)
            return info;

        info.wholeTicks = duration / runtimePeriod;
        if (ctx.targetSpell && (ctx.targetSpell->m_attributesExE & SpellAttr5::StartPeriodicAtApply))
            ++info.wholeTicks;
        info.wholeTicks = (std::max)(1u, info.wholeTicks);

        uint32_t const auraType = OwnedTooltipEffectAuraType(ctx);
        if ((auraType == AuraPeriodicDamage || auraType == AuraPeriodicHeal) && hastedPeriod > 0.0f)
        {
            float exactTicks = static_cast<float>(duration) / hastedPeriod;
            if (ctx.targetSpell && (ctx.targetSpell->m_attributesExE & SpellAttr5::StartPeriodicAtApply))
                exactTicks += 1.0f;

            if (exactTicks > static_cast<float>(info.wholeTicks))
                info.amountMultiplier = exactTicks / static_cast<float>(info.wholeTicks);
        }

        return info;
    }

    static uint32_t OwnedTooltipActivePlayerLevel()
    {
        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        return (activePlayer && activePlayer->m_unit) ? activePlayer->m_unit->m_unitFieldLevel : 0;
    }

    static float OwnedTooltipEffectMinPoints(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.targetSpell && ctx.effectIndex < 3)
        {
            float minP = 0.0f, maxP = 0.0f;
            Spell_C_GetMinMaxPoints(ctx.targetSpell, ctx.effectIndex, &minP, &maxP, OwnedTooltipActivePlayerLevel(), 0, 0, 0);
            if (minP != 0.0f || maxP != 0.0f)
                return minP;

            return static_cast<float>(ctx.targetSpell->m_effectBasePoints[ctx.effectIndex] + 1);
        }
        return 0.0f;
    }

    static float OwnedTooltipEffectMaxPoints(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.targetSpell && ctx.effectIndex < 3)
        {
            float minP = 0.0f, maxP = 0.0f;
            Spell_C_GetMinMaxPoints(ctx.targetSpell, ctx.effectIndex, &minP, &maxP, OwnedTooltipActivePlayerLevel(), 0, 0, 0);
            if (minP != 0.0f || maxP != 0.0f)
                return maxP;

            return static_cast<float>(ctx.targetSpell->m_effectBasePoints[ctx.effectIndex] + ctx.targetSpell->m_effectDieSides[ctx.effectIndex]);
        }
        return 0.0f;
    }

    static void AppendOwnedTooltipValueMacro(OwnedTooltipMacroContext& ctx, std::string& out, bool totalPeriodic)
    {
        float const baseValue     = OwnedTooltipEffectMinPoints(ctx);
        float const baseMaxValue  = OwnedTooltipEffectMaxPoints(ctx);
        float value               = baseValue;
        float maxValue            = baseMaxValue;
        uint32_t const effectType = OwnedTooltipEffectType(ctx);
        RecordOwnedTooltipExpressionValue(ctx, effectType);
        const char* valueColor    = OwnedTooltipValueColor(ctx, effectType);

        ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModAllEffects, value);
        ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModAllEffects, maxValue);
        if (OwnedTooltipValueIsHealing(ctx, effectType))
        {
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModHealing, value);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModHealing, maxValue);
        }
        else if (OwnedTooltipValueIsDamage(ctx, effectType))
        {
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModDamage, value);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModDamage, maxValue);
        }

        OwnedTooltipTickInfo const tickInfo = OwnedTooltipEffectTickInfo(ctx);
        if (tickInfo.amountMultiplier != 1.0f)
        {
            value = std::round(value * tickInfo.amountMultiplier);
            maxValue = std::round(maxValue * tickInfo.amountMultiplier);
        }

        if (totalPeriodic && tickInfo.wholeTicks > 1)
        {
            value *= static_cast<float>(tickInfo.wholeTicks);
            maxValue *= static_cast<float>(tickInfo.wholeTicks);
        }

        if (ctx.auraStackCount > 1)
        {
            value *= static_cast<float>(ctx.auraStackCount);
            maxValue *= static_cast<float>(ctx.auraStackCount);
        }

        value                  = std::fabs(value);
        maxValue               = std::fabs(maxValue);
        float const rangeStart = (std::min)(value, maxValue);
        float const rangeEnd   = (std::max)(value, maxValue);

        std::string suffix = OwnedTooltipValueSuffix(ctx, effectType);
        if (valueColor)
            out.append(valueColor);
        AppendOwnedTooltipNumber(out, rangeStart, true);
        if (std::fabs(rangeEnd - rangeStart) >= 0.01f)
        {
            out.append(" to ");
            AppendOwnedTooltipNumber(out, rangeEnd, true);
        }
        out.append(suffix);
        if (valueColor)
            out.append("|r");
    }

    static bool ResolveOwnedTooltipMacroSpell(OwnedTooltipMacroContext& ctx, uint32_t spellId)
    {
        if (ctx.currentSpell && static_cast<uint32_t>(ctx.currentSpell->m_id) == spellId)
        {
            ctx.targetSpell = ctx.currentSpell;
            return true;
        }

        if (spellId && g_spellDB.GetRecord(static_cast<int32_t>(spellId), &ctx.targetStorage))
        {
            ctx.targetSpell = &ctx.targetStorage;
            return true;
        }

        ctx.targetSpell = nullptr;
        return false;
    }

    static bool ResolveOwnedTooltipSpellMacro(OwnedTooltipMacroContext& ctx)
    {
        if (ctx.args.empty())
        {
            ctx.targetSpell = ctx.currentSpell;
            return ctx.targetSpell != nullptr;
        }

        if (ctx.args.size() != 1)
            return false;

        return ResolveOwnedTooltipMacroSpell(ctx, ctx.args[0]);
    }

    static bool ResolveOwnedTooltipEffectMacro(OwnedTooltipMacroContext& ctx)
    {
        uint32_t spellId = 0;
        uint32_t ordinal = 0;

        if (ctx.args.empty())
        {
            if (!ctx.currentSpell)
                return false;
            spellId = static_cast<uint32_t>(ctx.currentSpell->m_id);
            ordinal = 1;
        }
        else if (ctx.args.size() == 1)
        {
            if (!ctx.currentSpell)
                return false;
            spellId = static_cast<uint32_t>(ctx.currentSpell->m_id);
            ordinal = ctx.args[0];
        }
        else if (ctx.args.size() == 2)
        {
            spellId = ctx.args[0];
            ordinal = ctx.args[1];
        }
        else
            return false;

        if (ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT)
            return false;

        ctx.effectIndex = ordinal - 1;
        ResolveOwnedTooltipMacroSpell(ctx, spellId);
        return true;
    }

    static float GetAttackPowerBonusForSpell(CGPlayer_C* player, const SpellRec* spell)
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

    static float TooltipEffectField(OwnedTooltipMacroContext& ctx, const char* field, bool& integerValue)
    {
        integerValue = true;
        if (!ctx.targetSpell)
            return 0.0f;

        uint32_t i               = ctx.effectIndex;
        bool const hasNativeSlot = i < 3;
        if (std::strcmp(field, "value") == 0 || std::strcmp(field, "min") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectBasePoints[i] + 1 : 0);
        if (std::strcmp(field, "basepoints") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectBasePoints[i] : 0);
        if (std::strcmp(field, "max") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectBasePoints[i] +
                                                          ctx.targetSpell->m_effectDieSides[i] : 0);
        if (std::strcmp(field, "multiplevalue") == 0)
        {
            integerValue = false;
            float value  = hasNativeSlot ? ctx.targetSpell->m_effectAmplitude[i] : 0.0f;
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModValueMult, value);
            return value;
        }
        if (std::strcmp(field, "period") == 0 || std::strcmp(field, "periodms") == 0)
        {
            float value = static_cast<float>((ctx.targetSpell->m_procTypeMask & 1) ? 5000 :
                                             hasNativeSlot ? ctx.targetSpell->m_effectAuraPeriod[i] : 0);
            if (!(ctx.targetSpell && (ctx.targetSpell->m_procTypeMask & 1)))
                ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModPeriod, value);
            return value;
        }
        if (std::strcmp(field, "radius") == 0)
        {
            integerValue = false;
            float value  = OwnedTooltipRadiusValue(hasNativeSlot ? ctx.targetSpell->m_effectRadiusIndex[i] : 0);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModRadius, value);
            return value;
        }
        if (std::strcmp(field, "chaintargets") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectChainTargets[i] : 0);
        if (std::strcmp(field, "chainamplitude") == 0)
        {
            integerValue = false;
            return hasNativeSlot ? ctx.targetSpell->m_effectChainAmplitude[i] : 0.0f;
        }
        if (std::strcmp(field, "misc") == 0 || std::strcmp(field, "misca") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectMiscValue[i] : 0);
        if (std::strcmp(field, "miscb") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectMiscValueB[i] : 0);
        if (std::strcmp(field, "triggerspell") == 0)
            return static_cast<float>(hasNativeSlot ? ctx.targetSpell->m_effectTriggerSpell[i] : 0);
        if (std::strcmp(field, "pointspercombo") == 0)
        {
            integerValue = false;
            return hasNativeSlot ? ctx.targetSpell->m_effectPointsPerCombo[i] : 0.0f;
        }
        if (std::strcmp(field, "bonuscoefficient") == 0 || std::strcmp(field, "coefficient") == 0)
        {
            integerValue = false;
            float value  = hasNativeSlot ? ctx.targetSpell->m_effectBonusCoefficient[i] : 0.0f;
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModScalingRatio, value);
            return value;
        }
        if (std::strcmp(field, "pointsperlevel") == 0)
        {
            integerValue = false;
            return hasNativeSlot ? ctx.targetSpell->m_effectRealPointsPerLevel[i] : 0.0f;
        }
        if (std::strcmp(field, "targets") == 0)
        {
            float value = static_cast<float>(ctx.targetSpell->m_maxTargets);
            ApplyOwnedTooltipSpellMod(ctx.targetSpell, SpellModTargetCount, value);
            return value;
        }

        return 0.0f;
    }

    static bool MacroEffectNumber(OwnedTooltipMacroContext& ctx, std::string& out, const char* field)
    {
        if (!ResolveOwnedTooltipEffectMacro(ctx))
            return false;

        bool integerValue = true;
        float value       = 0.0f;
        if (std::strcmp(field, "value") == 0 || std::strcmp(field, "min") == 0)
        {
            AppendOwnedTooltipValueMacro(ctx, out, true);
            return true;
        }
        if (std::strcmp(field, "tickvalue") == 0)
        {
            AppendOwnedTooltipValueMacro(ctx, out, false);
            return true;
        }

        if (std::strcmp(field, "period") == 0 || std::strcmp(field, "periodms") == 0)
        {
            uint32_t const basePeriod = OwnedTooltipEffectPeriodMs(ctx);
            value = static_cast<float>(basePeriod);
            if (OwnedTooltipEffectIsPeriodic(ctx))
                value = OwnedTooltipEffectHastedPeriodMs(basePeriod);

            if (std::strcmp(field, "period") == 0)
            {
                AppendOwnedTooltipTimeValue(out, value);
                return true;
            }

            AppendOwnedTooltipNumber(out, value, true);
            return true;
        }

        value = TooltipEffectField(ctx, field, integerValue);

        if (std::strcmp(field, "radius") == 0)
        {
            AppendOwnedTooltipNumber(out, value, true);
            out.append(" yards");
            return true;
        }

        AppendOwnedTooltipNumber(out, value, integerValue);
        return true;
    }

    static bool MacroSchoolDamage(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
        {
            if (ctx.args.size() == 1)
                return true;
            return false;
        }

        if (!ctx.targetSpell)
            return true;

        uint32_t const schoolMask = OwnedTooltipSpellSchoolMask(ctx.targetSpell);
        const char* schoolName    = OwnedTooltipSchoolName(schoolMask);
        if (!schoolName)
            return true;

        const char* schoolColor =
            s_ownedTooltipExpressionEvaluationDepth == 0 ? OwnedTooltipSchoolColor(schoolMask) : nullptr;
        if (schoolColor)
            out.append(schoolColor);
        out.append(schoolName);
        out.append(" damage");
        if (schoolColor)
            out.append("|r");
        return true;
    }

    static bool MacroMastery(OwnedTooltipMacroContext& /*ctx*/, std::string& out)
    {
        AppendOwnedTooltipNumber(out, 0.0f, false);
        return true;
    }

    static bool MacroAttackPower(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        AppendOwnedTooltipNumber(out, GetAttackPowerBonusForSpell(activePlayer, ctx.targetSpell), true);
        return true;
    }

    static bool MacroProcChance(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        AppendOwnedTooltipNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_procChance) : 0.0f, true);
        return true;
    }

    static bool MacroProcCharges(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        AppendOwnedTooltipNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_procCharges) : 0.0f, true);
        return true;
    }

    static bool MacroMaxTargets(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return false;

        AppendOwnedTooltipNumber(out, ctx.targetSpell ? static_cast<float>(ctx.targetSpell->m_maxTargets) : 0.0f, true);
        return true;
    }

    static bool MacroDuration(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx))
            return true;
        AppendOwnedTooltipDuration(out, OwnedTooltipDurationMs(ctx.targetSpell));
        return true;
    }

    static bool MacroCooldown(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
        {
            AppendOwnedTooltipNumber(out, 0.0f, true);
            return true;
        }
        AppendOwnedTooltipDuration(out, OwnedTooltipCooldownMs(ctx.targetSpell));
        return true;
    }

    static bool MacroName(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return true;
        if (ctx.targetSpell->m_name)
            out.append(ctx.targetSpell->m_name);
        return true;
    }

    static bool MacroIcon(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
            return true;
        SpellIconRec* iconRow = g_spellIconDB.GetRecord(ctx.targetSpell->m_spellIconID);
        if (iconRow && iconRow->m_textureFilename)
            out.append(iconRow->m_textureFilename);
        return true;
    }

    static bool MacroStacks(OwnedTooltipMacroContext& ctx, std::string& out)
    {
        if (!ResolveOwnedTooltipSpellMacro(ctx) || !ctx.targetSpell)
        {
            AppendOwnedTooltipNumber(out, 0.0f, true);
            return true;
        }
        AppendOwnedTooltipNumber(out, static_cast<float>(ctx.targetSpell->m_cumulativeAura), true);
        return true;
    }

    static bool MacroValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "value"); }
    static bool MacroTickValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "tickvalue"); }
    static bool MacroBasePoints(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "basepoints"); }
    static bool MacroMin(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "min"); }
    static bool MacroMax(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "max"); }
    static bool MacroMultipleValue(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "multiplevalue"); }
    static bool MacroPeriod(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "period"); }
    static bool MacroPeriodMs(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "periodms"); }
    static bool MacroRadius(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "radius"); }
    static bool MacroChainTargets(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "chaintargets"); }
    static bool MacroChainAmplitude(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "chainamplitude"); }
    static bool MacroMisc(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "misc"); }
    static bool MacroMiscA(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "misca"); }
    static bool MacroMiscB(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "miscb"); }
    static bool MacroTriggerSpell(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "triggerspell"); }
    static bool MacroPointsPerCombo(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "pointspercombo"); }
    static bool MacroBonusCoefficient(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "bonuscoefficient"); }
    static bool MacroCoefficient(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "coefficient"); }
    static bool MacroPointsPerLevel(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "pointsperlevel"); }
    static bool MacroTargetcount(OwnedTooltipMacroContext& ctx, std::string& out) { return MacroEffectNumber(ctx, out, "targets"); }

    using OwnedTooltipMacroHandler = bool (*)(OwnedTooltipMacroContext&, std::string&);

    static const std::unordered_map<std::string, OwnedTooltipMacroHandler>& OwnedTooltipMacroDefinitions()
    {
        static const std::unordered_map<std::string, OwnedTooltipMacroHandler> definitions = {
            {"duration", MacroDuration},
            {"cooldown", MacroCooldown},
            {"name", MacroName},
            {"icon", MacroIcon},
            {"stacks", MacroStacks},
            {"schooldamage", MacroSchoolDamage},
            {"mastery", MacroMastery},
            {"attackpower", MacroAttackPower},
            {"procchance", MacroProcChance},
            {"proccharges", MacroProcCharges},
            {"maxtargets", MacroMaxTargets},
            {"value", MacroValue},
            {"tickvalue", MacroTickValue},
            {"basepoints", MacroBasePoints},
            {"min", MacroMin},
            {"max", MacroMax},
            {"multiplevalue", MacroMultipleValue},
            {"period", MacroPeriod},
            {"periodms", MacroPeriodMs},
            {"radius", MacroRadius},
            {"radious", MacroRadius},
            {"chaintargets", MacroChainTargets},
            {"chainamplitude", MacroChainAmplitude},
            {"misc", MacroMisc},
            {"misca", MacroMiscA},
            {"miscb", MacroMiscB},
            {"triggerspell", MacroTriggerSpell},
            {"pointspercombo", MacroPointsPerCombo},
            {"bonuscoefficient", MacroBonusCoefficient},
            {"coefficient", MacroCoefficient},
            {"pointsperlevel", MacroPointsPerLevel},
            {"targets", MacroTargetcount},
        };

        return definitions;
    }

    static std::string NormalizeMacroName(const std::string& name)
    {
        std::string normalized;
        normalized.reserve(name.size());
        for (char ch : name)
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        return normalized;
    }

    static bool TryParseMacroArguments(const char* text, size_t textLen, size_t& pos, std::vector<uint32_t>& args)
    {
        args.clear();
        size_t cursor = pos;
        while (cursor < textLen && std::isspace(static_cast<unsigned char>(text[cursor])))
            ++cursor;

        if (cursor >= textLen || text[cursor] != '(')
            return true;

        ++cursor;
        while (cursor < textLen)
        {
            while (cursor < textLen && std::isspace(static_cast<unsigned char>(text[cursor])))
                ++cursor;

            if (cursor < textLen && text[cursor] == ')')
            {
                pos = cursor + 1;
                return true;
            }

            if (cursor >= textLen || !std::isdigit(static_cast<unsigned char>(text[cursor])))
                return false;

            char* end           = nullptr;
            unsigned long value = std::strtoul(text + cursor, &end, 10);
            if (!end || end == text + cursor || value > UINT32_MAX)
                return false;

            args.push_back(static_cast<uint32_t>(value));
            cursor = static_cast<size_t>(end - text);

            while (cursor < textLen && std::isspace(static_cast<unsigned char>(text[cursor])))
                ++cursor;

            if (cursor < textLen && text[cursor] == ',')
            {
                ++cursor;
                continue;
            }

            if (cursor < textLen && text[cursor] == ')')
            {
                pos = cursor + 1;
                return true;
            }

            return false;
        }

        return false;
    }

    static bool ParseOwnedTooltipTextToString(const SpellRec* spell, const char* source, size_t textLen, uint32_t auraStackCount,
                                              std::string& out);

    static bool OwnedTooltipActivePlayerHasSpellOrAura(uint32_t spellId)
    {
        if (!spellId)
            return false;

        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        if (!activePlayer)
            return false;

        return activePlayer->HasAuraBySpellId(static_cast<int32_t>(spellId)) ||
               activePlayer->IsSpellKnown(static_cast<int32_t>(spellId));
    }

    static bool TryAppendOwnedTooltipIfMacro(const SpellRec* spell, const char* text, size_t textLen, size_t& pos,
                                             uint32_t auraStackCount, std::string& out)
    {
        std::vector<uint32_t> args;
        size_t cursor = pos;
        if (!TryParseMacroArguments(text, textLen, cursor, args) || args.size() != 1)
            return false;

        while (cursor < textLen && std::isspace(static_cast<unsigned char>(text[cursor])))
            ++cursor;
        if (cursor >= textLen || text[cursor] != '[')
            return false;

        size_t const trueStart = ++cursor;
        size_t separator       = SIZE_MAX;
        uint32_t squareDepth   = 1;
        uint32_t parenDepth    = 0;
        uint32_t braceDepth    = 0;
        while (cursor < textLen)
        {
            char const ch = text[cursor++];
            if (ch == '[')
                ++squareDepth;
            else if (ch == ']')
            {
                if (--squareDepth == 0)
                {
                    if (separator == SIZE_MAX)
                        return false;

                    bool const condition = OwnedTooltipActivePlayerHasSpellOrAura(args[0]);
                    size_t const branchStart = condition ? trueStart : separator + 1;
                    size_t const branchEnd   = condition ? separator : cursor - 1;
                    std::string expanded;
                    if (!ParseOwnedTooltipTextToString(spell, text + branchStart, branchEnd - branchStart,
                                                       auraStackCount, expanded))
                        return false;
                    out.append(expanded);

                    pos = cursor;
                    return true;
                }
            }
            else if (ch == '(')
                ++parenDepth;
            else if (ch == ')' && parenDepth > 0)
                --parenDepth;
            else if (ch == '{')
                ++braceDepth;
            else if (ch == '}' && braceDepth > 0)
                --braceDepth;
            else if (ch == ':' && squareDepth == 1 && parenDepth == 0 && braceDepth == 0 && separator == SIZE_MAX)
                separator = cursor - 1;
        }

        return false;
    }

    static bool TryAppendOwnedTooltipAsTimeMacro(const SpellRec* spell, const char* text, size_t textLen, size_t& pos,
                                                 uint32_t auraStackCount, std::string& out)
    {
        size_t cursor = pos;
        while (cursor < textLen && std::isspace(static_cast<unsigned char>(text[cursor])))
            ++cursor;

        if (cursor >= textLen || text[cursor] != '(')
            return false;

        size_t const innerStart = ++cursor;
        uint32_t depth          = 1;
        while (cursor < textLen)
        {
            char const ch = text[cursor++];
            if (ch == '(')
                ++depth;
            else if (ch == ')')
            {
                if (--depth == 0)
                {
                    size_t const innerLen = (cursor - 1) - innerStart;
                    std::string expanded;
                    if (!ParseOwnedTooltipTextToString(spell, text + innerStart, innerLen, auraStackCount, expanded))
                        return false;

                    char* end          = nullptr;
                    float milliseconds = std::strtof(expanded.c_str(), &end);
                    if (!end || end == expanded.c_str())
                        milliseconds = 0.0f;

                    AppendOwnedTooltipTimeValue(out, milliseconds);
                    pos = cursor;
                    return true;
                }
            }
        }

        return false;
    }

    static bool TryAppendOwnedTooltipMacro(const SpellRec* spell, const char* text, size_t textLen, size_t& pos,
                                           uint32_t auraStackCount, std::string& out)
    {
        if (pos >= textLen || text[pos] != '@')
            return false;

        size_t nameStart = pos + 1;
        if (nameStart >= textLen || !std::isalpha(static_cast<unsigned char>(text[nameStart])))
            return false;

        size_t nameEnd = nameStart + 1;
        while (nameEnd < textLen && std::isalpha(static_cast<unsigned char>(text[nameEnd])))
            ++nameEnd;

        std::string macro(text + nameStart, nameEnd - nameStart);
        size_t afterName                  = nameEnd;
        std::string const normalizedMacro = NormalizeMacroName(macro);
        if (normalizedMacro == "astime")
        {
            if (!TryAppendOwnedTooltipAsTimeMacro(spell, text, textLen, afterName, auraStackCount, out))
                return false;
            pos = afterName;
            return true;
        }
        if (normalizedMacro == "if")
        {
            if (!TryAppendOwnedTooltipIfMacro(spell, text, textLen, afterName, auraStackCount, out))
                return false;
            pos = afterName;
            return true;
        }

        std::vector<uint32_t> args;
        if (!TryParseMacroArguments(text, textLen, afterName, args))
            return false;

        auto const& definitions = OwnedTooltipMacroDefinitions();
        auto it                 = definitions.find(normalizedMacro);
        if (it == definitions.end())
            return false;

        OwnedTooltipMacroContext ctx;
        ctx.currentSpell   = spell;
        ctx.args           = args;
        ctx.auraStackCount = auraStackCount;
        if (!it->second(ctx, out))
            return false;

        if (normalizedMacro == "schooldamage" && afterName < textLen &&
            std::isspace(static_cast<unsigned char>(text[afterName])))
        {
            static constexpr const char* kColorEnd = "|r";
            static constexpr size_t kColorEndLen   = 2;
            if (out.size() >= kColorEndLen && out.compare(out.size() - kColorEndLen, kColorEndLen, kColorEnd) == 0)
            {
                out.push_back(text[afterName++]);
            }
        }

        pos = afterName;
        return true;
    }

    static bool TryFindOwnedTooltipMathBlockEnd(const char* text, size_t textLen, size_t start, bool doubleBrace,
                                                size_t& innerStart, size_t& innerLen, size_t& endPos)
    {
        if (doubleBrace)
        {
            innerStart   = start + 2;
            size_t depth = 1;
            for (size_t pos = innerStart; pos + 1 < textLen; ++pos)
            {
                if (text[pos] == '{' && text[pos + 1] == '{')
                {
                    ++depth;
                    ++pos;
                    continue;
                }

                if (text[pos] == '}' && text[pos + 1] == '}')
                {
                    if (--depth == 0)
                    {
                        innerLen = pos - innerStart;
                        endPos   = pos + 2;
                        return true;
                    }
                    ++pos;
                }
            }
            return false;
        }

        innerStart   = start + 2;
        size_t depth = 1;
        for (size_t pos = innerStart; pos < textLen; ++pos)
        {
            if (text[pos] == '{')
                ++depth;
            else if (text[pos] == '}')
            {
                if (--depth == 0)
                {
                    innerLen = pos - innerStart;
                    endPos   = pos + 1;
                    return true;
                }
            }
        }

        return false;
    }

    struct OwnedTooltipExpressionParser
    {
        const char* text = nullptr;
        size_t textLen   = 0;
        size_t pos       = 0;

        void SkipSpaces()
        {
            while (pos < textLen && std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
        }

        bool ParseNumber(double& out)
        {
            SkipSpaces();
            if (pos >= textLen)
                return false;

            char* end = nullptr;
            out       = std::strtod(text + pos, &end);
            if (!end || end == text + pos)
                return false;

            pos = static_cast<size_t>(end - text);
            return pos <= textLen;
        }

        bool ParseFactor(double& out)
        {
            SkipSpaces();
            if (pos >= textLen)
                return false;

            if (text[pos] == '+')
            {
                ++pos;
                return ParseFactor(out);
            }

            if (text[pos] == '-')
            {
                ++pos;
                if (!ParseFactor(out))
                    return false;
                out = -out;
                return true;
            }

            if (text[pos] == '(')
            {
                ++pos;
                if (!ParseExpression(out))
                    return false;
                SkipSpaces();
                if (pos >= textLen || text[pos] != ')')
                    return false;
                ++pos;
                return true;
            }

            return ParseNumber(out);
        }

        bool ParseTerm(double& out)
        {
            if (!ParseFactor(out))
                return false;

            while (true)
            {
                SkipSpaces();
                if (pos >= textLen || (text[pos] != '*' && text[pos] != '/'))
                    return true;

                char const op = text[pos++];
                double rhs    = 0.0;
                if (!ParseFactor(rhs))
                    return false;

                if (op == '*')
                    out *= rhs;
                else
                {
                    if (rhs == 0.0)
                        return false;
                    out /= rhs;
                }
            }
        }

        bool ParseExpression(double& out)
        {
            if (!ParseTerm(out))
                return false;

            while (true)
            {
                SkipSpaces();
                if (pos >= textLen || (text[pos] != '+' && text[pos] != '-'))
                    return true;

                char const op = text[pos++];
                double rhs    = 0.0;
                if (!ParseTerm(rhs))
                    return false;

                if (op == '+')
                    out += rhs;
                else
                    out -= rhs;
            }
        }
    };

    static bool TryEvaluateOwnedTooltipExpression(const std::string& expression, double& value)
    {
        OwnedTooltipExpressionParser parser;
        parser.text    = expression.c_str();
        parser.textLen = expression.size();
        if (!parser.ParseExpression(value))
            return false;

        parser.SkipSpaces();
        return parser.pos == parser.textLen;
    }

    static bool TryAppendOwnedTooltipMathBlock(const SpellRec* spell, const char* text, size_t textLen, size_t& pos,
                                               uint32_t auraStackCount, std::string& out)
    {
        bool const atBrace     = pos + 1 < textLen && text[pos] == '@' && text[pos + 1] == '{';
        bool const doubleBrace = pos + 1 < textLen && text[pos] == '{' && text[pos + 1] == '{';
        if (!atBrace && !doubleBrace)
            return false;

        size_t innerStart = 0;
        size_t innerLen   = 0;
        size_t endPos     = pos;
        if (!TryFindOwnedTooltipMathBlockEnd(text, textLen, pos, doubleBrace, innerStart, innerLen, endPos))
        {
            AppendOwnedTooltipError(out);
            pos = textLen;
            return true;
        }

        std::string expanded;
        OwnedTooltipExpressionColorState const previousColorState = s_ownedTooltipExpressionColor;
        s_ownedTooltipExpressionColor                             = {};
        ++s_ownedTooltipExpressionEvaluationDepth;
        bool const expandedOk =
            ParseOwnedTooltipTextToString(spell, text + innerStart, innerLen, auraStackCount, expanded);
        --s_ownedTooltipExpressionEvaluationDepth;
        OwnedTooltipExpressionColorState const mathColorState = s_ownedTooltipExpressionColor;
        s_ownedTooltipExpressionColor                         = previousColorState;
        if (!expandedOk)
        {
            AppendOwnedTooltipError(out);
            pos = endPos;
            return true;
        }

        double value = 0.0;
        if (!TryEvaluateOwnedTooltipExpression(expanded, value))
            AppendOwnedTooltipError(out);
        else
        {
            const char* color = (mathColorState.hasDamage && !mathColorState.conflict) ?
                                    OwnedTooltipDamageTypeColor(mathColorState.schoolMask) :
                                    nullptr;
            if (color)
                out.append(color);
            AppendOwnedTooltipExpressionNumber(out, value);
            if (color)
                out.append("|r");
        }

        pos = endPos;
        return true;
    }

    static bool TryAppendOwnedTooltipPluralToken(const char* text, size_t textLen, size_t& pos, std::string& out)
    {
        if (pos + 1 >= textLen || text[pos] != '$' || text[pos + 1] != 'l')
            return false;

        size_t singularStart = pos + 2;
        size_t colon         = singularStart;
        while (colon < textLen && text[colon] != ':' && text[colon] != ';')
            ++colon;

        if (colon >= textLen || text[colon] != ':')
            return false;

        size_t pluralStart = colon + 1;
        size_t end         = pluralStart;
        while (end < textLen && text[end] != ';')
            ++end;

        if (end >= textLen)
            return false;

        bool const singular =
            s_ownedTooltipHasLastPluralValue && std::fabs(std::fabs(s_ownedTooltipLastPluralValue) - 1.0f) < 0.0001f;
        if (singular)
            out.append(text + singularStart, colon - singularStart);
        else
            out.append(text + pluralStart, end - pluralStart);

        pos = end + 1;
        return true;
    }

    static bool ParseOwnedTooltipTextToString(const SpellRec* spell, const char* source, size_t textLen, uint32_t auraStackCount,
                                              std::string& out)
    {
        out.clear();
        if (!source)
            return false;

        bool const rootParse = s_ownedTooltipTextParseDepth == 0;
        if (rootParse)
        {
            s_ownedTooltipHasLastPluralValue = false;
            s_ownedTooltipLastPluralValue    = 0.0f;
        }

        ++s_ownedTooltipTextParseDepth;
        out.reserve(textLen);
        for (size_t pos = 0; pos < textLen;)
        {
            if (TryAppendOwnedTooltipMathBlock(spell, source, textLen, pos, auraStackCount, out))
                continue;

            if (TryAppendOwnedTooltipMacro(spell, source, textLen, pos, auraStackCount, out))
                continue;

            if (TryAppendOwnedTooltipPluralToken(source, textLen, pos, out))
                continue;

            out.push_back(source[pos++]);
        }

        --s_ownedTooltipTextParseDepth;
        return true;
    }

    bool TooltipEvaluator::HasMacro(const char* text)
    {
        if (!text)
            return false;

        for (const char* pos = text; *pos; ++pos)
        {
            if (pos[0] == '@' && (pos[1] == '{' || std::isalpha(static_cast<unsigned char>(pos[1]))))
                return true;
            if (pos[0] == '{' && pos[1] == '{')
                return true;
        }

        return false;
    }

    bool TooltipEvaluator::EvaluateText(const SpellRec* spell, const char* source, char* dest, size_t destSize, uint32_t auraStackCount)
    {
        if (!source || !dest || destSize == 0)
            return false;

        // 1. Check ModCore Lua OnTooltipEvaluate hook
        std::string textToParse = source;
        std::string luaText;
        bool luaOverridden = false;
        if (spell)
        {
            luaOverridden = ModCore::Manager().OnTooltipEvaluate(spell->m_id, ClntObjMgrGetActivePlayerObj(), source, luaText);
            if (luaOverridden && !luaText.empty())
            {
                textToParse = luaText;
            }
        }

        // 2. Check if macro tokens are present
        if (!HasMacro(textToParse.c_str()))
        {
            if (luaOverridden)
            {
                if (textToParse.size() >= destSize)
                    textToParse.resize(destSize - 1);
                std::memcpy(dest, textToParse.data(), textToParse.size());
                dest[textToParse.size()] = '\0';
                return true;
            }
            return false;
        }

        // 3. Parse macro tags
        std::string out;
        if (!ParseOwnedTooltipTextToString(spell, textToParse.c_str(), textToParse.size(), auraStackCount, out))
        {
            if (luaOverridden)
            {
                if (textToParse.size() >= destSize)
                    textToParse.resize(destSize - 1);
                std::memcpy(dest, textToParse.data(), textToParse.size());
                dest[textToParse.size()] = '\0';
                return true;
            }
            return false;
        }

        if (out.size() >= destSize)
            out.resize(destSize - 1);

        std::memcpy(dest, out.data(), out.size());
        dest[out.size()] = '\0';
        return dest[0] != '\0';
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

        if (ordinal < 1 || ordinal > SPELLVARIABLE_EFFECT_SLOT_COUNT)
        {
            expanded.push_back('0');
            i = static_cast<size_t>(indexEnd - text);
            return true;
        }

        uint32_t effectIndex = static_cast<uint32_t>(ordinal - 1);
        float val = 0.0f;
        bool isInt = true;

        switch (token)
        {
            case 'd': case 'D':
                val = effectIndex < 3 ? spell->m_effectPointsPerCombo[effectIndex] : 0.0f;
                isInt = false;
                break;
            case 'g': case 'G':
                val = effectIndex < 3 ? spell->m_effectAmplitude[effectIndex] : 0.0f;
                isInt = false;
                break;
            case 'h': case 'H':
                val = effectIndex < 3 ? spell->m_effectChainAmplitude[effectIndex] : 0.0f;
                isInt = (token == 'H');
                break;
            case 's': case 'S':
                val = static_cast<float>(effectIndex < 3 ? spell->m_effectMiscValue[effectIndex] : 0);
                break;
            case 'v': case 'V':
                val = static_cast<float>((spell->m_procTypeMask & 1) ? 5000 : (effectIndex < 3 ? spell->m_effectAuraPeriod[effectIndex] : 0));
                break;
            case 'z': case 'Z':
                val = static_cast<float>(effectIndex < 3 ? spell->m_effectChainTargets[effectIndex] : 0);
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

    std::string TooltipEvaluator::ExpandTooltipText(const SpellRec* spell, const char* text)
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
}
