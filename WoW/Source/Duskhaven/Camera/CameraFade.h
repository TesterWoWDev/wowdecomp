#pragma once

#include "Engine/Source/Tempest/C3Vector.h"
#include <cstdint>

class CM2Model;

namespace Duskhaven
{
    namespace CameraFade
    {
        bool IsCollisionCheckActive();
        bool IsModelCurrent(CM2Model* model);
        void OnModelRelease(CM2Model* model);
        int32_t IntersectCamera(const C3Vector& start, const C3Vector& end, C3Vector* hit, float* distance, uint32_t flags);
    }
}
