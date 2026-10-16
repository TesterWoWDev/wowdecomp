-- ==============================================================================
-- Duskhaven Client Configuration System (DHConfig)
-- ==============================================================================

local DHConfig = {}

local values = {}
local defaults = {
    allowDiscord = "1",
    terrainReplacementEnabled = "0"
}

local configPath = "WTF/DHConfig"

local function SerializeTable(val)
    if type(val) ~= "table" then
        if type(val) == "string" then
            return string.format("%q", val)
        else
            return tostring(val)
        end
    end

    local parts = {}
    for k, v in pairs(val) do
        local kStr
        if type(k) == "string" then
            kStr = string.format("[%q]", k)
        elseif type(k) == "number" then
            kStr = string.format("[%d]", k)
        end

        if kStr then
            local vStr = SerializeTable(v)
            table.insert(parts, kStr .. "=" .. vStr)
        end
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

local function LoadFile()
    if not io or not io.open then return end
    local f = io.open(configPath, "r")
    if not f then return end

    for line in f:lines() do
        local key, val = string.match(line, "^([^=]+)=(.*)$")
        if key then
            values[key] = val
        end
    end
    f:close()
end

local function SaveFile()
    if not io or not io.open then return end
    local f = io.open(configPath, "w")
    if not f then return end

    for k, v in pairs(values) do
        f:write(tostring(k) .. "=" .. tostring(v) .. "\n")
    end
    f:close()
end

pcall(LoadFile)

function DHConfig.ReadString(key, def)
    key = tostring(key or "")
    if values[key] ~= nil then
        return values[key]
    end
    if defaults[key] ~= nil then
        return defaults[key]
    end
    return tostring(def or "")
end

function DHConfig.ReadInt(key, def)
    local s = DHConfig.ReadString(key, "")
    local num = tonumber(s)
    if num ~= nil then
        return math.floor(num)
    end
    return tonumber(def) or 0
end

function DHConfig.ReadFloat(key, def)
    local s = DHConfig.ReadString(key, "")
    local num = tonumber(s)
    if num ~= nil then
        return num
    end
    return tonumber(def) or 0.0
end

function DHConfig.Write(key, val)
    key = tostring(key or "")
    val = tostring(val or "")
    values[key] = val
    pcall(SaveFile)
end

function DHConfig.WriteTable(key, tbl)
    if type(tbl) ~= "table" then return end
    local serialized = SerializeTable(tbl)
    DHConfig.Write(key, serialized)
end

function DHConfig.ReadTable(key, defTbl)
    local s = DHConfig.ReadString(key, "")
    if s == "" then
        return defTbl or {}
    end

    local chunk = "return " .. s
    local loader = loadstring and loadstring(chunk) or load(chunk)
    if loader then
        local ok, res = pcall(loader)
        if ok and type(res) == "table" then
            return res
        end
    end
    return defTbl or {}
end

function DHConfig.AddDefault(key, def)
    defaults[tostring(key or "")] = tostring(def or "")
end

function DHConfig.WriteDefaults()
    local changed = false
    for k, v in pairs(defaults) do
        if values[k] == nil then
            values[k] = v
            changed = true
        end
    end
    if changed then
        pcall(SaveFile)
    end
end

function DHConfig.Has(key)
    key = tostring(key or "")
    return (values[key] ~= nil or defaults[key] ~= nil)
end

-- Export to global Lua environment (_G)
_G.DHConfigReadString = DHConfig.ReadString
_G.DHConfigRead = DHConfig.ReadString
_G.DHConfigReadInt = DHConfig.ReadInt
_G.DHConfigReadFloat = DHConfig.ReadFloat
_G.DHConfigWrite = DHConfig.Write
_G.DHConfigWriteTable = DHConfig.WriteTable
_G.DHConfigReadTable = DHConfig.ReadTable
_G.DHConfigAddDefault = DHConfig.AddDefault
_G.DHConfigWriteDefaults = DHConfig.WriteDefaults
_G.DHConfigHas = DHConfig.Has

ModCore.LogInfo("Duskhaven DHConfig module registered.")
return DHConfig
