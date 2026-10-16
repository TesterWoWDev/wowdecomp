-- ==============================================================================
-- Duskhaven Core Mod Entry Point
-- ==============================================================================

local DuskhavenCore = {}

function DuskhavenCore.Init(modContext)
    ModCore.LogInfo("Initializing Duskhaven Core Mod...")

    -- Register core lifecycle listener
    ModCore.Hooks.Register("OnEngineInit", function()
        ModCore.LogInfo("Duskhaven Core: Engine Initialized.")
    end, 100, "Core initialization hook for Duskhaven")

    -- Load sub-modules
    local ok, spells = pcall(require, "spells.local_spells")
    if not ok then
        ModCore.LogWarn("Could not load spells.local_spells: %s", tostring(spells))
    end

    ModCore.Hooks.Register("OnWorldUpdate", function(dt)
        -- Hook point for visual pumps, combat animations, danger zone ticks
    end, 100)

    ModCore.LogInfo("Duskhaven Core Mod loaded successfully.")
end

DuskhavenCore.Init(...)
return DuskhavenCore
