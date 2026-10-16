#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace RuntimeUpdate
{
    struct ReleasePointer
    {
        std::string indexUrl;
        std::string manifestUrl;
        std::string manifestSha256;

        bool Available() const
        {
            return !indexUrl.empty() && !manifestUrl.empty() && !manifestSha256.empty();
        }
    };

    struct ProgressCallbacks
    {
        std::function<void(uint64_t, uint32_t)> beginDownloads;
        std::function<void(const std::string&, uint64_t)> beginFile;
        std::function<bool(uint64_t, uint64_t)> updateFile;
        std::function<void(uint64_t)> completeFile;
    };

    enum class Result
    {
        Current,
        RestartRequired,
    };

    // Starts a helper that must survive the current client process. The
    // implementation verifies that the child escaped any kill-on-close job
    // before allowing it to run.
    bool LaunchDetachedHelper(const std::filesystem::path& executable, const std::wstring& command,
                              const std::filesystem::path& workingDirectory, std::string& error);

    bool Prepare(const ReleasePointer& release, const std::string& gameRoot, bool forceRepair,
                 const ProgressCallbacks& progress, Result& result, std::string& error);
} // namespace RuntimeUpdate
