#pragma once

#include "WoW/Source/Object/ObjectClient/VisibleItem_C.h"
#include <cstdint>

class CustomPacketRead;

namespace Duskhaven
{
    namespace VisibleItemOverrides
    {
        void Initialize();

        class ScopedContext
        {
        public:
            explicit ScopedContext(uint64_t guid);
            ~ScopedContext();
        private:
            uint64_t m_previousGuid;
        };

        void SetCurrentContextGuid(uint64_t guid);
        uint64_t GetCurrentContextGuid();

        const VisibleItem_C* GetOverrideItem(uint64_t guid, int32_t slot);
        bool GetShoulderOverride(uint64_t guid, int32_t* outLeftDisplay, int32_t* outRightDisplay);
        bool GetShoulderOverrideForCurrentContext(int32_t* outLeftDisplay, int32_t* outRightDisplay);

        void HandleVisibleItemOverride(CustomPacketRead* packet);
        void HandleCharSelectShoulderOverride(CustomPacketRead* packet);
        void HandleCharSelectVisibleItems(CustomPacketRead* packet);
    }
}
