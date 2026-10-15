#pragma once

#include <cstdint>
#include <functional>
#include <vector>

class CGWorldFrame;
class CDataStore;
class C3Vector;
struct WOWGUID;

namespace Duskhaven
{
    namespace Lifecycle
    {
        class ILifecycleListener
        {
        public:
            virtual ~ILifecycleListener() = default;

            virtual void OnEngineInitialize() {}
            virtual void OnEngineShutdown() {}

            virtual void OnGameInitialize(uint32_t mapId, const C3Vector& position) {}
            virtual void OnGameDestroy() {}

            virtual void OnEnterWorld() {}
            virtual void OnLeaveWorld() {}

            virtual void OnWorldUpdate() {}
            virtual void OnWorldRender(CGWorldFrame* worldFrame) {}

            virtual void OnWorldHitTest(const WOWGUID& guid, const C3Vector& point, bool hitObject) {}
            virtual bool OnTerrainTrack(const C3Vector& point) { return false; }

            virtual void OnSpellDestroy() {}
            virtual void OnSpellGoOrStart(uint32_t msgId, CDataStore* msg) {}
            virtual void OnChannelStart(CDataStore* msg) {}
            virtual void OnChannelUpdate(CDataStore* msg) {}

            virtual bool OnPacketReceived(uint32_t msgId, uint32_t time, CDataStore* msg) { return false; }
            virtual void OnRegisterScriptFunctions() {}
        };

        void RegisterListener(ILifecycleListener* listener);
        void UnregisterListener(ILifecycleListener* listener);

        // Functional callbacks for quick / lambda hook registrations
        void AddEngineInitHook(std::function<void()> hook);
        void AddEngineShutdownHook(std::function<void()> hook);
        void AddGameInitHook(std::function<void(uint32_t, const C3Vector&)> hook);
        void AddGameDestroyHook(std::function<void()> hook);
        void AddEnterWorldHook(std::function<void()> hook);
        void AddLeaveWorldHook(std::function<void()> hook);
        void AddUpdateHook(std::function<void()> hook);
        void AddRenderHook(std::function<void(CGWorldFrame*)> hook);
        void AddHitTestHook(std::function<void(const WOWGUID&, const C3Vector&, bool)> hook);
        void AddTerrainTrackHook(std::function<bool(const C3Vector&)> hook);
        void AddSpellDestroyHook(std::function<void()> hook);
        void AddPacketHook(std::function<bool(uint32_t, uint32_t, CDataStore*)> hook);
        void AddScriptRegisterHook(std::function<void()> hook);

        // Internal dispatcher helpers
        void DispatchEngineInitialize();
        void DispatchEngineShutdown();
        void DispatchGameInitialize(uint32_t mapId, const C3Vector& position);
        void DispatchGameDestroy();
        void DispatchEnterWorld();
        void DispatchLeaveWorld();
        void DispatchWorldUpdate();
        void DispatchWorldRender(CGWorldFrame* worldFrame);
        void DispatchWorldHitTest(const WOWGUID& guid, const C3Vector& point, bool hitObject);
        bool DispatchTerrainTrack(const C3Vector& point);
        void DispatchSpellDestroy();
        void DispatchSpellGoOrStart(uint32_t msgId, CDataStore* msg);
        void DispatchChannelStart(CDataStore* msg);
        void DispatchChannelUpdate(CDataStore* msg);
        bool DispatchPacketReceived(uint32_t msgId, uint32_t time, CDataStore* msg);
        void DispatchRegisterScriptFunctions();
    } // namespace Lifecycle
} // namespace Duskhaven
