#pragma once

#include <cstdint>

struct lua_State;

struct WeaponTrailRow
{
    uint32_t ID;
    char* FilePath;
    float Roll;
    float Pitch;
    float Yaw;
    char* TexturePath[3];
    float TextureScrollRateU[3];
    float TextureScrollRateV[3];
    float TextureScaleU[3];
    float TextureScaleV[3];
};

namespace WeaponTrailDB
{
    WeaponTrailRow* Get(uint32_t id);
    uint32_t GetNumRows();
    WeaponTrailRow* GetRowByIndex(uint32_t index);
    int LuaGet(lua_State* L, int id);
} // namespace WeaponTrailDB
