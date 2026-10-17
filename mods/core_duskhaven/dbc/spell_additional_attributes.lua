-- ==============================================================================
-- Duskhaven CDBC: SpellAdditionalAttributes
-- ==============================================================================

local DBC = require("system.dbc")

local SpellAdditionalAttributes = {}

SpellAdditionalAttributes.SpellAttr0Cu = {
    ENCHANT_PROC                 = 0x00000001,
    CONE_BACK                    = 0x00000002,
    CONE_LINE                    = 0x00000004,
    SHARE_DAMAGE                 = 0x00000008,
    NO_INITIAL_THREAT            = 0x00000010,
    AURA_CC                      = 0x00000020,
    DONT_BREAK_STEALTH           = 0x00000040,
    CAN_CRIT                     = 0x00000080,
    DIRECT_DAMAGE                = 0x00000100,
    CHARGE                       = 0x00000200,
    PICKPOCKET                   = 0x00000400,
    ROLLING_PERIODIC             = 0x00000800,
    NEGATIVE_EFF0                = 0x00001000,
    NEGATIVE_EFF1                = 0x00002000,
    NEGATIVE_EFF2                = 0x00004000,
    IGNORE_ARMOR                 = 0x00008000,
    REQ_TARGET_FACING_CASTER     = 0x00010000,
    REQ_CASTER_BEHIND_TARGET     = 0x00020000,
    ALLOW_INFLIGHT_TARGET        = 0x00040000,
    NEEDS_AMMO_DATA              = 0x00080000,
    BINARY_SPELL                 = 0x00100000,
    SCHOOLMASK_NORMAL_WITH_MAGIC = 0x00200000,
    DEPRECATED_LIQUID_AURA       = 0x00400000,
    IS_TALENT                    = 0x00800000,
    AURA_CANNOT_BE_SAVED         = 0x01000000,
    DONT_RESTART_P_TIMER         = 0x02000000,
    USE_RANGED_NO_AMMO           = 0x04000000,
    BYPASS_MECHANIC_IMMUNITY     = 0x80000000,
}

SpellAdditionalAttributes.SpellAttr1Cu = {
    REQUIRES_COMBAT                     = 0x00000001,
    NO_ATTACK_BLOCK                     = 0x00000002,
    SCALE_DAMAGE_EFFECTS_ONLY           = 0x00000004,
    SCALE_HEALING_EFFECTS_ONLY          = 0x00000008,
    USE_TARGETS_LEVEL_FOR_SPELL_SCALING = 0x00000010,
    REMOVE_OUTSIDE_DUNGEONS_AND_RAIDS   = 0x00000020,
    NOT_USABLE_IN_INSTANCES             = 0x00000040,
    USABLE_IN_INSTANCES_ONLY            = 0x00000080,
    PERIODIC_CAN_CRIT                   = 0x00000100,
    IGNORES_CASTER_LEVEL                = 0x00000200,
    ONLY_PROC_FROM_CLASS_ABILITIES      = 0x00000400,
    ACTIVATES_REQUIRED_SHAPESHIFT       = 0x00000800,
    REAPPLY_NO_REFRESH_DURATION         = 0x00001000,
    SPECIAL_DELAY_CALCULATION           = 0x00002000,
    CAST_TIME_UNAFFECTED_BY_HASTE       = 0x00004000,
    LOW_CAST_TIME_OK                    = 0x00008000,
    STACKS_DONT_ADDUP                   = 0x00010000,
    PANDEMIC_TIMER                      = 0x00020000,
    MISSILE_SPEED_IS_DELAY_IN_SEC       = 0x00040000,
    ALLOW_DEFENSE_WHILE_CASTING         = 0x00080000,
    DROP_STACK_ON_EXPIRE                = 0x00100000,
    COMBODAMAGE                         = 0x00200000,
    COMBODURATION                       = 0x00400000,
}

SpellAdditionalAttributes.SpellAttr2Cu = {
    TREAT_AS_INSTANT                    = 0x00000001,
    FORCE_HIDE_CASTBAR                  = 0x00000002,
    DO_NOT_DISPLAY_POWER_COST           = 0x00000004,
    SUPPRESS_LEARN_MSG                  = 0x00000008,
    SUPPRESS_UNLEARN_MSG                = 0x00000010,
    INVERT_CASTBAR                      = 0x00000020,
    LOW_TIME_TREAT_AS_INSTANT           = 0x00000040,
    LOW_TIME_FORCE_HIDE_CASTBAR         = 0x00000080,
    LOW_CAST_TIME_DONT_INTERRUPT        = 0x00000100,
}

local schema = { "spellID", "customAttr0", "customAttr1", "customAttr2", "customAttr3" }

local defaultRows = {
    -- Pre-register common Duskhaven custom spell attributes
    [98100] = { spellID = 98100, customAttr0 = 0, customAttr1 = 0, customAttr2 = SpellAdditionalAttributes.SpellAttr2Cu.TREAT_AS_INSTANT, customAttr3 = 0 },
}

DBC.RegisterTable("SpellAdditionalAttributes", schema, defaultRows)

function SpellAdditionalAttributes.RegisterSpell(spellId, attr0, attr1, attr2, attr3)
    spellId = tonumber(spellId)
    if not spellId then return end
    DBC.DefineRow("SpellAdditionalAttributes", spellId, {
        spellID = spellId,
        customAttr0 = tonumber(attr0) or 0,
        customAttr1 = tonumber(attr1) or 0,
        customAttr2 = tonumber(attr2) or 0,
        customAttr3 = tonumber(attr3) or 0
    })
end

function SpellAdditionalAttributes.Get(spellId)
    return DBC.GetRow("SpellAdditionalAttributes", spellId)
end

function SpellAdditionalAttributes.HasAttribute(spellId, group, flag)
    local row = DBC.GetRow("SpellAdditionalAttributes", spellId)
    if not row or not flag then return false end

    local mask = 0
    if group == 0 or group == "Attr0" or group == "customAttr0" then
        mask = row.customAttr0 or 0
    elseif group == 1 or group == "Attr1" or group == "customAttr1" then
        mask = row.customAttr1 or 0
    elseif group == 2 or group == "Attr2" or group == "customAttr2" then
        mask = row.customAttr2 or 0
    elseif group == 3 or group == "Attr3" or group == "customAttr3" then
        mask = row.customAttr3 or 0
    end

    return bit and bit.band and (bit.band(mask, flag) ~= 0) or false
end

_G.SpellAdditionalAttributes = SpellAdditionalAttributes
return SpellAdditionalAttributes
