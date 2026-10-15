#include "WoW/Source/Duskhaven/Lifecycle/LifecycleHooks.h"
#include <algorithm>
#include <vector>

namespace Duskhaven::Lifecycle
{
    namespace
    {
        struct Registry
        {
            std::vector<ILifecycleListener*> listeners;
            std::vector<std::function<void()>> engineInitHooks;
            std::vector<std::function<void()>> engineShutdownHooks;
            std::vector<std::function<void(uint32_t, const C3Vector&)>> gameInitHooks;
            std::vector<std::function<void()>> gameDestroyHooks;
            std::vector<std::function<void()>> enterWorldHooks;
            std::vector<std::function<void()>> leaveWorldHooks;
            std::vector<std::function<void()>> updateHooks;
            std::vector<std::function<void(CGWorldFrame*)>> renderHooks;
            std::vector<std::function<void(const WOWGUID&, const C3Vector&, bool)>> hitTestHooks;
            std::vector<std::function<bool(const C3Vector&)>> terrainTrackHooks;
            std::vector<std::function<void()>> spellDestroyHooks;
            std::vector<std::function<bool(uint32_t, uint32_t, CDataStore*)>> packetHooks;
            std::vector<std::function<void()>> scriptRegisterHooks;
        };

        Registry& GetRegistry()
        {
            static Registry s_registry;
            return s_registry;
        }
    } // namespace

    void RegisterListener(ILifecycleListener* listener)
    {
        if (!listener)
            return;
        auto& list = GetRegistry().listeners;
        if (std::find(list.begin(), list.end(), listener) == list.end())
        {
            list.push_back(listener);
        }
    }

    void UnregisterListener(ILifecycleListener* listener)
    {
        if (!listener)
            return;
        auto& list = GetRegistry().listeners;
        list.erase(std::remove(list.begin(), list.end(), listener), list.end());
    }

    void AddEngineInitHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().engineInitHooks.push_back(std::move(hook));
    }

    void AddEngineShutdownHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().engineShutdownHooks.push_back(std::move(hook));
    }

    void AddGameInitHook(std::function<void(uint32_t, const C3Vector&)> hook)
    {
        if (hook) GetRegistry().gameInitHooks.push_back(std::move(hook));
    }

    void AddGameDestroyHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().gameDestroyHooks.push_back(std::move(hook));
    }

    void AddEnterWorldHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().enterWorldHooks.push_back(std::move(hook));
    }

    void AddLeaveWorldHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().leaveWorldHooks.push_back(std::move(hook));
    }

    void AddUpdateHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().updateHooks.push_back(std::move(hook));
    }

    void AddRenderHook(std::function<void(CGWorldFrame*)> hook)
    {
        if (hook) GetRegistry().renderHooks.push_back(std::move(hook));
    }

    void AddHitTestHook(std::function<void(const WOWGUID&, const C3Vector&, bool)> hook)
    {
        if (hook) GetRegistry().hitTestHooks.push_back(std::move(hook));
    }

    void AddTerrainTrackHook(std::function<bool(const C3Vector&)> hook)
    {
        if (hook) GetRegistry().terrainTrackHooks.push_back(std::move(hook));
    }

    void AddSpellDestroyHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().spellDestroyHooks.push_back(std::move(hook));
    }

    void AddPacketHook(std::function<bool(uint32_t, uint32_t, CDataStore*)> hook)
    {
        if (hook) GetRegistry().packetHooks.push_back(std::move(hook));
    }

    void AddScriptRegisterHook(std::function<void()> hook)
    {
        if (hook) GetRegistry().scriptRegisterHooks.push_back(std::move(hook));
    }

    void DispatchEngineInitialize()
    {
        for (auto* l : GetRegistry().listeners) l->OnEngineInitialize();
        for (const auto& cb : GetRegistry().engineInitHooks) cb();
    }

    void DispatchEngineShutdown()
    {
        for (auto* l : GetRegistry().listeners) l->OnEngineShutdown();
        for (const auto& cb : GetRegistry().engineShutdownHooks) cb();
    }

    void DispatchGameInitialize(uint32_t mapId, const C3Vector& position)
    {
        for (auto* l : GetRegistry().listeners) l->OnGameInitialize(mapId, position);
        for (const auto& cb : GetRegistry().gameInitHooks) cb(mapId, position);
    }

    void DispatchGameDestroy()
    {
        for (auto* l : GetRegistry().listeners) l->OnGameDestroy();
        for (const auto& cb : GetRegistry().gameDestroyHooks) cb();
    }

    void DispatchEnterWorld()
    {
        for (auto* l : GetRegistry().listeners) l->OnEnterWorld();
        for (const auto& cb : GetRegistry().enterWorldHooks) cb();
    }

    void DispatchLeaveWorld()
    {
        for (auto* l : GetRegistry().listeners) l->OnLeaveWorld();
        for (const auto& cb : GetRegistry().leaveWorldHooks) cb();
    }

    void DispatchWorldUpdate()
    {
        for (auto* l : GetRegistry().listeners) l->OnWorldUpdate();
        for (const auto& cb : GetRegistry().updateHooks) cb();
    }

    void DispatchWorldRender(CGWorldFrame* worldFrame)
    {
        for (auto* l : GetRegistry().listeners) l->OnWorldRender(worldFrame);
        for (const auto& cb : GetRegistry().renderHooks) cb(worldFrame);
    }

    void DispatchWorldHitTest(const WOWGUID& guid, const C3Vector& point, bool hitObject)
    {
        for (auto* l : GetRegistry().listeners) l->OnWorldHitTest(guid, point, hitObject);
        for (const auto& cb : GetRegistry().hitTestHooks) cb(guid, point, hitObject);
    }

    bool DispatchTerrainTrack(const C3Vector& point)
    {
        for (auto* l : GetRegistry().listeners)
        {
            if (l->OnTerrainTrack(point))
                return true;
        }
        for (const auto& cb : GetRegistry().terrainTrackHooks)
        {
            if (cb(point))
                return true;
        }
        return false;
    }

    void DispatchSpellDestroy()
    {
        for (auto* l : GetRegistry().listeners) l->OnSpellDestroy();
        for (const auto& cb : GetRegistry().spellDestroyHooks) cb();
    }

    void DispatchSpellGoOrStart(uint32_t msgId, CDataStore* msg)
    {
        for (auto* l : GetRegistry().listeners) l->OnSpellGoOrStart(msgId, msg);
    }

    void DispatchChannelStart(CDataStore* msg)
    {
        for (auto* l : GetRegistry().listeners) l->OnChannelStart(msg);
    }

    void DispatchChannelUpdate(CDataStore* msg)
    {
        for (auto* l : GetRegistry().listeners) l->OnChannelUpdate(msg);
    }

    bool DispatchPacketReceived(uint32_t msgId, uint32_t time, CDataStore* msg)
    {
        for (const auto& cb : GetRegistry().packetHooks)
        {
            if (cb(msgId, time, msg))
                return true;
        }
        for (auto* l : GetRegistry().listeners)
        {
            if (l->OnPacketReceived(msgId, time, msg))
                return true;
        }
        return false;
    }

    void DispatchRegisterScriptFunctions()
    {
        for (auto* l : GetRegistry().listeners) l->OnRegisterScriptFunctions();
        for (const auto& cb : GetRegistry().scriptRegisterHooks) cb();
    }
} // namespace Duskhaven::Lifecycle
