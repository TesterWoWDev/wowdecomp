#include "WoW/Source/Duskhaven/Duskhaven.h"
#include "WoW/Source/Duskhaven/Config/DHConfig.h"
#include "WoW/Source/Duskhaven/CVars/DuskhavenCVars.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/Network/DuskhavenNetwork.h"
#include "WoW/Source/Duskhaven/Lua/PlayerFunctions.h"
#include "WoW/Source/Duskhaven/Lua/UnitFunctions.h"
#include "WoW/Source/Duskhaven/Lua/UIFunctions.h"
#include "WoW/Source/Duskhaven/Lua/ModelFunctions.h"
#include "WoW/Source/Duskhaven/Lua/SpellFunctions.h"
#include "WoW/Source/Duskhaven/Lua/HousingFunctions.h"
#include "WoW/Source/Duskhaven/Lua/CharCreateFunctions.h"
#include "WoW/Source/Duskhaven/Lua/EmbeddedScripts.h"

#include "WoW/Source/Duskhaven/Character/VisibleItemOverrides.h"
#include "WoW/Source/Duskhaven/Housing/HousingManager.h"
#include "WoW/Source/Duskhaven/Spells/DangerZoneVisuals.h"
#include "WoW/Source/Duskhaven/Spells/LocalSpellData.h"
#include "WoW/Source/Duskhaven/Spells/ScriptedMissileVisuals.h"
#include "WoW/Source/Duskhaven/Spells/SpellTargeting.h"
#include "WoW/Source/Duskhaven/Spells/WeaponTrailPlayback.h"
#include "WoW/Source/Duskhaven/Display/GxMonitor.h"
#include "WoW/Source/Duskhaven/IPC/DiscordPresenceIPC.h"
#include "WoW/Source/Duskhaven/Camera/WorldSpace.h"
#include "WoW/Source/Duskhaven/FileIntegrity/FileIntegrity.h"
#include "WoW/Source/Duskhaven/Highlight/Highlight.h"
#include "WoW/Source/Duskhaven/Editor/EditorRuntime.h"
#include "WoW/Source/Duskhaven/Streaming/ContentStreaming.h"
#include "WoW/Source/Duskhaven/ModCore/ModCore.h"
#include "WoW/Source/Ui/GameUI.h"
#include "Engine/Source/Event/Types.h"

namespace Duskhaven
{
    void Initialize()
    {
        // 0. ModCore Framework Runtime
        ModCore::ModManager::Get().Initialize();

        // 1. Config
        DHConfig::Initialize();

        // 2. Custom CVars
        CVars::Initialize();

        // 3. Custom DBCs
        CDBCRegistry::LoadAll();

        // 4. Visible Item & Transmog Overrides
        VisibleItemOverrides::Initialize();

        // 5. Scripted Missile Visuals
        ScriptedMissileVisuals::Initialize();

        // 6. Multi-monitor support
        GxMonitor::Initialize();

        // 7. Network
        Network::Initialize();

        // 8. Visual Highlights & Outlines
        Highlight::Initialize();

        // 9. In-Game 3D Gizmo & Transform Editor
        Editor::EditorRuntime::Apply();

        // 10. Live Content Streaming & Auto-Patching
        ContentStreaming::Initialize();

        Lifecycle::DispatchEngineInitialize();
    }

    void Shutdown()
    {
        ModCore::ModManager::Get().Shutdown();
        Lifecycle::DispatchEngineShutdown();
    }

    void OnGameInitialize(uint32_t mapId, const C3Vector& position)
    {
        Editor::EditorRuntime::OnGameClientInitialize();
        Lifecycle::DispatchGameInitialize(mapId, position);
    }

    void OnGameDestroy()
    {
        Editor::EditorRuntime::OnGameClientDestroy();
        OnSpellDestroy();
        Lifecycle::DispatchGameDestroy();
    }

    void OnEnterWorld()
    {
        Lifecycle::DispatchEnterWorld();
    }

    void OnLeaveWorld()
    {
        OnSpellDestroy();
        Lifecycle::DispatchLeaveWorld();
    }

    void OnWorldUpdate()
    {
        ModCore::ModManager::Get().OnWorldUpdate(1.0f / 60.0f);
        ContentStreaming::PumpMountQueue();
        LocalSpellData::PumpExtendedLanguageRefresh();
        DangerZoneVisuals::Pump();
        ScriptedMissileVisuals::Pump();
        Lifecycle::DispatchWorldUpdate();
    }

    void OnWorldRender(CGWorldFrame* worldFrame)
    {
        ModCore::ModManager::Get().OnWorldRender(worldFrame);
        WeaponTrailPlayback::OnWorldRender(worldFrame);
        Highlight::OnWorldRender();
        Editor::EditorRuntime::OnWorldRender(worldFrame);
        Lifecycle::DispatchWorldRender(worldFrame);
    }

    void OnWorldHitTest(const WOWGUID& guid, const C3Vector& point, bool hitObject)
    {
        Housing::RecordLastMouseHit(guid, point, hitObject);
        Lifecycle::DispatchWorldHitTest(guid, point, hitObject);
    }

    bool OnTerrainTrack(const C3Vector& point)
    {
        if (Spells::s_castAtCursor)
        {
            Spells::s_castAtCursor = false;
            CTerrainClickEvent evt;
            evt.m_guid = WOWGUID();
            evt.m_point = point;
            evt.m_button = MOUSE_BUTTON_LEFT;
            CGGameUI::HandleTerrainClick(evt);
            return true;
        }

        return Lifecycle::DispatchTerrainTrack(point);
    }

    void OnSpellDestroy()
    {
        DangerZoneVisuals::ClearAll();
        ScriptedMissileVisuals::ClearAll();
        WeaponTrailPlayback::ReleaseResources();
        Lifecycle::DispatchSpellDestroy();
    }

    void OnSpellGoOrStart(uint32_t msgId, CDataStore* msg)
    {
        DangerZoneVisuals::HandleSpellPacketPreservingCursor(msgId, msg);
        Lifecycle::DispatchSpellGoOrStart(msgId, msg);
    }

    void OnChannelStart(CDataStore* msg)
    {
        DangerZoneVisuals::HandleChannelStart(msg);
        Lifecycle::DispatchChannelStart(msg);
    }

    void OnChannelUpdate(CDataStore* msg)
    {
        DangerZoneVisuals::HandleChannelUpdate(msg);
        Lifecycle::DispatchChannelUpdate(msg);
    }

    bool OnPacketReceived(uint32_t msgId, uint32_t time, CDataStore* msg)
    {
        if (ModCore::ModManager::Get().OnPacketReceived(msgId, time, msg))
        {
            return true;
        }

        if (msgId == Network::SERVER_TO_CLIENT_OPCODE || msgId == 0x7A00)
        {
            Network::HandleServerToClientPacket(nullptr, msgId, time, msg);
            return true;
        }

        if (msgId == 0x7A50)
        {
            ContentStreaming::Trigger(false);
            return true;
        }

        if (msgId == 0x7A51)
        {
            ContentStreaming::Trigger(true);
            return true;
        }

        return Lifecycle::DispatchPacketReceived(msgId, time, msg);
    }

    void RegisterScriptFunctions()
    {
        ModCore::ModManager::Get().RegisterLuaBindings(nullptr);

        DHConfig::RegisterLuaFunctions();
        Network::RegisterLuaFunctions();
        IPC::RegisterLuaFunctions();
        Lua::RegisterPlayerFunctions();
        Lua::RegisterUnitFunctions();
        Lua::RegisterUIFunctions();
        Lua::RegisterModelFunctions();
        Lua::RegisterSpellFunctions();
        Lua::RegisterHousingFunctions();
        Lua::RegisterCharCreateFunctions();
        Lua::RegisterEmbeddedScripts();
        GxMonitor::RegisterLuaFunctions();
        WorldSpace::RegisterLuaFunctions();
        FileIntegrity::RegisterLuaFunctions();
        ContentStreaming::RegisterLuaFunctions();

        Lifecycle::DispatchRegisterScriptFunctions();
    }
}
