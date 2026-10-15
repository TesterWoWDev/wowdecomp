#pragma once
#include <cstdint>

class CDataStore;

namespace Duskhaven
{
    namespace DangerZoneVisuals
    {
        void HandleSpellPacketPreservingCursor(uint32_t opcode, CDataStore* packet);
        void HandleChannelStart(CDataStore* packet);
        void HandleChannelUpdate(CDataStore* packet);
        void Pump();
        void ClearAll();
    } // namespace DangerZoneVisuals
} // namespace Duskhaven
