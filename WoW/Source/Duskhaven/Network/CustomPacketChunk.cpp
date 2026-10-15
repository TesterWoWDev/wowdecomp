#include "WoW/Source/Duskhaven/Network/CustomPacketChunk.h"
#include <cstring>
#include <string>
#include <utility>

CustomPacketChunk::CustomPacketChunk(CustomPacketChunk const& other) : m_size(other.m_size), m_chunk(nullptr)
{
    if (other.m_chunk != nullptr && other.m_size > 0)
    {
        chunkSize_t fullSize = other.m_size + CustomHeaderSize;
        m_chunk              = new char[fullSize];
        memcpy(m_chunk, other.m_chunk, fullSize);
    }
}

CustomPacketChunk::CustomPacketChunk(CustomPacketChunk&& other) noexcept
    : m_size(other.m_size), m_chunk(other.m_chunk)
{
    other.m_size = 0;
    other.m_chunk = nullptr;
}

CustomPacketChunk& CustomPacketChunk::operator=(CustomPacketChunk const& other)
{
    if (this != &other)
    {
        CustomPacketChunk copy(other);
        *this = std::move(copy);
    }
    return *this;
}

CustomPacketChunk& CustomPacketChunk::operator=(CustomPacketChunk&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_size = other.m_size;
        m_chunk = other.m_chunk;
        other.m_size = 0;
        other.m_chunk = nullptr;
    }
    return *this;
}

CustomPacketChunk::CustomPacketChunk(chunkSize_t size, char* chunk) : m_size(size), m_chunk(chunk) {}

CustomPacketChunk::CustomPacketChunk(chunkSize_t size) : m_size(size), m_chunk(new char[size + CustomHeaderSize]) {}

CustomPacketChunk::CustomPacketChunk() : CustomPacketChunk(0, nullptr) {}

void CustomPacketChunk::Destroy()
{
    delete[] m_chunk;
    m_chunk = nullptr;
    m_size = 0;
}

char* CustomPacketChunk::Data()
{
    return m_chunk;
}

CustomPacketHeader* CustomPacketChunk::Header()
{
    return (CustomPacketHeader*)m_chunk;
}

void CustomPacketChunk::Increase(chunkSize_t size)
{
    if (m_chunk == nullptr)
    {
        m_chunk = new char[size + CustomHeaderSize];
    }
    else
    {
        char* old = m_chunk;
        m_chunk   = new char[m_size + size + CustomHeaderSize];
        memcpy(m_chunk, old, m_size + CustomHeaderSize);
        m_size = m_size + size;
        delete[] old;
    }
}

chunkSize_t CustomPacketChunk::FullSize()
{
    return Size() + CustomHeaderSize;
}

chunkSize_t CustomPacketChunk::Size()
{
    return m_size;
}

chunkSize_t CustomPacketChunk::RemBytes(chunkSize_t idx)
{
    return m_size - idx;
}

char* CustomPacketChunk::Offset(chunkSize_t offset)
{
    return m_chunk + CustomHeaderSize + offset;
}

void CustomPacketChunk::WriteBytes(chunkSize_t idx, chunkSize_t size, char const* value)
{
    memcpy(Offset(idx), value, size);
}

void CustomPacketChunk::ReadBytes(chunkSize_t idx, chunkSize_t size, char* out)
{
    memcpy(out, Offset(idx), size);
}

void CustomPacketChunk::Print(std::function<void(std::ostream&, uint8_t)> fn, std::ostream& stream, size_t indent)
{
    std::string spaces = std::string(indent, ' ');
    for (size_t i = 0; i < indent; ++i)
    {
        stream << ' ';
    }
    stream << "chunk [ header (";
    for (size_t i = 0; i < CustomHeaderSize; ++i)
    {
        fn(stream, (uint8_t)m_chunk[i]);
        if (i < CustomHeaderSize - 1)
            stream << ",";
    }
    stream << "), ";
    for (chunkSize_t i = 0; i < m_size; ++i)
    {
        fn(stream, (uint8_t)Offset(i)[0]);
        if (i < chunkSize_t(m_size - 1))
            stream << ",";
    }
    stream << "]\n";
}

void CustomPacketChunk::PrintBytes(std::ostream& stream, size_t indent)
{
    return Print([](auto& s, uint8_t byte) { s << "0x" << std::hex << uint32_t(byte); }, stream, indent);
}

void CustomPacketChunk::PrintAscii(std::ostream& stream, size_t indent)
{
    return Print([](auto& s, uint8_t byte) { s << char(byte); }, stream, indent);
}
