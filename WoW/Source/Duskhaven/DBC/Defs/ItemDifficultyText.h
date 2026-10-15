#pragma once
#include <cstdint>
struct lua_State;

struct ItemDifficultyTextRow
{
    int itemID;
    char* text;
    float red;
    float green;
    float blue;
};

namespace ItemDifficultyTextDB
{
    ItemDifficultyTextRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
} // namespace ItemDifficultyTextDB
