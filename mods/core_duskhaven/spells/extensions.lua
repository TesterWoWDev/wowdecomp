-- ==============================================================================
-- Duskhaven Spell & Profession API Extensions
-- ==============================================================================

local SpellExt = {}

local tooltipScanner = nil
local function GetScanner()
    if not tooltipScanner and CreateFrame then
        tooltipScanner = CreateFrame("GameTooltip", "DHSpellDescScanner", nil, "GameTooltipTemplate")
    end
    return tooltipScanner
end

function SpellExt.GetSpellDescription(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return "" end

    local scanner = GetScanner()
    if scanner then
        scanner:SetOwner(WorldFrame or UIParent or UIParent, "ANCHOR_NONE")
        scanner:ClearLines()
        if scanner.SetSpellByID then
            scanner:SetSpellByID(spellId)
        end
        local numLines = scanner:NumLines()
        for i = 2, numLines do
            local line = _G["DHSpellDescScannerTextLeft" .. i]
            if line then
                local text = line:GetText()
                if text and text ~= "" and
                   not text:find("^Rank ") and
                   not text:find("yd range") and
                   not text:find("Cast") and
                   not text:find("Cooldown") and
                   not text:find("Requires") then
                    return text
                end
            end
        end
    end

    if GetSpellInfo then
        local name = GetSpellInfo(spellId)
        return name or ""
    end
    return ""
end

function SpellExt.GetSpellNameById(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return nil, nil end

    if GetSpellInfo then
        local name, rank = GetSpellInfo(spellId)
        if name then
            return name, rank or ""
        end
    end
    return nil, nil
end

function SpellExt.GetSpellIconById(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return nil end

    if GetSpellInfo then
        local _, _, icon = GetSpellInfo(spellId)
        if icon then
            return icon
        end
    end
    return nil
end

function SpellExt.SpellRequiresComboPoints(spellId)
    return false
end

function SpellExt.UnitCustomCastingData(unit)
    if not unit then return nil end

    if UnitCastingInfo then
        local name, _, _, _, _, _, _, castId = UnitCastingInfo(unit)
        if name then
            return castId or 0, false, false
        end
    end

    if UnitChannelInfo then
        local name, _, _, _, _, _, _, castId = UnitChannelInfo(unit)
        if name then
            return castId or 0, false, false
        end
    end

    return nil
end

function SpellExt.PreloadProfessionItemCache(itemId)
    return true
end

function SpellExt.GetProfessionRecipeCount(skillLineId)
    return 0
end

function SpellExt.GetProfessionRecipeSpellId(skillLineId, index)
    return nil
end

function SpellExt.GetProfessionRecipeInfo(skillLineId, index)
    return nil
end

function SpellExt.GetProfessionRecipeRequirements(skillLineId, index)
    return "", 0, 0
end

function SpellExt.GetProfessionRecipeReagentCount(skillLineId, index)
    return 0
end

function SpellExt.GetProfessionRecipeReagentInfo(skillLineId, index, reagentIndex)
    return nil
end

function SpellExt.Duskhaven_GetUnitDisplayID(unit)
    return 0
end

-- Export to global Lua environment (_G)
_G.GetSpellDescription = SpellExt.GetSpellDescription
_G.GetSpellNameById = SpellExt.GetSpellNameById
_G.GetSpellIconById = SpellExt.GetSpellIconById
_G.SpellRequiresComboPoints = SpellExt.SpellRequiresComboPoints
_G.UnitCustomCastingData = SpellExt.UnitCustomCastingData
_G.PreloadProfessionItemCache = SpellExt.PreloadProfessionItemCache
_G.GetProfessionRecipeCount = SpellExt.GetProfessionRecipeCount
_G.GetProfessionRecipeSpellId = SpellExt.GetProfessionRecipeSpellId
_G.GetProfessionRecipeInfo = SpellExt.GetProfessionRecipeInfo
_G.GetProfessionRecipeRequirements = SpellExt.GetProfessionRecipeRequirements
_G.GetProfessionRecipeReagentCount = SpellExt.GetProfessionRecipeReagentCount
_G.GetProfessionRecipeReagentInfo = SpellExt.GetProfessionRecipeReagentInfo
_G.Duskhaven_GetUnitDisplayID = SpellExt.Duskhaven_GetUnitDisplayID

ModCore.LogInfo("Duskhaven Spell Extensions module registered.")
return SpellExt
