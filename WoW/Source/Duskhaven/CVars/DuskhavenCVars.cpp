#include "WoW/Source/Duskhaven/CVars/DuskhavenCVars.h"
#include "WoW/Source/Duskhaven/Display/GxMonitor.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include <algorithm>
#include <cstdlib>

namespace Duskhaven
{
    namespace CVars
    {
        CVar* s_bugReportHideHints              = nullptr;
        CVar* s_bugReportHideButton             = nullptr;
        CVar* s_modelFadeAlpha                  = nullptr;
        CVar* s_spellMinClipDistancePercentage  = nullptr;
        CVar* s_duskAnim                        = nullptr;
        CVar* s_ccAnim                          = nullptr;
        CVar* s_gxMonitor                       = nullptr;
        CVar* s_showInGameNavigation            = nullptr;
        CVar* s_questMarkerOpacity              = nullptr;

        float g_modelFadeAlpha                  = 0.1f;
        float g_spellMinClipDistancePercentage  = 0.0f;

        static bool ModelFadeAlphaCallback(CVar*, const char*, const char* value, void*)
        {
            if (!value) return false;
            float alpha = static_cast<float>(std::atof(value));
            if (alpha < 0.0f || alpha > 1.0f)
                return false;
            g_modelFadeAlpha = alpha;
            return true;
        }

        static bool SpellMinClipCallback(CVar*, const char*, const char* value, void*)
        {
            if (!value) return false;
            float clip = static_cast<float>(std::atof(value));
            if (clip < 0.0f || clip > 1.0f)
                return false;
            g_spellMinClipDistancePercentage = clip;
            return true;
        }

        static bool GxMonitorCallback(CVar*, const char*, const char* value, void*)
        {
            if (!value) return false;
            int32_t monitor = std::atoi(value);
            if (monitor < 0 || monitor > 16) return false;
            Duskhaven::GxMonitor::SetSelectedMonitor(monitor);
            Duskhaven::GxMonitor::ApplySelectedMonitor();
            return true;
        }

        void Initialize()
        {
            s_bugReportHideHints = CVar::Register("bugReportHideHints", "Hides the bug report hint in tooltips and the quest log", 1, "0", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
            s_bugReportHideButton = CVar::Register("bugReportHideButton", "Hides the floating Issue Reporter button", 1, "0", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
            s_modelFadeAlpha = CVar::Register("modelFadeAlpha", "Alpha value for objects in camera LOS", 1, "0.1", ModelFadeAlphaCallback, CATEGORY_GRAPHICS, false, nullptr, false);
            s_spellMinClipDistancePercentage = CVar::Register("spellMinClipDistancePercentage", "Sets the minimum distance the clipping needs to be to activate", 1, "0.0", SpellMinClipCallback, CATEGORY_DEFAULT, false, nullptr, false);
            s_duskAnim = CVar::Register("DuskAnim", "Show or hide the character-create animation debug window", 1, "0", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
            s_ccAnim = CVar::Register("ccanim", "Play a character-create animation ID", 1, "-1", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
            s_gxMonitor = CVar::Register("gxMonitor", "Selects the monitor used by the graphics device.", 1, "0", GxMonitorCallback, CATEGORY_GRAPHICS, false, nullptr, false);
            s_showInGameNavigation = CVar::Register("showInGameNavigation", "Shows in-game quest navigation markers", 1, "1", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
            s_questMarkerOpacity = CVar::Register("questMarkerOpacity", "Sets the opacity of in-game quest marker textures", 1, "0.9", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
        }
    } // namespace CVars
} // namespace Duskhaven
