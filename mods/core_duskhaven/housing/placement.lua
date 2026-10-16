-- ==============================================================================
-- Duskhaven Housing Placement & Grid Alignment Logic
-- ==============================================================================

local Net = require("system.net")

local Housing = {}
Housing.GridSize = 0.5       -- Snapping resolution in world meters
Housing.AngleSnap = 15.0     -- Rotation snap in degrees

-- Returns the 3D world coordinate currently under the mouse cursor
function Housing.GetMouseWorldPosition()
    if GetMouseWorldPosition then
        local x, y, z = GetMouseWorldPosition()
        return { x = x or 0, y = y or 0, z = z or 0 }
    end
    return { x = 0, y = 0, z = 0 }
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
    if SelectEditorGobByMouse then
        return SelectEditorGobByMouse()
    end
    return false
end

-- Clear active object selection
function Housing.ClearSelection()
    if ClearSelectedGob then
        ClearSelectedGob()
    end
end

ModCore.LogInfo("Duskhaven Housing Placement module registered.")
return Housing
