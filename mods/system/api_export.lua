-- ==============================================================================
-- wowdecomp API Auto-Export Subsystem (for AI & IDE Tools)
-- ==============================================================================

local ApiExport = {}
ApiExport.Classes = {}
ApiExport.Functions = {}
ApiExport.Hooks = {}
ApiExport.Opcodes = {}

function ApiExport.RegisterClass(className, doc, fields, methods)
    ApiExport.Classes[className] = {
        name = className,
        doc = doc or "",
        fields = fields or {},
        methods = methods or {}
    }
end

function ApiExport.RegisterFunction(funcName, doc, params, returnType)
    ApiExport.Functions[funcName] = {
        name = funcName,
        doc = doc or "",
        params = params or {},
        returnType = returnType or "any"
    }
end

function ApiExport.RegisterHook(hookName, doc, signature)
    ApiExport.Hooks[hookName] = {
        name = hookName,
        doc = doc or "",
        signature = signature or "fun(...): any?"
    }
end

function ApiExport.RegisterOpcode(name, opcode, direction, schema)
    ApiExport.Opcodes[name] = {
        name = name,
        opcode = opcode,
        direction = direction,
        schema = schema or {}
    }
end

-- Generates the EmmyLua / LuaLS .d.lua file string
function ApiExport.GenerateEmmyLua()
    local lines = {}
    table.insert(lines, "---@meta wowdecomp")
    table.insert(lines, "-- Auto-generated API definitions for wowdecomp Modding Framework")
    table.insert(lines, "")

    -- Classes
    for _, cls in pairs(ApiExport.Classes) do
        table.insert(lines, string.format("---@class %s", cls.name))
        for _, f in ipairs(cls.fields) do
            table.insert(lines, string.format("---@field %s %s %s", f.name, f.type or "any", f.doc or ""))
        end
        table.insert(lines, string.format("local %s = {}", cls.name))
        table.insert(lines, "")

        for _, m in ipairs(cls.methods) do
            if m.doc and m.doc ~= "" then
                table.insert(lines, string.format("---%s", m.doc))
            end
            for _, p in ipairs(m.params or {}) do
                table.insert(lines, string.format("---@param %s %s", p.name, p.type or "any"))
            end
            if m.returnType then
                table.insert(lines, string.format("---@return %s", m.returnType))
            end
            local paramNames = {}
            for _, p in ipairs(m.params or {}) do
                table.insert(paramNames, p.name)
            end
            table.insert(lines, string.format("function %s:%s(%s) end", cls.name, m.name, table.concat(paramNames, ", ")))
            table.insert(lines, "")
        end
    end

    -- Global Functions
    for _, fn in pairs(ApiExport.Functions) do
        if fn.doc and fn.doc ~= "" then
            table.insert(lines, string.format("---%s", fn.doc))
        end
        for _, p in ipairs(fn.params or {}) do
            table.insert(lines, string.format("---@param %s %s", p.name, p.type or "any"))
        end
        if fn.returnType then
            table.insert(lines, string.format("---@return %s", fn.returnType))
        end
        local paramNames = {}
        for _, p in ipairs(fn.params or {}) do
            table.insert(paramNames, p.name)
        end
        table.insert(lines, string.format("function %s(%s) end", fn.name, table.concat(paramNames, ", ")))
        table.insert(lines, "")
    end

    return table.concat(lines, "\n")
end

-- Pre-register baseline engine types
ApiExport.RegisterClass("C3Vector", "3D coordinate vector in World space", {
    { name = "x", type = "number", doc = "X coordinate" },
    { name = "y", type = "number", doc = "Y coordinate" },
    { name = "z", type = "number", doc = "Z coordinate" }
})

ApiExport.RegisterClass("WOWGUID", "64-bit entity identifier in World of Warcraft", {
    { name = "low", type = "integer", doc = "Low 32 bits" },
    { name = "high", type = "integer", doc = "High 32 bits" }
}, {
    { name = "IsValid", doc = "Returns true if GUID is non-zero", returnType = "boolean" }
})

ApiExport.RegisterHook("OnEngineInit", "Fired when client engine completes initialization", "fun()")
ApiExport.RegisterHook("OnWorldRender", "Fired each frame during 3D world rendering pass", "fun(worldFrame: any)")
ApiExport.RegisterHook("OnWorldUpdate", "Fired every game tick with delta time", "fun(dt: number)")
ApiExport.RegisterHook("OnPacketReceived", "Fired when custom network packet arrives", "fun(opcode: integer, time: integer, reader: any): boolean?")
ApiExport.RegisterHook("OnSpellQuery", "Fired when spell power/range/cast time is queried", "fun(queryType: string, spellId: integer, ...): any?")

return ApiExport
