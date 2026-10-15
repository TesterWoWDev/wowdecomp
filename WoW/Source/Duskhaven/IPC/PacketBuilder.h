#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "WoW/Source/Duskhaven/IPC/IPCClient.h"
#include "WoW/Source/Duskhaven/IPC/Opcodes.h"

namespace Duskhaven::IPC
{
#pragma pack(push, 1)
    struct MsgHeader
    {
        Opcode opcode;
        uint32_t length;
    };
#pragma pack(pop)

    static const std::wstring PIPE_NAME = L"duskhaven_social_sdk_pipe";

    inline IPCClient& CommandIPCClient()
    {
        static IPCClient client;
        return client;
    }

    inline std::mutex& CommandIPCMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline void append_u32_le(std::vector<uint8_t>& out, uint32_t v)
    {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    inline void append_u64_le(std::vector<uint8_t>& out, uint64_t v)
    {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 32) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 40) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 48) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 56) & 0xFF));
    }

    inline void append_bool(std::vector<uint8_t>& out, bool v)
    {
        out.push_back(v ? 1 : 0);
    }

    inline void append_f32_le(std::vector<uint8_t>& out, float v)
    {
        uint32_t raw;
        static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
        std::memcpy(&raw, &v, sizeof(uint32_t));
        append_u32_le(out, raw);
    }

    inline uint32_t read_u32_le(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    inline uint64_t read_u64_le(const uint8_t* p)
    {
        return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
               ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
    }

    inline bool read_bool(const uint8_t* p)
    {
        return p[0] != 0;
    }

    inline float read_f32_le(const uint8_t* p)
    {
        uint32_t raw = read_u32_le(p);
        float value;
        std::memcpy(&value, &raw, sizeof(float));
        return value;
    }

    class PacketBuilder
    {
      public:
        static PacketBuilder CreatePacket(Opcode opcode)
        {
            PacketBuilder pb;
            pb.header_.opcode = opcode;
            pb.header_.length = 0;
            return pb;
        }

        PacketBuilder& writeUInt32(uint32_t v)
        {
            append_u32_le(buffer_, v);
            return *this;
        }

        PacketBuilder& writeUInt64(uint64_t v)
        {
            append_u64_le(buffer_, v);
            return *this;
        }

        PacketBuilder& writeBool(bool v)
        {
            append_bool(buffer_, v);
            return *this;
        }

        PacketBuilder& writeFloat(float v)
        {
            append_f32_le(buffer_, v);
            return *this;
        }

        PacketBuilder& writeString(std::string_view s)
        {
            if (s.size() > UINT32_MAX)
            {
                throw std::runtime_error("String too large to serialize with uint32 length");
            }
            writeUInt32(static_cast<uint32_t>(s.size()));
            buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(s.data()),
                           reinterpret_cast<const uint8_t*>(s.data()) + s.size());
            return *this;
        }

        PacketBuilder& writeBytes(const void* data, uint32_t size)
        {
            if (!data || size == 0)
            {
                return *this;
            }

            const auto* bytes = static_cast<const uint8_t*>(data);
            buffer_.insert(buffer_.end(), bytes, bytes + size);
            return *this;
        }

        void Send()
        {
            std::lock_guard<std::mutex> lock(CommandIPCMutex());

            const size_t payloadLen = buffer_.size();
            if (payloadLen > UINT32_MAX)
            {
                throw std::runtime_error("Payload too large for uint32 length");
            }
            header_.length = static_cast<uint32_t>(payloadLen);

            uint8_t headerBytes[sizeof(MsgHeader)]{};
            StoreU32LE(headerBytes, header_.opcode.raw());
            StoreU32LE(headerBytes + sizeof(uint32_t), header_.length);

            IPCClient& client = CommandIPCClient();
            if (!client.IsConnected() && !client.Connect(PIPE_NAME))
            {
                return;
            }

            SharedRingSegment segments[2] = {
                {headerBytes, static_cast<uint32_t>(sizeof(headerBytes))},
                {buffer_.empty() ? nullptr : buffer_.data(), static_cast<uint32_t>(buffer_.size())}};
            const uint32_t segmentCount = buffer_.empty() ? 1u : 2u;
            if (!client.Send(segments, segmentCount))
            {
                if (!client.Connect(PIPE_NAME) || !client.Send(segments, segmentCount))
                {
                    // Failed to send
                }
            }
        }

        const std::vector<uint8_t>& bytes() const
        {
            return buffer_;
        }

      private:
        PacketBuilder() = default;

        MsgHeader header_{};
        std::vector<uint8_t> buffer_;
    };
} // namespace Duskhaven::IPC
