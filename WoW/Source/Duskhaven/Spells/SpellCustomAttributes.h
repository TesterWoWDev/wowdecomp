#pragma once

#include <cstdint>

class SpellRec;

namespace Duskhaven
{
    enum SpellAttr0Cu : uint32_t
    {
        SPELL_ATTR0_CU_ENCHANT_PROC                 = 0x00000001,
        SPELL_ATTR0_CU_CONE_BACK                    = 0x00000002,
        SPELL_ATTR0_CU_CONE_LINE                    = 0x00000004,
        SPELL_ATTR0_CU_SHARE_DAMAGE                 = 0x00000008,
        SPELL_ATTR0_CU_NO_INITIAL_THREAT            = 0x00000010,
        SPELL_ATTR0_CU_AURA_CC                      = 0x00000020,
        SPELL_ATTR0_CU_DONT_BREAK_STEALTH           = 0x00000040,
        SPELL_ATTR0_CU_CAN_CRIT                     = 0x00000080,
        SPELL_ATTR0_CU_DIRECT_DAMAGE                = 0x00000100,
        SPELL_ATTR0_CU_CHARGE                       = 0x00000200,
        SPELL_ATTR0_CU_PICKPOCKET                   = 0x00000400,
        SPELL_ATTR0_CU_ROLLING_PERIODIC             = 0x00000800,
        SPELL_ATTR0_CU_NEGATIVE_EFF0                = 0x00001000,
        SPELL_ATTR0_CU_NEGATIVE_EFF1                = 0x00002000,
        SPELL_ATTR0_CU_NEGATIVE_EFF2                = 0x00004000,
        SPELL_ATTR0_CU_IGNORE_ARMOR                 = 0x00008000,
        SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER     = 0x00010000,
        SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET     = 0x00020000,
        SPELL_ATTR0_CU_ALLOW_INFLIGHT_TARGET        = 0x00040000,
        SPELL_ATTR0_CU_NEEDS_AMMO_DATA              = 0x00080000,
        SPELL_ATTR0_CU_BINARY_SPELL                 = 0x00100000,
        SPELL_ATTR0_CU_SCHOOLMASK_NORMAL_WITH_MAGIC = 0x00200000,
        SPELL_ATTR0_CU_DEPRECATED_LIQUID_AURA       = 0x00400000,
        SPELL_ATTR0_CU_IS_TALENT                    = 0x00800000,
        SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED         = 0x01000000,
        SPELL_ATTR0_CU_DONT_RESTART_P_TIMER         = 0x02000000,
        SPELL_ATTR0_CU_USE_RANGED_NO_AMMO           = 0x04000000,
        SPELL_ATTR0_CU_BYPASS_MECHANIC_IMMUNITY     = 0x80000000,
        SPELL_ATTR0_CU_NEGATIVE = SPELL_ATTR0_CU_NEGATIVE_EFF0 | SPELL_ATTR0_CU_NEGATIVE_EFF1 | SPELL_ATTR0_CU_NEGATIVE_EFF2
    };

    enum SpellAttr1Cu : uint32_t
    {
        SPELL_ATTR1_CU_REQUIRES_COMBAT                     = 0x00000001,
        SPELL_ATTR1_CU_NO_ATTACK_BLOCK                     = 0x00000002,
        SPELL_ATTR1_CU_SCALE_DAMAGE_EFFECTS_ONLY           = 0x00000004,
        SPELL_ATTR1_CU_SCALE_HEALING_EFFECTS_ONLY          = 0x00000008,
        SPELL_ATTR1_CU_USE_TARGETS_LEVEL_FOR_SPELL_SCALING = 0x00000010,
        SPELL_ATTR1_CU_REMOVE_OUTSIDE_DUNGEONS_AND_RAIDS   = 0x00000020,
        SPELL_ATTR1_CU_NOT_USABLE_IN_INSTANCES             = 0x00000040,
        SPELL_ATTR1_CU_USABLE_IN_INSTANCES_ONLY            = 0x00000080,
        SPELL_ATTR1_CU_PERIODIC_CAN_CRIT                   = 0x00000100,
        SPELL_ATTR1_CU_IGNORES_CASTER_LEVEL                = 0x00000200,
        SPELL_ATTR1_CU_ONLY_PROC_FROM_CLASS_ABILITIES      = 0x00000400,
        SPELL_ATTR1_CU_ACTIVATES_REQUIRED_SHAPESHIFT       = 0x00000800,
        SPELL_ATTR1_CU_REAPPLY_NO_REFRESH_DURATION         = 0x00001000,
        SPELL_ATTR1_CU_SPECIAL_DELAY_CALCULATION           = 0x00002000,
        SPELL_ATTR1_CU_CAST_TIME_UNAFFECTED_BY_HASTE       = 0x00004000,
        SPELL_ATTR1_CU_LOW_CAST_TIME_OK                    = 0x00008000,
        SPELL_ATTR1_CU_STACKS_DONT_ADDUP                   = 0x00010000,
        SPELL_ATTR1_CU_PANDEMIC_TIMER                      = 0x00020000,
        SPELL_ATTR1_CU_MISSILE_SPEED_IS_DELAY_IN_SEC       = 0x00040000,
        SPELL_ATTR1_CU_ALLOW_DEFENSE_WHILE_CASTING         = 0x00080000,
        SPELL_ATTR1_CU_DROP_STACK_ON_EXPIRE                = 0x00100000,
        SPELL_ATTR1_CU_COMBODAMAGE                         = 0x00200000,
        SPELL_ATTR1_CU_COMBODURATION                       = 0x00400000,
    };

    enum SpellAttr2Cu : uint32_t
    {
        SPELL_ATTR2_CU_TREAT_AS_INSTANT                    = 0x00000001,
        SPELL_ATTR2_CU_FORCE_HIDE_CASTBAR                  = 0x00000002,
        SPELL_ATTR2_CU_DO_NOT_DISPLAY_POWER_COST           = 0x00000004,
        SPELL_ATTR2_CU_SUPPRESS_LEARN_MSG                  = 0x00000008,
        SPELL_ATTR2_CU_SUPPRESS_UNLEARN_MSG                = 0x00000010,
        SPELL_ATTR2_CU_INVERT_CASTBAR                      = 0x00000020,
        SPELL_ATTR2_CU_LOW_TIME_TREAT_AS_INSTANT           = 0x00000040,
        SPELL_ATTR2_CU_LOW_TIME_FORCE_HIDE_CASTBAR         = 0x00000080,
        SPELL_ATTR2_CU_LOW_CAST_TIME_DONT_INTERRUPT        = 0x00000100,
    };

    inline bool HasAttribute(const SpellRec* spell, SpellAttr0Cu attr);
    inline bool HasAttribute(const SpellRec* spell, SpellAttr1Cu attr);
    inline bool HasAttribute(const SpellRec* spell, SpellAttr2Cu attr);
    inline bool HasAttribute(uint32_t spellId, SpellAttr2Cu attr);
} // namespace Duskhaven

#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalAttributes.h"

namespace Duskhaven
{
    inline bool HasAttribute(const SpellRec* spell, SpellAttr0Cu attr)
    {
        if (!spell)
            return false;
        SpellAdditionalAttributesRow* row = Duskhaven::SpellAdditionalAttributesDB::Get(spell->m_id);
        return row && (row->customAttr0 & attr);
    }

    inline bool HasAttribute(const SpellRec* spell, SpellAttr1Cu attr)
    {
        if (!spell)
            return false;
        SpellAdditionalAttributesRow* row = Duskhaven::SpellAdditionalAttributesDB::Get(spell->m_id);
        return row && (row->customAttr1 & attr);
    }

    inline bool HasAttribute(const SpellRec* spell, SpellAttr2Cu attr)
    {
        if (!spell)
            return false;
        SpellAdditionalAttributesRow* row = Duskhaven::SpellAdditionalAttributesDB::Get(spell->m_id);
        return row && (row->customAttr2 & attr);
    }

    inline bool HasAttribute(uint32_t spellId, SpellAttr2Cu attr)
    {
        SpellAdditionalAttributesRow* row = Duskhaven::SpellAdditionalAttributesDB::Get(spellId);
        return row && (row->customAttr2 & attr);
    }
} // namespace Duskhaven
