-- ==============================================================================
-- Duskhaven Local Spell Data & Rules
-- ==============================================================================

local LocalSpells = {}

-- Custom spell rule table
-- Define spell behavior overrides directly here
local SPELL_RULES = {
    -- Blood Presence
    [48266] = { isPresence = true, showOnActionBar = true },
    -- Frost Presence
    [48263] = { isPresence = true, showOnActionBar = true },
    -- Unholy Presence
    [48265] = { isPresence = true, showOnActionBar = true },
    -- Waiting to Resurrect
    [2584] = { isPassive = true, hiddenClientSide = true }
}

-- Hook into spell queries from engine
ModCore.Hooks.Register("OnSpellQuery", function(queryType, spellId, caster)
    local rule = SPELL_RULES[spellId]
    if not rule then return nil end

    if queryType == "IsPassive" then
        if rule.isPresence then
            return false -- Show on action bars even if DBC flags say passive
        elseif rule.isPassive ~= nil then
            return rule.isPassive
        end
    elseif queryType == "PowerCost" then
        if rule.customPowerCost then
            return rule.customPowerCost
        end
    end

    return nil
end, 100, "Duskhaven spell custom attribute queries")

ModCore.LogInfo("Duskhaven LocalSpells rules registered.")
return LocalSpells
