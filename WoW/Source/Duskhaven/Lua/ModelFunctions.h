#pragma once

struct lua_State;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterModelFunctions();

        int Script_Duskhaven_GetCreatureDisplayInfo(lua_State* L);
        int Script_Duskhaven_GetModelPath(lua_State* L);
        int Script_Duskhaven_SetModelDisplay(lua_State* L);
    } // namespace Lua
} // namespace Duskhaven
