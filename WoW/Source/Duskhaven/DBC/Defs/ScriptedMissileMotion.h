#pragma once
#include <cstdint>
struct lua_State;

struct ScriptedMissileMotionRow
{
    int id;
    int motion;
    int count;
    int durationMs;
    int tickMs;
    int flags;
    float collisionRadius;
    float radius;
    float radiusVelocity;
    float startDistance;
    float height;
    float verticalVelocity;
    float forwardSpeed;
    float angularSpeed;
    float sineAmplitude;
    float sineFrequency;
    char* modelPath;
    int visualFlags;
    int attachMode;
    int attachPoint;
    float visualScale;
};

namespace ScriptedMissileMotionDB
{
    ScriptedMissileMotionRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
} // namespace ScriptedMissileMotionDB
