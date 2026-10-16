#include "WoW/Source/Duskhaven/Highlight/NPCHighlight/NPCHighlight.h"
#include "WoW/Source/Duskhaven/Highlight/NPCHighlight/Outline.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include <windows.h>
#include <cstdlib>

namespace Duskhaven::Highlight::NPCHighlight
{
    namespace
    {
        constexpr float AttentionDurationMs = 650.0f;

        bool s_enabled           = true;
        bool s_debugEnabled      = false;
        CM2Model* s_hoveredModel = nullptr;
        float s_red              = 0.92f;
        float s_green            = 1.0f;
        float s_blue             = 0.48f;
        DWORD s_hoverStartedAt   = 0;

        void SetColor(CImVector color)
        {
            s_red   = static_cast<float>(color.r) / 255.0f;
            s_green = static_cast<float>(color.g) / 255.0f;
            s_blue  = static_cast<float>(color.b) / 255.0f;
        }

        void SetColorForUnit(CGUnit_C* unit)
        {
            if (!unit)
                return;

            CImVector color{};
            if (unit->GetSelectionHighlightColor(&color))
                SetColor(color);
        }

        bool NPCHighlightCVarCallback(CVar*, const char*, const char* value, void*)
        {
            s_enabled = !value || std::atoi(value) != 0;
            if (!s_enabled)
                s_hoveredModel = nullptr;
            return true;
        }

        bool NPCHighlightDebugCVarCallback(CVar*, const char*, const char* value, void*)
        {
            s_debugEnabled = value && std::atoi(value) != 0;
            return true;
        }
    } // namespace

    void TrackHoveredObject(CGObject_C* object)
    {
        if (!object || !(object->m_obj->m_type & TYPEMASK_UNIT))
        {
            s_hoveredModel = nullptr;
            return;
        }

        auto* unit       = static_cast<CGUnit_C*>(object);
        s_hoveredModel   = object->GetObjectModel();
        SetColorForUnit(unit);
        s_hoverStartedAt = GetTickCount();
    }

    void UntrackHoveredObject(CGObject_C* object)
    {
        if (object && object->GetObjectModel() == s_hoveredModel)
        {
            s_hoveredModel = nullptr;
        }
    }

    void Apply()
    {
        CVar::Register("npcHighlight", "Draws an outline around highlighted NPCs", 1, "1",
                       NPCHighlightCVarCallback, CATEGORY_GRAPHICS, false, nullptr, false);

        CVar::Register("npcHighlightDebug", "Logs NPC highlight model batches for diagnostics", 1, "0",
                       NPCHighlightDebugCVarCallback, CATEGORY_GRAPHICS, false, nullptr, false);

        Outline::Apply();
    }

    void OnWorldRender()
    {
        if (!IsEnabled())
            return;

        Outline::OnWorldRender();
    }

    bool IsEnabled()
    {
        return s_enabled;
    }

    bool IsDebugEnabled()
    {
        return s_debugEnabled;
    }

    CM2Model* GetHoveredModel()
    {
        if (!IsEnabled())
            return nullptr;

        return s_hoveredModel;
    }

    void GetOutlineColor(float& red, float& green, float& blue)
    {
        red   = s_red;
        green = s_green;
        blue  = s_blue;
    }

    float GetOutlineStrength()
    {
        const float elapsed   = static_cast<float>(GetTickCount() - s_hoverStartedAt);
        const float attention = elapsed < AttentionDurationMs ? 1.0f - elapsed / AttentionDurationMs : 0.0f;
        return 1.0f + attention * 0.55f;
    }
}
