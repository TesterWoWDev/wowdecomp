#pragma once
#include <cstdint>
struct lua_State;

struct SpellAdditionalAttributesRow
{
    int spellID;
    uint32_t customAttr0;
    uint32_t customAttr1;
    uint32_t customAttr2;
    uint32_t customAttr3;
};

namespace SpellAdditionalAttributesDB
{
    SpellAdditionalAttributesRow* Get(uint32_t id);
    int LuaGet(lua_State* L, int id);
} // namespace SpellAdditionalAttributesDB

namespace Duskhaven
{
    namespace SpellAdditionalAttributesDB
    {
        using ::SpellAdditionalAttributesDB::Get;
        using ::SpellAdditionalAttributesDB::LuaGet;
    }
}
