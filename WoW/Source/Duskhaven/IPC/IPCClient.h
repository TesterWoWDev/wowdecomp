#pragma once

#ifdef _WIN32
#    include <windows.h>
#endif

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "WoW/Source/Duskhaven/IPC/SharedMemoryRingBuffer.h"

namespace Duskhaven::IPC
{
    class IPCClient
    {
      public:
        IPCClient() = default;
        ~IPCClient()
        {
            Disconnect();
        }
        bool Connect(const std::wstring& pipeName);
        void Disconnect();

        bool IsConnected() const;
        bool Send(const std::vector<uint8_t>& payload = {});
        bool Send(const SharedRingSegment* segments, uint32_t segmentCount);

      private:
#ifdef _WIN32
        SharedMemoryRingBuffer channel_;
#endif
        mutable std::mutex mutex_;
    };
} // namespace Duskhaven::IPC
