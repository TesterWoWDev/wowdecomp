#include "WoW/Source/Duskhaven/Highlight/TargetSelection.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/Circle.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/FindYourself.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"

namespace Duskhaven::Highlight::TargetSelection
{
    bool IsSelfTargeted()
    {
        const WOWGUID playerGuid = ClntObjMgrGetActivePlayer();
        return playerGuid && (CGGameUI::m_lockedTarget == playerGuid);
    }

    bool ShouldRenderActivePlayerCircle()
    {
        if (IsSelfTargeted())
            return true;

        return FindYourself::Circle::IsEnabled() && FindYourself::ShouldRender();
    }

    void RenderActivePlayerCircle()
    {
        __try
        {
            CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));
            if (player)
                player->RenderTargetSelection();
        }
        __except (1)
        {
        }
    }

    void Apply()
    {
    }
}
