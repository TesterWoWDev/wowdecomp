#pragma once

#include <cstdint>

struct CVar;

namespace Duskhaven
{
    namespace CVars
    {
        extern CVar* s_bugReportHideHints;
        extern CVar* s_bugReportHideButton;
        extern CVar* s_modelFadeAlpha;
        extern CVar* s_spellMinClipDistancePercentage;
        extern CVar* s_duskAnim;
        extern CVar* s_ccAnim;
        extern CVar* s_gxMonitor;
        extern CVar* s_showInGameNavigation;
        extern CVar* s_questMarkerOpacity;

        extern float g_modelFadeAlpha;
        extern float g_spellMinClipDistancePercentage;

        void Initialize();
    } // namespace CVars
} // namespace Duskhaven
