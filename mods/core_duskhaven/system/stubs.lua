-- ==============================================================================
-- Duskhaven System, Camera, Discord & Graphics Stubs
-- ==============================================================================

local Stubs = {}

local DBC = require("system.dbc")

-- CDBC Delegation
function Stubs._CDBC_GET(tableName, id)
    return DBC.GetRowUnpacked(tableName, id)
end

function Stubs.GetCDBCRow(tableName, id)
    return DBC.GetRowUnpacked(tableName, id)
end

-- Discord Stubs
function Stubs.DiscordSetGamePresence(...)
    return 0
end

function Stubs.DiscordClearGamePresence(...)
    return 0
end

-- Streaming Stubs
function Stubs.Duskhaven_GetContentStreamingProgress(...)
    return 0
end

function Stubs.Duskhaven_AcknowledgeContentStreamingInterfaceUpdate(...)
    return 0
end

function Stubs.Duskhaven_CancelContentStreaming(...)
    return 0
end

function Stubs.GetMPQHashResults(...)
    return ""
end

-- Display & Monitor
function Stubs.Duskhaven_GetMonitorCount()
    return 1
end

function Stubs.Duskhaven_GetCurrentMonitorSelection()
    return 0
end

-- World Coordinate Transformations
function Stubs.ConvertCoordsToScreenSpace(x, y, z)
    return nil, nil, nil
end

function Stubs.GetActivePlayerWorldPosition()
    return nil, nil, nil
end

function Stubs.GetDistanceFromActivePlayerToWorldPosition(x, y, z)
    return nil
end

function Stubs.TranslateToMapCoords(continentID, x, y, z)
    return 0.0, 0.0
end

-- Graphics / Engine Debugging Toggles
local function PrintMessage(msg)
    if DEFAULT_CHAT_FRAME and DEFAULT_CHAT_FRAME.AddMessage then
        DEFAULT_CHAT_FRAME:AddMessage(msg)
    else
        print(msg)
    end
end

function Stubs.ReloadMap()
    PrintMessage("ReloadMap: Requested map reload.")
end

function Stubs.ToggleDisplayNormals()
    PrintMessage("ToggleDisplayNormals toggled.")
end

function Stubs.ToggleGroundEffects()
    PrintMessage("ToggleGroundEffects toggled.")
end

function Stubs.ToggleLiquids()
    PrintMessage("ToggleLiquids toggled.")
end

function Stubs.ToggleM2()
    PrintMessage("ToggleM2 toggled.")
end

function Stubs.ToggleTerrain()
    PrintMessage("ToggleTerrain toggled.")
end

function Stubs.ToggleTerrainCulling()
    PrintMessage("ToggleTerrainCulling toggled.")
end

function Stubs.ToggleWireframeMode()
    PrintMessage("ToggleWireframeMode toggled.")
end

function Stubs.ToggleWMO()
    PrintMessage("ToggleWMO toggled.")
end

-- Creature & Model Display
function Stubs.Duskhaven_GetCreatureDisplayInfo(displayId)
    return nil
end

function Stubs.Duskhaven_GetModelPath(modelFrame)
    return nil
end

function Stubs.Duskhaven_SetModelDisplay(modelFrame, displayId)
    return { Applied = false }
end

-- Export to global Lua environment (_G)
_G._CDBC_GET = Stubs._CDBC_GET
_G.GetCDBCRow = Stubs.GetCDBCRow
_G.DiscordSetGamePresence = Stubs.DiscordSetGamePresence
_G.DiscordClearGamePresence = Stubs.DiscordClearGamePresence
_G.Duskhaven_GetContentStreamingProgress = _G.Duskhaven_GetContentStreamingProgress or Stubs.Duskhaven_GetContentStreamingProgress
_G.Duskhaven_AcknowledgeContentStreamingInterfaceUpdate = _G.Duskhaven_AcknowledgeContentStreamingInterfaceUpdate or Stubs.Duskhaven_AcknowledgeContentStreamingInterfaceUpdate
_G.Duskhaven_CancelContentStreaming = _G.Duskhaven_CancelContentStreaming or Stubs.Duskhaven_CancelContentStreaming
_G.GetMPQHashResults = _G.GetMPQHashResults or Stubs.GetMPQHashResults
_G.Duskhaven_GetMonitorCount = _G.Duskhaven_GetMonitorCount or Stubs.Duskhaven_GetMonitorCount
_G.Duskhaven_GetCurrentMonitorSelection = _G.Duskhaven_GetCurrentMonitorSelection or Stubs.Duskhaven_GetCurrentMonitorSelection
_G.ConvertCoordsToScreenSpace = _G.ConvertCoordsToScreenSpace or Stubs.ConvertCoordsToScreenSpace
_G.GetActivePlayerWorldPosition = _G.GetActivePlayerWorldPosition or Stubs.GetActivePlayerWorldPosition
_G.GetDistanceFromActivePlayerToWorldPosition = _G.GetDistanceFromActivePlayerToWorldPosition or Stubs.GetDistanceFromActivePlayerToWorldPosition
_G.TranslateToMapCoords = _G.TranslateToMapCoords or Stubs.TranslateToMapCoords
_G.ReloadMap = _G.ReloadMap or Stubs.ReloadMap
_G.ToggleDisplayNormals = _G.ToggleDisplayNormals or Stubs.ToggleDisplayNormals
_G.ToggleGroundEffects = _G.ToggleGroundEffects or Stubs.ToggleGroundEffects
_G.ToggleLiquids = _G.ToggleLiquids or Stubs.ToggleLiquids
_G.ToggleM2 = _G.ToggleM2 or Stubs.ToggleM2
_G.ToggleTerrain = _G.ToggleTerrain or Stubs.ToggleTerrain
_G.ToggleTerrainCulling = _G.ToggleTerrainCulling or Stubs.ToggleTerrainCulling
_G.ToggleWireframeMode = _G.ToggleWireframeMode or Stubs.ToggleWireframeMode
_G.ToggleWMO = _G.ToggleWMO or Stubs.ToggleWMO
_G.Duskhaven_GetCreatureDisplayInfo = _G.Duskhaven_GetCreatureDisplayInfo or Stubs.Duskhaven_GetCreatureDisplayInfo
_G.Duskhaven_GetModelPath = _G.Duskhaven_GetModelPath or Stubs.Duskhaven_GetModelPath
_G.Duskhaven_SetModelDisplay = _G.Duskhaven_SetModelDisplay or Stubs.Duskhaven_SetModelDisplay

ModCore.LogInfo("Duskhaven System & World Stubs module registered.")
return Stubs
