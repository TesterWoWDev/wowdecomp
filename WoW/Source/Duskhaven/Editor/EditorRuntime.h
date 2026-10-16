#pragma once

#include <cstdint>

class CGGameObject_C;
class CGWorldFrame;

namespace Duskhaven::Editor::EditorRuntime
{
    bool SelectGameObject(uint64_t guid);
    void Apply();
    void OnGameClientInitialize();
    void OnGameClientDestroy();
    void ClearSelection();
    CGGameObject_C* SelectedGameObject();
    uint64_t CurrentSelectedGobGUID();
    void OnWorldRender(CGWorldFrame* worldFrame);
} // namespace Duskhaven::Editor::EditorRuntime
