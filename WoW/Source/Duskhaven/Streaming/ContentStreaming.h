#pragma once

#include <cstdint>
#include <string>

namespace ContentStreaming
{
    void Initialize();
    void Trigger(bool forceRepair = false);
    bool IsLoginBlocked();
    void OnGlueIdle();
    void PumpMountQueue();
    void RegisterLuaFunctions();
} // namespace ContentStreaming

namespace Duskhaven::ContentStreaming
{
    using namespace ::ContentStreaming;
}
