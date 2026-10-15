#pragma once

struct lua_State;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterHousingFunctions();

        int Script_GetMouseWorldPosition(lua_State* L);
        int Script_GetLastMouseoverGUID(lua_State* L);
        int Script_ClearSelectedGob(lua_State* L);
        int Script_SelectEditorGobByMouse(lua_State* L);
        int Script_GetSelectedGobGUID(lua_State* L);
        int Script_GetSelectedGobPosition(lua_State* L);
        int Script_SnapSelectedGobToGround(lua_State* L);
        int Script_GetSelectedGobRotation(lua_State* L);
        int Script_SetGobPositionByGUID(lua_State* L);
        int Script_SetGobRotationByGUID(lua_State* L);
        int Script_LogMouseoverGobValues(lua_State* L);
    } // namespace Lua
} // namespace Duskhaven
