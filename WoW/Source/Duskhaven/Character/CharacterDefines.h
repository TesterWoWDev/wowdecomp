#pragma once

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace Duskhaven
{
    namespace Character
    {
        struct SpellCharge
        {
            uint8_t currentCharges = 0;
            uint8_t maxCharges = 0;
            uint32_t async = 0;
            uint32_t remainingCooldown = 0;
            uint32_t cooldown = 0;
        };

        extern uint32_t s_activeCharSpec;
        extern uint32_t s_masteryAmount;
        extern float s_masteryPct;
        extern float s_masteryRatingSpec[4];
        extern std::unordered_map<uint32_t, SpellCharge> s_spellChargeMap;

        inline void SetCharActiveSpec(uint32_t spec) { s_activeCharSpec = spec; }
        inline uint32_t GetCharActiveSpec() { return s_activeCharSpec; }

        inline void SetMasteryAmount(uint32_t amount) { s_masteryAmount = amount; }
        inline uint32_t GetMasteryAmount() { return s_masteryAmount; }

        inline void SetMasteryPct(float pct) { s_masteryPct = pct; }
        inline float GetMasteryPct() { return s_masteryPct; }

        inline void SetMasteryRatingSpec(int index, float val)
        {
            if (index >= 0 && index < 4)
                s_masteryRatingSpec[index] = val;
        }

        inline float GetMasteryRatingSpec(int index)
        {
            if (index >= 0 && index < 4)
                return s_masteryRatingSpec[index];
            return 1.0f;
        }

        inline float GetMasteryForSpec(int index)
        {
            float rating = GetMasteryRatingSpec(index);
            if (rating <= 0.0f)
                rating = 1.0f;
            return ((static_cast<float>(s_masteryAmount) / rating) + s_masteryPct);
        }

        int SpecToIndex(uint32_t specID);
    } // namespace Character
} // namespace Duskhaven
