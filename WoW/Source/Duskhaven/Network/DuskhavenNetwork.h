#pragma once

#include "WoW/Source/Duskhaven/Network/CustomPacketDefines.h"
#include "WoW/Source/Duskhaven/Network/CustomPacketRead.h"
#include "WoW/Source/Duskhaven/Network/CustomPacketWrite.h"
#include <functional>

class CDataStore;

namespace Duskhaven
{
    namespace Network
    {
        void Initialize();
        int32_t HandleServerToClientPacket(void* param, uint32_t msgId, uint32_t time, CDataStore* msg);

        int OnCustomPacket(opcode_t opcode, std::function<void(CustomPacketRead*)> callback);
        void SendCustomPacket(opcode_t opcode, totalSize_t size, std::function<void(CustomPacketWrite&)> writer);

        void RegisterLuaFunctions();
    } // namespace Network
} // namespace Duskhaven

#define ON_CUSTOM_PACKET(__listener_name, opcode, callback) \
    [[maybe_unused]] static const int __listener_name##__Result = Duskhaven::Network::OnCustomPacket(opcode, callback);
