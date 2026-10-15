#pragma once
#include <cstdint>

struct lua_State;

namespace Duskhaven
{
    namespace GxMonitor
    {
        void Initialize();
        void RegisterLuaFunctions();

        int32_t GetSelectedMonitor();
        int32_t GetMonitorCount();
        void SetSelectedMonitor(int32_t monitor);
        void ApplySelectedMonitor();

        int Script_Duskhaven_GetMonitorCount(lua_State* L);
        int Script_Duskhaven_GetCurrentMonitorSelection(lua_State* L);
    } // namespace GxMonitor
} // namespace Duskhaven
