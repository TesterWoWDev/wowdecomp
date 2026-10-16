-- ==============================================================================
-- Duskhaven Owned Tooltip Evaluator & Dynamic Scaling Formulas
-- ==============================================================================

local TooltipEvaluator = {}

-- Custom spell tooltip scaling formulas
local SPELL_TOOLTIP_FORMULAS = {
    -- Shadowflame Cleave
    [98100] = function(caster)
        local ap = caster and caster:GetAttackPower() or 0
        local sp = caster and caster:GetSpellPower("SHADOW") or 0
        local minDmg = 300 + (ap * 0.45) + (sp * 0.20)
        local maxDmg = 350 + (ap * 0.45) + (sp * 0.20)
        return string.format("Strike enemies in front for |cffff4040%.0f|r to |cffff4040%.0f|r Shadowflame damage.\n|cff808080(Scales with 45%% Attack Power and 20%% Spell Power)|r", minDmg, maxDmg)
    end
}

-- Hook into tooltip evaluation
ModCore.Hooks.Register("OnTooltipEvaluate", function(spellId, caster, defaultText)
    local formula = SPELL_TOOLTIP_FORMULAS[spellId]
    if formula then
        local evaluatedText = formula(caster)
        return evaluatedText
    end
    return nil -- Return nil to use standard client evaluation
end, 100, "Duskhaven dynamic tooltip formula evaluation")

ModCore.LogInfo("Duskhaven TooltipEvaluator module registered.")
return TooltipEvaluator
