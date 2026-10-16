#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace Patcher::Http
{
    bool IsHttpUrl(const std::string& url);
    bool IsSecureOrLoopbackUrl(const std::string& url);
    bool HaveSameOrigin(const std::string& first, const std::string& second);
    bool ResolveUrl(const std::string& baseUrl, const std::string& target, std::string& out, std::string& error);
    // maxBytes == 0 disables the download-size guard.
    bool DownloadString(const std::string& url, size_t maxBytes, std::string& out, std::string& error);
    struct ResumeOptions
    {
        uint64_t resumeFrom   = 0;
        uint64_t expectedSize = 0;
        // Chunked whole-file downloads write independent payloads into
        // non-overlapping ranges of one preallocated destination.
        uint64_t destinationOffset = 0;
        bool preserveDestination   = false;
        // When supplied, the download is hashed while bytes are written. A
        // resumed prefix is hashed once before the request continues.
        std::string expectedMd5;
        bool flushToDisk = true;
        std::function<bool(uint64_t, uint64_t)> onProgress;
    };

    // Resumes on HTTP 206; a normal 200 response restarts from byte zero.
    // Transient transport/server failures retry from the bytes already written.
    bool DownloadFileResumable(const std::string& url, const std::string& destinationPath, const ResumeOptions& options,
                               std::string& error);
} // namespace Patcher::Http
