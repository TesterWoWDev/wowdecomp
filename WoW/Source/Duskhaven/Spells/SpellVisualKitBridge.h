#pragma once
#include <cstdint>

class CGObject_C;

namespace Duskhaven
{
    namespace SpellVisualKitBridge
    {
        void PlayExtraEffects(CGObject_C* object, uint32_t kitId);
    } // namespace SpellVisualKitBridge
} // namespace Duskhaven
