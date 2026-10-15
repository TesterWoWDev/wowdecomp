#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<SpellEffectCacheRow> s_table(
        "SpellEffect",
        [](SpellEffectCacheRow const& row)
        {
            return static_cast<uint32_t>(SpellEffectCacheKey(row.spellID, row.effectIndex));
        });
} // namespace

SpellEffectCacheRow* SpellEffectCacheDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

void SpellEffectCacheDB::AddRow(uint32_t id, SpellEffectCacheRow const& row)
{
    s_table.AddRow(id, row);
}

void SpellEffectCacheDB::EraseRow(uint32_t id)
{
    s_table.EraseRow(id);
}

void SpellEffectCacheDB::ForEachRow(const std::function<void(SpellEffectCacheRow const*)>& callback)
{
    s_table.ForEachRow(callback);
}

int SpellEffectCacheDB::LuaGet(lua_State* L, int id)
{
    SpellEffectCacheRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->spellID);
    ClientLua::PushNumber(L, r->effectIndex);
    ClientLua::PushNumber(L, r->effect);
    ClientLua::PushNumber(L, r->effectApplyAuraName);
    ClientLua::PushNumber(L, r->effectBasePoints);
    return 5;
}

REGISTER_CDBC_TABLE(SpellEffect, s_table, SpellEffectCacheDB::LuaGet);
