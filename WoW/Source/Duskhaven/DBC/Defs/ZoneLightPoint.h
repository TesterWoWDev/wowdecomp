#pragma once
#include <cstdint>
struct lua_State;

struct ZoneLightPointRow
{
    int32_t ID;
    int32_t zoneLightID;
    float positionX;
    float positionY;
    int32_t pointOrder;
};

namespace ZoneLightPointDB
{
    ZoneLightPointRow* Get(uint32_t id);
    uint32_t GetNumRows();
    ZoneLightPointRow* GetRowByIndex(uint32_t idx);
    int LuaGet(lua_State* L, int id);
} // namespace ZoneLightPointDB
