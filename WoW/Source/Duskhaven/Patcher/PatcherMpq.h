#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Patcher::Mpq
{
    using ProgressCallback =
        std::function<void(uint64_t completedBytes, uint64_t totalBytes, size_t completedFiles, size_t totalFiles)>;

    struct FileReplacement
    {
        std::string logicalPath;
        // Bundles are read directly; loose payloads use downloadedPath.
        std::string downloadedPath;
        std::string bundlePath;
        uint64_t size = 0;
        std::string md5;
        std::string sha256;
    };

    struct FileExpectation
    {
        std::string logicalPath;
        uint64_t size = 0;
        std::string md5;
    };

    using MatchProgressCallback = ProgressCallback;

    bool ArchiveContainsFile(const std::string& archivePath, const std::string& logicalPath);
    bool MatchArchiveFilesMd5(const std::string& archivePath, const std::vector<FileExpectation>& files,
                              std::vector<bool>& matches, std::string& error,
                              const MatchProgressCallback& onProgress = {});

    bool InstallChangesInMountedArchive(const std::string& archivePath, const std::vector<FileReplacement>& files,
                                        const std::vector<std::string>& deletedFiles,
                                        const ProgressCallback& onProgress, std::string& error);
} // namespace Patcher::Mpq
