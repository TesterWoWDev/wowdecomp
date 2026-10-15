#include "WoW/Source/Duskhaven/DBC/Defs/ExtraSpellVisualKitEffect.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    CDBCTable<ExtraSpellVisualKitEffectRow> s_table(
        "ExtraSpellVisualKitEffect",
        [](ExtraSpellVisualKitEffectRow const& row) { return row.ID; });
} // namespace

ExtraSpellVisualKitEffectRow* ExtraSpellVisualKitEffectDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

uint32_t ExtraSpellVisualKitEffectDB::GetNumRows()
{
    return s_table.GetNumRows();
}

ExtraSpellVisualKitEffectRow* ExtraSpellVisualKitEffectDB::GetRowByIndex(uint32_t index)
{
    return s_table.GetRowByIndex(index);
}

int ExtraSpellVisualKitEffectDB::LuaGet(lua_State* L, int id)
{
    ExtraSpellVisualKitEffectRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->ID);
    ClientLua::PushNumber(L, r->ParentSpellVisualKitID);
    ClientLua::PushNumber(L, r->EffectType);
    ClientLua::PushNumber(L, r->Effect);
    return 4;
}

REGISTER_CDBC_TABLE(ExtraSpellVisualKitEffect, s_table, ExtraSpellVisualKitEffectDB::LuaGet);
