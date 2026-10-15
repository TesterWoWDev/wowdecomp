#pragma once

#include <cstdint>
#include "WoW/Source/Duskhaven/Lifecycle/LifecycleHooks.h"

class CGWorldFrame;
class CDataStore;
class C3Vector;
struct WOWGUID;

namespace Duskhaven
{
    // Engine / Subsystem Lifecycle
    void Initialize();
    void Shutdown();

    // Game Session Lifecycle (Map/Session load & unload)
    void OnGameInitialize(uint32_t mapId, const C3Vector& position);
    void OnGameDestroy();

    // World / Player In-Game Lifecycle
    void OnEnterWorld();
    void OnLeaveWorld();

    // Frame Lifecycle
    void OnWorldUpdate();
    void OnWorldRender(CGWorldFrame* worldFrame);

    // Interaction / Terrain / World Hooks
    void OnWorldHitTest(const WOWGUID& guid, const C3Vector& point, bool hitObject);
    bool OnTerrainTrack(const C3Vector& point);

    // Spell Subsystem Hooks
    void OnSpellDestroy();
    void OnSpellGoOrStart(uint32_t msgId, CDataStore* msg);
    void OnChannelStart(CDataStore* msg);
    void OnChannelUpdate(CDataStore* msg);

    // Network Hook (returns true if packet was consumed)
    bool OnPacketReceived(uint32_t msgId, uint32_t time, CDataStore* msg);

    // Script / Lua Registration
    void RegisterScriptFunctions();
}
