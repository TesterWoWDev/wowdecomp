#include "WoW/Source/Duskhaven/Character/CharacterDefines.h"

namespace Duskhaven
{
    namespace Character
    {
        uint32_t s_activeCharSpec = 0;
        uint32_t s_masteryAmount = 0;
        float s_masteryPct = 0.0f;
        float s_masteryRatingSpec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::unordered_map<uint32_t, SpellCharge> s_spellChargeMap;

        int SpecToIndex(uint32_t specID)
        {
            static const uint32_t spec1[] = { 1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 34 };
            static const uint32_t spec2[] = { 2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 35 };
            static const uint32_t spec3[] = { 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 36 };
            static const uint32_t spec4[] = { 31, 32, 33 };

            for (uint32_t s : spec1)
            {
                if (s == specID)
                    return 0;
            }

            for (uint32_t s : spec2)
            {
                if (s == specID)
                    return 1;
            }

            for (uint32_t s : spec3)
            {
                if (s == specID)
                    return 2;
            }

            for (uint32_t s : spec4)
            {
                if (s == specID)
                    return 3;
            }

            return -1;
        }
    } // namespace Character
} // namespace Duskhaven
