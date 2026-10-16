-- ==============================================================================
-- wowdecomp Modding Framework Bootstrap
-- ==============================================================================

ModCore = ModCore or {}
ModCore.Version = "1.0.0"
ModCore.LoadedMods = {}

local function Log(level, fmt, ...)
    local msg = string.format(fmt, ...)
    if _MODCORE_LOG then
        _MODCORE_LOG(level, msg)
    else
        print(string.format("[%s] [ModCore] %s", level, msg))
    end
end

ModCore.LogDebug = function(...) Log("DEBUG", ...) end
ModCore.LogInfo  = function(...) Log("INFO", ...) end
ModCore.LogWarn  = function(...) Log("WARN", ...) end
ModCore.LogError = function(...) Log("ERROR", ...) end

ModCore.LogInfo("Initializing wowdecomp Modding Framework v%s", ModCore.Version)

-- Setup package table and safe require() for FrameScript
local basePath = ModCore.BasePath or "mods/"

package = package or {}
package.loaded = package.loaded or {}
package.path = basePath .. "?.lua;" ..
               basePath .. "?/init.lua;" ..
               basePath .. "system/?.lua;" ..
               basePath .. "core_duskhaven/?.lua;" ..
               basePath .. "user_mods/?.lua;" ..
               (package.path or "")

if not require then
    function require(modname)
        if package.loaded[modname] ~= nil then
            return package.loaded[modname]
        end

        local rel = string.gsub(modname, "%.", "/")
        local tried = {}
        for pattern in string.gmatch(package.path, "[^;]+") do
            local filename = string.gsub(pattern, "%?", rel)
            local fn, err = loadfile(filename)
            if fn then
                local res = fn()
                if res == nil then res = true end
                package.loaded[modname] = res
                return res
            else
                table.insert(tried, string.format("no file '%s'", filename))
            end
        end

        error(string.format("module '%s' not found:\n\t%s", modname, table.concat(tried, "\n\t")), 2)
    end
end

-- Load Event & Hook Bus
ModCore.Hooks = require("system.hooks")

-- Load Declarative Network Engine
ModCore.Net = require("system.net")

-- Load Auto-Export Engine (used for AI schema & EmmyLua generator)
ModCore.ApiExport = require("system.api_export")

-- Function to mount an individual mod directory
function ModCore.MountMod(modDir, manifest)
    if not manifest or not manifest.id then
        ModCore.LogError("Cannot mount mod at %s: missing manifest.id", modDir)
        return false
    end

    if ModCore.LoadedMods[manifest.id] then
        ModCore.LogWarn("Mod %s already mounted, skipping duplicate", manifest.id)
        return false
    end

    ModCore.LogInfo("Mounting Mod: %s (%s) v%s by %s", manifest.name or manifest.id, manifest.id, manifest.version or "1.0", manifest.author or "Unknown")

    local modContext = {
        id = manifest.id,
        dir = modDir,
        manifest = manifest,
        state = {}
    }

    -- Execute entry point if present
    local entryPoint = manifest.entryPoint or manifest.entryPoints and manifest.entryPoints.onEngineInit or "init.lua"
    local entryFullPath = modDir .. "/" .. entryPoint
    local initFunc, err = loadfile(entryFullPath)
    if initFunc then
        local success, runErr = pcall(initFunc, modContext)
        if not success then
            ModCore.LogError("Error executing entrypoint %s: %s", entryFullPath, tostring(runErr))
        end
    else
        -- Check if it's optional
        if manifest.entryPoint then
            ModCore.LogError("Failed to load mod entrypoint %s: %s", entryFullPath, tostring(err))
        end
    end

    ModCore.LoadedMods[manifest.id] = modContext
    ModCore.Hooks.Fire("OnModMounted", manifest.id, modContext)
    return true
end

-- Lifecycle Hook: Called by C++ engine when all core subsystems are ready
function ModCore.OnEngineInit()
    ModCore.LogInfo("ModCore.OnEngineInit dispatched")
    ModCore.Hooks.Fire("OnEngineInit")
end

-- Lifecycle Hook: Called when world is rendered each frame
function ModCore.OnWorldRender(worldFrame)
    ModCore.Hooks.Fire("OnWorldRender", worldFrame)
end

-- Lifecycle Hook: Called during per-frame tick
function ModCore.OnWorldUpdate(dt)
    ModCore.Hooks.Fire("OnWorldUpdate", dt)
end

-- Lifecycle Hook: Called on packet reception
function ModCore.OnPacketReceived(opcode, time, packetReader)
    return ModCore.Hooks.Fire("OnPacketReceived", opcode, time, packetReader)
end

-- Lifecycle Hook: Called on spell query
function ModCore.OnSpellQuery(queryType, spellId, ...)
    return ModCore.Hooks.Fire("OnSpellQuery", queryType, spellId, ...)
end

ModCore.LogInfo("Bootstrap completed successfully.")
