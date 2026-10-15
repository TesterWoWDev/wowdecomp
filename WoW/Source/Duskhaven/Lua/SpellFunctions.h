#pragma once

struct lua_State;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterSpellFunctions();

        int Script_GetSpellDescription(lua_State* L);
        int Script_GetSpellNameById(lua_State* L);
        int Script_GetSpellIconById(lua_State* L);
        int Script_SpellRequiresComboPoints(lua_State* L);
        int Script_UnitCustomCastingData(lua_State* L);
        int Script_PreloadProfessionItemCache(lua_State* L);
        int Script_GetProfessionRecipeCount(lua_State* L);
        int Script_GetProfessionRecipeSpellId(lua_State* L);
        int Script_GetProfessionRecipeInfo(lua_State* L);
        int Script_GetProfessionRecipeRequirements(lua_State* L);
        int Script_GetProfessionRecipeReagentCount(lua_State* L);
        int Script_GetProfessionRecipeReagentInfo(lua_State* L);
    } // namespace Lua
} // namespace Duskhaven
