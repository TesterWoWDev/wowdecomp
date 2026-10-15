#include "WoW/Source/Duskhaven/Network/CustomPacketBuffer.h"

CustomPacketBuffer::CustomPacketBuffer(chunkSize_t minFragmentSize, totalSize_t quota, chunkSize_t bufferSize)
: m_minFragmentSize(minFragmentSize), m_maxFragmentSize(bufferSize), m_quota(quota), m_cur(0, bufferSize)
{
}

CustomPacketBuffer::~CustomPacketBuffer()
{
    m_cur.Destroy();
}

CustomPacketResult CustomPacketBuffer::callOnSuccess()
{
    return _onSuccess();
}

CustomPacketRead* CustomPacketBuffer::getCur()
{
    return &m_cur;
}

void CustomPacketBuffer::clearPacket()
{
    m_cur.Destroy();
}

CustomPacketResult CustomPacketBuffer::ReceivePacket(chunkSize_t size, char* data, bool skipSuccess)
{
    if (size <= CustomHeaderSize)
    {
        return _onError(CustomPacketResult::NO_HEADER, data);
    }

    if (size > m_maxFragmentSize)
    {
        return _onError(CustomPacketResult::TOO_BIG_FRAGMENT, data);
    }

    if (size + m_cur.Size() > m_quota)
    {
        m_cur.Destroy();
        return _onError(CustomPacketResult::OUT_OF_SPACE, data);
    }

    CustomPacketChunk chnk(size - CustomHeaderSize, data);
    CustomPacketHeader* hdr = chnk.Header();

    switch (hdr->totalFrags)
    {
        case 0:
            return _onError(CustomPacketResult::INVALID_FRAG_COUNT, data);
        case 1:
            AppendFragment(chnk, true);
            if (skipSuccess)
                return CustomPacketResult::HANDLED_MESSAGE;
            else
                return _onSuccess();
        default:
            if (m_cur.ChunkCount() == 0)
            {
                if (hdr->fragmentId != 0)
                {
                    return _onError(CustomPacketResult::INVALID_FIRST_FRAG, data);
                }

                if (size < m_minFragmentSize)
                {
                    return _onError(CustomPacketResult::TOO_SMALL_FRAGMENT, data);
                }
                AppendFragment(chnk, false);
                return CustomPacketResult::HANDLED_FRAGMENT;
            }
            else
            {
                CustomPacketHeader* lastH = m_cur->Chunk(m_cur->ChunkCount() - 1)->Header();
                if (lastH->totalFrags != hdr->totalFrags)
                {
                    return _onError(CustomPacketResult::HEADER_MISMATCH, data);
                }

                if (hdr->fragmentId != lastH->fragmentId + 1)
                {
                    return _onError(CustomPacketResult::INVALID_FRAG_ID, data);
                }
            }

            if (m_cur.ChunkCount() > 0 && hdr->fragmentId == m_cur.Chunk(0)->Header()->totalFrags - 1)
            {
                AppendFragment(chnk, true);
                return _onSuccess();
            }
            else
            {
                if (size < m_minFragmentSize)
                {
                    return _onError(CustomPacketResult::TOO_SMALL_FRAGMENT, data);
                }
                AppendFragment(chnk, false);
                return CustomPacketResult::HANDLED_FRAGMENT;
            }
    }
}

CustomPacketResult CustomPacketBuffer::_onSuccess()
{
    m_cur.Reset();
    OnPacket(&m_cur);
    m_cur.Destroy();
    return CustomPacketResult::HANDLED_MESSAGE;
}

CustomPacketResult CustomPacketBuffer::_onError(CustomPacketResult error, char* data)
{
    OnError(error);
    m_cur.Destroy();
    return error;
}

void CustomPacketBuffer::AppendFragment(CustomPacketChunk& chunk, bool isLast)
{
    if (isLast)
        m_cur.SetOpcode(chunk.Header()->opcode);

    m_cur.Push(chunk);
}

totalSize_t CustomPacketBuffer::Size()
{
    return m_cur.Size();
}
