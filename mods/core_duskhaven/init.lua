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

    -- 1. Configuration Subsystem
    local okConfig, dhConfig = pcall(require, "config.dh_config")
    if not okConfig then
        ModCore.LogWarn("Could not load config.dh_config: %s", tostring(dhConfig))
    end

    -- 2. CDBC Subsystem
    local okDBC, dbcModule = pcall(require, "dbc.init")
    if not okDBC then
        ModCore.LogWarn("Could not load dbc.init: %s", tostring(dbcModule))
    end

    -- 3. Player Subsystems (Charges, Specs, Mastery, Stats, Item Overrides)
    local okCharges, charges = pcall(require, "player.charges")
    if not okCharges then
        ModCore.LogWarn("Could not load player.charges: %s", tostring(charges))
    end

    local okSpecs, specs = pcall(require, "player.specs")
    if not okSpecs then
        ModCore.LogWarn("Could not load player.specs: %s", tostring(specs))
    end

    local okStats, stats = pcall(require, "player.stats")
    if not okStats then
        ModCore.LogWarn("Could not load player.stats: %s", tostring(stats))
    end

    local okItemOv, itemOv = pcall(require, "player.item_overrides")
    if not okItemOv then
        ModCore.LogWarn("Could not load player.item_overrides: %s", tostring(itemOv))
    end

    -- 3. UI & Frame Extensions
    local okUIExt, uiExt = pcall(require, "ui.extensions")
    if not okUIExt then
        ModCore.LogWarn("Could not load ui.extensions: %s", tostring(uiExt))
    end

    local okFont, fontConfig = pcall(require, "ui.font_config")
    if not okFont then
        ModCore.LogWarn("Could not load ui.font_config: %s", tostring(fontConfig))
    end

    local okCharCreate, charCreate = pcall(require, "ui.char_create")
    if not okCharCreate then
        ModCore.LogWarn("Could not load ui.char_create: %s", tostring(charCreate))
    end

    -- 4. Spell & Profession Subsystems
    local okSpellExt, spellExt = pcall(require, "spells.extensions")
    if not okSpellExt then
        ModCore.LogWarn("Could not load spells.extensions: %s", tostring(spellExt))
    end

    local okSpells, spells = pcall(require, "spells.local_spells")
    if not okSpells then
        ModCore.LogWarn("Could not load spells.local_spells: %s", tostring(spells))
    end

    -- 5. System, Network & World Stubs
    local okStubs, stubs = pcall(require, "system.stubs")
    if not okStubs then
        ModCore.LogWarn("Could not load system.stubs: %s", tostring(stubs))
    end

    local okPackets, packets = pcall(require, "packets.custom_opcodes")
    if not okPackets then
        ModCore.LogWarn("Could not load packets.custom_opcodes: %s", tostring(packets))
    end

    local okHousing, housing = pcall(require, "housing.placement")
    if not okHousing then
        ModCore.LogWarn("Could not load housing.placement: %s", tostring(housing))
    end

    local okTooltip, tooltip = pcall(require, "tooltips.evaluator")
    if not okTooltip then
        ModCore.LogWarn("Could not load tooltips.evaluator: %s", tostring(tooltip))
    end

    local okStream, streamConfig = pcall(require, "streaming.config")
    if not okStream then
        ModCore.LogWarn("Could not load streaming.config: %s", tostring(streamConfig))
    end

    ModCore.Hooks.Register("OnWorldUpdate", function(dt)
        -- Hook point for visual pumps, combat animations, danger zone ticks
    end, 100)

    ModCore.LogInfo("Duskhaven Core Mod loaded successfully.")
end

DuskhavenCore.Init(...)
return DuskhavenCore
