#include "WoW/Source/Duskhaven/Display/GxMonitor.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Os/Gui.h"
#include <windows.h>
#include <vector>
#include <cstdlib>

namespace Duskhaven
{
    namespace GxMonitor
    {
        namespace
        {
            int32_t s_selectedMonitor = 0;

            struct MonitorInfo
            {
                HMONITOR monitor = nullptr;
                RECT rect        = {};
                RECT workRect    = {};
            };

            BOOL CALLBACK EnumMonitorsProc(HMONITOR monitor, HDC, LPRECT, LPARAM param)
            {
                MONITORINFOEXA info = {};
                info.cbSize         = sizeof(info);
                if (!GetMonitorInfoA(monitor, &info))
                    return TRUE;

                auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(param);
                monitors->push_back({ monitor, info.rcMonitor, info.rcWork });
                return TRUE;
            }

            bool EnumerateMonitors(std::vector<MonitorInfo>& monitors)
            {
                monitors.clear();
                return EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsProc, reinterpret_cast<LPARAM>(&monitors)) != FALSE;
            }
        } // namespace

        void Initialize()
        {
            CVar* cvar = CVar::Lookup("gxMonitor");
            if (cvar && cvar->m_intValue > 0)
            {
                s_selectedMonitor = cvar->m_intValue;
                ApplySelectedMonitor();
            }
        }

        int32_t GetSelectedMonitor()
        {
            return s_selectedMonitor;
        }

        int32_t GetMonitorCount()
        {
            std::vector<MonitorInfo> monitors;
            if (!EnumerateMonitors(monitors))
                return 0;

            return static_cast<int32_t>(monitors.size());
        }

        void SetSelectedMonitor(int32_t monitor)
        {
            s_selectedMonitor = monitor;
        }

        void ApplySelectedMonitor()
        {
            if (s_selectedMonitor <= 0)
                return;

            CVar* gxWindow = CVar::Lookup("gxWindow");
            if (gxWindow && gxWindow->m_intValue == 0)
            {
                // Exclusive fullscreen handles monitor selection via adapter ordinal
                return;
            }

            HWND hwnd = static_cast<HWND>(OsGuiGetWindow(0));
            if (!hwnd || !IsWindow(hwnd))
            {
                hwnd = GetActiveWindow();
            }

            if (!hwnd || !IsWindow(hwnd))
                return;

            std::vector<MonitorInfo> monitors;
            if (!EnumerateMonitors(monitors) || monitors.empty())
                return;

            if (s_selectedMonitor > static_cast<int32_t>(monitors.size()))
                return;

            const MonitorInfo& target = monitors[static_cast<size_t>(s_selectedMonitor - 1)];

            CVar* gxMaximize = CVar::Lookup("gxMaximize");
            bool isBorderlessOrMaximized = (gxMaximize && gxMaximize->m_intValue == 1) || IsZoomed(hwnd);

            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;

            if (isBorderlessOrMaximized)
            {
                int width  = target.rect.right - target.rect.left;
                int height = target.rect.bottom - target.rect.top;
                SetWindowPos(hwnd, nullptr, target.rect.left, target.rect.top, width, height, flags | SWP_FRAMECHANGED);
                return;
            }

            RECT windowRect = {};
            if (!GetWindowRect(hwnd, &windowRect))
                return;

            int windowWidth  = windowRect.right - windowRect.left;
            int windowHeight = windowRect.bottom - windowRect.top;

            int targetWidth  = target.workRect.right - target.workRect.left;
            int targetHeight = target.workRect.bottom - target.workRect.top;

            if (windowWidth > targetWidth)
                windowWidth = targetWidth;
            if (windowHeight > targetHeight)
                windowHeight = targetHeight;

            int x = target.workRect.left + (targetWidth - windowWidth) / 2;
            int y = target.workRect.top + (targetHeight - windowHeight) / 2;

            SetWindowPos(hwnd, nullptr, x, y, windowWidth, windowHeight, flags);
        }

        int Script_Duskhaven_GetMonitorCount(lua_State* L)
        {
            ClientLua::PushNumber(L, static_cast<double>(GetMonitorCount()));
            return 1;
        }

        int Script_Duskhaven_GetCurrentMonitorSelection(lua_State* L)
        {
            ClientLua::PushNumber(L, static_cast<double>(GetSelectedMonitor()));
            return 1;
        }

        void RegisterLuaFunctions()
        {
            ClientLua::AddFunction("Duskhaven_GetMonitorCount", Script_Duskhaven_GetMonitorCount);
            ClientLua::AddFunction("Duskhaven_GetCurrentMonitorSelection", Script_Duskhaven_GetCurrentMonitorSelection);
        }
    } // namespace GxMonitor
} // namespace Duskhaven
