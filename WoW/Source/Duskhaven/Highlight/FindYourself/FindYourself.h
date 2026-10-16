#pragma once

#include "Engine/Source/Tempest/CImVector.h"

namespace Duskhaven::Highlight::FindYourself
{
    void Apply();
    bool IsEnabled();
    bool ShouldRender();
    void OnWorldRender();
    void RefreshPlayerNameColor();
    CImVector SelfHighlightColor();
}
