#pragma once

#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Duskhaven/Housing/QuatMath.h"
#include "GizmoPick.h"
#include <cstdint>

namespace Duskhaven::Editor
{
    struct DragState
    {
        bool active       = false;
        bool isRotation   = false;
        Axis axis         = Axis::None;
        C3Vector startObjPos{};
        C3Vector anchorOnAxis{};
        float anchorAngle = 0.0f;
        C3Vector ringAxis{};
        C3Vector ringRefDir{};
        C3Vector ringRefPerp{};
        Housing::Quat startRot{};
    };

    struct EditorState
    {
        C3Vector start{};
        C3Vector end{};
        C3Vector gizmoPosition{};
        Axis gizmoTranslationAxis = Axis::None;
        Axis gizmoRotationAxis    = Axis::None;
        DragState gizmoDragState{};
        uint64_t currentObjectGuid = 0;
    };
} // namespace Duskhaven::Editor
