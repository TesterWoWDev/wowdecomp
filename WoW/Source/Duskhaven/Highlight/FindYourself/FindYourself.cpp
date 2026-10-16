#include "WoW/Source/Duskhaven/Highlight/FindYourself/FindYourself.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/Circle.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/Outline.h"
#include "WoW/Source/Game/GameClient/PlayerName.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include <cstdlib>

namespace Duskhaven::Highlight::FindYourself
{
    namespace
    {
        bool s_combatOnly   = false;
        bool s_shouldRender = false;

        bool IsPlayerInCombat()
        {
            CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));
            return player && player->IsInCombat();
        }

        bool FindYourselfCombatOnlyCVarCallback(CVar*, const char*, const char* value, void*)
        {
            s_combatOnly = value && std::atoi(value) != 0;
            RefreshPlayerNameColor();
            return true;
        }
    } // namespace

    CImVector SelfHighlightColor()
    {
        CImVector color{};
        color.b = 0x00;
        color.g = 0xFF;
        color.r = 0xFF;
        color.a = 0xFF;
        return color;
    }

    void Apply()
    {
        CVar::Register("findYourselfModeCombatOnly", "Only shows the active player highlight while in combat", 1, "0",
                       FindYourselfCombatOnlyCVarCallback, CATEGORY_GRAPHICS, false, nullptr, false);

        Circle::Apply();
        Outline::Apply();
    }

    bool IsEnabled()
    {
        return Outline::IsEnabled() || Circle::IsEnabled();
    }

    bool ShouldRender()
    {
        return s_shouldRender;
    }

    void OnWorldRender()
    {
        const bool shouldRender = IsEnabled() && (!s_combatOnly || IsPlayerInCombat());
        if (shouldRender != s_shouldRender)
        {
            s_shouldRender = shouldRender;
            RefreshPlayerNameColor();
        }

        Outline::OnWorldRender();
    }

    void RefreshPlayerNameColor()
    {
        CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));
        if (player && player->m_nameDesc)
        {
            PlayerNameTriggerColorUpdate(player->m_nameDesc);
        }
    }
}
