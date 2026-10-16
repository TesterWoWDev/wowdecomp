#include "WoW/Source/Duskhaven/Camera/WorldSpace.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/Ui/WorldMap.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/ChatFrame.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "Engine/Source/Base/Coordinate.h"
#include <cmath>

namespace Duskhaven
{
    namespace WorldSpace
    {
        static bool IsFinitePosition(float x, float y, float z)
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
        }

        static void PushNilPosition(lua_State* L)
        {
            ClientLua::PushNil(L);
            ClientLua::PushNil(L);
            ClientLua::PushNil(L);
        }

        static bool TryGetActivePlayerPosition(C3Vector& out)
        {
            WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
            if (!activePlayer)
                return false;

            CGUnit_C* activeObjectPtr = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(activePlayer, TYPEMASK_UNIT, __FILE__, __LINE__));
            if (!activeObjectPtr)
                return false;

            out = activeObjectPtr->GetPosition();
            return IsFinitePosition(out.x, out.y, out.z);
        }

        static int Script_ConvertCoordsToScreenSpace(lua_State* L)
        {
            float ox = static_cast<float>(ClientLua::GetNumber(L, 1));
            float oy = static_cast<float>(ClientLua::GetNumber(L, 2));
            float oz = static_cast<float>(ClientLua::GetNumber(L, 3));

            if (!CGWorldFrame::s_currentWorldFrame || !IsFinitePosition(ox, oy, oz))
            {
                PushNilPosition(L);
                return 3;
            }

            C3Vector pos3d = { ox, oy, oz };
            C3Vector pos2d = {};
            uint32_t flags = 0;
            bool result = CGWorldFrame::s_currentWorldFrame->GetScreenCoordinates(pos3d, &pos2d, &flags);
            if (!result || !std::isfinite(pos2d.x) || !std::isfinite(pos2d.y) || !std::isfinite(pos2d.z))
            {
                PushNilPosition(L);
                return 3;
            }

            float screenX = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * pos2d.x);
            float screenY = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * pos2d.y);

            ClientLua::PushNumber(L, screenX);
            ClientLua::PushNumber(L, screenY);
            ClientLua::PushNumber(L, pos2d.z);
            return 3;
        }

        static int Script_GetActivePlayerWorldPosition(lua_State* L)
        {
            C3Vector position = {};
            if (!TryGetActivePlayerPosition(position))
            {
                PushNilPosition(L);
                return 3;
            }

            ClientLua::PushNumber(L, position.x);
            ClientLua::PushNumber(L, position.y);
            ClientLua::PushNumber(L, position.z);
            return 3;
        }

        static int Script_GetDistanceFromActivePlayerToWorldPosition(lua_State* L)
        {
            float ox = static_cast<float>(ClientLua::GetNumber(L, 1));
            float oy = static_cast<float>(ClientLua::GetNumber(L, 2));
            float oz = static_cast<float>(ClientLua::GetNumber(L, 3));

            if (!IsFinitePosition(ox, oy, oz))
            {
                ClientLua::PushNil(L);
                return 1;
            }

            C3Vector playerPosition = {};
            if (!TryGetActivePlayerPosition(playerPosition))
            {
                ClientLua::PushNil(L);
                return 1;
            }

            float dx = ox - playerPosition.x;
            float dy = oy - playerPosition.y;
            float dz = oz - playerPosition.z;
            ClientLua::PushNumber(L, std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
            return 1;
        }

        static int Script_TranslateToMapCoords(lua_State* L)
        {
            uint32_t continentID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            C3Vector worldPos = {
                static_cast<float>(ClientLua::GetNumber(L, 2)),
                static_cast<float>(ClientLua::GetNumber(L, 3)),
                static_cast<float>(ClientLua::GetNumber(L, 4))
            };
            float mapX = 0.0f;
            float mapY = 0.0f;

            CGWorldMap::GetWorldPosition(worldPos, continentID, &mapX, &mapY, nullptr, false, 0);

            ClientLua::PushNumber(L, mapX);
            ClientLua::PushNumber(L, mapY);
            return 2;
        }

        static void PrintChatMessage(const char* text)
        {
            CGChat::AddChatMessage(text, CHAT_MSG_SYSTEM, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, nullptr);
        }

        static int Script_ReloadMap(lua_State* L)
        {
            char buffer[512];
            WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
            if (activePlayer)
            {
                int32_t mapId = CGGameUI::m_iCurrentMapID;
                CGUnit_C* player = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(activePlayer, TYPEMASK_UNIT, __FILE__, __LINE__));
                if (player && mapId >= 0)
                {
                    MapRec* row = g_mapDB.GetRecord(mapId);
                    if (row)
                    {
                        C3Vector pos = player->GetPosition();
                        World::UnloadMap();
                        CWorld::LoadMap(row->m_directory, &pos, mapId, 0);

                        SStrPrintf(buffer, sizeof(buffer), "Map ID: %d (Directory: \"%s\", x: %f, y: %f, z: %f) reloaded.",
                            mapId, row->m_directory, pos.x, pos.y, pos.z);
                        PrintChatMessage(buffer);
                        return 0;
                    }
                }
            }
            PrintChatMessage("ReloadMap: Failed to reload map.");
            return 0;
        }

        static int Script_ToggleDisplayNormals(lua_State* L)
        {
            CWorld::enables ^= Enable_ShowNormals;
            bool on = (CWorld::enables & Enable_ShowNormals) != 0;
            PrintChatMessage(on ? "Normal display turned on." : "Normal display turned off.");
            return 0;
        }

        static int Script_ToggleGroundEffects(lua_State* L)
        {
            CWorld::enables ^= Enable_DetailDoodads;
            bool on = (CWorld::enables & Enable_DetailDoodads) != 0;
            PrintChatMessage(on ? "Ground clutter shown." : "Ground clutter hidden.");
            return 0;
        }

        static int Script_ToggleLiquids(lua_State* L)
        {
            CWorld::enables ^= (Enable_Water | Enable_Particulates);
            bool on = (CWorld::enables & (Enable_Water | Enable_Particulates)) != 0;
            PrintChatMessage(on ? "Liquids shown." : "Liquids hidden.");
            return 0;
        }

        static int Script_ToggleM2(lua_State* L)
        {
            CWorld::enables ^= Enable_Doodads;
            bool on = (CWorld::enables & Enable_Doodads) != 0;
            PrintChatMessage(on ? "Client-side M2s shown." : "Client-side M2s hidden.");
            return 0;
        }

        static int Script_ToggleTerrain(lua_State* L)
        {
            CWorld::enables ^= Enable_Chunks;
            bool on = (CWorld::enables & Enable_Chunks) != 0;
            PrintChatMessage(on ? "Terrain shown." : "Terrain hidden.");
            return 0;
        }

        static int Script_ToggleTerrainCulling(lua_State* L)
        {
            CWorld::enables ^= Enable_Culling;
            bool on = (CWorld::enables & Enable_Culling) != 0;
            PrintChatMessage(on ? "Terrain culling enabled." : "Terrain culling disabled.");
            return 0;
        }

        static int Script_ToggleWireframeMode(lua_State* L)
        {
            CWorld::enables ^= Enable_ShowTris;
            bool on = (CWorld::enables & Enable_ShowTris) != 0;
            PrintChatMessage(on ? "Wireframe mode on." : "Wireframe mode off.");
            return 0;
        }

        static int Script_ToggleWMO(lua_State* L)
        {
            CWorld::enables ^= Enable_MapObjs;
            bool on = (CWorld::enables & Enable_MapObjs) != 0;
            PrintChatMessage(on ? "WMOs shown." : "WMOs hidden.");
            return 0;
        }

        void RegisterLuaFunctions()
        {
            ClientLua::AddFunction("ConvertCoordsToScreenSpace", Script_ConvertCoordsToScreenSpace);
            ClientLua::AddFunction("GetActivePlayerWorldPosition", Script_GetActivePlayerWorldPosition);
            ClientLua::AddFunction("GetDistanceFromActivePlayerToWorldPosition", Script_GetDistanceFromActivePlayerToWorldPosition);
            ClientLua::AddFunction("TranslateToMapCoords", Script_TranslateToMapCoords);
            ClientLua::AddFunction("ReloadMap", Script_ReloadMap);
            ClientLua::AddFunction("ToggleDisplayNormals", Script_ToggleDisplayNormals);
            ClientLua::AddFunction("ToggleGroundEffects", Script_ToggleGroundEffects);
            ClientLua::AddFunction("ToggleLiquids", Script_ToggleLiquids);
            ClientLua::AddFunction("ToggleM2", Script_ToggleM2);
            ClientLua::AddFunction("ToggleTerrain", Script_ToggleTerrain);
            ClientLua::AddFunction("ToggleTerrainCulling", Script_ToggleTerrainCulling);
            ClientLua::AddFunction("ToggleWireframeMode", Script_ToggleWireframeMode);
            ClientLua::AddFunction("ToggleWMO", Script_ToggleWMO);
        }
    }
}
