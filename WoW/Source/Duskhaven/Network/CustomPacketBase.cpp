#include "WoW/Source/Duskhaven/Network/CustomPacketBase.h"
#include <stdexcept>
#include <cmath>
#include <string>

CustomPacketBase::CustomPacketBase(CustomPacketBase const& base)
: m_size(base.m_size), m_maxChunkSize(base.m_maxChunkSize), m_idx(0), m_global_idx(0), m_chunk(0),
  m_chunks(base.m_chunks), m_opcode(base.m_opcode)
{
}

CustomPacketBase::CustomPacketBase() : m_size(0), m_maxChunkSize(0), m_idx(0), m_global_idx(0), m_chunk(0), m_opcode(0)
{
}

CustomPacketBase::CustomPacketBase(opcode_t opcode, chunkSize_t maxChunkSize, totalSize_t initialSize)
: m_size(0), m_idx(0), m_global_idx(0), m_chunk(0), m_maxChunkSize(maxChunkSize), m_opcode(opcode)
{
    if (maxChunkSize <= CustomHeaderSize)
    {
        throw std::runtime_error("Maximum chunk size (" + std::to_string(maxChunkSize) +
                                 ") is <= message header size (" + std::to_string(CustomHeaderSize) +
                                 "), you can't send any messages!");
    }

    if (initialSize > 0)
    {
        Increase(initialSize);
    }
}

std::vector<CustomPacketChunk>& CustomPacketBase::buildMessages()
{
    for (chunkCount_t i = 0; i < (chunkCount_t)m_chunks.size(); ++i)
    {
        CustomPacketChunk& chnk = m_chunks[i];
        CustomPacketHeader* hdr = chnk.Header();
        hdr->opcode             = m_opcode;
        hdr->fragmentId         = i;
        hdr->totalFrags         = chunkCount_t(m_chunks.size());
    }
    return m_chunks;
}

void CustomPacketBase::Reset()
{
    m_chunk      = 0;
    m_idx        = 0;
    m_global_idx = 0;
}

void CustomPacketBase::Destroy()
{
    for (auto& chunk : m_chunks)
    {
        chunk.Destroy();
    }
    m_chunks.clear();
    m_size = 0;
    Reset();
}

void CustomPacketBase::Push(CustomPacketChunk& chnk)
{
    m_size += chnk.Size();
    m_chunks.push_back(chnk);
}

totalSize_t CustomPacketBase::Size()
{
    return m_size;
}

CustomPacketChunk* CustomPacketBase::Chunk(chunkCount_t index)
{
    return &m_chunks[index];
}

chunkSize_t CustomPacketBase::ChunkSize(chunkCount_t index)
{
    return m_chunks[index].Size();
}

chunkCount_t CustomPacketBase::ChunkCount()
{
    return chunkCount_t(m_chunks.size());
}

void CustomPacketBase::Print(std::function<void(std::ostream&, uint8_t)> cb, std::ostream& stream)
{
    stream << "message [\n";
    for (size_t chnkCtr = 0; chnkCtr < m_chunks.size(); ++chnkCtr)
    {
        CustomPacketChunk& chnk = m_chunks[chnkCtr];
        chnk.Print(cb, stream, 2);
    }
    stream << "]\n";
}

void CustomPacketBase::PrintBytes(std::ostream& stream)
{
    return Print([](auto& s, uint8_t byte) { s << "0x" << std::hex << uint32_t(byte); }, stream);
}

void CustomPacketBase::PrintAscii(std::ostream& stream)
{
    return Print([](auto& s, uint8_t byte) { s << char(byte); }, stream);
}

chunkSize_t CustomPacketBase::MaxWritableChunkSize()
{
    return m_maxChunkSize - CustomHeaderSize;
}

void CustomPacketBase::Clear()
{
    m_chunks.clear();
    m_size = 0;
    Reset();
}

void CustomPacketBase::Increase(totalSize_t increase)
{
    totalSize_t remIncrease = increase;
    if (m_chunks.size() > 0)
    {
        CustomPacketChunk& last = m_chunks[m_chunks.size() - 1];
        chunkSize_t chunkRem    = MaxWritableChunkSize() - last.Size();
        if (chunkRem > 0)
        {
            chunkSize_t toInc = (chunkSize_t)std::min((totalSize_t)chunkRem, remIncrease);
            last.Increase(toInc);
            remIncrease -= toInc;
        }
    }

    if (remIncrease > 0)
    {
        chunkCount_t chunks = (chunkCount_t)std::ceil((double)remIncrease / (double)MaxWritableChunkSize());
        for (chunkCount_t i = 0; i < chunks; ++i)
        {
            chunkSize_t chunkSize = (chunkSize_t)std::min((totalSize_t)MaxWritableChunkSize(), remIncrease);
            m_chunks.push_back(CustomPacketChunk(chunkSize));
            remIncrease -= chunkSize;
        }
    }
    m_size += increase;
}

void CustomPacketBase::WriteBytes(totalSize_t size, char const* bytes)
{
    totalSize_t remSize = size;
    chunkCount_t chunk  = 0;
    if (m_chunks.size() > 0)
    {
        chunk = (chunkCount_t)(m_chunks.size() - 1);
    }

    while (remSize > 0)
    {
        Increase(remSize);
        CustomPacketChunk& chnk = m_chunks[chunk];
        chunkSize_t chunkSize   = chnk.Size();
        chunkSize_t toWrite     = (chunkSize_t)std::min(remSize, (totalSize_t)chunkSize);
        chnk.WriteBytes(chunkSize - toWrite, toWrite, bytes + (size - remSize));
        remSize -= toWrite;
        chunk++;
    }
}

void CustomPacketBase::incIdx(chunkSize_t amount)
{
    m_idx += amount;
    m_global_idx += amount;
    if (m_chunk < m_chunks.size() && m_idx >= m_chunks[m_chunk].Size())
    {
        m_idx = 0;
        m_chunk++;
    }
}

void CustomPacketBase::ReadBytes(totalSize_t size, char* bytes)
{
    totalSize_t rem = size;
    while (rem > 0)
    {
        if (m_chunk >= m_chunks.size())
        {
            return;
        }
        CustomPacketChunk& curChunk = m_chunks[m_chunk];
        chunkSize_t readAmount      = (chunkSize_t)std::min(rem, (totalSize_t)curChunk.RemBytes(m_idx));
        curChunk.ReadBytes(m_idx, readAmount, bytes + (size - rem));
        incIdx(readAmount);
        rem -= readAmount;
    }
}

char* CustomPacketBase::ReadBytes(totalSize_t size, bool padStr)
{
    if (size > m_size - m_global_idx)
    {
        return nullptr;
    }

    char* ret = new char[size + (padStr ? 1 : 0)];
    ReadBytes(size, ret);
    return ret;
}

opcode_t CustomPacketBase::Opcode()
{
    return m_opcode;
}
