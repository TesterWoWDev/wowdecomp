#pragma once

#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "GizmoPick.h"

namespace Duskhaven::Editor::GizmoDraw
{
    void DrawArrowSolid(const C3Vector& origin, const C3Vector& dir,
                        const C3Vector& side, float length, float headLength, float headRadius,
                        float shaftRadius, CImVector color);
    void DrawTranslationGizmo(const C3Vector& origin, float scale, Axis selected);
    void DrawLine(const C3Vector& a, const C3Vector& b, CImVector color);
    void DrawSphere(const C3Vector& center, float radius, CImVector color);
    void DrawWireSphere(const C3Vector& center, float radius, CImVector color);
    void DrawTorus(const C3Vector& origin, const C3Vector& axis,
                   const C3Vector& side, float ringRadius, float tubeRadius, CImVector color);
    void DrawRotationGizmo(const C3Vector& origin, float scale, Axis selected);
} // namespace Duskhaven::Editor::GizmoDraw
