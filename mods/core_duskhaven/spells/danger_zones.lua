-- ==============================================================================
-- Duskhaven Boss Encounter Danger Zones & Telegraphs
-- ==============================================================================

local Visuals = require("system.visuals")
local DBC = require("system.dbc")

local DangerZones = {}

-- Pre-defined boss danger zone configurations
local DANGER_ZONE_SPELLS = {
    [98101] = { shape = "CIRCLE", radius = 8.0, duration = 3.5, color = { r = 1.0, g = 0.1, b = 0.1, a = 0.6 } },
    [98102] = { shape = "CONE", radius = 15.0, coneAngle = 60.0, duration = 2.5, color = { r = 1.0, g = 0.5, b = 0.1, a = 0.6 } },
    [98103] = { shape = "DONUT", radius = 20.0, innerRadius = 8.0, duration = 4.0, color = { r = 0.6, g = 0.1, b = 1.0, a = 0.6 } },
    [98104] = { shape = "RECTANGLE", radius = 30.0, innerRadius = 4.0, duration = 3.0, color = { r = 0.2, g = 0.8, b = 1.0, a = 0.6 } }
}

-- Triggered when spell go / start or danger zone packet is received
function DangerZones.SpawnForSpell(spellId, position)
    local config = DANGER_ZONE_SPELLS[spellId]
    if not config then
        -- Check if there is a DangerZoneVisualProfile DBC entry
        local profile = DBC.GetRow("DangerZoneVisualProfile", spellId)
        if profile then
            config = {
                shape = (profile.shapeMask == 1) and "CIRCLE" or (profile.shapeMask == 2 and "CONE" or "DONUT"),
                radius = profile.modelRadius > 0 and profile.modelRadius or 6.0,
                duration = profile.maxDurationMs > 0 and (profile.maxDurationMs / 1000.0) or 3.0,
                color = { r = profile.red or 1.0, g = profile.green or 0.2, b = profile.blue or 0.2, a = profile.alpha or 0.6 }
            }
        end
    end

    if config and position then
        local decal = Visuals.CreateDecal({
            shape = config.shape,
            position = position,
            radius = config.radius,
            innerRadius = config.innerRadius or 0,
            coneAngle = config.coneAngle or 90.0,
            duration = config.duration,
            color = config.color
        })
        ModCore.LogDebug("DangerZones: Spawned %s telegraph for spell %d (Radius: %.1f)", config.shape, spellId, config.radius)
        return decal
    end

    return nil
end

return DangerZones
