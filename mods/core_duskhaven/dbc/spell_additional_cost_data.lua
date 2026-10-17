-- ==============================================================================
-- Duskhaven CDBC: SpellAdditionalCostData
-- ==============================================================================

local DBC = require("system.dbc")

local SpellAdditionalCostData = {}

SpellAdditionalCostData.Flags = {
    PluralizeResourceName = 0x00000001,
}

local schema = {
    "spellID",
    "resourceName",
    "cost",
    "flags",
    "resourceNamePlural",
    "maximumAdditionalSpend",
    "scalarPerPower",
    "damageSpellID",
    "powerType",
    "powerUnitMultiplier",
    "requiredAuraID",
    "excludedAuraID",
    "freeMaxSpendAuraID",
    "freeMaxSpendConditionAuraID",
    "generatedPowerAuraID",
    "generatedPower"
}

local defaultRows = {
    -- Pre-register common Duskhaven additional cost spells
    [98101] = {
        spellID = 98101,
        resourceName = "Energy",
        cost = 20,
        flags = 0,
        resourceNamePlural = "Energy",
        maximumAdditionalSpend = 30,
        scalarPerPower = 1.0,
        damageSpellID = 0,
        powerType = 3, -- Energy
        powerUnitMultiplier = 1,
        requiredAuraID = 0,
        excludedAuraID = 0,
        freeMaxSpendAuraID = 0,
        freeMaxSpendConditionAuraID = 0,
        generatedPowerAuraID = 0,
        generatedPower = 0
    }
}

DBC.RegisterTable("SpellAdditionalCostData", schema, defaultRows)

function SpellAdditionalCostData.Register(spellId, data)
    spellId = tonumber(spellId)
    if not spellId or not data then return end
    data.spellID = spellId
    DBC.DefineRow("SpellAdditionalCostData", spellId, data)
end

function SpellAdditionalCostData.Get(spellId)
    return DBC.GetRow("SpellAdditionalCostData", spellId)
end

function SpellAdditionalCostData.FormatCost(spellId, baseCost)
    local row = DBC.GetRow("SpellAdditionalCostData", spellId)
    if not row then return nil end

    baseCost = tonumber(baseCost) or (row.cost or 0)
    local maxSpend = row.maximumAdditionalSpend or 0

    if maxSpend > 0 and baseCost > 0 then
        local totalMax = baseCost + maxSpend
        local rName = row.resourceName or ""
        if totalMax ~= 1 and row.resourceNamePlural and row.resourceNamePlural ~= "" then
            rName = row.resourceNamePlural
        elseif totalMax ~= 1 and bit and bit.band and (bit.band(row.flags or 0, SpellAdditionalCostData.Flags.PluralizeResourceName) ~= 0) then
            rName = rName .. "s"
        end

        return string.format("%d to %d %s", baseCost, totalMax, rName)
    end

    return nil
end

_G.SpellAdditionalCostData = SpellAdditionalCostData
return SpellAdditionalCostData
