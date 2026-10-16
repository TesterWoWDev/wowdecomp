#pragma once

#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/WGUID.h"

#include "QuatMath.h"

class CGGameObject_C;

namespace Duskhaven
{
    namespace Housing
    {
        void RecordLastMouseHit(const WOWGUID& guid, const C3Vector& hitPoint, bool hitObject);
        const C3Vector& GetLastMouseHitPoint();
        const WOWGUID& GetLastMouseHitGuid();

        CGGameObject_C* GetSelectedGameObject();
        bool SetSelectedGameObject(const WOWGUID& guid);
        void ClearSelectedGameObject();
        const WOWGUID& GetSelectedGameObjectGuid();

        bool TraceGround(const C3Vector& position, C3Vector& out);
        bool FindGroundAlignedRotation(CGGameObject_C* gameObject, const C3Vector& groundPosition, float sampleRadius, Quat& out);
    } // namespace Housing
} // namespace Duskhaven
