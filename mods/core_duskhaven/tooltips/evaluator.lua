-- ==============================================================================
-- Duskhaven Owned Tooltip Evaluator, CDBC Decorators & Dynamic Scaling Formulas
-- ==============================================================================

local DBC = require("system.dbc")
local ItemDifficultyText = require("dbc.item_difficulty_text")
local SpellAdditionalCostData = require("dbc.spell_additional_cost_data")
local SpellAdditionalAttributes = require("dbc.spell_additional_attributes")
local ItemOverrides = require("player.item_overrides")

local TooltipEvaluator = {}

-- Custom spell tooltip scaling formulas
local SPELL_TOOLTIP_FORMULAS = {
    -- Shadowflame Cleave
    [98100] = function(caster)
        local ap = 0
        local sp = 0
        if caster and type(caster) == "table" and caster.GetAttackPower then
            ap = caster:GetAttackPower() or 0
        elseif UnitAttackPower then
            local b, p, n = UnitAttackPower("player")
            ap = (b or 0) + (p or 0) + (n or 0)
        end
        if caster and type(caster) == "table" and caster.GetSpellPower then
            sp = caster:GetSpellPower("SHADOW") or 0
        elseif GetSpellBonusDamage then
            sp = GetSpellBonusDamage(6) or 0
        end
        local minDmg = 300 + (ap * 0.45) + (sp * 0.20)
        local maxDmg = 350 + (ap * 0.45) + (sp * 0.20)
        return string.format("Strike enemies in front for |cffff4040%.0f|r to |cffff4040%.0f|r Shadowflame damage.\n|cff808080(Scales with 45%% Attack Power and 20%% Spell Power)|r", minDmg, maxDmg)
    end
}

-- 1. Hook dynamic spell tooltip evaluation (called from C++ QuestTextParser / Tooltip dispatch)
ModCore.Hooks.Register("OnTooltipEvaluate", function(spellId, caster, defaultText)
    spellId = tonumber(spellId)
    if not spellId then return nil end

    local formula = SPELL_TOOLTIP_FORMULAS[spellId]
    if formula then
        local evaluatedText = formula(caster)
        return evaluatedText
    end

    return nil -- Return nil to use standard client evaluation
end, 100, "Duskhaven dynamic tooltip formula evaluation")

-- 2. Hook item tooltips: ItemDifficultyText & Transmog Overrides
ModCore.Hooks.Register("OnTooltipSetItem", function(tooltip, itemId)
    itemId = tonumber(itemId)
    if not itemId or not tooltip then return end

    -- A. ItemDifficultyText (Heroic, Mythic, Ascended, etc.)
    local diff = ItemDifficultyText.Get(itemId)
    if diff and diff.text and diff.text ~= "" then
        if tooltip.AddLine then
            tooltip:AddLine(diff.text, diff.red or 1.0, diff.green or 1.0, diff.blue or 1.0)
        end
    end

    -- B. Transmog Overrides
    local currentGuid = ItemOverrides.GetCurrentContextGuid()
    if currentGuid and currentGuid ~= 0 then
        -- Search visible item slots for this item entry
        for slot = 0, 19 do
            local ov = ItemOverrides.GetOverrideItem(currentGuid, slot)
            if ov and ov.entryId == itemId and ov.transmog and ov.transmog > 0 then
                local transmogName = "Custom Appearance"
                if GetItemInfo then
                    local name = GetItemInfo(ov.transmog)
                    if name then transmogName = name end
                end
                if tooltip.AddLine then
                    tooltip:AddLine(string.format("Transmogrified to: %s", transmogName), 1.0, 0.5, 1.0)
                end
                break
            end
        end
    end
end, 100, "Duskhaven item difficulty text and transmog decorator")

-- 3. Hook spell tooltips: SpellAdditionalCostData & SpellAdditionalAttributes
ModCore.Hooks.Register("OnTooltipSetSpell", function(tooltip, spellId)
    spellId = tonumber(spellId)
    if not spellId or not tooltip then return end

    -- Check SpellAdditionalCostData
    local costStr = SpellAdditionalCostData.FormatCost(spellId)
    if costStr and tooltip.AddLine then
        tooltip:AddLine(costStr, 1.0, 1.0, 1.0)
    end
end, 100, "Duskhaven spell additional cost decorator")

-- Support GameTooltip / ItemRefTooltip script hooking if available
if GameTooltip and GameTooltip.HookScript then
    pcall(function()
        GameTooltip:HookScript("OnTooltipSetItem", function(self)
            local _, link = self:GetItem()
            if link then
                local itemId = tonumber(string.match(link, "item:(%d+)"))
                if itemId then
                    ModCore.Hooks.Fire("OnTooltipSetItem", self, itemId)
                end
            end
        end)

        GameTooltip:HookScript("OnTooltipSetSpell", function(self)
            local _, _, spellId = self:GetSpell()
            if spellId then
                ModCore.Hooks.Fire("OnTooltipSetSpell", self, spellId)
            end
        end)
    end)
end

if ItemRefTooltip and ItemRefTooltip.HookScript then
    pcall(function()
        ItemRefTooltip:HookScript("OnTooltipSetItem", function(self)
            local _, link = self:GetItem()
            if link then
                local itemId = tonumber(string.match(link, "item:(%d+)"))
                if itemId then
                    ModCore.Hooks.Fire("OnTooltipSetItem", self, itemId)
                end
            end
        end)
    end)
end

ModCore.LogInfo("Duskhaven TooltipEvaluator & Decorator module registered.")
return TooltipEvaluator
