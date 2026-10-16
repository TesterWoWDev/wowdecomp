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

ModCore.LogInfo("Duskhaven custom packet schemas registered.")
