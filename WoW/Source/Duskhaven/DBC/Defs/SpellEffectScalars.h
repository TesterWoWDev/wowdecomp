#pragma once
#include <cstdint>
struct lua_State;

struct SpellEffectScalarsRow
{
    int spellID;
    int effectIdx;
    float sp;
    float ap;
    float bv;
    int mode;
};

namespace SpellEffectScalarsDB
{
    SpellEffectScalarsRow* Get(uint32_t index);
    SpellEffectScalarsRow* Find(uint32_t spellId, uint32_t effectIdx);
    uint32_t GetNumRows();
    int LuaGet(lua_State* L, int id);
} // namespace SpellEffectScalarsDB
