-- ==============================================================================
-- Duskhaven Housing Placement & Grid Alignment Logic
-- ==============================================================================

local Net = require("system.net")

local Housing = {}
Housing.GridSize = 0.5       -- Snapping resolution in world meters
Housing.AngleSnap = 15.0     -- Rotation snap in degrees

local lastHitGuid = 0
local lastHitPoint = { x = 0.0, y = 0.0, z = 0.0 }
local selectedGob = {
    guid = "0",
    pos = { x = 0.0, y = 0.0, z = 0.0 },
    rot = { x = 0.0, y = 0.0, z = 0.0, w = 1.0 }
}

-- Hook into ModCore world hit test
ModCore.OnWorldHitTest = function(guid, point, hitObject)
    lastHitGuid = guid or 0
    if type(point) == "table" then
        lastHitPoint.x = point.x or 0.0
        lastHitPoint.y = point.y or 0.0
        lastHitPoint.z = point.z or 0.0
    end
end

-- Export global Housing APIs
_G.GetMouseWorldPosition = _G.GetMouseWorldPosition or function()
    return lastHitPoint.x, lastHitPoint.y, lastHitPoint.z
end

_G.GetLastMouseoverGUID = _G.GetLastMouseoverGUID or function()
    local full = lastHitGuid
    local t = 0
    local entry = 0
    local low = 0
    if type(full) == "number" and full > 0 then
        t = math.floor(full / (2^48)) % (2^16)
        entry = math.floor(full / (2^24)) % (2^24)
        low = full % (2^24)
    end
    return tostring(full), entry, low, t
end

_G.ClearSelectedGob = _G.ClearSelectedGob or function()
    selectedGob.guid = "0"
end

_G.SelectEditorGobByMouse = _G.SelectEditorGobByMouse or function()
    if lastHitGuid == 0 then return nil end
    selectedGob.guid = tostring(lastHitGuid)
    return selectedGob.guid, lastHitPoint.x, lastHitPoint.y, lastHitPoint.z
end

_G.GetSelectedGobGUID = _G.GetSelectedGobGUID or function()
    return selectedGob.guid
end

_G.GetSelectedGobPosition = _G.GetSelectedGobPosition or function()
    return selectedGob.pos.x, selectedGob.pos.y, selectedGob.pos.z
end

_G.SnapSelectedGobToGround = _G.SnapSelectedGobToGround or function()
    return false
end

_G.GetSelectedGobRotation = _G.GetSelectedGobRotation or function()
    return selectedGob.rot.x, selectedGob.rot.y, selectedGob.rot.z, selectedGob.rot.w
end

_G.SetGobPositionByGUID = _G.SetGobPositionByGUID or function(guid, x, y, z)
    if tostring(guid) == selectedGob.guid then
        selectedGob.pos.x = tonumber(x) or 0.0
        selectedGob.pos.y = tonumber(y) or 0.0
        selectedGob.pos.z = tonumber(z) or 0.0
        return true
    end
    return false
end

_G.SetGobRotationByGUID = _G.SetGobRotationByGUID or function(guid, x, y, z, w)
    if tostring(guid) == selectedGob.guid then
        selectedGob.rot.x = tonumber(x) or 0.0
        selectedGob.rot.y = tonumber(y) or 0.0
        selectedGob.rot.z = tonumber(z) or 0.0
        selectedGob.rot.w = tonumber(w) or 1.0
        return true
    end
    return false
end

_G.LogMouseoverGobValues = _G.LogMouseoverGobValues or function()
    return 0
end

-- Returns the 3D world coordinate currently under the mouse cursor
function Housing.GetMouseWorldPosition()
    local x, y, z = _G.GetMouseWorldPosition()
    return { x = x or 0.0, y = y or 0.0, z = z or 0.0 }
end

-- Snaps a world vector to the current placement grid
function Housing.SnapToGrid(pos, gridSize)
    gridSize = gridSize or Housing.GridSize
    if gridSize <= 0 then return pos end

    return {
        x = math.floor((pos.x / gridSize) + 0.5) * gridSize,
        y = math.floor((pos.y / gridSize) + 0.5) * gridSize,
        z = math.floor((pos.z / gridSize) + 0.5) * gridSize
    }
end

-- Request the server to spawn or place a housing object
function Housing.PlaceObject(entryId, position, rotation, scale)
    position = position or Housing.GetMouseWorldPosition()
    rotation = rotation or { w = 1.0, x = 0.0, y = 0.0, z = 0.0 }
    scale = scale or 1.0

    ModCore.LogInfo("Housing: Requesting placement of object %d at (%.2f, %.2f, %.2f)", entryId, position.x, position.y, position.z)

    return Net.Send("CMSG_HOUSING_PLACE_OBJECT", {
        entryId = entryId,
        position = position,
        rotation = rotation,
        scale = scale
    })
end

-- Select the game object currently under mouse cursor
function Housing.SelectMouseoverObject()
    return _G.SelectEditorGobByMouse()
end

-- Clear active object selection
function Housing.ClearSelection()
    _G.ClearSelectedGob()
end

ModCore.LogInfo("Duskhaven Housing Placement module registered.")
return Housing
