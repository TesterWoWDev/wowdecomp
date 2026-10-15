#pragma once

struct lua_State;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterUnitFunctions();
        int Script_Duskhaven_GetUnitDisplayID(lua_State* L);
    } // namespace Lua
} // namespace Duskhaven
