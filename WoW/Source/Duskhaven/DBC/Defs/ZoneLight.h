#pragma once
#include <cstdint>
struct lua_State;

struct ZoneLightRow
{
    int32_t ID;
    char* name;
    int32_t mapID;
    int32_t lightID;
};

namespace ZoneLightDB
{
    ZoneLightRow* Get(uint32_t id);
    uint32_t GetNumRows();
    ZoneLightRow* GetRowByIndex(uint32_t idx);
    int LuaGet(lua_State* L, int id);
} // namespace ZoneLightDB
