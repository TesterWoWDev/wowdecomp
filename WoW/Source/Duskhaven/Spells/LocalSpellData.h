#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/Object/WGUID.h"

class CGUnit_C;
class CGPlayer_C;
class CGGameObject_C;
class CGItem_C;
class PendingSpellCast;
class AuraData;

namespace LocalSpellData
{
    // Extended effect querying
    bool HasSpell(uint32_t spellId);
    bool TryGetSpellRow(uint32_t spellId, SpellRec& out);
    void ForEachSpell(const std::function<void(const SpellRec&)>& visitor);
    uint32_t ForEachSpellEffect(uint32_t spellId, const std::function<bool(const SpellEffectCacheRow&)>& visitor);
    uint32_t GetSpellEffectCount(uint32_t spellId);
    bool TryGetSpellEffect(uint32_t spellId, uint32_t effectIndex, const SpellEffectCacheRow*& out);
    bool HasExtendedEffects(uint32_t spellId);
    bool HasSpellEffect(uint32_t spellId, uint32_t effectId);
    bool HasSpellAura(uint32_t spellId, uint32_t auraId);
    bool HasAnySpellAura(uint32_t spellId, const uint32_t* auraIds, uint32_t count);
    bool TryFindSpellEffectByEffect(uint32_t spellId, uint32_t effectId, const SpellEffectCacheRow*& out);
    bool TryFindSpellEffectByAura(uint32_t spellId, uint32_t auraId, const SpellEffectCacheRow*& out);

    // Profession recipe indexing
    SkillLineAbilityRec* GetSkillLineAbility(uint32_t spellId);
    uint32_t GetKnownProfessionRecipeCount(uint32_t skillLineId);
    bool TryGetKnownProfessionRecipeSpellId(uint32_t skillLineId, uint32_t oneBasedIndex, uint32_t& spellId);
    void InvalidateProfessionRecipes();

    // Known spell effects & language refresh
    void ResetLocalKnownEffects();
    void RefreshLocalKnownEffects();
    void PumpExtendedLanguageRefresh();

    // Skinning & locks
    int32_t GetPlayerSkillLineValueWithProfessionAliases(uint32_t skillLineId);
    int32_t GetExtendedSkinningCapability(uint32_t spellId, CGPlayer_C* player, int32_t skinnableType);
    bool ResolveExtraOpenLock(const CGGameObject_C* gob, uint32_t requestedSpell, uint32_t& spellId, int32_t& cap, int32_t& required, uint32_t& lockIndex);

    // Spell helpful / harmful & side list acceptance
    uint32_t GetExtendedSpellHelpfulHarmful(const SpellRec* spell);
    bool ExtendedAuraSideListAcceptsSpell(const SpellRec& row, const AuraData* aura);
    bool ExtendedKnownAuraShouldEnterSideList(const SpellRec& row);
    bool SpellClassMaskMatchesEffect(const SpellRec& source, const SpellRec& aura, const SpellEffectCacheRow& effect);

    // Power cost & cast target helpers
    uint32_t GetExtendedEffect97ActionBarPowerCost(const SpellEffectCacheRow& effect, const SpellRec& source, const CGUnit_C* unit);
    bool ExtraEffectsNeedAuraState(const SpellRec* spell);
    bool ExtraEffectsNeedCastTarget(const SpellRec* spell);
    void HandleExtendedCurrentAuraSideEffects(const SpellRec* spell, CGUnit_C* unit, int32_t value);
    bool ValidateExtendedTargetMaskEffects(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& targetGuid, uint8_t* inRangeOut, bool reportErrors, PendingSpellCast* spellCast);
    bool TryPrepareExtendedCastTarget(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& target, PendingSpellCast* spellCast, bool confirmed);
    bool ValidateExtendedSendCastEffectInhibits(CGUnit_C* caster, PendingSpellCast* spellCast, const SpellRec* spell, const CGItem_C* castItem);
    bool ExtendedCanCastTargetPredicatePasses(CGUnit_C* caster, const SpellRec* spell, const WOWGUID& targetGuid);
    bool ValidateExtendedAuraTargetEffects(CGUnit_C* caster, const SpellRec* spell, uint32_t targetCount, CGUnit_C** targets, int32_t* errorCode);
    bool ExtendedSpellCurrentOnUnit(const SpellRec* spell, const CGUnit_C* unit);
} // namespace LocalSpellData
