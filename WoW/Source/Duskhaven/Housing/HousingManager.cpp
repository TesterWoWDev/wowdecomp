#include "HousingManager.h"
#include "WoW/Source/Duskhaven/Editor/EditorRuntime.h"
#include "Engine/Source/Tempest/C4Quaternion.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/WorldClient/World.h"
#include <cmath>

namespace Duskhaven
{
    namespace Housing
    {
        namespace
        {
            WOWGUID s_lastMouseGuid;
            C3Vector s_lastMouseHitPoint;
            WOWGUID s_selectedGobGuid;

            bool GroundNormal(const C3Vector& groundPosition, float sampleRadius, C3Vector& out)
            {
                if (sampleRadius <= 0.0f)
                    return false;

                C3Vector sampleX{};
                C3Vector sampleY{};
                if (!TraceGround({groundPosition.x + sampleRadius, groundPosition.y, groundPosition.z}, sampleX) ||
                    !TraceGround({groundPosition.x, groundPosition.y + sampleRadius, groundPosition.z}, sampleY))
                    return false;

                C3Vector tangentX = sampleX - groundPosition;
                C3Vector tangentY = sampleY - groundPosition;
                out = C3Vector::Cross(tangentX, tangentY);
                out.Normalize();
                if (out.z < 0.0f)
                    out = -out;

                return (out.x != 0.0f || out.y != 0.0f || out.z != 0.0f);
            }

            Quat QuatFromBasis(const C3Vector& xAxis, const C3Vector& yAxis, const C3Vector& zAxis)
            {
                float const m00 = xAxis.x;
                float const m01 = yAxis.x;
                float const m02 = zAxis.x;
                float const m10 = xAxis.y;
                float const m11 = yAxis.y;
                float const m12 = zAxis.y;
                float const m20 = xAxis.z;
                float const m21 = yAxis.z;
                float const m22 = zAxis.z;

                float const trace = m00 + m11 + m22;
                if (trace > 0.0f)
                {
                    float const s = sqrtf(trace + 1.0f) * 2.0f;
                    return NormalizeQuat({0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s});
                }

                if (m00 > m11 && m00 > m22)
                {
                    float const s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
                    return NormalizeQuat({(m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s});
                }

                if (m11 > m22)
                {
                    float const s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
                    return NormalizeQuat({(m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s});
                }

                float const s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
                return NormalizeQuat({(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s});
            }
        } // namespace

        void RecordLastMouseHit(const WOWGUID& guid, const C3Vector& hitPoint, bool hitObject)
        {
            s_lastMouseGuid = hitObject ? guid : WOWGUID();
            s_lastMouseHitPoint = hitPoint;
        }

        const C3Vector& GetLastMouseHitPoint()
        {
            return s_lastMouseHitPoint;
        }

        const WOWGUID& GetLastMouseHitGuid()
        {
            return s_lastMouseGuid;
        }

        CGGameObject_C* GetSelectedGameObject()
        {
            if (s_selectedGobGuid.m_guid == 0)
                return nullptr;

            CGObject_C* obj = ClntObjMgrObjectPtr(s_selectedGobGuid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
            return static_cast<CGGameObject_C*>(obj);
        }

        bool SetSelectedGameObject(const WOWGUID& guid)
        {
            CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
            if (!obj)
            {
                ClearSelectedGameObject();
                return false;
            }

            s_selectedGobGuid = guid;
            Editor::EditorRuntime::SelectGameObject(guid.m_guid);
            return true;
        }

        void ClearSelectedGameObject()
        {
            s_selectedGobGuid = WOWGUID();
            Editor::EditorRuntime::ClearSelection();
        }

        const WOWGUID& GetSelectedGameObjectGuid()
        {
            return s_selectedGobGuid;
        }

        bool TraceGround(const C3Vector& position, C3Vector& out)
        {
            C3Vector start{position.x, position.y, position.z + 5.0f};
            C3Vector end{position.x, position.y, position.z - 100.0f};
            C3Vector hit{};
            float distance = 1.0f;

            if (!World::Intersect(start, end, &hit, &distance, 0x10111, nullptr))
                return false;

            out = hit;
            return true;
        }

        bool FindGroundAlignedRotation(CGGameObject_C* gameObject, const C3Vector& groundPosition, float sampleRadius,
                                       Quat& out)
        {
            if (!gameObject)
                return false;

            C3Vector normal{};
            if (!GroundNormal(groundPosition, sampleRadius, normal))
                return false;

            float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
            int64_t packed = reinterpret_cast<const C4QuaternionCompressed*>(&gameObject->m_passenger.m_facing)->m_packed;
            Quat curRot = UnpackQuat(static_cast<uint64_t>(packed));
            QuatToEuler(curRot, roll, pitch, yaw);

            C3Vector yawForward = {cosf(yaw), sinf(yaw), 0.0f};
            float dotNorm = yawForward.x * normal.x + yawForward.y * normal.y + yawForward.z * normal.z;
            C3Vector xAxis = yawForward - normal * dotNorm;
            xAxis.Normalize();
            if (xAxis.x == 0.0f && xAxis.y == 0.0f && xAxis.z == 0.0f)
                return false;

            C3Vector yAxis = C3Vector::Cross(normal, xAxis);
            yAxis.Normalize();
            C3Vector zAxis = C3Vector::Cross(xAxis, yAxis);
            zAxis.Normalize();

            out = QuatFromBasis(xAxis, yAxis, zAxis);
            return true;
        }
    } // namespace Housing
} // namespace Duskhaven
