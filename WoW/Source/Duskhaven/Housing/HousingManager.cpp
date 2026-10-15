#include "HousingManager.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/WorldClient/World.h"

namespace Duskhaven
{
    namespace Housing
    {
        namespace
        {
            WOWGUID s_lastMouseGuid;
            C3Vector s_lastMouseHitPoint;
            WOWGUID s_selectedGobGuid;
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
            return true;
        }

        void ClearSelectedGameObject()
        {
            s_selectedGobGuid = WOWGUID();
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
    } // namespace Housing
} // namespace Duskhaven
