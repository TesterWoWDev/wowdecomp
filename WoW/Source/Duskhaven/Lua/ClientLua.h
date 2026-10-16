#pragma once

#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include <cstdint>
#include <string>

namespace ClientLua
{
    inline int GetTop(lua_State* L)
    {
        return lua_gettop(L);
    }

    inline void SetTop(lua_State* L, int idx)
    {
        lua_settop(L, idx);
    }

    inline void PushNil(lua_State* L)
    {
        lua_pushnil(L);
    }

    inline void PushNumber(lua_State* L, double n)
    {
        lua_pushnumber(L, n);
    }

    inline void PushInteger(lua_State* L, int n)
    {
        lua_pushinteger(L, n);
    }

    inline void PushString(lua_State* L, const char* s)
    {
        lua_pushstring(L, s ? s : "");
    }

    inline void PushBoolean(lua_State* L, int b)
    {
        lua_pushboolean(L, b);
    }

    inline void PushValue(lua_State* L, int idx)
    {
        lua_pushvalue(L, idx);
    }

    inline double ToNumber(lua_State* L, int idx)
    {
        return lua_tonumber(L, idx);
    }

    inline int ToInteger(lua_State* L, int idx)
    {
        return lua_tointeger(L, idx);
    }

    inline int ToBoolean(lua_State* L, int idx)
    {
        return lua_toboolean(L, idx);
    }

    inline const char* ToString(lua_State* L, int idx)
    {
        return lua_tostring(L, idx);
    }

    inline const char* ToLString(lua_State* L, int idx, size_t* len)
    {
        return lua_tolstring(L, idx, len);
    }

    inline void* ToUserdata(lua_State* L, int idx)
    {
        return lua_touserdata(L, idx);
    }

    inline int Type(lua_State* L, int idx)
    {
        return lua_type(L, idx);
    }

    inline int IsNumber(lua_State* L, int idx)
    {
        return lua_isnumber(L, idx);
    }

    inline int IsString(lua_State* L, int idx)
    {
        return lua_isstring(L, idx);
    }

    inline int IsBoolean(lua_State* L, int idx)
    {
        return lua_isboolean(L, idx);
    }

    inline int IsTable(lua_State* L, int idx)
    {
        return lua_istable(L, idx);
    }

    inline int IsNil(lua_State* L, int idx)
    {
        return lua_isnil(L, idx);
    }

    inline double GetNumber(lua_State* L, int idx, double def = 0.0)
    {
        return lua_isnumber(L, idx) ? lua_tonumber(L, idx) : def;
    }

    inline std::string GetString(lua_State* L, int idx, const std::string& def = "")
    {
        const char* s = lua_tostring(L, idx);
        return s ? std::string(s) : def;
    }

    inline bool GetBoolean(lua_State* L, int idx, bool def = false)
    {
        return lua_isboolean(L, idx) ? (lua_toboolean(L, idx) != 0) : def;
    }

    inline void CreateTable(lua_State* L, int narr = 0, int nrec = 0)
    {
        lua_createtable(L, narr, nrec);
    }

    inline void SetField(lua_State* L, int idx, const char* k)
    {
        lua_setfield(L, idx, k);
    }

    inline void GetField(lua_State* L, int idx, const char* k)
    {
        lua_getfield(L, idx, k);
    }

    inline void RawSet(lua_State* L, int idx)
    {
        lua_rawset(L, idx);
    }

    inline void RawGet(lua_State* L, int idx)
    {
        lua_rawget(L, idx);
    }

    inline void RawSetI(lua_State* L, int idx, int n)
    {
        lua_rawseti(L, idx, n);
    }

    inline void RawGetI(lua_State* L, int idx, int n)
    {
        lua_rawgeti(L, idx, n);
    }

    inline int Next(lua_State* L, int idx)
    {
        return lua_next(L, idx);
    }

    inline int PCall(lua_State* L, int nargs, int nresults, int errfunc)
    {
        return lua_pcall(L, nargs, nresults, errfunc);
    }

    inline void AddFunction(const char* name, lua_CFunction fn)
    {
        FrameScript_RegisterFunction(name, fn);
    }

    inline void RegisterFunction(const char* name, lua_CFunction fn)
    {
        FrameScript_RegisterFunction(name, fn);
    }

    inline int DisplayError(lua_State* L, const char* msg)
    {
        return luaL_error(L, "%s", msg ? msg : "");
    }

    inline lua_State* State()
    {
        return FrameScript_GetContext();
    }

    inline bool DoString(const char* str, const char* chunkName = nullptr)
    {
        FrameScript_Execute(str, chunkName ? chunkName : "ClientLua", nullptr);
        return true;
    }
}
