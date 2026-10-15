#include "WoW/Source/Duskhaven/IPC/IPCClient.h"

#ifdef _WIN32
#    include <cstdio>

namespace Duskhaven::IPC
{
    bool IPCClient::Connect(const std::wstring& pipeName)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel_.Close();

        if (!channel_.Open(pipeName, 2000))
        {
            return false;
        }

        return true;
    }

    void IPCClient::Disconnect()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel_.Close();
    }

    bool IPCClient::IsConnected() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return channel_.IsOpen();
    }

    bool IPCClient::Send(const std::vector<uint8_t>& payload)
    {
        SharedRingSegment segment{payload.empty() ? nullptr : payload.data(),
                                  static_cast<uint32_t>(payload.size())};
        return Send(&segment, 1);
    }

    bool IPCClient::Send(const SharedRingSegment* segments, uint32_t segmentCount)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!channel_.IsOpen())
            return false;

        if (!channel_.Write(segments, segmentCount, 2000))
        {
            channel_.Close();
            return false;
        }
        return true;
    }
} // namespace Duskhaven::IPC

#endif // _WIN32
