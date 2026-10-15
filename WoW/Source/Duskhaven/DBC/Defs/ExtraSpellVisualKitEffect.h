#pragma once

#include <cstdint>

struct lua_State;

struct ExtraSpellVisualKitEffectRow
{
    uint32_t ID;
    uint32_t ParentSpellVisualKitID;
    uint32_t EffectType;
    uint32_t Effect;
};

namespace ExtraSpellVisualKitEffectDB
{
    ExtraSpellVisualKitEffectRow* Get(uint32_t id);
    uint32_t GetNumRows();
    ExtraSpellVisualKitEffectRow* GetRowByIndex(uint32_t index);
    int LuaGet(lua_State* L, int id);
} // namespace ExtraSpellVisualKitEffectDB
