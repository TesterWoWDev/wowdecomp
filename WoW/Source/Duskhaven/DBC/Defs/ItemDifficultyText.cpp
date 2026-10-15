#include "WoW/Source/Duskhaven/DBC/Defs/ItemDifficultyText.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBCTable.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace
{
    void UnpackItemDifficultyText(ItemDifficultyTextRow& row, const uint32_t* cols, const char* str)
    {
        row.itemID = static_cast<int>(cols[0]);
        row.text = const_cast<char*>(str ? (str + cols[1]) : "");
        row.red = *reinterpret_cast<const float*>(&cols[2]);
        row.green = *reinterpret_cast<const float*>(&cols[3]);
        row.blue = *reinterpret_cast<const float*>(&cols[4]);
    }

    CDBCTable<ItemDifficultyTextRow> s_table(
        "ItemDifficultyText",
        [](ItemDifficultyTextRow const& row) { return static_cast<uint32_t>(row.itemID); },
        UnpackItemDifficultyText);
} // namespace

ItemDifficultyTextRow* ItemDifficultyTextDB::Get(uint32_t id)
{
    return s_table.Get(id);
}

int ItemDifficultyTextDB::LuaGet(lua_State* L, int id)
{
    ItemDifficultyTextRow* r = Get(static_cast<uint32_t>(id));
    if (!r)
        return 0;

    ClientLua::PushNumber(L, r->itemID);
    ClientLua::PushString(L, r->text);
    ClientLua::PushNumber(L, r->red);
    ClientLua::PushNumber(L, r->green);
    ClientLua::PushNumber(L, r->blue);
    return 5;
}

REGISTER_CDBC_TABLE(ItemDifficultyText, s_table, ItemDifficultyTextDB::LuaGet);
