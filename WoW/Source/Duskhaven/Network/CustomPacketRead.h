#pragma once

#include "WoW/Source/Duskhaven/Network/CustomPacketBase.h"
#include "WoW/Source/Duskhaven/Network/CustomPacketDefines.h"

class CustomPacketWrite;

class CustomPacketRead : public CustomPacketBase
{
  public:
    CustomPacketRead();
    CustomPacketRead(opcode_t opcode, chunkSize_t chunkSize);
    CustomPacketRead(CustomPacketWrite const& write);
    CustomPacketRead* operator->();

    std::string ReadString(std::string const& def = "");

    template <typename T>
    T Read(T def)
    {
        return CustomPacketBase::Read(def);
    }

    char* ReadBytes(totalSize_t size, bool padStr = false);
};
