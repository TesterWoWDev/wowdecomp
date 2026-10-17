-- ==============================================================================
-- Duskhaven Declarative Custom Network Packet Schemas
-- ==============================================================================

local Net = require("system.net")

-- Housing: Object Placement Request (Client -> Server)
Net.DefinePacket("CMSG_HOUSING_PLACE_OBJECT", {
    opcode = 0x7001,
    direction = "CLIENT_TO_SERVER",
    schema = {
        { name = "entryId",   type = "uint32" },
        { name = "position",  type = "vector3" },
        { name = "rotation",  type = "quat" },
        { name = "scale",     type = "float" }
    }
})

-- Housing: Object State Synchronization (Server -> Client)
Net.DefinePacket("SMSG_HOUSING_OBJECT_UPDATE", {
    opcode = 0x7002,
    direction = "SERVER_TO_CLIENT",
    schema = {
        { name = "guid",      type = "guid" },
        { name = "position",  type = "vector3" },
        { name = "rotation",  type = "quat" }
    },
    handler = function(packet)
        ModCore.LogDebug("Received SMSG_HOUSING_OBJECT_UPDATE for entity %08X:%08X", packet.guid.high, packet.guid.low)
    end
})

-- Boss Encounter: Danger Zone Telegraph Broadcast (Server -> Client)
Net.DefinePacket("SMSG_SPAWN_DANGER_ZONE", {
    opcode = 0x7101,
    direction = "SERVER_TO_CLIENT",
    schema = {
        { name = "shapeType", type = "uint8" },
        { name = "position",  type = "vector3" },
        { name = "radius",    type = "float" },
        { name = "duration",  type = "float" }
    },
    handler = function(packet)
        local DangerZones = require("spells.danger_zones")
        if DangerZones then
            DangerZones.SpawnForSpell(packet.shapeType, packet.position)
        end
    end
})

-- ==============================================================================
-- Item Visual & Shoulder Overrides (Server -> Client)
-- ==============================================================================

if OnCustomPacket then
    local ItemOverrides = require("player.item_overrides")

    -- 0x7A10: SMSG_SET_VISIBLE_ITEM_OVERRIDE
    OnCustomPacket(0x7A10, function(reader)
        local guid = reader:ReadUInt64()
        local slot = reader:ReadUInt8()
        local hasItem = (reader:ReadUInt8() ~= 0)

        if not hasItem then
            ItemOverrides.ClearOverrideItem(guid, slot)
        else
            local entryId = reader:ReadUInt32()
            local permEnchant = reader:ReadUInt16()
            local tempEnchant = reader:ReadUInt16()
            local transmog = reader:ReadUInt32()
            local leftShoulder = reader:ReadInt32()
            local rightShoulder = reader:ReadInt32()
            local flags = reader:ReadUInt32()

            ItemOverrides.SetOverrideItem(guid, slot, entryId, permEnchant, tempEnchant, transmog, leftShoulder, rightShoulder, flags)
        end
    end)

    -- 0x7A11: SMSG_CHAR_SELECT_SHOULDER_OVERRIDE
    OnCustomPacket(0x7A11, function(reader)
        local count = reader:ReadUInt8()
        for i = 1, count do
            local guid = reader:ReadUInt64()
            local left = reader:ReadInt32()
            local right = reader:ReadInt32()
            _G.CharSelectShoulderOverrides[guid] = { left = left, right = right }
        end
    end)

    -- 0x7A13: SMSG_CHAR_SELECT_VISIBLE_ITEMS
    OnCustomPacket(0x7A13, function(reader)
        local characterCount = reader:ReadUInt8()
        for i = 1, characterCount do
            local guid = reader:ReadUInt64()
            local slotCount = reader:ReadUInt8()
            _G.CharSelectVisibleItems[guid] = _G.CharSelectVisibleItems[guid] or {}
            for s = 1, slotCount do
                local slot = reader:ReadUInt8()
                local visibleEntry = reader:ReadUInt32()
                local displayId = reader:ReadUInt32()
                local transmogEntry = reader:ReadUInt32()
                local leftShoulder = reader:ReadInt32()
                local rightShoulder = reader:ReadInt32()
                local flags = reader:ReadUInt32()

                _G.CharSelectVisibleItems[guid][slot] = {
                    visibleEntry = visibleEntry,
                    displayId = displayId,
                    transmogEntry = transmogEntry,
                    leftShoulder = leftShoulder,
                    rightShoulder = rightShoulder,
                    flags = flags
                }

                if slot == 2 then
                    _G.CharSelectShoulderOverrides[guid] = { left = leftShoulder, right = rightShoulder }
                end
            end
        end
    end)
end

ModCore.LogInfo("Duskhaven custom packet schemas registered.")
