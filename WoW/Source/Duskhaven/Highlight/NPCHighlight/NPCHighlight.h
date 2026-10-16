#pragma once

class CGObject_C;
class CM2Model;

namespace Duskhaven::Highlight::NPCHighlight
{
    void Apply();
    void OnWorldRender();
    bool IsEnabled();
    bool IsDebugEnabled();
    CM2Model* GetHoveredModel();
    void TrackHoveredObject(CGObject_C* object);
    void UntrackHoveredObject(CGObject_C* object);
    void GetOutlineColor(float& red, float& green, float& blue);
    float GetOutlineStrength();
}
