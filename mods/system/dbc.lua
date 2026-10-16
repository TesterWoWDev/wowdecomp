-- ==============================================================================
-- wowdecomp DBC Virtualization & Transformation Engine
-- ==============================================================================

local DBC = {}
local virtualRows = {}
local rowOverrides = {}
local lookupHooks = {}

-- Retrieve a DBC row by table name and ID (checks virtual/overrides first, falls back to native)
function DBC.GetRow(tableName, id)
    if not tableName or not id then return nil end

    -- 1. Check custom lookup hooks
    if lookupHooks[tableName] then
        for _, hook in ipairs(lookupHooks[tableName]) do
            local res = hook(id)
            if res ~= nil then return res end
        end
    end

    -- 2. Check virtual rows added by mods
    if virtualRows[tableName] and virtualRows[tableName][id] then
        return virtualRows[tableName][id]
    end

    -- 3. Query native client DBC
    local nativeRow = nil
    if GetCDBCRow then
        nativeRow = GetCDBCRow(tableName, id)
    elseif _CDBC_GET then
        nativeRow = _CDBC_GET(tableName, id)
    end

    -- 4. Apply row overrides if any
    if nativeRow and rowOverrides[tableName] and rowOverrides[tableName][id] then
        for k, v in pairs(rowOverrides[tableName][id]) do
            nativeRow[k] = v
        end
    end

    return nativeRow
end

-- Inject a new virtual row into a DBC table
function DBC.RegisterVirtualRow(tableName, id, rowData)
    if not virtualRows[tableName] then
        virtualRows[tableName] = {}
    end
    rowData.id = id
    virtualRows[tableName][id] = rowData
    ModCore.LogDebug("DBC: Registered virtual row in %s [ID: %d]", tableName, id)
end

-- Override specific fields on an existing DBC row
function DBC.OverrideRow(tableName, id, fieldOverrides)
    if not rowOverrides[tableName] then
        rowOverrides[tableName] = {}
    end
    rowOverrides[tableName][id] = fieldOverrides
end

-- Hook dynamic row lookups
function DBC.OnRowLookup(tableName, callback)
    if not lookupHooks[tableName] then
        lookupHooks[tableName] = {}
    end
    table.insert(lookupHooks[tableName], callback)
end

-- Hook into ModCore API registration
if ModCore and ModCore.ApiExport then
    ModCore.ApiExport.RegisterClass("DbcEngine", "Virtual DBC tables and row injection", {}, {
        { name = "GetRow", params = {{name="tableName", type="string"}, {name="id", type="integer"}}, returnType = "table?", doc = "Retrieves a DBC row by table and ID" },
        { name = "RegisterVirtualRow", params = {{name="tableName", type="string"}, {name="id", type="integer"}, {name="rowData", type="table"}}, doc = "Injects a virtual row into client memory" },
        { name = "OverrideRow", params = {{name="tableName", type="string"}, {name="id", type="integer"}, {name="fieldOverrides", type="table"}}, doc = "Overrides specific fields on a DBC row" }
    })
end

return DBC
