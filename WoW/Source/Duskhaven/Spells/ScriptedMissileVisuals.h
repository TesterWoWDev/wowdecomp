#pragma once
#include <cstdint>

namespace Duskhaven
{
    namespace ScriptedMissileVisuals
    {
        void Initialize();
        void Pump();
        void SendCasterFacing(bool force = false);
        void ClearAll();
    } // namespace ScriptedMissileVisuals
} // namespace Duskhaven
