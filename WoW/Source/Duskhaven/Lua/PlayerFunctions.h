#pragma once

struct lua_State;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterPlayerFunctions();

        int Script_GetShapeshiftFormID(lua_State* L);
        int Script_GetActiveSpec(lua_State* L);
        int Script_SetActiveSpec(lua_State* L);
        int Script_GetMasteryRating(lua_State* L);
        int Script_SetMasteryRatings(lua_State* L);
        int Script_UpdateMasteryAmount(lua_State* L);
        int Script_UpdateChargeInfoForSpell(lua_State* L);
        int Script_GetChargeInfoForSpell(lua_State* L);
        int Script_GetCombatRatingMult(lua_State* L);
        int Script_GetCombatRatingScalar(lua_State* L);
        int Script_GetModCastSpeed(lua_State* L);
        int Script_GetWeaponAP(lua_State* L);
        int Script_GetPetSP(lua_State* L);
        int Script_GetSpeedPercent(lua_State* L);
        int Script_GetLeechPercent(lua_State* L);
        int Script_GetAvoidancePercent(lua_State* L);
    } // namespace Lua
} // namespace Duskhaven
