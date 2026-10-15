#include "HousingFunctions.h"
#include "ClientLua.h"
#include "WoW/Source/Duskhaven/Housing/HousingManager.h"
#include "WoW/Source/Duskhaven/Housing/QuatMath.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "Engine/Source/Tempest/C4Quaternion.h"

#include <cstdlib>
#include <string>

namespace Duskhaven
{
    namespace Lua
    {
        int Script_GetMouseWorldPosition(lua_State* L)
        {
            const C3Vector& hit = Housing::GetLastMouseHitPoint();
            ClientLua::PushNumber(L, hit.x);
            ClientLua::PushNumber(L, hit.y);
            ClientLua::PushNumber(L, hit.z);
            return 3;
        }

        int Script_GetLastMouseoverGUID(lua_State* L)
        {
            const WOWGUID& guid = Housing::GetLastMouseHitGuid();
            uint64_t full = guid.m_guid;
            uint16_t type = uint16_t(full >> 48);
            uint32_t entry = uint32_t((full >> 24) & 0xFFFFFF);
            uint32_t low = uint32_t(full & 0xFFFFFF);

            ClientLua::PushString(L, std::to_string(full).c_str());
            ClientLua::PushNumber(L, entry);
            ClientLua::PushNumber(L, low);
            ClientLua::PushNumber(L, type);
            return 4;
        }

        int Script_ClearSelectedGob(lua_State* L)
        {
            Housing::ClearSelectedGameObject();
            return 0;
        }

        int Script_SelectEditorGobByMouse(lua_State* L)
        {
            const WOWGUID& guid = Housing::GetLastMouseHitGuid();
            if (guid.m_guid == 0 || !Housing::SetSelectedGameObject(guid))
                return 0;

            CGGameObject_C* gob = Housing::GetSelectedGameObject();
            if (!gob)
                return 0;

            C3Vector pos = gob->GetPosition();
            ClientLua::PushString(L, std::to_string(guid.m_guid).c_str());
            ClientLua::PushNumber(L, pos.x);
            ClientLua::PushNumber(L, pos.y);
            ClientLua::PushNumber(L, pos.z);
            return 4;
        }

        int Script_GetSelectedGobGUID(lua_State* L)
        {
            ClientLua::PushString(L, std::to_string(Housing::GetSelectedGameObjectGuid().m_guid).c_str());
            return 1;
        }

        int Script_GetSelectedGobPosition(lua_State* L)
        {
            CGGameObject_C* gob = Housing::GetSelectedGameObject();
            if (!gob)
                return 0;

            C3Vector pos = gob->GetPosition();
            ClientLua::PushNumber(L, pos.x);
            ClientLua::PushNumber(L, pos.y);
            ClientLua::PushNumber(L, pos.z);
            return 3;
        }

        int Script_GetSelectedGobRotation(lua_State* L)
        {
            CGGameObject_C* gob = Housing::GetSelectedGameObject();
            if (!gob)
                return 0;

            C4Quaternion q = gob->GetRotation();
            ClientLua::PushNumber(L, q.x);
            ClientLua::PushNumber(L, q.y);
            ClientLua::PushNumber(L, q.z);
            ClientLua::PushNumber(L, q.w);
            return 4;
        }

        int Script_SnapSelectedGobToGround(lua_State* L)
        {
            CGGameObject_C* gob = Housing::GetSelectedGameObject();
            if (!gob)
            {
                ClientLua::PushBoolean(L, 0);
                return 1;
            }

            float const zOffset = static_cast<float>(ClientLua::GetNumber(L, 1, 0.0));
            C3Vector curPos = gob->GetPosition();
            C3Vector groundPos{};
            if (!Housing::TraceGround(curPos, groundPos))
            {
                ClientLua::PushBoolean(L, 0);
                return 1;
            }

            groundPos.z += zOffset;
            gob->m_passenger.m_position = groundPos;
            gob->UpdateWorldObject(0);

            C4Quaternion q = gob->GetRotation();
            ClientLua::PushBoolean(L, 1);
            ClientLua::PushNumber(L, groundPos.x);
            ClientLua::PushNumber(L, groundPos.y);
            ClientLua::PushNumber(L, groundPos.z);
            ClientLua::PushNumber(L, q.x);
            ClientLua::PushNumber(L, q.y);
            ClientLua::PushNumber(L, q.z);
            ClientLua::PushNumber(L, q.w);
            return 8;
        }

        int Script_SetGobPositionByGUID(lua_State* L)
        {
            uint64_t guidVal = 0;
            if (ClientLua::IsString(L, 1))
            {
                guidVal = std::strtoull(ClientLua::ToLString(L, 1, 0), nullptr, 10);
            }
            else if (ClientLua::IsNumber(L, 1))
            {
                guidVal = static_cast<uint64_t>(ClientLua::GetNumber(L, 1));
            }

            WOWGUID guid{guidVal};
            CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
            if (!obj)
            {
                ClientLua::PushBoolean(L, 0);
                return 1;
            }

            CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
            float x = static_cast<float>(ClientLua::GetNumber(L, 2));
            float y = static_cast<float>(ClientLua::GetNumber(L, 3));
            float z = static_cast<float>(ClientLua::GetNumber(L, 4));

            gob->m_passenger.m_position = {x, y, z};
            gob->UpdateWorldObject(0);
            ClientLua::PushBoolean(L, 1);
            return 1;
        }

        int Script_SetGobRotationByGUID(lua_State* L)
        {
            uint64_t guidVal = 0;
            if (ClientLua::IsString(L, 1))
            {
                guidVal = std::strtoull(ClientLua::ToLString(L, 1, 0), nullptr, 10);
            }
            else if (ClientLua::IsNumber(L, 1))
            {
                guidVal = static_cast<uint64_t>(ClientLua::GetNumber(L, 1));
            }

            WOWGUID guid{guidVal};
            CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
            if (!obj)
            {
                ClientLua::PushBoolean(L, 0);
                return 1;
            }

            CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
            float x = static_cast<float>(ClientLua::GetNumber(L, 2));
            float y = static_cast<float>(ClientLua::GetNumber(L, 3));
            float z = static_cast<float>(ClientLua::GetNumber(L, 4));
            float w = static_cast<float>(ClientLua::GetNumber(L, 5));

            C4Quaternion q;
            q.x = x;
            q.y = y;
            q.z = z;
            q.w = w;
            *reinterpret_cast<C4QuaternionCompressed*>(&gob->m_passenger.m_facing) = q;
            gob->UpdateWorldObject(0);
            ClientLua::PushBoolean(L, 1);
            return 1;
        }

        int Script_LogMouseoverGobValues(lua_State* L)
        {
            return 0;
        }

        void RegisterHousingFunctions()
        {
            ClientLua::RegisterFunction("GetMouseWorldPosition", Script_GetMouseWorldPosition);
            ClientLua::RegisterFunction("GetLastMouseoverGUID", Script_GetLastMouseoverGUID);
            ClientLua::RegisterFunction("ClearSelectedGob", Script_ClearSelectedGob);
            ClientLua::RegisterFunction("SelectEditorGobByMouse", Script_SelectEditorGobByMouse);
            ClientLua::RegisterFunction("GetSelectedGobGUID", Script_GetSelectedGobGUID);
            ClientLua::RegisterFunction("GetSelectedGobPosition", Script_GetSelectedGobPosition);
            ClientLua::RegisterFunction("SnapSelectedGobToGround", Script_SnapSelectedGobToGround);
            ClientLua::RegisterFunction("GetSelectedGobRotation", Script_GetSelectedGobRotation);
            ClientLua::RegisterFunction("SetGobPositionByGUID", Script_SetGobPositionByGUID);
            ClientLua::RegisterFunction("SetGobRotationByGUID", Script_SetGobRotationByGUID);
            ClientLua::RegisterFunction("LogMouseoverGobValues", Script_LogMouseoverGobValues);
        }
    } // namespace Lua
} // namespace Duskhaven
