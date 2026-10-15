#pragma once

class CGWorldFrame;

namespace Duskhaven
{
    void Initialize();
    void RegisterScriptFunctions();

    void OnWorldUpdate();
    void OnWorldRender(CGWorldFrame* worldFrame);
}
