#include "WoW/Source/Duskhaven/Spells/LocalSpellData.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/DB/Rec/SummonPropertiesRec.h"
#include "WoW/Source/DB/Rec/SpellShapeshiftFormRec.h"
#include "WoW/Source/DB/Rec/SpellMissileRec.h"
#include "WoW/Source/DB/Rec/LockRec.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/PetInfo.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/Net/Types.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/CDataStore.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/Object/ItemStats.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint32_t WAITING_TO_RESURRECT_SPELL_ID = 2584;
    constexpr uint32_t BLOOD_PRESENCE_SPELL_ID = 48266;
    constexpr uint32_t FROST_PRESENCE_SPELL_ID = 48263;
    constexpr uint32_t UNHOLY_PRESENCE_SPELL_ID = 48265;

    constexpr uint32_t LOCAL_SPELL_ATTR0_PASSIVE = 0x40;
    constexpr uint32_t LOCAL_SPELL_ATTR0_HIDDEN_CLIENTSIDE = 0x80;
    constexpr uint32_t SPELL_EFFECT_SUMMON = 28;
    constexpr uint32_t SPELL_EFFECT_APPLY_AURA = 6;
    constexpr uint32_t SPELL_EFFECT_POWER_COST_FROM_ACTIONBAR = 97;
    constexpr uint32_t SPELL_EFFECT_APPLY_AREA_AURA_PARTY = 2;
    constexpr uint32_t SPELL_EFFECT_APPLY_AREA_AURA_RAID = 177;
    constexpr uint32_t SPELL_EFFECT_SKINNING = 95;
    constexpr uint32_t SPELL_EFFECT_SKIN_PLAYER_CORPSE = 116;
    constexpr uint32_t SPELL_ATTR_EX_C_DONT_DISPLAY_RANGE = 0x4000;
    constexpr uint32_t SPELL_ATTR_EX_G_INCLUDE_IN_ACTIONBAR_POWER_COST = 0x20;
    constexpr uint32_t SPELL_AURA_MOUNTED = 78;
    constexpr uint32_t SPELL_AURA_MOD_SHAPESHIFT = 36;
    constexpr uint32_t SPELL_AURA_PERIODIC_TRIGGER_SPELL = 48;
    constexpr uint32_t SPELL_AURA_MOD_SPEED_ALWAYS = 291;
    constexpr uint32_t ACTIONBAR_EFFECT97_BASE_SLOT = 132;
    constexpr uint32_t SUMMON_PROPERTIES_SLOT_CRITTER = 5;

    constexpr uint32_t LocalLanguageEffect = 39;
    constexpr uint32_t LocalOpenLockEffect = 33;
    constexpr uint32_t LocalSkillEffect = 118;
    constexpr uint32_t LocalComprehendLanguageAura = 244;
    constexpr uint32_t MaxNativeLanguages = 256;

    std::unordered_map<uint32_t, std::array<SpellEffectCacheRow, 3>> s_legacyEffects;

    SpellEffectCacheRow LegacyEffect(const SpellRec& r, uint32_t i)
    {
        SpellEffectCacheRow e{};
        e.spellID = r.m_id;
        e.effectIndex = i;
        e.effect = r.m_effect[i];
        e.effectDieSides = r.m_effectDieSides[i];
        e.effectRealPointsPerLevel = r.m_effectRealPointsPerLevel[i];
        e.effectBasePoints = r.m_effectBasePoints[i];
        e.effectMechanic = r.m_effectMechanic[i];
        e.effectImplicitTargetA = r.m_implicitTargetA[i];
        e.effectImplicitTargetB = r.m_implicitTargetB[i];
        e.effectRadiusIndex = r.m_effectRadiusIndex[i];
        e.effectApplyAuraName = r.m_effectAura[i];
        e.effectAmplitude = r.m_effectAuraPeriod[i];
        e.effectMultipleValue = r.m_effectAmplitude[i];
        e.effectChainTargets = r.m_effectChainTargets[i];
        e.effectItemType = r.m_effectItemType[i];
        e.effectMiscValue = r.m_effectMiscValue[i];
        e.effectMiscValueB = r.m_effectMiscValueB[i];
        e.effectTriggerSpell = r.m_effectTriggerSpell[i];
        e.effectPointsPerCombo = r.m_effectPointsPerCombo[i];
        e.effectSpellClassMaskA = r.m_effectSpellClassMaskA[i];
        e.effectSpellClassMaskB = r.m_effectSpellClassMaskB[i];
        e.effectSpellClassMaskC = r.m_effectSpellClassMaskC[i];
        e.effectChainAmplitude = r.m_effectChainAmplitude[i];
        e.effectBonusMultiplier = r.m_effectBonusCoefficient[i];
        return e;
    }

    std::unordered_map<uint32_t, std::vector<uint32_t>> s_professionSpells;
    std::unordered_map<uint32_t, std::vector<uint32_t>> s_knownProfessionSpells;
    std::unordered_map<uint32_t, SkillLineAbilityRec> s_professionAbilities;
    bool s_professionIndexLoaded = false;
    WOWGUID s_professionPlayer = WOWGUID();

    struct OwnedKnownEffectSlot
    {
        uint32_t spellId;
        uint32_t previousSpellId;
    };
    std::unordered_map<uint32_t, OwnedKnownEffectSlot> s_localLanguageSlots;
    std::unordered_map<uint32_t, OwnedKnownEffectSlot> s_localSkinningSlots;
    OwnedKnownEffectSlot s_localInsigniaSlot{};
    std::vector<SpellEffectCacheRow> s_localKnownEffectRows;
    bool s_localKnownEffectIndexLoaded = false;
    bool s_localLanguageRefreshPending = false;
    WOWGUID s_localKnownEffectPlayer = WOWGUID();

    void LoadProfessionIndex()
    {
        if (s_professionIndexLoaded || !g_skillLineAbilityDB.m_loaded)
            return;

        s_professionSpells.clear();
        s_professionAbilities.clear();
        for (int32_t i = 0; i < g_skillLineAbilityDB.m_numRecords; ++i)
        {
            const SkillLineAbilityRec& ability = g_skillLineAbilityDB.m_records[i];
            s_professionSpells[ability.m_skillLine].push_back(ability.m_spell);
            s_professionAbilities[ability.m_spell] = ability;
        }
        for (auto& pair : s_professionSpells)
        {
            auto& spells = pair.second;
            std::sort(spells.begin(), spells.end());
            spells.erase(std::unique(spells.begin(), spells.end()), spells.end());
        }
        s_professionIndexLoaded = true;
    }

    uint32_t SpellRankNumber(const SpellRec& spell)
    {
        uint32_t value = 0;
        if (spell.m_nameSubtext)
        {
            for (const char* cursor = spell.m_nameSubtext; *cursor; ++cursor)
            {
                unsigned char const ch = static_cast<unsigned char>(*cursor);
                if (std::isdigit(ch))
                {
                    value = value * 10u + static_cast<uint32_t>(ch - '0');
                    continue;
                }
                if (value)
                    break;
            }
        }
        return value ? value : static_cast<uint32_t>(spell.m_spellLevel);
    }

    const std::vector<uint32_t>& ProfessionRecipes(uint32_t skillLine)
    {
        WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
        if (s_professionPlayer != activePlayer)
            LocalSpellData::InvalidateProfessionRecipes();

        auto found = s_knownProfessionSpells.find(skillLine);
        if (found != s_knownProfessionSpells.end())
            return found->second;

        LoadProfessionIndex();
        auto& recipes = s_knownProfessionSpells[skillLine];
        CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(activePlayer, TYPEMASK_PLAYER, __FILE__, __LINE__));
        if (!player || !skillLine)
            return recipes;

        auto abilities = s_professionSpells.find(skillLine);
        if (abilities == s_professionSpells.end())
            return recipes;

        for (uint32_t id : abilities->second)
        {
            SpellRec row;
            if (!player->IsSpellKnown(id) || !LocalSpellData::TryGetSpellRow(id, row) ||
                !(row.m_attributes & 0x20u) || (row.m_attributes & 0x80u) || (row.m_attributesExD & 0x8000u) ||
                !row.m_name || !*row.m_name)
                continue;
            recipes.push_back(id);
        }

        std::sort(recipes.begin(), recipes.end(), [](uint32_t a, uint32_t b) {
            SpellRec ar{}, br{};
            LocalSpellData::TryGetSpellRow(a, ar);
            LocalSpellData::TryGetSpellRow(b, br);
            int cmp = std::strcmp(ar.m_name ? ar.m_name : "", br.m_name ? br.m_name : "");
            if (cmp)
                return cmp < 0;
            uint32_t const leftRank = SpellRankNumber(ar);
            uint32_t const rightRank = SpellRankNumber(br);
            if (leftRank && rightRank && leftRank != rightRank)
                return leftRank < rightRank;
            cmp = std::strcmp(ar.m_nameSubtext ? ar.m_nameSubtext : "",
                              br.m_nameSubtext ? br.m_nameSubtext : "");
            return cmp ? cmp < 0 : a < b;
        });

        return recipes;
    }

    bool HasMatchingNativeRegistration(const SpellEffectCacheRow& effect)
    {
        if (effect.effect == LocalOpenLockEffect)
            return false;
        SpellRec spell;
        if (!LocalSpellData::TryGetSpellRow(effect.spellID, spell))
            return true;
        return static_cast<uint32_t>(spell.m_effect[0]) == effect.effect &&
               (effect.effect == SPELL_EFFECT_SKIN_PLAYER_CORPSE ||
                spell.m_effectMiscValue[0] == effect.effectMiscValue);
    }

    bool NeedsLocalKnownRegistration(const SpellEffectCacheRow& effect)
    {
        if (effect.effectIndex >= 32)
            return false;
        if (effect.effect == LocalOpenLockEffect)
            return effect.effectIndex >= 3;
        return effect.effectIndex >= 1 &&
               (effect.effect == LocalLanguageEffect || effect.effect == SPELL_EFFECT_SKINNING ||
                effect.effect == SPELL_EFFECT_SKIN_PLAYER_CORPSE) && !HasMatchingNativeRegistration(effect);
    }

    void LoadLocalKnownEffectIndex()
    {
        if (s_localKnownEffectIndexLoaded || !CDBCRegistry::IsLoaded("SpellEffect"))
            return;

        LocalSpellData::ForEachSpell([](const SpellRec& spell) {
            for (uint32_t i = 1; i < 3; ++i)
            {
                if (spell.m_effect[i] != LocalLanguageEffect && spell.m_effect[i] != SPELL_EFFECT_SKINNING &&
                    spell.m_effect[i] != SPELL_EFFECT_SKIN_PLAYER_CORPSE)
                    continue;
                const SpellEffectCacheRow* effect = nullptr;
                if (LocalSpellData::TryGetSpellEffect(spell.m_id, i, effect) && NeedsLocalKnownRegistration(*effect))
                    s_localKnownEffectRows.push_back(*effect);
            }
        });

        SpellEffectCacheDB::ForEachRow([](const SpellEffectCacheRow* row) {
            const SpellEffectCacheRow* effect = nullptr;
            if (row && row->effectIndex >= 3 && row->effectIndex < 32 &&
                (row->effect == LocalLanguageEffect || row->effect == SPELL_EFFECT_SKINNING ||
                 row->effect == SPELL_EFFECT_SKIN_PLAYER_CORPSE || row->effect == LocalOpenLockEffect) &&
                LocalSpellData::TryGetSpellEffect(row->spellID, row->effectIndex, effect) &&
                NeedsLocalKnownRegistration(*effect))
            {
                s_localKnownEffectRows.push_back(*effect);
            }
        });

        s_localKnownEffectIndexLoaded = true;
    }

    bool RestoreOwnedSlot(int32_t& slot, const OwnedKnownEffectSlot& owned)
    {
        if (!owned.spellId || static_cast<uint32_t>(slot) != owned.spellId)
            return false;
        uint32_t const previous = HasSpell(owned.previousSpellId) ? owned.previousSpellId : 0;
        bool const changed = static_cast<uint32_t>(slot) != previous;
        slot = static_cast<int32_t>(previous);
        return changed;
    }

    void AssignOwnedSlot(int32_t& slot, uint32_t spellId, OwnedKnownEffectSlot& owned)
    {
        if (static_cast<uint32_t>(slot) == spellId)
            return;
        if (!owned.spellId)
            owned.previousSpellId = static_cast<uint32_t>(slot);
        owned.spellId = spellId;
        slot = static_cast<int32_t>(spellId);
    }

    int32_t LocalSkillValueForAbility(uint32_t spellId)
    {
        int32_t value = 0;
        if (auto* ability = LocalSpellData::GetSkillLineAbility(spellId))
            value = LocalSpellData::GetPlayerSkillLineValueWithProfessionAliases(ability->m_skillLine);

        LocalSpellData::ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& effect) {
            if (effect.effect == LocalSkillEffect && effect.effectMiscValue > 0)
                value = (std::max)(value, LocalSpellData::GetPlayerSkillLineValueWithProfessionAliases(effect.effectMiscValue));
            return true;
        });
        return value;
    }

    bool LocalOpenLockCap(const SpellEffectCacheRow& effect, int32_t& cap)
    {
        int32_t const skillValue = LocalSkillValueForAbility(effect.spellID);
        if (!HasSpell(effect.spellID) && skillValue <= 0)
            return false;

        cap = effect.effectBasePoints + 1;
        if (cap <= 0)
            cap = skillValue;
        return cap > 0;
    }

    bool DefaultLockRequirement(const CGGameObject_C* gob, int32_t& required)
    {
        if (!gob || !gob->m_gameObject)
            return false;
        required = static_cast<int32_t>(5u * gob->m_gameObject->m_level);
        return required >= 0;
    }

    bool ShapeshiftFormHasFlag(int32_t formId, uint32_t flag)
    {
        if (formId <= 0)
            return false;
        SpellShapeshiftFormRec* form = g_spellShapeshiftFormDB.GetRecord(formId);
        return form && (static_cast<uint32_t>(form->m_flags) & flag) != 0;
    }

    bool SummonPropertySlotIsCritter(int32_t summonPropertyId)
    {
        if (summonPropertyId <= 0)
            return false;
        SummonPropertiesRec* rec = g_summonPropertiesDB.GetRecord(summonPropertyId);
        return rec && rec->m_slot == SUMMON_PROPERTIES_SLOT_CRITTER;
    }

    bool ExtendedImplicitTargetLooksHelpful(uint32_t implicitTarget, uint32_t auraId)
    {
        if (implicitTarget == 1)
            return auraId != 4;
        return implicitTarget == 2;
    }

    bool IsNativeTargetMaskEffect(uint32_t implicitTarget)
    {
        switch (implicitTarget)
        {
        case 1: case 5: case 6: case 21: case 23: case 25: case 26: case 35:
        case 45: case 53: case 57: case 61: case 63: case 64: case 65: case 66:
        case 67: case 68: case 69: case 70: case 71: case 74: case 75:
            return true;
        default:
            return false;
        }
    }

    bool IsNativeTargetMaskEffect(const SpellEffectCacheRow& effect)
    {
        return IsNativeTargetMaskEffect(effect.effectImplicitTargetA) ||
               IsNativeTargetMaskEffect(effect.effectImplicitTargetB);
    }

    bool IsNativeCastTargetPredicateEffect(const SpellEffectCacheRow& effect)
    {
        switch (effect.effectImplicitTargetA)
        {
        case 6: case 21: case 35: case 57: case 61:
            return true;
        default:
            break;
        }
        return effect.effect == 78 /* SPELL_EFFECT_TRADE_SKILL */ || effect.effect == 2 /* SPELL_EFFECT_ATTACK */;
    }

    bool ExtendedEffectShouldDriveCastTarget(const SpellEffectCacheRow& effect)
    {
        return IsNativeTargetMaskEffect(effect) || IsNativeCastTargetPredicateEffect(effect);
    }

    bool IsCurrentAuraGateEffect(uint32_t effectId)
    {
        switch (effectId)
        {
        case 6: case 35: case 65: case 119: case 128: case 129: case 143:
            return true;
        default:
            return false;
        }
    }

    bool IsNativeNonQueryableAuraEffect(uint32_t effectId)
    {
        switch (effectId)
        {
        case 27: case 35: case 65: case 119: case 128: case 129: case 143:
            return true;
        default:
            return false;
        }
    }

    bool IsAreaOrApplyAuraEffect(uint32_t effectId)
    {
        return effectId == SPELL_EFFECT_APPLY_AURA || effectId == SPELL_EFFECT_APPLY_AREA_AURA_PARTY ||
               effectId == SPELL_EFFECT_APPLY_AREA_AURA_RAID;
    }

    bool IsNativeAuraTargetValidationEffect(const SpellEffectCacheRow& effect)
    {
        return effect.effect == 38 || effect.effect == 108 || effect.effect == 126;
    }

    void ApplyImplicitTargetAToNativeTargetMask(uint32_t implicitTarget, uint32_t& targetMask, bool& usesPetTarget)
    {
        switch (implicitTarget)
        {
        case 1:  targetMask &= ~0x400u; break;
        case 2:  usesPetTarget = true; targetMask &= ~0x8000u; break;
        case 3:  targetMask |= 0x80u; break;
        case 4:  targetMask |= 0x100u; break;
        case 5:  targetMask |= 0x800u; break;
        case 6:  targetMask |= 0x2u; break;
        case 7:  targetMask |= 0x4000u; break;
        case 8:  targetMask |= 0x8u; break;
        case 9:  targetMask |= 0x4u; break;
        default: break;
        }
        targetMask &= 0xFFFFu;
    }

    void ApplyImplicitTargetAToPrepareCastFlags(const SpellRec& spell, uint32_t implicitTarget,
                                               uint32_t& targetFlags, bool& blockedTargetMode)
    {
        switch (implicitTarget)
        {
        case 1:  targetFlags &= ~0x400u; break;
        case 2:  targetFlags &= ~0x8000u; break;
        case 3:  targetFlags |= 0x80u; break;
        case 4:  blockedTargetMode = true; break;
        case 5:  targetFlags |= 0x100u; break;
        case 6:  targetFlags |= 0x800u; break;
        case 7:  targetFlags |= 0x2u; break;
        case 8:  targetFlags |= 0x4000u; break;
        case 9:  targetFlags |= 0x8u; break;
        case 10: targetFlags |= 0x4u; break;
        case 11:
            if (spell.m_spellMissileID)
            {
                SpellMissileRec* missile = g_spellMissileDB.GetRecord(spell.m_spellMissileID);
                if (missile && (missile->m_flags & 1))
                    targetFlags &= ~0x40u;
            }
            break;
        case 12: targetFlags |= 0x10000u; break;
        default: break;
        }
    }
} // namespace

namespace LocalSpellData
{
    bool HasSpell(uint32_t spellId)
    {
        SpellRec row;
        return TryGetSpellRow(spellId, row);
    }

    bool TryGetSpellRow(uint32_t spellId, SpellRec& out)
    {
        if (!spellId)
            return false;
        return g_spellDB.GetRecord(spellId, &out) != 0;
    }

    void ForEachSpell(const std::function<void(const SpellRec&)>& visitor)
    {
        if (!g_spellDB.m_loaded || g_spellDB.m_numRecords <= 0)
            return;

        for (int32_t i = 0; i < g_spellDB.m_numRecords; ++i)
        {
            SpellRec row;
            if (g_spellDB.GetRecordByIndex(i, &row))
                visitor(row);
        }
    }

    bool TryGetSpellEffect(uint32_t spellId, uint32_t index, const SpellEffectCacheRow*& out)
    {
        out = nullptr;
        if (!spellId || index >= 32)
            return false;

        const SpellEffectCacheRow* supplemental = nullptr;
        if (spellId <= 0x07ffffffu)
            supplemental = SpellEffectCacheDB::Get(static_cast<uint32_t>(SpellEffectCacheKey(spellId, index)));

        if (supplemental && (supplemental->spellID != spellId || supplemental->effectIndex != index))
            supplemental = nullptr;

        if (index >= 3)
        {
            out = supplemental;
            return out != nullptr;
        }

        SpellRec row;
        if (!g_spellDB.GetRecord(spellId, &row) || !row.m_effect[index])
            return false;

        auto& effect = s_legacyEffects[spellId][index];
        effect = LegacyEffect(row, index);

        if (supplemental)
        {
            effect.effectSpellPowerBonus = supplemental->effectSpellPowerBonus;
            effect.effectAttackPowerBonus = supplemental->effectAttackPowerBonus;
            effect.effectBlockValueBonus = supplemental->effectBlockValueBonus;
            effect.effectScalingMode = supplemental->effectScalingMode;
        }
        out = &effect;
        return true;
    }

    uint32_t ForEachSpellEffect(uint32_t spellId, const std::function<bool(const SpellEffectCacheRow&)>& visitor)
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < 32; ++i)
        {
            const SpellEffectCacheRow* effect = nullptr;
            if (!TryGetSpellEffect(spellId, i, effect) || !effect->effect)
                continue;
            ++count;
            if (visitor && !visitor(*effect))
                break;
        }
        return count;
    }

    uint32_t GetSpellEffectCount(uint32_t spellId)
    {
        return ForEachSpellEffect(spellId, {});
    }

    bool HasExtendedEffects(uint32_t spellId)
    {
        for (uint32_t i = 3; i < 32; ++i)
        {
            const SpellEffectCacheRow* e = nullptr;
            if (TryGetSpellEffect(spellId, i, e) && e->effect)
                return true;
        }
        return false;
    }

    bool TryFindSpellEffectByEffect(uint32_t spellId, uint32_t effectId, const SpellEffectCacheRow*& out)
    {
        out = nullptr;
        ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& e) {
            if (e.effect == effectId)
            {
                out = &e;
                return false;
            }
            return true;
        });
        return out != nullptr;
    }

    bool TryFindSpellEffectByAura(uint32_t spellId, uint32_t auraId, const SpellEffectCacheRow*& out)
    {
        out = nullptr;
        ForEachSpellEffect(spellId, [&](const SpellEffectCacheRow& e) {
            if (e.effectApplyAuraName == auraId)
            {
                out = &e;
                return false;
            }
            return true;
        });
        return out != nullptr;
    }

    bool HasSpellEffect(uint32_t spellId, uint32_t effectId)
    {
        const SpellEffectCacheRow* e = nullptr;
        return TryFindSpellEffectByEffect(spellId, effectId, e);
    }

    bool HasSpellAura(uint32_t spellId, uint32_t auraId)
    {
        const SpellEffectCacheRow* e = nullptr;
        return TryFindSpellEffectByAura(spellId, auraId, e);
    }

    bool HasAnySpellAura(uint32_t spellId, const uint32_t* auraIds, uint32_t count)
    {
        if (!auraIds)
            return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (HasSpellAura(spellId, auraIds[i]))
                return true;
        }
        return false;
    }

    SkillLineAbilityRec* GetSkillLineAbility(uint32_t spellId)
    {
        LoadProfessionIndex();
        auto found = s_professionAbilities.find(spellId);
        return found == s_professionAbilities.end() ? nullptr : &found->second;
    }

    uint32_t GetKnownProfessionRecipeCount(uint32_t skillLineId)
    {
        return static_cast<uint32_t>(ProfessionRecipes(skillLineId).size());
    }

    bool TryGetKnownProfessionRecipeSpellId(uint32_t skillLineId, uint32_t oneBasedIndex, uint32_t& spellId)
    {
        const auto& recipes = ProfessionRecipes(skillLineId);
        spellId = (oneBasedIndex && oneBasedIndex <= recipes.size()) ? recipes[oneBasedIndex - 1] : 0;
        return spellId != 0;
    }

    void InvalidateProfessionRecipes()
    {
        s_knownProfessionSpells.clear();
        s_professionPlayer = ClntObjMgrGetActivePlayer();
    }

    void ResetLocalKnownEffects()
    {
        s_localLanguageSlots.clear();
        s_localSkinningSlots.clear();
        s_localInsigniaSlot = {};
        s_localLanguageRefreshPending = false;
        s_localKnownEffectPlayer = ClntObjMgrGetActivePlayer();
    }

    void RefreshLocalKnownEffects()
    {
        WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
        if (s_localKnownEffectPlayer != activePlayer)
            ResetLocalKnownEffects();

        LoadLocalKnownEffectIndex();

        for (auto const& slot : s_localLanguageSlots)
        {
            if (slot.first < CGSpellBook::m_languageSpells.Count())
                s_localLanguageRefreshPending |= RestoreOwnedSlot(CGSpellBook::m_languageSpells.m_data[slot.first], slot.second);
        }

        s_localLanguageSlots.clear();
        s_localSkinningSlots.clear();
        s_localInsigniaSlot = {};

        for (auto const& effect : s_localKnownEffectRows)
        {
            if (!::HasSpell(effect.spellID))
                continue;

            uint32_t const slot = static_cast<uint32_t>(effect.effectMiscValue);
            if (effect.effect == LocalLanguageEffect && slot < MaxNativeLanguages)
            {
                if (slot < CGSpellBook::m_languageSpells.Count())
                {
                    bool const changed = static_cast<uint32_t>(CGSpellBook::m_languageSpells.m_data[slot]) != effect.spellID;
                    AssignOwnedSlot(CGSpellBook::m_languageSpells.m_data[slot], effect.spellID, s_localLanguageSlots[slot]);
                    s_localLanguageRefreshPending |= changed;
                }
                else
                {
                    s_localLanguageRefreshPending = true;
                }
            }
        }
    }

    void PumpExtendedLanguageRefresh()
    {
        if (!s_localLanguageRefreshPending || !ClntObjMgrGetActivePlayer())
            return;

        RefreshLocalKnownEffects();
        s_localLanguageRefreshPending = false;
        FrameScript_SignalEvent(SCRIPT_EVENT_LANGUAGE_LIST_CHANGED, nullptr);
    }

    int32_t GetPlayerSkillLineValueWithProfessionAliases(uint32_t skillLineId)
    {
        CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));
        if (!player)
            return 0;

        int32_t value = player->GetSkillRank(static_cast<int32_t>(skillLineId));
        auto includeAlias = [&](uint32_t alias) {
            if (alias && alias != skillLineId) {
                int32_t aliasVal = player->GetSkillRank(static_cast<int32_t>(alias));
                if (aliasVal > value)
                    value = aliasVal;
            }
        };

        switch (skillLineId)
        {
        case 182: case 823: includeAlias(182); includeAlias(823); break;
        case 186: case 824: includeAlias(186); includeAlias(824); break;
        case 202: case 820: includeAlias(202); includeAlias(820); break;
        default: break;
        }

        return value;
    }

    int32_t GetExtendedSkinningCapability(uint32_t spellId, CGPlayer_C* player, int32_t skinnableType)
    {
        int32_t effectCap = 0;
        const SpellEffectCacheRow* effect = nullptr;
        if (TryFindSpellEffectByEffect(spellId, SPELL_EFFECT_SKINNING, effect) ||
            TryFindSpellEffectByEffect(spellId, SPELL_EFFECT_SKIN_PLAYER_CORPSE, effect))
        {
            if (effect)
                effectCap = (std::max)(0, effect->effectBasePoints + 1);
        }

        int32_t playerSkill = 0;
        uint32_t skillLine = 0;
        switch (skinnableType)
        {
        case 0: skillLine = 393; break;
        case 1: skillLine = 182; break;
        case 2: skillLine = 186; break;
        case 3: skillLine = 202; break;
        default: break;
        }

        if (skillLine)
            playerSkill = GetPlayerSkillLineValueWithProfessionAliases(skillLine);

        return (std::max)(effectCap, playerSkill);
    }

    bool ResolveExtraOpenLock(const CGGameObject_C* gob, uint32_t requestedSpell, uint32_t& spellId, int32_t& cap, int32_t& required, uint32_t& lockIndex)
    {
        if (!gob)
            return false;

        LoadLocalKnownEffectIndex();
        LockRec* lock = gob->GetLockRec();
        if (!lock)
            return false;

        for (uint32_t i = 0; i < 8; ++i)
        {
            if (lock->m_type[i] != 2 || lock->m_index[i] <= 0 || !gob->IsValidOpenAction(lock->m_action[i]))
                continue;

            uint32_t candidateId = 0;
            int32_t candidateCap = 0;
            for (auto const& effect : s_localKnownEffectRows)
            {
                if (effect.effect != LocalOpenLockEffect || effect.effectMiscValue != lock->m_index[i] ||
                    (requestedSpell && effect.spellID != requestedSpell))
                    continue;

                int32_t value = 0;
                if (LocalOpenLockCap(effect, value) && (!candidateId || value > candidateCap))
                {
                    candidateId = effect.spellID;
                    candidateCap = value;
                }
            }

            if (!candidateId)
                continue;

            required = lock->m_skill[i];
            if (required <= 0 && !DefaultLockRequirement(gob, required))
                continue;

            spellId = candidateId;
            cap = candidateCap;
            lockIndex = i;
            return true;
        }

        return false;
    }

    uint32_t GetExtendedSpellHelpfulHarmful(const SpellRec* spell)
    {
        if (!spell || !HasExtendedEffects(spell->m_id))
            return 0;

        if (spell->m_targets & 0x100u)
            return 1;
        if (spell->m_targets & 0x80u)
            return 2;

        uint32_t result = 0;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (effect.effectImplicitTargetA == 2 ||
                (effect.effectImplicitTargetA == 3 && effect.effectImplicitTargetB == 2))
            {
                result = 2;
                return false;
            }
            return true;
        });

        if (result)
            return result;

        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (ExtendedImplicitTargetLooksHelpful(effect.effectImplicitTargetA, effect.effectApplyAuraName) ||
                ExtendedImplicitTargetLooksHelpful(effect.effectImplicitTargetB, effect.effectApplyAuraName))
            {
                result = 1;
                return false;
            }
            return true;
        });

        return result;
    }

    bool ExtendedAuraSideListAcceptsSpell(const SpellRec& row, const AuraData* aura)
    {
        if ((row.m_attributes & LOCAL_SPELL_ATTR0_HIDDEN_CLIENTSIDE) || (row.m_attributesEx & 0x10000000u))
            return false;

        if (row.m_attributes & LOCAL_SPELL_ATTR0_PASSIVE)
        {
            bool hasAcceptedPassiveEffect = false;
            ForEachSpellEffect(row.m_id, [&](const SpellEffectCacheRow& effect) {
                if (!IsNativeNonQueryableAuraEffect(effect.effect))
                {
                    hasAcceptedPassiveEffect = true;
                    return false;
                }
                return true;
            });
            if (!hasAcceptedPassiveEffect)
                return false;
        }

        if (aura)
        {
            if (aura->m_flags & 0x8u)
            {
                if (row.m_attributesExE & 0x08000000u)
                    return false;
            }
            else if (row.m_attributesExE & 0x10000000u)
            {
                return false;
            }

            if (row.m_attributesExF & 0x00100000u)
            {
                WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
                if (!aura->m_creator || (aura->m_creator != activePlayer && aura->m_creator != CGPetInfo::GetPet(0)))
                    return false;
            }
        }

        return true;
    }

    bool ExtendedKnownAuraShouldEnterSideList(const SpellRec& row)
    {
        if (!::HasSpell(row.m_id) || (row.m_attributesExC & SPELL_ATTR_EX_C_DONT_DISPLAY_RANGE))
            return false;

        bool eligible = true;
        ForEachSpellEffect(row.m_id, [&](const SpellEffectCacheRow& e) {
            if (e.effectMechanic == 1 || IsNativeNonQueryableAuraEffect(e.effect))
            {
                eligible = false;
                return false;
            }
            return true;
        });
        return eligible;
    }

    bool SpellClassMaskMatchesEffect(const SpellRec& source, const SpellRec& aura, const SpellEffectCacheRow& effect)
    {
        if (source.m_spellClassSet != aura.m_spellClassSet)
            return false;

        return (effect.effectSpellClassMaskA & source.m_spellClassMask[0]) != 0 ||
               (effect.effectSpellClassMaskB & source.m_spellClassMask[1]) != 0 ||
               (effect.effectSpellClassMaskC & source.m_spellClassMask[2]) != 0;
    }

    uint32_t GetExtendedEffect97ActionBarPowerCost(const SpellEffectCacheRow& effect, const SpellRec& source, const CGUnit_C* unit)
    {
        if (effect.effect != SPELL_EFFECT_POWER_COST_FROM_ACTIONBAR || effect.effectMiscValue < 0 || effect.effectMiscValueB <= 0)
            return 0;

        uint32_t const startOffset = static_cast<uint32_t>(effect.effectMiscValue);
        uint32_t const firstSlot = ACTIONBAR_EFFECT97_BASE_SLOT + startOffset;
        if (firstSlot >= 144)
            return 0;

        uint32_t const availableSlots = 144u - firstSlot;
        uint32_t const requestedSlots = static_cast<uint32_t>(effect.effectMiscValueB);
        uint32_t const slotCount = (std::min)(requestedSlots, availableSlots);
        uint32_t total = 0;

        for (uint32_t i = 0; i < slotCount; ++i)
        {
            int32_t petSpell = 0;
            int32_t actionSpellId = CGActionBar::GetSpell(firstSlot + i, petSpell);
            if (!actionSpellId || static_cast<uint32_t>(actionSpellId) == static_cast<uint32_t>(source.m_id))
                continue;

            SpellRec actionRow;
            if (!g_spellDB.GetRecord(actionSpellId, &actionRow) || !(actionRow.m_attributesExG & SPELL_ATTR_EX_G_INCLUDE_IN_ACTIONBAR_POWER_COST))
                continue;

            int64_t cost = actionRow.m_manaCost;
            if (actionRow.m_manaCostPct && unit)
            {
                int32_t powerType = actionRow.m_powerType;
                uint32_t maxPower = 0;
                if (powerType == -2)
                    maxPower = unit->m_unit ? unit->m_unit->m_unitFieldMaxhealth : 0;
                else if (powerType >= 0 && powerType <= 6)
                    maxPower = unit->GetBasePowerForType(static_cast<POWER_TYPE>(powerType));

                cost += (maxPower * actionRow.m_manaCostPct) / 100u;
            }

            if (cost <= 0)
                continue;

            uint32_t actionCost = static_cast<uint32_t>(cost);
            if (actionCost > (std::numeric_limits<uint32_t>::max)() - total)
                return (std::numeric_limits<uint32_t>::max)();
            total += actionCost;
        }

        return total;
    }

    bool ExtraEffectsNeedAuraState(const SpellRec* spell)
    {
        if (!spell)
            return false;
        for (uint32_t i = 3; i < 32; ++i)
        {
            const SpellEffectCacheRow* effect = nullptr;
            if (TryGetSpellEffect(spell->m_id, i, effect) && effect &&
                (IsCurrentAuraGateEffect(effect->effect) || effect->effect == SPELL_EFFECT_SUMMON))
                return true;
        }
        return false;
    }

    bool ExtraEffectsNeedCastTarget(const SpellRec* spell)
    {
        if (!spell)
            return false;
        for (uint32_t i = 3; i < 32; ++i)
        {
            const SpellEffectCacheRow* effect = nullptr;
            if (TryGetSpellEffect(spell->m_id, i, effect) && effect && ExtendedEffectShouldDriveCastTarget(*effect))
                return true;
        }
        return false;
    }

    void HandleExtendedCurrentAuraSideEffects(const SpellRec* spell, CGUnit_C* unit, int32_t value)
    {
        if (!spell || !unit)
            return;

        if (!unit->IsActivePlayer())
        {
            if (ExtraEffectsNeedAuraState(spell))
            {
                CDataStore msg;
                msg.Put(static_cast<uint32_t>(CMSG_PET_CANCEL_AURA));
                msg.Put(unit->m_obj->m_guid.m_guid);
                msg.Put(static_cast<uint32_t>(spell->m_id));
                msg.Finalize();
                ClientServices::Send2(&msg);
            }
            return;
        }

        if (unit->m_unit && unit->m_unit->m_unitFieldCharmedby)
            return;

        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (effect.effect == SPELL_EFFECT_SUMMON && SummonPropertySlotIsCritter(effect.effectMiscValueB))
            {
                uint64_t critterGuid = unit->m_unit ? unit->m_unit->m_unitFieldCritter.m_guid : 0;
                if (critterGuid && (((critterGuid >> 24) & 0x0FFFFFFFu) == static_cast<uint32_t>(effect.effectMiscValue)))
                {
                    CDataStore msg;
                    msg.Put(static_cast<uint32_t>(CMSG_DISMISS_CRITTER));
                    msg.Put(critterGuid);
                    msg.Finalize();
                    ClientServices::Send2(&msg);
                }
            }
            return true;
        });

        if (!ExtraEffectsNeedAuraState(spell))
            return;

        if (HasSpellAura(spell->m_id, SPELL_AURA_MOUNTED) && unit->m_mountDisplayId > 0 && !(unit->m_clientFlags & 0x10000000))
        {
            unit->OnMountDisplayChanged(0);
        }

        if (unit->GetShapeshiftForm() && HasSpellAura(spell->m_id, SPELL_AURA_MOD_SHAPESHIFT))
        {
            ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
                if (effect.effectApplyAuraName != SPELL_AURA_MOD_SHAPESHIFT)
                    return true;

                if (ShapeshiftFormHasFlag(effect.effectMiscValue, 2))
                    return false;

                if (unit->GetShapeshiftForm() == static_cast<uint32_t>(effect.effectMiscValue))
                {
                    unit->PreemptivelyCancelShapeshift();
                    return false;
                }
                return true;
            });
        }

        if ((spell->m_attributesEx & 0x2000) && !(spell->m_attributesEx & 0x4) && (unit->m_obj->m_type & TYPEMASK_PLAYER))
        {
            static_cast<CGPlayer_C*>(unit)->ToggleFarSight(nullptr);
        }

        if (spell->m_id == WAITING_TO_RESURRECT_SPELL_ID)
        {
            FrameScript_SignalEvent(SCRIPT_EVENT_AREA_SPIRIT_HEALER_OUT_OF_RANGE, nullptr);
        }

        CDataStore cancelMsg;
        cancelMsg.Put(static_cast<uint32_t>(CMSG_CANCEL_AURA));
        cancelMsg.Put(static_cast<uint32_t>(spell->m_id));
        cancelMsg.Finalize();
        ClientServices::Send2(&cancelMsg);
    }

    bool ValidateExtendedTargetMaskEffects(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& targetGuid, uint8_t* inRangeOut, bool reportErrors, PendingSpellCast* spellCast)
    {
        if (!caster || !spell)
            return false;

        WOWGUID effectiveTargetGuid = targetGuid ? targetGuid : CGGameUI::m_lockedTarget;
        CGObject_C* target = ClntObjMgrObjectPtr(effectiveTargetGuid, TYPEMASK_OBJECT, __FILE__, __LINE__);
        if (!target)
            return false;

        uint32_t targetMask = spell->m_targets & 0xFFFFu;
        bool usesPetTarget = false;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            ApplyImplicitTargetAToNativeTargetMask(effect.effectImplicitTargetA, targetMask, usesPetTarget);
            return true;
        });

        if (usesPetTarget && !(targetMask & 0x2u))
        {
            target = ClntObjMgrObjectPtr(CGPetInfo::GetPet(0), TYPEMASK_OBJECT, __FILE__, __LINE__);
            if (!target)
                return false;
            targetMask |= 0x2u;
        }

        uint32_t objectType = target->m_obj->m_type;
        bool targetAccepted = false;

        if (objectType & TYPEMASK_UNIT)
        {
            CGUnit_C* targetUnit = static_cast<CGUnit_C*>(target);

            if (targetUnit->m_unit && (targetUnit->m_unit->m_unitFieldFlags & 0x00010000 /* NOT_SELECTABLE */) && !(spell->m_attributesExF & 0x01000000u))
                return false;

            if (spell->m_targetCreatureType && targetUnit->m_unit)
            {
                int32_t creatureType = targetUnit->GetCreatureType();
                if (creatureType <= 0 || !(spell->m_targetCreatureType & (1u << static_cast<uint32_t>(creatureType - 1))))
                    return false;
            }

            uint32_t health = targetUnit->m_unit ? targetUnit->m_unit->m_unitFieldHealth : 0;
            if (health > 0)
            {
                if (targetMask & 0x400u)
                    return false;
            }
            else
            {
                uint32_t dynFlags = targetUnit->m_unit ? targetUnit->m_unit->m_unitDynamicFlags : 0;
                if (!(dynFlags & 0x00000020 /* LOOTABLE */) && !(targetMask & 0x8600u) && !(spell->m_attributesExB & 0x1u))
                    return false;
            }

            if ((targetMask & 0x8u) && caster->IsInMyPartyOrRaid(targetUnit) && caster->CanAssist(targetUnit, false))
                targetAccepted = true;
            else if ((targetMask & 0x4u) && caster->IsInMyParty(targetUnit) && caster->CanAssist(targetUnit, false))
                targetAccepted = true;
            else if ((targetMask & 0x100u) && caster->CanAssist(targetUnit, (spell->m_attributesExF & 0x8u) != 0))
                targetAccepted = true;
            else if ((targetMask & 0x80u) && caster->CanAttack(targetUnit))
                targetAccepted = true;
            else if ((targetMask & 0x2u) && targetUnit->m_unit && !(targetUnit->m_unit->m_unitFieldFlags & 0x00000100))
                targetAccepted = true;
            else if (targetMask & 0x8000u)
            {
                if (health == 0 && ((objectType & TYPEMASK_PLAYER) || (targetUnit->m_unit && targetUnit->m_unit->m_unitFieldPetnumber)) &&
                    caster->CanAssist(targetUnit, false))
                    targetAccepted = true;
            }

            if (!targetAccepted && (targetMask & 0x200u) && health == 0 && (objectType & TYPEMASK_PLAYER) && !caster->CanAssist(targetUnit, false))
                targetAccepted = true;
        }
        else if (objectType & TYPEMASK_GAMEOBJECT)
        {
            targetAccepted = (targetMask & 0x4800u) != 0;
        }
        else if (objectType & TYPEMASK_CORPSE)
        {
            targetAccepted = (targetMask & 0x8200u) != 0;
        }

        if (!targetAccepted)
            return false;

        float minRange = 0.0f;
        float maxRange = 0.0f;
        Spell_C::GetSpellRange(caster, spell->m_id, &minRange, &maxRange, target);

        C3Vector casterPos = caster->GetPosition();
        C3Vector targetPos = target->GetPosition();
        float dx = casterPos.x - targetPos.x;
        float dy = casterPos.y - targetPos.y;
        float dz = casterPos.z - targetPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        bool inRange = true;
        bool tooFar = false;
        if (minRange > 0.0f && minRange * minRange > distSq)
            inRange = false;
        if (maxRange > 0.0f && distSq > maxRange * maxRange)
        {
            inRange = false;
            tooFar = true;
        }

        if (inRangeOut)
            *inRangeOut = inRange ? 1 : 0;

        if (!inRange && reportErrors)
            Spell_C_SpellFailed(spellCast, spell, tooFar ? 97 : 128, -1, -1, false);

        return true;
    }

    bool TryPrepareExtendedCastTarget(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& target, PendingSpellCast* spellCast, bool confirmed)
    {
        if (!caster || !spell || !spellCast)
            return false;

        bool sawDrivingEffect = false;
        bool blockedTargetMode = false;
        uint32_t targetFlags = spell->m_targets;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (!ExtendedEffectShouldDriveCastTarget(effect))
                return true;

            sawDrivingEffect = true;
            ApplyImplicitTargetAToPrepareCastFlags(*spell, effect.effectImplicitTargetA, targetFlags, blockedTargetMode);
            return true;
        });

        if (!sawDrivingEffect)
            return false;

        spellCast->m_data.m_targetFlags = targetFlags;
        return true;
    }

    bool ValidateExtendedSendCastEffectInhibits(CGUnit_C* caster, PendingSpellCast* spellCast, const SpellRec* spell, const CGItem_C* castItem)
    {
        if (!caster || !spell)
            return true;

        bool valid = true;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (effect.effectIndex < 3)
                return true;

            if (effect.effect == 36)
            {
                int32_t learn = effect.effectTriggerSpell;
                if (!learn && castItem)
                {
                    WOWGUID none = { 0 };
                    ItemStats_C* stats = g_itemDBCache.GetRecord(castItem->m_obj->m_entryID, none, nullptr, nullptr, false);
                    if (stats)
                    {
                        for (uint32_t j = 0; j < 5; ++j)
                        {
                            if (stats->m_spellTrigger[j] == 6)
                            {
                                learn = stats->m_spellId[j];
                                break;
                            }
                        }
                    }
                }

                if (learn && (caster->IsSpellKnown(learn) || caster->IsSpellSuperceded(learn)))
                {
                    SpellRec known;
                    if (g_spellDB.GetRecord(learn, &known))
                        CGGameUI::DisplayError(ERR_SPELL_ALREADY_KNOWN_S, known.m_name);
                    else
                        CGGameUI::DisplayError(ERR_SPELL_ALREADY_KNOWN_S, "UNKNOWN");
                    valid = false;
                    return false;
                }
            }

            if (effect.effect == 56 || effect.effect == 55 || effect.effectApplyAuraName == 6 || effect.effectApplyAuraName == 2)
            {
                const WOWGUID* controlled = caster->m_unit ? &caster->m_unit->m_unitFieldCharm : nullptr;
                if (controlled && !*controlled && caster->m_unit)
                    controlled = &caster->m_unit->m_unitFieldSummon;

                if (controlled && *controlled && !(spell->m_attributesEx & 0x1))
                {
                    Spell_C_SpellFailed(spellCast, spell, (caster->m_unit && caster->m_unit->m_unitFieldCharm) ? 6 : 7, -1, -1, false);
                    valid = false;
                    return false;
                }
            }

            if (effect.effectImplicitTargetA == 5 || effect.effectImplicitTargetB == 5)
            {
                const WOWGUID* controlled = caster->m_unit ? &caster->m_unit->m_unitFieldCharm : nullptr;
                if (controlled && !*controlled && caster->m_unit)
                    controlled = &caster->m_unit->m_unitFieldSummon;

                if (!controlled || !*controlled)
                {
                    Spell_C_SpellFailed(spellCast, spell, 84, -1, -1, false);
                    valid = false;
                    return false;
                }
            }

            return true;
        });

        return valid;
    }

    bool ExtendedCanCastTargetPredicatePasses(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& targetGuid)
    {
        if (!caster || !spell)
            return false;

        WOWGUID effectiveGuid = targetGuid ? targetGuid : CGGameUI::m_lockedTarget;
        CGUnit_C* target = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(effectiveGuid, TYPEMASK_UNIT, __FILE__, __LINE__));

        bool matchedPredicate = false;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (!IsNativeCastTargetPredicateEffect(effect))
                return true;

            if (effect.effect == 78 /* SPELL_EFFECT_TRADE_SKILL */ || effect.effect == 2 /* SPELL_EFFECT_ATTACK */)
            {
                matchedPredicate = true;
                return false;
            }

            if (!target)
                return true;

            switch (effect.effectImplicitTargetA)
            {
            case 6:  matchedPredicate = caster->CanAttack(target); break;
            case 21: matchedPredicate = caster->CanAssist(target, false); break;
            case 35: matchedPredicate = caster->IsInMyPartyOrRaid(target) && caster->CanAssist(target, false); break;
            case 57:
            case 61: matchedPredicate = caster->IsInMyParty(target) && caster->CanAssist(target, false); break;
            default: matchedPredicate = false; break;
            }

            return !matchedPredicate;
        });

        return matchedPredicate;
    }

    bool ValidateExtendedAuraTargetEffects(CGUnit_C* caster, const SpellRec* spell, uint32_t targetCount, CGUnit_C** targets, int32_t* errorCode)
    {
        if (!caster || !spell || !targetCount || !targets)
            return true;

        uint32_t matchedTargets = 0;
        bool hasBadTargetsErrorEffect = false;

        for (uint32_t targetIndex = 0; targetIndex < targetCount; ++targetIndex)
        {
            CGUnit_C* target = targets[targetCount - targetIndex - 1];
            if (!target || !(target->m_obj->m_type & TYPEMASK_UNIT))
                continue;

            ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
                if (!IsNativeAuraTargetValidationEffect(effect))
                    return true;

                bool const isEffect108 = effect.effect == 108;
                bool const isEffect126 = effect.effect == 126;
                if (!isEffect126)
                    hasBadTargetsErrorEffect = true;

                int32_t reactionMode = -1;
                if (!(spell->m_attributesEx & 0x10000u))
                {
                    if (caster->CanAttack(target))
                        reactionMode = 1;
                    else if (caster->CanAssist(target, false))
                        reactionMode = 0;
                    else
                        return true;
                }

                if (target->HasDispellableAura(effect.effectMiscValue, reactionMode, isEffect108, isEffect126))
                {
                    ++matchedTargets;
                    return false;
                }

                return true;
            });
        }

        if (matchedTargets)
            return true;

        if (errorCode)
            *errorCode = hasBadTargetsErrorEffect ? 86 : 87;
        return false;
    }

    bool ExtendedSpellCurrentOnUnit(const SpellRec* spell, const CGUnit_C* unit)
    {
        if (!spell || !unit || !HasExtendedEffects(spell->m_id))
            return false;

        CGUnit_C* self = const_cast<CGUnit_C*>(unit);

        if (spell->m_activeIconID || spell->m_id == BLOOD_PRESENCE_SPELL_ID ||
            spell->m_id == FROST_PRESENCE_SPELL_ID || spell->m_id == UNHOLY_PRESENCE_SPELL_ID)
        {
            for (uint32_t i = 0; i < unit->GetAuraCount(); ++i)
            {
                if (self->m_auras[i].m_spellId == spell->m_id && (self->m_auras[i].m_flags & 0x10))
                    return true;
            }
        }

        uint64_t critterGuid = unit->m_unit ? unit->m_unit->m_unitFieldCritter.m_guid : 0;
        uint32_t critterEntry = static_cast<uint32_t>((critterGuid >> 24) & 0x0FFFFFFFu);

        bool active = false;
        ForEachSpellEffect(spell->m_id, [&](const SpellEffectCacheRow& effect) {
            if (effect.effect == SPELL_EFFECT_SUMMON && critterEntry &&
                static_cast<uint32_t>(effect.effectMiscValue) == critterEntry &&
                SummonPropertySlotIsCritter(effect.effectMiscValueB))
            {
                active = true;
                return false;
            }

            if (effect.effectApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT)
            {
                uint32_t currentFormId = unit->GetShapeshiftForm();
                if (currentFormId && static_cast<uint32_t>(effect.effectMiscValue) == currentFormId)
                {
                    active = true;
                    return false;
                }
            }

            if (effect.effectApplyAuraName == SPELL_AURA_MOUNTED)
            {
                for (uint32_t i = 0; i < unit->GetAuraCount(); ++i)
                {
                    const SpellEffectCacheRow* auraEffect = nullptr;
                    if (TryFindSpellEffectByAura(self->m_auras[i].m_spellId, SPELL_AURA_MOUNTED, auraEffect) &&
                        auraEffect && auraEffect->effectMiscValue == effect.effectMiscValue)
                    {
                        active = true;
                        return false;
                    }
                }
            }

            return true;
        });

        return active;
    }
} // namespace LocalSpellData
