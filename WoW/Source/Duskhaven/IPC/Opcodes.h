#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace Duskhaven::IPC
{
    class Opcode
    {
      public:
        enum Value : uint32_t
        {
            DEFAULT_VALUE = 0x0,

            // Voice / Call control
            CMSG_VOICE_START_CALL    = 0x1101,
            CMSG_VOICE_LEAVE_CALL    = 0x1102,
            CMSG_VOICE_END_ALL_CALLS = 0x1103,

            // Self controls
            CMSG_VOICE_SET_SELF_MUTE     = 0x1110,
            CMSG_VOICE_SET_SELF_DEAF     = 0x1111,
            CMSG_VOICE_SET_INPUT_VOLUME  = 0x1112,
            CMSG_VOICE_SET_OUTPUT_VOLUME = 0x1113,

            // Per-user controls
            CMSG_VOICE_SET_LOCAL_MUTE  = 0x1120,
            CMSG_VOICE_SET_USER_VOLUME = 0x1121,

            // Input modes / tuning
            CMSG_VOICE_SET_PTT_ACTIVE        = 0x1130,
            CMSG_VOICE_SET_PTT_RELEASE_DELAY = 0x1131,
            CMSG_VOICE_SET_VAD_THRESHOLD     = 0x1132,
            CMSG_VOICE_SET_AUDIO_MODE        = 0x1133,

            // Device / processing controls
            CMSG_VOICE_SET_INPUT_DEVICE           = 0x1140,
            CMSG_VOICE_SET_OUTPUT_DEVICE          = 0x1141,
            CMSG_VOICE_SET_AUTOMATIC_GAIN_CONTROL = 0x1142,
            CMSG_VOICE_SET_ECHO_CANCELLATION      = 0x1143,
            CMSG_VOICE_SET_NOISE_SUPPRESSION      = 0x1144,

            // Rich presence
            CMSG_DISCORD_SET_GAME_PRESENCE   = 0x1200, // characterName, level, className, zoneName
            CMSG_DISCORD_CLEAR_GAME_PRESENCE = 0x1201, // (no payload)

            // Terrain bridge
            CMSG_TERRAIN_FRAME_SNAPSHOT  = 0x1300,
            CMSG_TERRAIN_PAGE_ACK_BATCH  = 0x1301,
            CMSG_TERRAIN_DEMAND_FEEDBACK = 0x1302,
            CMSG_WMO_ROOT_TELEMETRY      = 0x1310,
            CMSG_WMO_DRAW_TELEMETRY      = 0x1311,

            // Server/SDK -> Client push
            SMSG_LOBBY_JOINED  = 0x2001,
            SMSG_LOBBY_LEFT    = 0x2002,
            SMSG_LOBBY_UPDATED = 0x2003,

            SMSG_VOICE_CALL_STATUS       = 0x2100,
            SMSG_VOICE_PARTICIPANTS      = 0x2101,
            SMSG_VOICE_SPEAKING          = 0x2102,
            SMSG_VOICE_SELF_STATE        = 0x2103,
            SMSG_VOICE_PARTICIPANT_STATE = 0x2104,
            SMSG_VOICE_DEVICES           = 0x2105,
            SMSG_VOICE_DEVICE_STATE      = 0x2106,
            SMSG_VOICE_ERROR             = 0x21FF,

            // Terrain authority
            SMSG_TERRAIN_DRAW_COMMANDS  = 0x2300,
            SMSG_TERRAIN_PAGE_BATCH     = 0x2301,
            SMSG_TERRAIN_MATERIAL_BATCH = 0x2302,
            SMSG_WMO_AUTHORITY_STATS    = 0x2310,
            SMSG_WMO_LOD_BATCH          = 0x2311
        };

        constexpr Opcode() : value_(DEFAULT_VALUE) {}
        constexpr Opcode(Value v) : value_(v) {}
        constexpr explicit Opcode(uint32_t v) : value_(static_cast<Value>(v)) {}
        constexpr uint32_t raw() const { return value_; }

        constexpr bool operator==(Opcode other) const { return value_ == other.value_; }
        constexpr bool operator!=(Opcode other) const { return value_ != other.value_; }
        constexpr bool operator==(uint32_t other) const { return value_ == other; }
        constexpr bool operator!=(uint32_t other) const { return value_ != other; }

      private:
        Value value_;
    };

    struct OpcodeHash
    {
        size_t operator()(const Opcode& op) const noexcept
        {
            return std::hash<uint32_t>{}(op.raw());
        }
    };
} // namespace Duskhaven::IPC
