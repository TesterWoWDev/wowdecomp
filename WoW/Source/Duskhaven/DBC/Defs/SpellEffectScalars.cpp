#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffectScalars.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    uint64_t ScalarKey(uint32_t spellId, uint32_t effectIdx)
    {
        return (static_cast<uint64_t>(spellId) << 32) | effectIdx;
    }

    uint64_t ScalarKey(SpellEffectScalarsRow const& row)
    {
        return ScalarKey(static_cast<uint32_t>(row.spellID), static_cast<uint32_t>(row.effectIdx));
    }

    CDBCTable<SpellEffectScalarsRow, uint64_t> s_table("SpellEffectScalars", ScalarKey);
} // namespace

SpellEffectScalarsRow* SpellEffectScalarsDB::Get(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

SpellEffectScalarsRow* SpellEffectScalarsDB::Find(uint32_t spellId, uint32_t effectIdx)
{
    return s_table.Get(ScalarKey(spellId, effectIdx));
}

uint32_t SpellEffectScalarsDB::GetNumRows()
{
    return s_table.GetNumRows();
}

int SpellEffectScalarsDB::LuaGet(lua_State* L, int id)
{
    SpellEffectScalarsRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->spellID);
    ClientLua::PushNumber(L, r->effectIdx);
    ClientLua::PushNumber(L, r->sp);
    ClientLua::PushNumber(L, r->ap);
    ClientLua::PushNumber(L, r->bv);
    ClientLua::PushNumber(L, r->mode);
    return 6;
}

REGISTER_CDBC_TABLE(SpellEffectScalars, s_table, SpellEffectScalarsDB::LuaGet);
