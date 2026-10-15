#pragma once

#include <cstdint>

struct lua_State;

struct WeaponTrailParamRow
{
    uint32_t ID;
    uint32_t Hand;
    float Duration;
    float FadeOutTime;
    float EdgeLifeSpan;
    float InitialDelay;
    float SmoothSampleAngle;
    uint32_t OverrideAttachTop;
    uint32_t OverrideAttachBot;
    uint32_t Flags;
    uint32_t WeaponTrailID;
};

namespace WeaponTrailParamDB
{
    WeaponTrailParamRow* Get(uint32_t id);
    uint32_t GetNumRows();
    WeaponTrailParamRow* GetRowByIndex(uint32_t index);
    int LuaGet(lua_State* L, int id);
} // namespace WeaponTrailParamDB
