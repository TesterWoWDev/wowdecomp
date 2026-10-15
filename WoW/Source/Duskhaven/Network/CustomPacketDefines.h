#pragma once

#include <cstdint>

typedef uint16_t opcode_t;
typedef uint32_t totalSize_t;
constexpr totalSize_t TotalSizeNpos = UINT16_MAX;
typedef uint16_t chunkSize_t;
typedef uint16_t chunkCount_t;

constexpr chunkSize_t MAX_FRAGMENT_SIZE = 30000;
constexpr chunkSize_t MIN_FRAGMENT_SIZE = 25000;
constexpr totalSize_t BUFFER_QUOTA = 8000000;

#define CustomHeaderSize chunkSize_t(sizeof(CustomPacketHeader))

constexpr uint16_t SERVER_TO_CLIENT_OPCODE = 0x102;
constexpr uint16_t CLIENT_TO_SERVER_OPCODE = 0x51f;

namespace Duskhaven
{
    namespace Network
    {
        constexpr uint16_t SERVER_TO_CLIENT_OPCODE = ::SERVER_TO_CLIENT_OPCODE;
        constexpr uint16_t CLIENT_TO_SERVER_OPCODE = ::CLIENT_TO_SERVER_OPCODE;
    }
}

#define CUSTOM_PACKET_API
