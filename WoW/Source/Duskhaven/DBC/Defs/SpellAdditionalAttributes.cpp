#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalAttributes.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<SpellAdditionalAttributesRow> s_table(
        "SpellAdditionalAttributes",
        [](SpellAdditionalAttributesRow const& row) { return static_cast<uint32_t>(row.spellID); });
} // namespace

SpellAdditionalAttributesRow* SpellAdditionalAttributesDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

int SpellAdditionalAttributesDB::LuaGet(lua_State* L, int id)
{
    SpellAdditionalAttributesRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->spellID);
    ClientLua::PushNumber(L, r->customAttr0);
    ClientLua::PushNumber(L, r->customAttr1);
    ClientLua::PushNumber(L, r->customAttr2);
    ClientLua::PushNumber(L, r->customAttr3);
    return 5;
}

REGISTER_CDBC_TABLE(SpellAdditionalAttributes, s_table, SpellAdditionalAttributesDB::LuaGet);
