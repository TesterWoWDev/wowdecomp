-- ==============================================================================
-- Duskhaven Spell & Profession API Extensions
-- ==============================================================================

local SpellExt = {}

local tooltipScanner = nil
local function GetScanner()
    if tooltipScanner then return tooltipScanner end

    -- Check if GameTooltip or GlueTooltip already exists in the environment
    if _G["GameTooltip"] then
        tooltipScanner = _G["GameTooltip"]
        return tooltipScanner
    end
    if _G["GlueTooltip"] then
        tooltipScanner = _G["GlueTooltip"]
        return tooltipScanner
    end

    -- Attempt safe creation without template so GlueXML never fails on missing templates
    if CreateFrame then
        local ok, frame = pcall(CreateFrame, "GameTooltip", "DHSpellDescScanner")
        if ok and frame then
            tooltipScanner = frame
            return tooltipScanner
        end
    end

    return nil
end

function SpellExt.GetSpellDescription(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return "" end

    if _GetSpellDescription then
        local desc = _GetSpellDescription(spellId)
        if desc and desc ~= "" then
            return desc
        end
    end

    local scanner = GetScanner()
    if scanner and scanner.SetSpellByID and scanner.NumLines then
        local desc = nil
        pcall(function()
            if scanner.SetOwner then
                scanner:SetOwner(WorldFrame or UIParent or scanner, "ANCHOR_NONE")
            end
            if scanner.ClearLines then
                scanner:ClearLines()
            end
            scanner:SetSpellByID(spellId)
            local name = scanner.GetName and scanner:GetName()
            if name then
                local numLines = scanner:NumLines() or 0
                for i = 2, numLines do
                    local line = _G[name .. "TextLeft" .. i]
                    if line and line.GetText then
                        local text = line:GetText()
                        if text and text ~= "" and
                           not text:find("^Rank ") and
                           not text:find("yd range") and
                           not text:find("Cast") and
                           not text:find("Cooldown") and
                           not text:find("Requires") then
                            desc = text
                            break
                        end
                    end
                end
            end
        end)
        if desc then return desc end
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

    if _GetSpellNameById then
        local name, rank = _GetSpellNameById(spellId)
        if name then
            return name, rank
        end
    end

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

    if _GetSpellIconById then
        local icon = _GetSpellIconById(spellId)
        if icon then
            return icon
        end
    end

    if GetSpellInfo then
        local _, _, icon = GetSpellInfo(spellId)
        if icon then
            return icon
        end
    end
    return nil
end

function SpellExt.SpellRequiresComboPoints(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return false end

    if _SpellRequiresComboPoints then
        return _SpellRequiresComboPoints(spellId)
    end
    return false
end

function SpellExt.UnitCustomCastingData(unit)
    if not unit then return nil end

    if _UnitCustomCastingData then
        local spellId, hideCastbar, invertCastbar = _UnitCustomCastingData(unit)
        if spellId then
            return spellId, hideCastbar, invertCastbar
        end
    end

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
    if _PreloadProfessionItemCache then
        return _PreloadProfessionItemCache(tonumber(itemId) or 0)
    end
    return true
end

function SpellExt.GetProfessionRecipeCount(skillLineId)
    if _GetProfessionRecipeCount then
        return _GetProfessionRecipeCount(tonumber(skillLineId) or 0)
    end
    return 0
end

function SpellExt.GetProfessionRecipeSpellId(skillLineId, index)
    if _GetProfessionRecipeSpellId then
        return _GetProfessionRecipeSpellId(tonumber(skillLineId) or 0, tonumber(index) or 0)
    end
    return nil
end

function SpellExt.GetProfessionRecipeInfo(skillLineId, index)
    if _GetProfessionRecipeInfo then
        return _GetProfessionRecipeInfo(tonumber(skillLineId) or 0, tonumber(index) or 0)
    end
    return nil
end

function SpellExt.GetProfessionRecipeRequirements(skillLineId, index)
    if _GetProfessionRecipeRequirements then
        return _GetProfessionRecipeRequirements(tonumber(skillLineId) or 0, tonumber(index) or 0)
    end
    return "", 0, 0
end

function SpellExt.GetProfessionRecipeReagentCount(skillLineId, index)
    if _GetProfessionRecipeReagentCount then
        return _GetProfessionRecipeReagentCount(tonumber(skillLineId) or 0, tonumber(index) or 0)
    end
    return 0
end

function SpellExt.GetProfessionRecipeReagentInfo(skillLineId, index, reagentIndex)
    if _GetProfessionRecipeReagentInfo then
        return _GetProfessionRecipeReagentInfo(tonumber(skillLineId) or 0, tonumber(index) or 0, tonumber(reagentIndex) or 0)
    end
    return nil
end

function SpellExt.Duskhaven_GetUnitDisplayID(unit)
    if _Duskhaven_GetUnitDisplayID then
        return _Duskhaven_GetUnitDisplayID(unit)
    end
    if _GetUnitDisplayID then
        return _GetUnitDisplayID(unit)
    end
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
_G.GetUnitDisplayID = SpellExt.Duskhaven_GetUnitDisplayID

ModCore.LogInfo("Duskhaven Spell Extensions module registered.")
return SpellExt
