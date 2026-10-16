-- ==============================================================================
-- Duskhaven UI & Action Bar Extensions
-- ==============================================================================

local UIExt = {}

function UIExt.FireActionBarSlotUpdateEvent(slotId)
    slotId = tonumber(slotId)
    if slotId and slotId < 144 then
        if ActionButton_Update then
            -- Trigger UI action button refresh
        end
        if ModCore and ModCore.Hooks then
            ModCore.Hooks.Fire("ACTIONBAR_SLOT_CHANGED", slotId + 1)
        end
    end
end

function UIExt.FireTalentUpdateEvent()
    if ModCore and ModCore.Hooks then
        ModCore.Hooks.Fire("PLAYER_TALENT_UPDATE")
    end
end

function UIExt.FlashGameWindow()
    -- Native window flash stub
end

function UIExt.FindSpellActionBarSlot(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return nil end

    local matches = {}
    if GetActionInfo then
        for i = 1, 144 do
            local actionType, id = GetActionInfo(i)
            if actionType == "spell" and id == spellId then
                table.insert(matches, i - 1)
            end
        end
    end

    if #matches == 0 then
        return nil
    end
    return unpack(matches)
end

function UIExt.ReplaceActionBarSpell(oldSpellId, newSpellId)
    oldSpellId = tonumber(oldSpellId)
    newSpellId = tonumber(newSpellId)
    if not oldSpellId or not newSpellId then return end

    if GetActionInfo and PickupSpell and PlaceAction and ClearCursor then
        for i = 1, 144 do
            local actionType, id = GetActionInfo(i)
            if actionType == "spell" and id == oldSpellId then
                PickupSpell(newSpellId)
                PlaceAction(i)
                ClearCursor()
            end
        end
    end
end

function UIExt.SetSpellInActionBarSlot(spellId, slotId)
    spellId = tonumber(spellId)
    slotId = tonumber(slotId)
    if spellId and slotId and slotId < 144 and PickupSpell and PlaceAction and ClearCursor then
        PickupSpell(spellId)
        PlaceAction(slotId + 1)
        ClearCursor()
    end
end

function UIExt.SetFrameChildClipping(frame, enabled)
    if type(frame) == "string" then
        frame = _G[frame]
    end
    if frame then
        if frame.SetClipsChildren then
            frame:SetClipsChildren(enabled and true or false)
        end
        frame._childClipping = (enabled and true or false)
    end
end

function UIExt.GetFrameChildClipping(frame)
    if type(frame) == "string" then
        frame = _G[frame]
    end
    if frame then
        if frame.GetClipsChildren then
            return frame:GetClipsChildren()
        end
        return frame._childClipping or false
    end
    return false
end

function UIExt.SetTextureWithMask(texture, source, mask)
    if type(texture) == "string" then
        texture = _G[texture]
    end
    if texture and texture.SetTexture and source then
        texture:SetTexture(source)
    end
end

function UIExt.SetPortraitToTexture(texture, source, mask)
    if type(texture) == "string" then
        texture = _G[texture]
    end
    if not texture then return end

    if SetPortraitTexture and type(source) == "string" and (source == "player" or source == "target" or source:find("^party%d$")) then
        SetPortraitTexture(texture, source)
    elseif texture.SetTexture and source then
        texture:SetTexture(source)
    end
end

function UIExt.EmitFrameEventByName(eventName, ...)
    if not eventName then return end
    if ModCore and ModCore.Hooks then
        ModCore.Hooks.Fire(eventName, ...)
    end
end

-- Export to global Lua environment (_G)
_G.FireActionBarSlotUpdateEvent = UIExt.FireActionBarSlotUpdateEvent
_G.FireTalentUpdateEvent = UIExt.FireTalentUpdateEvent
_G.FlashGameWindow = UIExt.FlashGameWindow
_G.FindSpellActionBarSlot = UIExt.FindSpellActionBarSlot
_G.ReplaceActionBarSpell = UIExt.ReplaceActionBarSpell
_G.SetSpellInActionBarSlot = UIExt.SetSpellInActionBarSlot
_G.SetFrameChildClipping = UIExt.SetFrameChildClipping
_G.GetFrameChildClipping = UIExt.GetFrameChildClipping
_G.SetTextureWithMask = UIExt.SetTextureWithMask
_G.SetPortraitToTexture = UIExt.SetPortraitToTexture
_G.EmitFrameEventByName = UIExt.EmitFrameEventByName

ModCore.LogInfo("Duskhaven UI Extensions module registered.")
return UIExt
