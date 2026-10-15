#include "WoW/Source/Duskhaven/Lua/EmbeddedScripts.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>

namespace Duskhaven
{
    namespace Lua
    {
        static int Script_LuaLog(lua_State* L)
        {
            int level = static_cast<int>(lua_tonumber(L, 1));
            const char* msg = lua_tolstring(L, 2, nullptr);
            if (!msg) return 0;

            const char* prefix = "[LUA]";
            switch (level)
            {
                case 0: prefix = "[LUA DEBUG]"; break;
                case 1: prefix = "[LUA INFO]"; break;
                case 2: prefix = "[LUA WARN]"; break;
                case 3: prefix = "[LUA ERROR]"; break;
                default: break;
            }
            printf("%s %s\n", prefix, msg);
            return 0;
        }

        static const char* s_loggerLua = R"lua(
local function serializeTable(val, name, skipnewlines, depth)
    skipnewlines = skipnewlines or false
    depth = depth or 0
    local tmp = string.rep(" ", depth)
    if name then
        tmp = tmp .. name .. " = "
    end
    if type(val) == "table" then
        tmp = tmp .. "{" .. (not skipnewlines and "\n" or "")
        for k, v in pairs(val) do
            tmp = tmp .. serializeTable(v, k, skipnewlines, depth + 1) .. "," .. (not skipnewlines and "\n" or "")
        end
        tmp = tmp .. string.rep(" ", depth) .. "}"
    elseif type(val) == "number" then
        tmp = tmp .. tostring(val)
    elseif type(val) == "string" then
        tmp = tmp .. string.format("%q", val)
    elseif type(val) == "boolean" then
        tmp = tmp .. (val and "true" or "false")
    else
        tmp = tmp .. "\"[inserializeable datatype:" .. type(val) .. "]\""
    end
    return tmp
end

local function stringify(...)
    local arg = {...}
    local str = ""
    for i, v in ipairs(arg) do
        if type(v) == "string" then
            str = str .. v
        elseif type(v) == "number" or type(v) == "boolean" then
            str = str .. tostring(v)
        elseif type(v) == "table" then
            str = str .. serializeTable(v, nil, true)
        else
            str = str .. type(v)
        end
    end
    return str
end

function LOG_DEBUG(...)
    _LUA_LOG(0, stringify(select(1, ...)))
end

function LOG_INFO(...)
    _LUA_LOG(1, stringify(select(1, ...)))
end

function LOG_WARN(...)
    _LUA_LOG(2, stringify(select(1, ...)))
end

function LOG_ERROR(...)
    _LUA_LOG(3, stringify(select(1, ...)))
end
)lua";

        static const char* s_clientNetworkLua = R"lua(
local LuaNetworkOpcode = {
    ["WRITE_SIZE"]          = 0,
    ["READ_SIZE"]           = 1,
    ["WRITE_UINT8"]         = 2,
    ["WRITE_INT8"]          = 3,
    ["WRITE_UINT16"]        = 4,
    ["WRITE_INT16"]         = 5,
    ["WRITE_UINT32"]        = 6,
    ["WRITE_INT32"]         = 7,
    ["WRITE_UINT64"]        = 8,
    ["WRITE_INT64"]         = 9,
    ["WRITE_FLOAT"]         = 10,
    ["WRITE_DOUBLE"]        = 11,
    ["WRITE_STRING"]        = 12,

    ["READ_UINT8"]          = 13,
    ["READ_INT8"]           = 14,
    ["READ_UINT16"]         = 15,
    ["READ_INT16"]          = 16,
    ["READ_UINT32"]         = 17,
    ["READ_INT32"]          = 18,
    ["READ_UINT64"]         = 19,
    ["READ_INT64"]          = 20,
    ["READ_FLOAT"]          = 21,
    ["READ_DOUBLE"]         = 22,
    ["READ_STRING"]         = 23,
    ["MAKE_CUSTOM_PACKET"]  = 24,
    ["SEND_CUSTOM_PACKET"]  = 25,
    ["RESET_CUSTOM_PACKET"] = 26,
};

function CreateCustomPacket(opcode, size)
    local writer = { id = _CLIENT_NETWORK(LuaNetworkOpcode.MAKE_CUSTOM_PACKET, opcode, size) }

    function writer:WriteUInt8(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_UINT8, self.id, value); return self end
    function writer:WriteInt8(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_INT8, self.id, value); return self end
    function writer:WriteUInt16(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_UINT16, self.id, value); return self end
    function writer:WriteInt16(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_INT16, self.id, value); return self end
    function writer:WriteUInt32(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_UINT32, self.id, value); return self end
    function writer:WriteInt32(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_INT32, self.id, value); return self end
    function writer:WriteUInt64(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_UINT64, self.id, value); return self end
    function writer:WriteInt64(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_INT64, self.id, value); return self end
    function writer:WriteFloat(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_FLOAT, self.id, value); return self end
    function writer:WriteDouble(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_DOUBLE, self.id, value); return self end
    function writer:WriteString(value) _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_STRING, self.id, value); return self end

    function writer:Size() return _CLIENT_NETWORK(LuaNetworkOpcode.WRITE_SIZE, self.id) end

    function writer:Send()
        _CLIENT_NETWORK(LuaNetworkOpcode.SEND_CUSTOM_PACKET, self.id)
        return self
    end

    return writer
end

function __ReadCustomPacket()
    local reader = {}
    function reader:ReadUInt8() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_UINT8) end
    function reader:ReadInt8() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_INT8) end
    function reader:ReadUInt16() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_UINT16) end
    function reader:ReadInt16() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_INT16) end
    function reader:ReadUInt32() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_UINT32) end
    function reader:ReadInt32() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_INT32) end
    function reader:ReadUInt64() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_UINT32) end
    function reader:ReadInt64() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_INT32) end
    function reader:ReadFloat() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_FLOAT) end
    function reader:ReadDouble() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_DOUBLE) end
    function reader:ReadString() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_STRING) end
    function reader:Size() return _CLIENT_NETWORK(LuaNetworkOpcode.READ_SIZE) end
    return reader
end

__callbacks = {}
function OnCustomPacket(opcode, cb)
    if __callbacks[opcode] == nil then
        __callbacks[opcode] = {}
    end
    table.insert(__callbacks[opcode], cb)
end

function __FireCustomPacket(opcode)
    if __callbacks[opcode] == nil then return end
    for _, v in pairs(__callbacks[opcode]) do
        v(__ReadCustomPacket())
        _CLIENT_NETWORK(LuaNetworkOpcode.RESET_CUSTOM_PACKET)
    end
end
)lua";

        void RegisterEmbeddedScripts()
        {
            FrameScript_RegisterFunction("_LUA_LOG", Script_LuaLog);
            FrameScript_Execute(s_loggerLua, "Logger.lua", nullptr);
            FrameScript_Execute(s_clientNetworkLua, "ClientNetwork.lua", nullptr);
        }
    } // namespace Lua
} // namespace Duskhaven
