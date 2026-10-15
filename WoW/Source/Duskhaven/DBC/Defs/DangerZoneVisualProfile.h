#pragma once
#include <cstdint>
struct lua_State;

struct DangerZoneVisualProfileRow
{
    int id;
    int schoolMask;
    int shapeMask;
    int flags;
    char* modelPath;
    float modelRadius;
    float radiusScale;
    float zOffset;
    int minDurationMs;
    int maxDurationMs;
    float red;
    float green;
    float blue;
    float alpha;
    int debugSpellId;
};

namespace DangerZoneVisualProfileDB
{
    DangerZoneVisualProfileRow* Get(uint32_t id);
    uint32_t GetNumRows();
    DangerZoneVisualProfileRow* GetRowByIndex(uint32_t idx);
    int LuaGet(lua_State* L, int id);
} // namespace DangerZoneVisualProfileDB
