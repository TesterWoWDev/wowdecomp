#include "WoW/Source/Duskhaven/Character/VisibleItemOverrides.h"
#include "WoW/Source/Duskhaven/Network/DuskhavenNetwork.h"
#include "WoW/Source/Duskhaven/Network/CustomPacketRead.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <array>
#include <unordered_map>

namespace Duskhaven
{
    namespace VisibleItemOverrides
    {
        namespace
        {
            constexpr opcode_t VISIBLE_ITEM_OVERRIDE_OPCODE         = 0x7A10;
            constexpr opcode_t CHAR_SELECT_SHOULDER_OVERRIDE_OPCODE = 0x7A11;
            constexpr opcode_t CHAR_SELECT_VISIBLE_ITEM_OPCODE      = 0x7A13;
            constexpr size_t MAX_VISIBLE_ITEM_SLOTS                 = 19;
            constexpr uint8_t SERVER_SHOULDER_SLOT                  = 2;

            struct VisibleItemOverride
            {
                uint32_t entryId             = 0;
                uint32_t enchantPacked       = 0;
                uint32_t transmogEntry       = 0;
                int32_t leftShoulderDisplay  = -1;
                int32_t rightShoulderDisplay = -1;
                uint32_t flags               = 0;
                bool active                  = false;
                VisibleItem_C cachedVisibleItem = {};
            };

            struct ShoulderVisualOverride
            {
                int32_t leftShoulderDisplay  = -1;
                int32_t rightShoulderDisplay = -1;
                bool active                  = false;
            };

            static std::unordered_map<uint64_t, std::array<VisibleItemOverride, MAX_VISIBLE_ITEM_SLOTS>> s_visibleItemOverrides;
            static std::unordered_map<uint64_t, ShoulderVisualOverride> s_charSelectShoulderOverrides;
            static thread_local uint64_t s_currentContextGuid = 0;
        }

        ScopedContext::ScopedContext(uint64_t guid) : m_previousGuid(s_currentContextGuid)
        {
            s_currentContextGuid = guid;
        }

        ScopedContext::~ScopedContext()
        {
            s_currentContextGuid = m_previousGuid;
        }

        void SetCurrentContextGuid(uint64_t guid)
        {
            s_currentContextGuid = guid;
        }

        uint64_t GetCurrentContextGuid()
        {
            return s_currentContextGuid;
        }

        const VisibleItem_C* GetOverrideItem(uint64_t guid, int32_t slot)
        {
            if (!guid || slot < 0 || slot >= static_cast<int32_t>(MAX_VISIBLE_ITEM_SLOTS))
                return nullptr;

            auto it = s_visibleItemOverrides.find(guid);
            if (it == s_visibleItemOverrides.end())
                return nullptr;

            const VisibleItemOverride& ov = it->second[slot];
            if (!ov.active)
                return nullptr;

            return &ov.cachedVisibleItem;
        }

        bool GetShoulderOverride(uint64_t guid, int32_t* outLeftDisplay, int32_t* outRightDisplay)
        {
            if (outLeftDisplay) *outLeftDisplay = -1;
            if (outRightDisplay) *outRightDisplay = -1;

            if (!guid) return false;

            auto it = s_visibleItemOverrides.find(guid);
            if (it != s_visibleItemOverrides.end())
            {
                const VisibleItemOverride& ov = it->second[SERVER_SHOULDER_SLOT];
                if (ov.active && (ov.leftShoulderDisplay >= 0 || ov.rightShoulderDisplay >= 0))
                {
                    if (outLeftDisplay) *outLeftDisplay = ov.leftShoulderDisplay;
                    if (outRightDisplay) *outRightDisplay = ov.rightShoulderDisplay;
                    return true;
                }
            }

            auto itSelect = s_charSelectShoulderOverrides.find(guid);
            if (itSelect != s_charSelectShoulderOverrides.end() && itSelect->second.active)
            {
                if (outLeftDisplay) *outLeftDisplay = itSelect->second.leftShoulderDisplay;
                if (outRightDisplay) *outRightDisplay = itSelect->second.rightShoulderDisplay;
                return true;
            }

            return false;
        }

        bool GetShoulderOverrideForCurrentContext(int32_t* outLeftDisplay, int32_t* outRightDisplay)
        {
            if (!s_currentContextGuid)
                return false;
            return GetShoulderOverride(s_currentContextGuid, outLeftDisplay, outRightDisplay);
        }

        void HandleVisibleItemOverride(CustomPacketRead* packet)
        {
            if (!packet) return;

            uint64_t guid = packet->Read<uint64_t>(0);
            uint8_t slot = packet->Read<uint8_t>(0);
            bool hasItem = packet->Read<uint8_t>(0) != 0;

            if (!guid || slot >= MAX_VISIBLE_ITEM_SLOTS)
                return;

            VisibleItemOverride& ov = s_visibleItemOverrides[guid][slot];
            if (!hasItem)
            {
                ov = {};
                ov.leftShoulderDisplay = -1;
                ov.rightShoulderDisplay = -1;
                return;
            }

            ov.entryId = packet->Read<uint32_t>(0);
            uint16_t permanentEnchant = packet->Read<uint16_t>(0);
            uint16_t temporaryEnchant = packet->Read<uint16_t>(0);
            ov.transmogEntry = packet->Read<uint32_t>(0);
            ov.leftShoulderDisplay = packet->Read<int32_t>(-1);
            ov.rightShoulderDisplay = packet->Read<int32_t>(-1);
            ov.flags = packet->Read<uint32_t>(0);
            ov.enchantPacked = static_cast<uint32_t>(permanentEnchant) | (static_cast<uint32_t>(temporaryEnchant) << 16);
            ov.active = true;

            ov.cachedVisibleItem.m_itemId = static_cast<int32_t>(ov.transmogEntry ? ov.transmogEntry : ov.entryId);
            ov.cachedVisibleItem.m_enchantment[0] = permanentEnchant;
            ov.cachedVisibleItem.m_enchantment[1] = temporaryEnchant;

            CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(WOWGUID{guid}, TYPEMASK_PLAYER, __FILE__, __LINE__));
            if (player)
            {
                ScopedContext context(guid);
                INVENTORY_SLOTS invSlot = static_cast<INVENTORY_SLOTS>(slot);
                player->EquipVisibleItem(&ov.cachedVisibleItem, invSlot);
            }
        }

        void HandleCharSelectShoulderOverride(CustomPacketRead* packet)
        {
            if (!packet) return;

            uint8_t count = packet->Read<uint8_t>(0);
            for (uint8_t i = 0; i < count; ++i)
            {
                uint64_t guid = packet->Read<uint64_t>(0);
                int32_t left = packet->Read<int32_t>(-1);
                int32_t right = packet->Read<int32_t>(-1);
                if (!guid) continue;

                ShoulderVisualOverride ov;
                ov.leftShoulderDisplay = left;
                ov.rightShoulderDisplay = right;
                ov.active = (left >= 0 || right >= 0);
                s_charSelectShoulderOverrides[guid] = ov;
            }
        }

        void HandleCharSelectVisibleItems(CustomPacketRead* packet)
        {
            if (!packet) return;

            uint8_t characterCount = packet->Read<uint8_t>(0);
            for (uint8_t i = 0; i < characterCount; ++i)
            {
                uint64_t guid = packet->Read<uint64_t>(0);
                uint8_t slotCount = packet->Read<uint8_t>(0);
                ShoulderVisualOverride shoulderOv;

                for (uint8_t s = 0; s < slotCount; ++s)
                {
                    uint8_t slot = packet->Read<uint8_t>(0);
                    packet->Read<uint32_t>(0); // visibleEntry
                    packet->Read<uint32_t>(0); // displayId
                    packet->Read<uint32_t>(0); // transmogEntry
                    int32_t leftShoulderDisplay = packet->Read<int32_t>(-1);
                    int32_t rightShoulderDisplay = packet->Read<int32_t>(-1);
                    packet->Read<uint32_t>(0); // flags

                    if (slot == SERVER_SHOULDER_SLOT)
                    {
                        shoulderOv.leftShoulderDisplay = leftShoulderDisplay;
                        shoulderOv.rightShoulderDisplay = rightShoulderDisplay;
                        shoulderOv.active = (leftShoulderDisplay >= 0 || rightShoulderDisplay >= 0);
                    }
                }

                if (guid)
                {
                    s_charSelectShoulderOverrides[guid] = shoulderOv;
                }
            }
        }

        void Initialize()
        {
            Duskhaven::Network::OnCustomPacket(VISIBLE_ITEM_OVERRIDE_OPCODE, HandleVisibleItemOverride);
            Duskhaven::Network::OnCustomPacket(CHAR_SELECT_SHOULDER_OVERRIDE_OPCODE, HandleCharSelectShoulderOverride);
            Duskhaven::Network::OnCustomPacket(CHAR_SELECT_VISIBLE_ITEM_OPCODE, HandleCharSelectVisibleItems);
        }
    }
}
