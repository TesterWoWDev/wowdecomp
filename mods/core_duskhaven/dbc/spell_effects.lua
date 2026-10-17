-- ==============================================================================
-- Duskhaven CDBC: SpellEffect & SpellEffectScalars
-- ==============================================================================

local DBC = require("system.dbc")

local SpellEffects = {}

local spellEffectSchema = {
    "spellID",
    "effectIndex",
    "effect",
    "effectDieSides",
    "effectRealPointsPerLevel",
    "effectBasePoints",
    "effectMechanic",
    "effectImplicitTargetA",
    "effectImplicitTargetB",
    "effectRadiusIndex",
    "effectApplyAuraName",
    "effectAmplitude",
    "effectMultipleValue",
    "effectChainTargets",
    "effectItemType",
    "effectMiscValue",
    "effectMiscValueB",
    "effectTriggerSpell",
    "effectPointsPerCombo",
    "effectSpellClassMaskA",
    "effectSpellClassMaskB",
    "effectSpellClassMaskC",
    "effectChainAmplitude",
    "effectBonusMultiplier",
    "effectSpellPowerBonus",
    "effectAttackPowerBonus",
    "effectBlockValueBonus",
    "effectScalingMode"
}

local spellEffectScalarsSchema = {
    "spellID",
    "effectIdx",
    "sp",
    "ap",
    "bv",
    "mode"
}

DBC.RegisterTable("SpellEffect", spellEffectSchema, {})
DBC.RegisterTable("SpellEffectScalars", spellEffectScalarsSchema, {})

local function MakeCacheKey(spellId, effectIndex)
    return (bit and bit.lshift) and (bit.lshift(spellId, 5) + (effectIndex or 0)) or ((spellId * 32) + (effectIndex or 0))
end

function SpellEffects.RegisterSpellEffect(spellId, effectIndex, data)
    spellId = tonumber(spellId)
    effectIndex = tonumber(effectIndex) or 0
    if not spellId or not data then return end
    data.spellID = spellId
    data.effectIndex = effectIndex
    local key = MakeCacheKey(spellId, effectIndex)
    DBC.DefineRow("SpellEffect", key, data)
end

function SpellEffects.GetSpellEffect(spellId, effectIndex)
    spellId = tonumber(spellId)
    effectIndex = tonumber(effectIndex) or 0
    if not spellId then return nil end
    local key = MakeCacheKey(spellId, effectIndex)
    return DBC.GetRow("SpellEffect", key)
end

function SpellEffects.RegisterSpellEffectScalars(spellId, effectIndex, sp, ap, bv, mode)
    spellId = tonumber(spellId)
    effectIndex = tonumber(effectIndex) or 0
    if not spellId then return end
    local key = MakeCacheKey(spellId, effectIndex)
    DBC.DefineRow("SpellEffectScalars", key, {
        spellID = spellId,
        effectIdx = effectIndex,
        sp = tonumber(sp) or 0.0,
        ap = tonumber(ap) or 0.0,
        bv = tonumber(bv) or 0.0,
        mode = tonumber(mode) or 0
    })
end

function SpellEffects.GetSpellEffectScalars(spellId, effectIndex)
    spellId = tonumber(spellId)
    effectIndex = tonumber(effectIndex) or 0
    if not spellId then return nil end
    local key = MakeCacheKey(spellId, effectIndex)
    return DBC.GetRow("SpellEffectScalars", key)
end

_G.SpellEffects = SpellEffects
return SpellEffects
