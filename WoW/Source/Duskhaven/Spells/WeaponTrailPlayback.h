#pragma once
#include <cstdint>

class CGObject_C;
class CGWorldFrame;
struct WeaponTrailRow;
struct WeaponTrailParamRow;

namespace Duskhaven
{
    namespace WeaponTrailPlayback
    {
        void Play(CGObject_C* object, uint32_t spellVisualKitId, WeaponTrailRow const& trail,
                  WeaponTrailParamRow const& param);
        void OnWorldRender(CGWorldFrame* worldFrame);
        void ReleaseResources();
    } // namespace WeaponTrailPlayback
} // namespace Duskhaven
