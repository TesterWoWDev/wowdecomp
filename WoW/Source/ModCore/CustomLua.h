#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

struct lua_State;

namespace ModCore
{
    struct SpellCharge
    {
        uint8_t currentCharges = 0;
        uint8_t maxCharges = 0;
        uint32_t async = 0;
        uint32_t remainingCooldown = 0;
        uint32_t cooldown = 0;
    };

    class CustomLuaManager
    {
    public:
        static CustomLuaManager& Get();

        void RegisterLuaBindings(lua_State* L);

        // Spell Charges
        void UpdateSpellCharge(uint32_t spellId, const SpellCharge& charge);
        bool GetSpellCharge(uint32_t spellId, SpellCharge& outCharge) const;
        void ClearSpellCharges();

        // Active Spec & Mastery
        void SetActiveSpec(uint32_t spec) { m_activeCharSpec = spec; }
        uint32_t GetActiveSpec() const { return m_activeCharSpec; }
        int SpecToIndex(uint32_t specId) const;

        void SetMasteryAmount(uint32_t amount) { m_masteryAmount = amount; }
        uint32_t GetMasteryAmount() const { return m_masteryAmount; }

        void SetMasteryPct(float pct) { m_masteryPct = pct; }
        float GetMasteryPct() const { return m_masteryPct; }

        void SetMasteryRatingSpec(int index, float val)
        {
            if (index >= 0 && index < 4)
                m_masteryRatingSpec[index] = val;
        }

        float GetMasteryRatingSpec(int index) const
        {
            if (index >= 0 && index < 4)
                return m_masteryRatingSpec[index];
            return 1.0f;
        }

        float GetMasteryForSpec(int index) const
        {
            float rating = GetMasteryRatingSpec(index);
            if (rating <= 0.0f)
                rating = 1.0f;
            return ((static_cast<float>(m_masteryAmount) / rating) + m_masteryPct);
        }

        // Mouse hit test tracking for Housing/World queries
        void RecordMouseHit(uint64_t guid, float x, float y, float z)
        {
            m_lastHitGuid = guid;
            m_lastHitPoint[0] = x;
            m_lastHitPoint[1] = y;
            m_lastHitPoint[2] = z;
        }

        uint64_t GetLastHitGuid() const { return m_lastHitGuid; }
        void GetLastHitPoint(float& x, float& y, float& z) const
        {
            x = m_lastHitPoint[0];
            y = m_lastHitPoint[1];
            z = m_lastHitPoint[2];
        }

    private:
        CustomLuaManager() = default;

        uint32_t m_activeCharSpec = 0;
        uint32_t m_masteryAmount = 0;
        float m_masteryPct = 0.0f;
        float m_masteryRatingSpec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::unordered_map<uint32_t, SpellCharge> m_spellCharges;

        uint64_t m_lastHitGuid = 0;
        float m_lastHitPoint[3] = { 0.0f, 0.0f, 0.0f };
    };
}
