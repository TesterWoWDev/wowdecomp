-- ==============================================================================
-- Duskhaven Item & Shoulder Visual Overrides (Pure Lua)
-- ==============================================================================

local ItemOverrides = {}

-- Global storage tables for live game and character select
_G.VisibleItemOverrides = _G.VisibleItemOverrides or {}
_G.CharSelectShoulderOverrides = _G.CharSelectShoulderOverrides or {}
_G.CharSelectVisibleItems = _G.CharSelectVisibleItems or {}

local currentContextGuid = 0

function ItemOverrides.SetCurrentContextGuid(guid)
    currentContextGuid = guid or 0
end

function ItemOverrides.GetCurrentContextGuid()
    return currentContextGuid
end

function ItemOverrides.GetOverrideItem(guid, slot)
    guid = guid or currentContextGuid
    slot = tonumber(slot) or 0
    if not guid or guid == 0 then return nil end

    local characterOverrides = _G.VisibleItemOverrides[guid]
    if not characterOverrides then return nil end

    return characterOverrides[slot]
end

function ItemOverrides.SetOverrideItem(guid, slot, entryId, permEnchant, tempEnchant, transmog, leftShoulder, rightShoulder, flags)
    if not guid or guid == 0 then return end
    slot = tonumber(slot) or 0

    _G.VisibleItemOverrides[guid] = _G.VisibleItemOverrides[guid] or {}

    if not entryId or entryId == 0 then
        _G.VisibleItemOverrides[guid][slot] = nil
    else
        _G.VisibleItemOverrides[guid][slot] = {
            entryId       = tonumber(entryId) or 0,
            permEnchant   = tonumber(permEnchant) or 0,
            tempEnchant   = tonumber(tempEnchant) or 0,
            transmog      = tonumber(transmog) or 0,
            leftShoulder  = tonumber(leftShoulder) or 0,
            rightShoulder = tonumber(rightShoulder) or 0,
            flags         = tonumber(flags) or 0
        }
    end

    if ModCore and ModCore.Hooks then
        ModCore.Hooks.Fire("OnVisibleItemOverrideUpdated", guid, slot, _G.VisibleItemOverrides[guid][slot])
    end
end

function ItemOverrides.ClearOverrideItem(guid, slot)
    if not guid then return end
    if slot then
        if _G.VisibleItemOverrides[guid] then
            _G.VisibleItemOverrides[guid][slot] = nil
        end
    else
        _G.VisibleItemOverrides[guid] = nil
    end
end

function ItemOverrides.GetShoulderOverride(guid)
    guid = guid or currentContextGuid
    if not guid or guid == 0 then return nil, nil end

    -- Check character select shoulder overrides first
    if _G.CharSelectShoulderOverrides[guid] then
        local ov = _G.CharSelectShoulderOverrides[guid]
        return ov.left or 0, ov.right or 0
    end

    -- Check live character shoulder slot (Slot 2 = Shoulder in WoW inventory)
    local charOverrides = _G.VisibleItemOverrides[guid]
    if charOverrides and charOverrides[2] then
        local ov = charOverrides[2]
        return ov.leftShoulder or 0, ov.rightShoulder or 0
    end

    return nil, nil
end

function ItemOverrides.GetShoulderOverrideForCurrentContext()
    return ItemOverrides.GetShoulderOverride(currentContextGuid)
end

function ItemOverrides.GetCharSelectShoulderOverride(guid)
    if not guid then return nil, nil end
    local ov = _G.CharSelectShoulderOverrides[guid]
    if ov then
        return ov.left or 0, ov.right or 0
    end
    return nil, nil
end

function ItemOverrides.GetCharSelectVisibleItem(guid, slot)
    if not guid then return nil end
    slot = tonumber(slot) or 0
    local items = _G.CharSelectVisibleItems[guid]
    return items and items[slot] or nil
end

function ItemOverrides.HasOverride(guid, slot)
    return ItemOverrides.GetOverrideItem(guid, slot) ~= nil
end

function ItemOverrides.GetTransmogDisplay(guid, slot)
    local ov = ItemOverrides.GetOverrideItem(guid, slot)
    if ov and ov.transmog and ov.transmog > 0 then
        return ov.transmog
    end
    return nil
end

-- Export global functions for UI & FrameXML scripts
_G.GetVisibleItemOverride             = ItemOverrides.GetOverrideItem
_G.SetVisibleItemOverride             = ItemOverrides.SetOverrideItem
_G.GetShoulderOverride                = ItemOverrides.GetShoulderOverride
_G.GetShoulderOverrideForCurrentContext = ItemOverrides.GetShoulderOverrideForCurrentContext
_G.GetCharSelectShoulderOverride      = ItemOverrides.GetCharSelectShoulderOverride
_G.GetCharSelectVisibleItem           = ItemOverrides.GetCharSelectVisibleItem
_G.GetTransmogDisplay                 = ItemOverrides.GetTransmogDisplay

ModCore.LogInfo("Duskhaven Item Overrides module registered.")
return ItemOverrides
