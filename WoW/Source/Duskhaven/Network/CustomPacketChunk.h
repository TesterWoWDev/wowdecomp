#pragma once

#include "WoW/Source/Duskhaven/Network/CustomPacketDefines.h"
#include <cstdint>
#include <functional>
#include <iostream>

#pragma pack(push, 1)
struct CustomPacketHeader
{
    chunkCount_t fragmentId;
    chunkCount_t totalFrags;
    opcode_t opcode;
};
#pragma pack(pop)

struct CustomPacketChunk
{
  public:
    CustomPacketChunk(CustomPacketChunk const& other);
    CustomPacketChunk(CustomPacketChunk&& other) noexcept;
    CustomPacketChunk& operator=(CustomPacketChunk const& other);
    CustomPacketChunk& operator=(CustomPacketChunk&& other) noexcept;
    CustomPacketChunk(chunkSize_t size, char* chunk);
    CustomPacketChunk(chunkSize_t size);
    CustomPacketChunk();
    void Destroy();
    char* Data();
    CustomPacketHeader* Header();
    void Increase(chunkSize_t size);
    chunkSize_t FullSize();
    chunkSize_t Size();
    chunkSize_t RemBytes(chunkSize_t idx);
    char* Offset(chunkSize_t offset);
    void WriteBytes(chunkSize_t idx, chunkSize_t size, char const* value);
    void ReadBytes(chunkSize_t idx, chunkSize_t size, char* out);

    void Print(std::function<void(std::ostream&, uint8_t)> fn, std::ostream& stream = std::cout, size_t indent = 0);
    void PrintBytes(std::ostream& stream = std::cout, size_t indent = 0);
    void PrintAscii(std::ostream& stream = std::cout, size_t indent = 0);

    template <typename T>
    void Write(chunkSize_t index, T value)
    {
        *((T*)(Offset(index))) = value;
    }

    template <typename T>
    T Read(chunkSize_t index)
    {
        return *((T*)(Offset(index)));
    }

  private:
    chunkSize_t m_size;
    char* m_chunk;

    friend class CustomPacketBuffer;
};
