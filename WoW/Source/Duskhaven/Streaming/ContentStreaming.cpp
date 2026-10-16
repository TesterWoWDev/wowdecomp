#include "ContentStreaming.h"
#include "WoW/Source/Duskhaven/Patcher/WineHostMount.h"
#include "WoW/Source/Duskhaven/Patcher/PatcherFilesystem.h"
#include "WoW/Source/Duskhaven/Patcher/PatcherHash.h"
#include "WoW/Source/Duskhaven/Patcher/PatcherHttp.h"
#include "WoW/Source/Duskhaven/Patcher/PatcherJson.h"
#include "WoW/Source/Duskhaven/Patcher/PatcherMpq.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/DBC/CDBC.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "StreamingLogger.h"
#include <Windows.h>
#include <winioctl.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <winternl.h>
#include "ClientArchive.h"
#include "DbcFromMpq.h"
#include "StreamingBootstrap.h"
#include "RuntimeUpdate.h"
#include "StreamingValidation.h"
#include "ContentUpdateLock.h"
#include "StockPatchDownload.h"

namespace
{
    enum class Phase
    {
        Disabled,
        Checking,
        Downloading,
        Verifying,
        Installing,
        Complete,
        RestartRequired,
        Failed,
        Canceled,
    };

    struct PayloadChunk
    {
        std::string payloadPath;
        uint32_t index = 0;
        uint32_t count = 0;
        uint64_t size  = 0;
        std::string md5;
    };

    struct ManifestEntry
    {
        enum class Kind
        {
            FullMpq,
            FullMpqIfMissing,
            MpqMember,
            MpqDelete,
        };

        Kind kind = Kind::FullMpq;
        std::string payloadPath;
        std::string targetPath;
        std::string containerPath;
        std::string memberPath;
        std::string archiveRevision;
        uint64_t size = 0;
        std::string md5;
        bool bundled         = false;
        uint64_t payloadSize = 0;
        std::string payloadMd5;
        std::vector<PayloadChunk> chunks;
    };

    struct Manifest
    {
        std::string sourceUrl;
        std::vector<ManifestEntry> entries;
    };

    // Bound attacker-controlled allocations while leaving ample room above the current catalog.
    constexpr size_t LATEST_INDEX_DOWNLOAD_LIMIT       = 256 * 1024;
    constexpr size_t RELEASE_MANIFEST_DOWNLOAD_LIMIT   = 128 * 1024 * 1024;
    constexpr uint32_t CONTENT_STREAMING_START_OPCODE  = 0x7A50;
    constexpr uint32_t CONTENT_STREAMING_REPAIR_OPCODE = 0x7A51;

    std::atomic<bool> g_started{false};
    std::atomic<bool> g_running{false};
    std::atomic<bool> g_recheckRequested{false};
    std::atomic<bool> g_forceRepairRequested{false};
    std::atomic<bool> g_cancelRequested{false};
    std::atomic<bool> g_downloadsFinished{false};
    // Keep this across fallback/catch-up passes: the last pass may have no work
    // even though this launch installed files. Download totals alone also miss
    // cached replacements and deletion-only MPQ updates.
    std::atomic<bool> g_contentUpdateRequired{false};
    std::atomic<Phase> g_phase{Phase::Disabled};
    std::atomic<uint64_t> g_completedBytes{0};
    std::atomic<uint64_t> g_currentFileBytes{0};
    std::atomic<uint64_t> g_totalBytes{0};
    std::atomic<uint32_t> g_filesDone{0};
    std::atomic<uint32_t> g_filesTotal{0};
    std::atomic<uint64_t> g_downloadCompletedBytes{0};
    std::atomic<uint64_t> g_downloadTotalBytes{0};
    std::atomic<uint32_t> g_downloadFilesDone{0};
    std::atomic<uint32_t> g_downloadFilesTotal{0};
    std::atomic<bool> g_postExitPromotionScheduled{false};
    std::atomic<bool> g_catchUpAfterFallbackMount{false};
    std::atomic<bool> g_interfaceReloadRequired{false};
    std::atomic<bool> g_runtimeRestartRequired{false};
    std::atomic<bool> g_activationFailed{false};
    ContentStreaming::UpdateLock g_updateLock;

    std::mutex g_statusMutex;
    std::string g_currentFile;
    std::string g_error;
    lua_State* g_stockDownloadGlueState = nullptr;
    std::chrono::steady_clock::time_point g_nextStockDownloadAttempt{};
    bool g_glueReloadRequired = false;
    bool g_glueReloadPending = false;



    struct PendingInstalledRelease
    {
        std::string gameRoot;
        std::string manifestContents;
        std::string releaseVersion;
        bool ready = false;
    };

    std::mutex g_installedReleaseMutex;
    PendingInstalledRelease g_pendingInstalledRelease;

    void ClearPendingInstalledRelease();
    void CommitPendingInstalledRelease();

    struct LocalHashEntry
    {
        uint64_t size    = 0;
        int64_t modified = 0;
        std::string md5;
    };

    std::map<std::string, LocalHashEntry> g_localHashes;
    std::string g_localHashCachePath;

    struct MountRequest
    {
        std::string path;
        std::string dedupeKey;
        bool refreshExisting   = false;
        bool refreshNativeDbcs = true;
        bool refreshAllDbcs    = true;
        std::vector<std::string> changedMembers;
        bool reloadGlue = false;
        bool deferActivation = false;
    };

    // Fallbacks are seed archives, not necessarily the current release. Keep
    // their tables/UI inactive until the member catch-up pass has verified them.
    // Accessed under g_mountMutex, like the mount queue.
    std::map<std::string, MountRequest> g_deferredFallbackActivations;

    constexpr int MOUNT_PRIORITY_BASE = 1000000;
    std::atomic<int> g_nextMountPriority{MOUNT_PRIORITY_BASE};
    std::atomic<uint32_t> g_pendingMountCount{0};
    std::mutex g_mountMutex;
    std::vector<MountRequest> g_mountQueue;
    std::set<std::string> g_queuedMounts;
    std::mutex g_promotionMutex;
    std::set<std::string> g_scheduledPromotions;

    class ScopedBackgroundThreadMode
    {
    public:
        explicit ScopedBackgroundThreadMode(bool enabled)
        {
            if (enabled)
                active_ = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN) != FALSE;
        }

        ~ScopedBackgroundThreadMode()
        {
            if (active_)
                SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
        }

        ScopedBackgroundThreadMode(const ScopedBackgroundThreadMode&)            = delete;
        ScopedBackgroundThreadMode& operator=(const ScopedBackgroundThreadMode&) = delete;

    private:
        bool active_ = false;
    };

    std::string Trim(std::string value)
    {
        auto whitespace = [](unsigned char character) { return std::isspace(character) != 0; };
        value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
        value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(), value.end());
        return value;
    }

    std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }

    enum class SeekPenalty
    {
        Unknown,
        Incurs,
        DoesNotIncur,
    };

    bool ResolveVolumePath(const std::string& targetPath, std::string& volumePath)
    {
        char buffer[MAX_PATH] = {};
        if (GetVolumePathNameA(targetPath.c_str(), buffer, MAX_PATH))
        {
            volumePath = buffer;
            return true;
        }

        const std::filesystem::path parent = std::filesystem::path(targetPath).parent_path();
        if (!parent.empty() && GetVolumePathNameA(parent.string().c_str(), buffer, MAX_PATH))
        {
            volumePath = buffer;
            return true;
        }

        return false;
    }

    SeekPenalty QuerySeekPenalty(const std::string& volumePath)
    {
        char volumeName[MAX_PATH] = {};
        if (!GetVolumeNameForVolumeMountPointA(volumePath.c_str(), volumeName, MAX_PATH))
            return SeekPenalty::Unknown;

        std::string devicePath = volumeName;
        while (!devicePath.empty() && (devicePath.back() == '\\' || devicePath.back() == '/'))
            devicePath.pop_back();

        HANDLE volume = CreateFileA(devicePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (volume == INVALID_HANDLE_VALUE)
            return SeekPenalty::Unknown;

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId             = StorageDeviceSeekPenaltyProperty;
        query.QueryType              = PropertyStandardQuery;
        DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor = {};
        DWORD bytesReturned                         = 0;
        const BOOL queried = DeviceIoControl(volume, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor,
                                             sizeof(descriptor), &bytesReturned, nullptr);
        CloseHandle(volume);

        if (!queried || bytesReturned < sizeof(descriptor))
            return SeekPenalty::Unknown;
        return descriptor.IncursSeekPenalty ? SeekPenalty::Incurs : SeekPenalty::DoesNotIncur;
    }

    SeekPenalty GetSeekPenalty(const std::string& targetPath)
    {
        std::string volumePath;
        if (!ResolveVolumePath(targetPath, volumePath))
            return SeekPenalty::Unknown;

        const std::string cacheKey = Lowercase(volumePath);
        static std::mutex cacheMutex;
        static std::map<std::string, SeekPenalty> cache;
        std::lock_guard<std::mutex> lock(cacheMutex);
        const auto cached = cache.find(cacheKey);
        if (cached != cache.end())
            return cached->second;

        const SeekPenalty penalty = QuerySeekPenalty(volumePath);
        cache.emplace(cacheKey, penalty);
        if (penalty == SeekPenalty::DoesNotIncur)
            LOG_INFO << "ContentStreaming: update volume " << volumePath << " reports no seek penalty";
        else if (penalty == SeekPenalty::Incurs)
            LOG_INFO << "ContentStreaming: update volume " << volumePath << " reports a seek penalty";
        else
            LOG_WARN << "ContentStreaming: could not determine the seek penalty for " << volumePath;
        return penalty;
    }

    bool IsInterfaceReloadMember(const ManifestEntry& entry)
    {
        std::string path = Lowercase(entry.memberPath);
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.rfind("interface/", 0) != 0)
            return false;

        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos)
            return false;
        const std::string extension = path.substr(dot);
        return extension == ".lua" || extension == ".xml" || extension == ".toc";
    }

    const char* PhaseName(Phase phase)
    {
        switch (phase)
        {
            case Phase::Disabled:
                return "disabled";
            case Phase::Checking:
                return "checking";
            case Phase::Downloading:
                return "downloading";
            case Phase::Verifying:
                return "verifying";
            case Phase::Installing:
                return "installing";
            case Phase::Complete:
                return "complete";
            case Phase::RestartRequired:
                return "restart-required";
            case Phase::Failed:
                return "failed";
            case Phase::Canceled:
                return "canceled";
        }
        return "unknown";
    }

    bool IsBusyPhase(Phase phase)
    {
        return phase == Phase::Downloading || phase == Phase::Verifying || phase == Phase::Installing;
    }

    bool IsLoginBlockedPhase(Phase phase)
    {
        return phase == Phase::Checking || phase == Phase::RestartRequired || IsBusyPhase(phase);
    }

    bool IsNativeLoginRejectedPhase(Phase phase)
    {
        return phase == Phase::Failed || phase == Phase::Canceled || IsLoginBlockedPhase(phase) ||
               !CDBC::PendingCompatibilityError().empty();
    }

    void SetStatus(Phase phase, const std::string& currentFile = {}, const std::string& error = {})
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_currentFile = currentFile;
        g_error       = error;
        g_phase       = phase;
    }

    void BeginPhase(Phase phase, const std::string& currentFile, uint64_t totalBytes, uint32_t totalFiles)
    {
        g_completedBytes   = 0;
        g_currentFileBytes = 0;
        g_totalBytes       = totalBytes;
        g_filesDone        = 0;
        g_filesTotal       = totalFiles;
        SetStatus(phase, currentFile);
    }

    void BeginDownload(const std::string& currentFile, uint64_t resumeFrom)
    {
        g_completedBytes   = g_downloadCompletedBytes.load();
        g_currentFileBytes = resumeFrom;
        g_totalBytes       = g_downloadTotalBytes.load();
        g_filesDone        = g_downloadFilesDone.load();
        g_filesTotal       = g_downloadFilesTotal.load();
        SetStatus(Phase::Downloading, currentFile);
    }

    void CompleteDownload(uint64_t bytes)
    {
        const uint64_t completedBytes = g_downloadCompletedBytes.fetch_add(bytes) + bytes;
        const uint32_t filesDone      = g_downloadFilesDone.fetch_add(1) + 1;

        // Keep the public Glue progress counters in sync immediately.  There
        // may not be another BeginDownload after the final file (runtime
        // updates transition straight to RestartRequired), so deferring this
        // copy can otherwise leave the completed transfer displayed as 0%.
        g_completedBytes = completedBytes;
        g_filesDone      = filesDone;
        g_currentFileBytes = 0;
    }

    bool IsHexMd5(const std::string& value)
    {
        if (value.size() != 32)
            return false;
        return std::all_of(value.begin(), value.end(),
                           [](unsigned char character) { return std::isxdigit(character) != 0; });
    }

    bool IsHexSha256(const std::string& value)
    {
        if (value.size() != 64)
            return false;
        return std::all_of(value.begin(), value.end(),
                           [](unsigned char character) { return std::isxdigit(character) != 0; });
    }

    bool EndsWithIgnoreCase(const std::string& value, const std::string& suffix)
    {
        return value.size() >= suffix.size() &&
               Lowercase(value.substr(value.size() - suffix.size())) == Lowercase(suffix);
    }

    bool IsSafeContentBaseUrl(const std::string& value)
    {
        if (value.empty() || value.size() > 2048 || value.back() != '/' ||
            !Patcher::Http::IsSecureOrLoopbackUrl(value) ||
            value.find('\\') != std::string::npos || value.find('@') != std::string::npos ||
            value.find('?') != std::string::npos || value.find('#') != std::string::npos ||
            value.find('%') != std::string::npos)
        {
            return false;
        }

        return std::all_of(value.begin(), value.end(),
                           [](unsigned char character) { return character > 0x20 && character < 0x7F; });
    }

    bool IsSafeReleaseId(const std::string& value)
    {
        return !value.empty() && value.size() <= 128 &&
               std::all_of(value.begin(), value.end(), [](unsigned char character)
                           { return std::isalnum(character) != 0 || character == '-' || character == '_' ||
                                    character == '.'; });
    }

    bool IsSafeManifestReference(const std::string& value)
    {
        if (value.empty() || value.size() > 2048 || value.find('\\') != std::string::npos ||
            value.find('@') != std::string::npos || value.find('?') != std::string::npos ||
            value.find('#') != std::string::npos || value.find('%') != std::string::npos)
        {
            return false;
        }
        return std::all_of(value.begin(), value.end(),
                           [](unsigned char character) { return character > 0x20 && character < 0x7F; });
    }

    bool IsImmutablePayloadPath(const std::string& value, const std::string& md5)
    {
        return value.rfind("objects/" + Lowercase(md5) + "/", 0) == 0;
    }

    bool IsStaticFullMpqPayloadPath(const std::string& value, const std::string& targetPath)
    {
        constexpr size_t dataPrefixLength = 5;
        if (targetPath.compare(0, dataPrefixLength, "Data/") != 0 ||
            targetPath.find('/', dataPrefixLength) != std::string::npos)
            return false;
        return value == "static-mpqs/" + targetPath.substr(dataPrefixLength);
    }

    std::string FullChunkPayloadPath(const std::string& kind, const std::string& targetPath, uint32_t index,
                                     const std::string& md5)
    {
        std::ostringstream path;
        path << "objects/" << Lowercase(md5) << '/' << kind << '/' << targetPath << "/part-" << std::setw(5)
             << std::setfill('0') << index;
        return path.str();
    }

    bool IsSafeRelativePath(const std::string& value)
    {
        return StreamingValidation::IsSafeRelativePath(value);
    }

    bool IsSafeArchivePath(const std::string& value)
    {
        return StreamingValidation::IsSafeArchivePath(value);
    }

    bool IsSafeMemberPath(const std::string& value)
    {
        if (!IsSafeRelativePath(value))
            return false;
        const std::string lower = Lowercase(value);
        return lower != "(listfile)" && lower != "(attributes)";
    }

    bool ParseUnsigned(const std::string& value, uint64_t& out)
    {
        return StreamingValidation::ParseUnsigned(value, out);
    }

    bool ParseUint32(const std::string& value, uint32_t& out, bool allowZero)
    {
        uint64_t parsed = 0;
        if (!StreamingValidation::ParseUnsigned(value, parsed, allowZero) ||
            parsed > (std::numeric_limits<uint32_t>::max)())
            return false;
        out = static_cast<uint32_t>(parsed);
        return true;
    }

    struct ChunkGroup
    {
        ManifestEntry::Kind kind = ManifestEntry::Kind::FullMpqIfMissing;
        std::string targetPath;
        uint32_t count    = 0;
        uint64_t fullSize = 0;
        std::string fullMd5;
        std::map<uint32_t, PayloadChunk> chunks;
    };

    bool ParseManifest(const std::string& contents, Manifest& out, std::string& error)
    {
        out = {};
        if (contents.size() > RELEASE_MANIFEST_DOWNLOAD_LIMIT)
        {
            error = "hosted manifest exceeds the supported catalog size";
            return false;
        }
        std::string version;
        bool hasSource = false;
        std::map<std::string, std::pair<uint64_t, std::string>> payloads;
        std::set<std::string> targets;
        std::set<std::string> fullContainers;
        std::set<std::string> fallbackContainers;
        std::set<std::string> memberContainers;
        std::map<std::string, std::string> archiveRevisions;
        std::map<std::string, ChunkGroup> chunkGroups;
        std::istringstream lines(contents);
        size_t rowCount = 0;

        for (std::string line; std::getline(lines, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            line = Trim(line);
            if (line.empty())
                continue;
            if (++rowCount > 250000 || line.size() > 16384)
            {
                error = "hosted manifest exceeds the supported entry count or line length";
                return false;
            }

            const size_t separator = line.find('=');
            if (separator == std::string::npos)
            {
                error = "hosted manifest contains a malformed line";
                return false;
            }

            const std::string key      = Trim(line.substr(0, separator));
            const std::string value    = Trim(line.substr(separator + 1));
            const std::string lowerKey = Lowercase(key);
            if (lowerKey == "version")
            {
                if (!version.empty() ||
                    (value != "1" && value != "2" && value != "3" && value != "4" && value != "5"))
                {
                    error = "hosted manifest must contain one supported version line";
                    return false;
                }
                version = value;
                continue;
            }
            if (lowerKey == "source")
            {
                if (hasSource || !IsSafeContentBaseUrl(value))
                {
                    error = "hosted manifest source must be one HTTPS URL ending in / "
                            "(HTTP is allowed only for localhost testing)";
                    return false;
                }
                hasSource     = true;
                out.sourceUrl = value;
                continue;
            }
            if (lowerKey == "istrial" || lowerKey == "bgpreloadsleep")
                continue;

            if (version.empty())
            {
                error = "hosted manifest version must appear before file entries";
                return false;
            }
            if (!IsSafeRelativePath(key))
            {
                error = "hosted manifest contains an unsafe payload path: " + key;
                return false;
            }

            std::vector<std::string> fields;
            std::istringstream fieldStream(value);
            for (std::string field; std::getline(fieldStream, field, ';');)
                fields.push_back(Trim(field));
            ManifestEntry entry;
            entry.payloadPath = key;

            if (version == "1")
            {
                if (fields.size() != 4 || fields[0] != key || fields[3] != "0" || !IsSafeArchivePath(key))
                {
                    error = "legacy hosted manifest entry has an invalid layout: " + key;
                    return false;
                }
                entry.kind       = ManifestEntry::Kind::FullMpq;
                entry.targetPath = key;
                if (!ParseUnsigned(fields[1], entry.size) || !IsHexMd5(fields[2]))
                {
                    error = "hosted manifest entry has invalid size or MD5: " + key;
                    return false;
                }
                entry.md5 = Lowercase(fields[2]);
            }
            else if (fields.size() == 4 && fields[0] == "full-mpq")
            {
                entry.kind       = ManifestEntry::Kind::FullMpq;
                entry.targetPath = fields[1];
                if ((version == "2" && key != entry.targetPath) ||
                    ((version == "3" || version == "4" || version == "5") &&
                     !IsImmutablePayloadPath(key, fields[3]) &&
                     (version != "5" || !IsStaticFullMpqPayloadPath(key, entry.targetPath))) ||
                    !IsSafeArchivePath(entry.targetPath) || !ParseUnsigned(fields[2], entry.size) ||
                    !IsHexMd5(fields[3]))
                {
                    error = "full-MPQ manifest entry is invalid: " + key;
                    return false;
                }
                entry.md5 = Lowercase(fields[3]);
            }
            else if (fields.size() == 4 && fields[0] == "full-mpq-if-missing")
            {
                entry.kind                        = ManifestEntry::Kind::FullMpqIfMissing;
                entry.targetPath                  = fields[1];
                const std::string expectedPayload = "fallbacks/" + entry.targetPath;
                if (((version == "2" && key != expectedPayload) ||
                     ((version == "3" || version == "4" || version == "5") &&
                      !IsImmutablePayloadPath(key, fields[3]))) ||
                    !IsSafeArchivePath(entry.targetPath) || !ParseUnsigned(fields[2], entry.size) ||
                    !IsHexMd5(fields[3]))
                {
                    error = "missing-MPQ fallback manifest entry is invalid: " + key;
                    return false;
                }
                entry.md5 = Lowercase(fields[3]);
            }
            else if (version == "5" && fields.size() == 8 &&
                     (fields[0] == "full-mpq-chunk" || fields[0] == "full-mpq-if-missing-chunk"))
            {
                const bool fallbackChunk = fields[0] == "full-mpq-if-missing-chunk";
                const ManifestEntry::Kind kind =
                    fallbackChunk ? ManifestEntry::Kind::FullMpqIfMissing : ManifestEntry::Kind::FullMpq;
                PayloadChunk chunk;
                chunk.payloadPath = key;
                uint64_t fullSize  = 0;
                std::string fullMd5;
                if (!IsSafeArchivePath(fields[1]) || !ParseUint32(fields[2], chunk.index, true) ||
                    !ParseUint32(fields[3], chunk.count, false) || chunk.index >= chunk.count ||
                    !ParseUnsigned(fields[4], chunk.size) || !IsHexMd5(fields[5]) ||
                    !ParseUnsigned(fields[6], fullSize) || !IsHexMd5(fields[7]) ||
                    key != FullChunkPayloadPath(fields[0], fields[1], chunk.index, fields[5]))
                {
                    error = "full-MPQ chunk manifest entry is invalid: " + key;
                    return false;
                }
                chunk.md5 = Lowercase(fields[5]);
                fullMd5   = Lowercase(fields[7]);

                const std::string groupKey =
                    (kind == ManifestEntry::Kind::FullMpqIfMissing ? "fallback::" : "full::") +
                    Lowercase(fields[1]);
                ChunkGroup& group = chunkGroups[groupKey];
                if (group.chunks.empty())
                {
                    group.kind       = kind;
                    group.targetPath = fields[1];
                    group.count      = chunk.count;
                    group.fullSize   = fullSize;
                    group.fullMd5    = fullMd5;
                }
                else if (group.kind != kind || group.count != chunk.count || group.fullSize != fullSize ||
                         !Patcher::Filesystem::EqualsIgnoreCase(group.fullMd5, fullMd5))
                {
                    error = "full-MPQ chunks disagree on parent metadata: " + fields[1];
                    return false;
                }
                if (!group.chunks.emplace(chunk.index, chunk).second)
                {
                    error = "hosted manifest contains a duplicate full-MPQ chunk: " + key;
                    return false;
                }

                const std::string payloadKey = Lowercase(key);
                const auto [payload, inserted] =
                    payloads.emplace(payloadKey, std::make_pair(chunk.size, chunk.md5));
                if (!inserted && payload->second != std::make_pair(chunk.size, chunk.md5))
                {
                    error = "hosted manifest payload metadata disagrees: " + key;
                    return false;
                }
                continue;
            }
            else if ((fields.size() == 6 || ((version == "4" || version == "5") && fields.size() == 9)) &&
                     fields[0] == "mpq-member")
            {
                entry.kind                        = ManifestEntry::Kind::MpqMember;
                entry.containerPath               = fields[1];
                entry.memberPath                  = fields[2];
                entry.archiveRevision             = Lowercase(fields[5]);
                const std::string expectedPayload = "members/" + entry.containerPath + "/" + entry.memberPath;
                if (((version == "2" && key != expectedPayload) ||
                     (version == "3" && !IsImmutablePayloadPath(key, fields[4])) ||
                      ((version == "4" || version == "5") && fields.size() == 6 &&
                       !IsImmutablePayloadPath(key, fields[4]))) ||
                    !IsSafeArchivePath(entry.containerPath) || !IsSafeMemberPath(entry.memberPath) ||
                    !ParseUnsigned(fields[3], entry.size) || !IsHexMd5(fields[4]) || !IsHexMd5(entry.archiveRevision))
                {
                    error = "MPQ-member manifest entry is invalid: " + key;
                    return false;
                }
                entry.md5         = Lowercase(fields[4]);
                entry.payloadSize = entry.size;
                entry.payloadMd5  = entry.md5;
                if (fields.size() == 9)
                {
                    const std::string expectedKey = "bundle-members/" + entry.containerPath + "/" + entry.memberPath;
                    entry.bundled                 = true;
                    entry.payloadPath             = fields[6];
                    if (key != expectedKey || !IsSafeRelativePath(entry.payloadPath) ||
                        !ParseUnsigned(fields[7], entry.payloadSize) || !IsHexMd5(fields[8]) ||
                        !IsImmutablePayloadPath(entry.payloadPath, fields[8]))
                    {
                        error = "bundled MPQ-member manifest entry is invalid: " + key;
                        return false;
                    }
                    entry.payloadMd5 = Lowercase(fields[8]);
                }

                const std::string containerKey = Lowercase(entry.containerPath);
                const auto revision            = archiveRevisions.find(containerKey);
                if (revision != archiveRevisions.end() && revision->second != entry.archiveRevision)
                {
                    error = "MPQ members disagree on archive revision: " + entry.containerPath;
                    return false;
                }
                archiveRevisions[containerKey] = entry.archiveRevision;
            }
            else if ((version == "3" || version == "4" || version == "5") && fields.size() == 4 &&
                     fields[0] == "mpq-delete")
            {
                entry.kind                    = ManifestEntry::Kind::MpqDelete;
                entry.containerPath           = fields[1];
                entry.memberPath              = fields[2];
                entry.archiveRevision         = Lowercase(fields[3]);
                const std::string expectedKey = "deletions/" + entry.containerPath + "/" + entry.memberPath;
                if (key != expectedKey || !IsSafeArchivePath(entry.containerPath) ||
                    !IsSafeMemberPath(entry.memberPath) || !IsHexMd5(entry.archiveRevision))
                {
                    error = "MPQ-delete manifest entry is invalid: " + key;
                    return false;
                }

                const std::string containerKey = Lowercase(entry.containerPath);
                const auto revision            = archiveRevisions.find(containerKey);
                if (revision != archiveRevisions.end() && revision->second != entry.archiveRevision)
                {
                    error = "MPQ operations disagree on archive revision: " + entry.containerPath;
                    return false;
                }
                archiveRevisions[containerKey] = entry.archiveRevision;
            }
            else
            {
                error = "hosted manifest entry has an unsupported layout: " + key;
                return false;
            }

            const std::string payloadKey = Lowercase(entry.payloadPath);
            const bool isMember          = entry.kind == ManifestEntry::Kind::MpqMember;
            const bool isMemberOperation = isMember || entry.kind == ManifestEntry::Kind::MpqDelete;
            const std::string targetKey =
                isMemberOperation ? Lowercase(entry.containerPath) + "::" + Lowercase(entry.memberPath) :
                                    (entry.kind == ManifestEntry::Kind::FullMpqIfMissing ? "fallback::" : "full::") +
                                        Lowercase(entry.targetPath);
            const std::string containerKey =
                isMemberOperation ? Lowercase(entry.containerPath) : Lowercase(entry.targetPath);
            if ((entry.kind == ManifestEntry::Kind::FullMpq &&
                 (memberContainers.count(containerKey) != 0 || fallbackContainers.count(containerKey) != 0)) ||
                (entry.kind == ManifestEntry::Kind::FullMpqIfMissing && fullContainers.count(containerKey) != 0) ||
                (isMemberOperation && fullContainers.count(containerKey) != 0))
            {
                error = "hosted manifest mixes full and member delivery for one MPQ: " + containerKey;
                return false;
            }
            if (entry.kind == ManifestEntry::Kind::FullMpq)
                fullContainers.insert(containerKey);
            else if (entry.kind == ManifestEntry::Kind::FullMpqIfMissing)
            {
                if (!fallbackContainers.insert(containerKey).second)
                {
                    error = "hosted manifest contains duplicate missing-MPQ fallback: " + key;
                    return false;
                }
            }
            else
                memberContainers.insert(containerKey);
            if (entry.kind != ManifestEntry::Kind::MpqDelete)
            {
                if (entry.payloadSize == 0)
                    entry.payloadSize = entry.size;
                if (entry.payloadMd5.empty())
                    entry.payloadMd5 = entry.md5;
                const auto [payload, inserted] =
                    payloads.emplace(payloadKey, std::make_pair(entry.payloadSize, entry.payloadMd5));
                if (!inserted && payload->second != std::make_pair(entry.payloadSize, entry.payloadMd5))
                {
                    error = "hosted manifest payload metadata disagrees: " + key;
                    return false;
                }
            }
            if (!targets.insert(targetKey).second)
            {
                error = "hosted manifest contains a duplicate target: " + key;
                return false;
            }
            out.entries.push_back(std::move(entry));
        }

        for (const auto& groupPair : chunkGroups)
        {
            const ChunkGroup& group = groupPair.second;
            ManifestEntry* parent = nullptr;
            for (ManifestEntry& entry : out.entries)
            {
                if (entry.kind == group.kind && Lowercase(entry.targetPath) == Lowercase(group.targetPath))
                {
                    parent = &entry;
                    break;
                }
            }
            if (!parent || parent->size != group.fullSize ||
                !Patcher::Filesystem::EqualsIgnoreCase(parent->md5, group.fullMd5))
            {
                error = "full-MPQ chunk group has no matching parent descriptor: " + group.targetPath;
                return false;
            }
            if (group.chunks.size() != group.count)
            {
                error = "full-MPQ chunk group is incomplete: " + group.targetPath;
                return false;
            }

            uint64_t chunkBytes = 0;
            parent->chunks.reserve(group.count);
            for (uint32_t index = 0; index < group.count; ++index)
            {
                const auto chunk = group.chunks.find(index);
                if (chunk == group.chunks.end() ||
                    chunkBytes > (std::numeric_limits<uint64_t>::max)() - chunk->second.size)
                {
                    error = "full-MPQ chunk sequence is invalid: " + group.targetPath;
                    return false;
                }
                chunkBytes += chunk->second.size;
                parent->chunks.push_back(chunk->second);
            }
            if (chunkBytes != parent->size)
            {
                error = "full-MPQ chunk sizes do not match the parent: " + group.targetPath;
                return false;
            }
        }

        for (const std::string& container : fallbackContainers)
        {
            if (memberContainers.count(container) == 0)
            {
                error = "hosted manifest missing-MPQ fallback has no member entries: " + container;
                return false;
            }
        }

        // Restrict disk/progress arithmetic before the planner sums payloads.
        // This is far above the current catalog while still bounding a bad
        // manifest in the 32-bit client and rejecting saturated uint64 sizes.
        constexpr uint64_t maximumReleaseBytes = uint64_t{1024} * 1024 * 1024 * 1024;
        uint64_t declaredBytes = 0;
        for (const ManifestEntry& entry : out.entries)
        {
            if (entry.size > maximumReleaseBytes - declaredBytes ||
                entry.payloadSize > maximumReleaseBytes ||
                (entry.kind == ManifestEntry::Kind::MpqMember &&
                 entry.size > (std::numeric_limits<uint32_t>::max)()))
            {
                error = "hosted manifest exceeds supported file or release sizes";
                return false;
            }
            declaredBytes += entry.size;
        }
        declaredBytes = 0;
        for (const auto& payload : payloads)
        {
            if (payload.second.first > maximumReleaseBytes - declaredBytes)
            {
                error = "hosted manifest exceeds supported total payload size";
                return false;
            }
            declaredBytes += payload.second.first;
        }

        if (version.empty() || !hasSource || out.entries.empty())
        {
            error = "hosted manifest is missing version, source, or streaming entries";
            return false;
        }
        return true;
    }

    std::string LocalPath(const std::string& gameRoot, const std::string& relativePath)
    {
        std::string local = relativePath;
        std::replace(local.begin(), local.end(), '/', '\\');
        return (std::filesystem::path(gameRoot) / std::filesystem::path(local)).string();
    }

    bool ValidateUpdatePath(const std::filesystem::path& path, bool directory, std::string& error)
    {
        if (!path.is_absolute())
        {
            error = "The game update path is not absolute.";
            return false;
        }
        std::filesystem::path current = path.root_path();
        for (const auto& component : path.relative_path())
        {
            current /= component;
            const DWORD attributes = GetFileAttributesW(current.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
            {
                const DWORD code = GetLastError();
                if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
                    continue;
                error = "The game update path could not be checked.";
                return false;
            }
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                // Only a host mount above the game may be crossed. Destinations
                // and directories inside the client retain the strict check.
                bool aboveGame = false;
                auto game = std::filesystem::path(Patcher::Filesystem::GetExeDirectory());
                if (game.filename().empty())
                    game = game.parent_path();
                for (auto parent = game.parent_path(); !parent.empty() && parent != parent.root_path(); parent = parent.parent_path())
                    if (parent == current)
                        aboveGame = true;
                if (!aboveGame || !Patcher::Filesystem::IsWineHostMount(current.c_str()))
                {
                    error = "Game updates cannot use symbolic links or junctions in their storage paths.";
                    return false;
                }
            }
            const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            const bool expectedDirectory = current != path || directory;
            if (isDirectory != expectedDirectory)
            {
                error = "The update destination is not the expected file or folder. Use a separate client for loose development patches.";
                return false;
            }
            if (!isDirectory)
            {
                HANDLE file = CreateFileW(current.c_str(), FILE_READ_ATTRIBUTES,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                          OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                BY_HANDLE_FILE_INFORMATION info{};
                const bool regular = file != INVALID_HANDLE_VALUE && GetFileInformationByHandle(file, &info) &&
                                     info.nNumberOfLinks == 1 &&
                                     !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
                if (file != INVALID_HANDLE_VALUE)
                    CloseHandle(file);
                if (!regular)
                {
                    error = "The game update destination is linked or could not be checked.";
                    return false;
                }
            }
        }
        return true;
    }

    bool ValidateUpdateCache(const std::filesystem::path& path, std::string& error)
    {
        if (!ValidateUpdatePath(path, true, error))
            return false;
        std::error_code filesystemError;
        if (!std::filesystem::exists(path, filesystemError))
            return !filesystemError;
        std::filesystem::recursive_directory_iterator iterator(path, filesystemError);
        const std::filesystem::recursive_directory_iterator end;
        while (!filesystemError && iterator != end)
        {
            const auto status = iterator->symlink_status(filesystemError);
            if (filesystemError || !ValidateUpdatePath(iterator->path(), std::filesystem::is_directory(status), error))
                return false;
            iterator.increment(filesystemError);
        }
        if (filesystemError)
        {
            error = "The game update cache could not be checked.";
            return false;
        }
        return true;
    }

    std::string StagedPath(const std::string& target, const std::string& md5)
    {
        std::filesystem::path path(target);
        return (path.parent_path() / ".dhstaged" /
                (path.stem().string() + ".dhnew-" + md5.substr(0, 8) + path.extension().string()))
            .string();
    }

    std::string FlatNonMountableStagedPath(const std::string& target, const std::string& md5)
    {
        std::filesystem::path path(target);
        return (path.parent_path() / (path.filename().string() + ".dhnew-" + md5.substr(0, 8))).string();
    }

    std::string FlatLegacyStagedPath(const std::string& target, const std::string& md5)
    {
        std::filesystem::path path(target);
        return (path.parent_path() / (path.stem().string() + ".dhnew-" + md5.substr(0, 8) + path.extension().string()))
            .string();
    }

    bool IsRegularFile(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    struct FileProcessIdsUsingFileInformation
    {
        ULONG processCount;
        ULONG_PTR processIds[1];
    };

    bool IsTrustedArchiveOwner(DWORD pid, const std::string& gameRoot)
    {
        if (pid == GetCurrentProcessId())
            return true;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
        if (!process)
            return GetLastError() == ERROR_INVALID_PARAMETER; // The owner exited after the snapshot.

        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
        {
            CloseHandle(process);
            return true;
        }

        std::vector<char> imagePath(32768);
        DWORD imageLength  = static_cast<DWORD>(imagePath.size());
        const bool queried = QueryFullProcessImageNameA(process, 0, imagePath.data(), &imageLength) != FALSE;
        CloseHandle(process);
        if (!queried)
            return false;

        const std::string ownerPath(imagePath.data(), imageLength);
        const std::string trustedCompanion = (std::filesystem::path(gameRoot) / "ClientExtensions64.exe").string();
        return Lowercase(std::filesystem::path(ownerPath).lexically_normal().string()) ==
               Lowercase(std::filesystem::path(trustedCompanion).lexically_normal().string());
    }

    bool TryFindExternalArchiveOwner(const std::string& path, const std::string& gameRoot, DWORD& externalPid)
    {
        externalPid = 0;
        HANDLE file = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        using NtQueryInformationFileFn =
            NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto query =
            ntdll ? reinterpret_cast<NtQueryInformationFileFn>(GetProcAddress(ntdll, "NtQueryInformationFile")) :
                    nullptr;
        if (!query)
        {
            CloseHandle(file);
            return false;
        }

        std::vector<unsigned char> buffer(64 * 1024);
        IO_STATUS_BLOCK statusBlock{};
        constexpr int FILE_PROCESS_IDS_USING_FILE_INFORMATION_CLASS = 47;
        const NTSTATUS status =
            query(file, &statusBlock, buffer.data(), static_cast<ULONG>(buffer.size()),
                  static_cast<FILE_INFORMATION_CLASS>(FILE_PROCESS_IDS_USING_FILE_INFORMATION_CLASS));
        CloseHandle(file);
        if (status < 0)
            return false;

        const auto* owners         = reinterpret_cast<const FileProcessIdsUsingFileInformation*>(buffer.data());
        const size_t headerSize    = offsetof(FileProcessIdsUsingFileInformation, processIds);
        const size_t maximumOwners = (buffer.size() - headerSize) / sizeof(ULONG_PTR);
        const size_t ownerCount    = (std::min)(static_cast<size_t>(owners->processCount), maximumOwners);
        for (size_t index = 0; index < ownerCount; ++index)
        {
            const DWORD pid = static_cast<DWORD>(owners->processIds[index]);
            if (pid == 0 || IsTrustedArchiveOwner(pid, gameRoot))
                continue;
            externalPid = pid;
            break;
        }
        return true;
    }

    bool DescribeExternalArchiveLock(const std::string& targetPath, const std::string& relativePath,
                                     const std::string& gameRoot, std::string& message)
    {
        if (!IsRegularFile(targetPath))
            return false;
        DWORD externalOwnerPid = 0;
        const bool queried     = TryFindExternalArchiveOwner(targetPath, gameRoot, externalOwnerPid);
        if (queried && externalOwnerPid == 0)
            return false;

        std::string archiveName = std::filesystem::path(relativePath).stem().string();
        if (!archiveName.empty())
            archiveName[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(archiveName[0])));
        message =
            queried ?
                archiveName + " is being locked by another program. Please close that program and restart the client." :
                archiveName + " could not be checked. Please restart the client and try again.";
        return true;
    }

    void ForgetLocalHash(const std::string& path);

    std::string ExistingStagedPath(const std::string& target, const std::string& md5)
    {
        const std::string stagedPath = StagedPath(target, md5);
        if (IsRegularFile(stagedPath))
            return stagedPath;

        const std::string candidates[] = {
            FlatNonMountableStagedPath(target, md5),
            FlatLegacyStagedPath(target, md5),
        };
        for (const std::string& candidate : candidates)
        {
            if (!IsRegularFile(candidate))
                continue;

            std::error_code directoryError;
            std::filesystem::create_directories(std::filesystem::path(stagedPath).parent_path(), directoryError);
            if (!directoryError &&
                MoveFileExA(candidate.c_str(), stagedPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                ForgetLocalHash(candidate);
                LOG_INFO << "ContentStreaming: migrated staged archive to mountable path: " << stagedPath;
                return stagedPath;
            }

            LOG_WARN << "ContentStreaming: could not migrate legacy staged archive: " << candidate;
            return candidate;
        }
        return stagedPath;
    }

    bool EnsureStagingDirectory(const std::string& stagedPath, std::string& error)
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(std::filesystem::path(stagedPath).parent_path(), filesystemError);
        if (!filesystemError)
            return true;

        error = "could not create staged archive directory: " + filesystemError.message();
        return false;
    }

    bool SchedulePromotionAfterExit(const std::string& stagedPath, const std::string& targetPath,
                                    const std::string& operationsPath = {}, const std::string& currentMarkerPath = {})
    {
        const std::string key = Lowercase(std::filesystem::path(stagedPath).lexically_normal().string()) + "|" +
                                Lowercase(std::filesystem::path(targetPath).lexically_normal().string());
        {
            std::lock_guard<std::mutex> lock(g_promotionMutex);
            if (!g_scheduledPromotions.insert(key).second)
            {
                g_postExitPromotionScheduled = true;
                return true;
            }
        }

        const std::filesystem::path helperPath =
            std::filesystem::path(Patcher::Filesystem::GetExeDirectory()) / "DuskhavenBackgroundDownloader.exe";
        if (!IsRegularFile(helperPath.string()))
        {
            {
                std::lock_guard<std::mutex> lock(g_promotionMutex);
                g_scheduledPromotions.erase(key);
            }
            LOG_ERROR << "ContentStreaming: post-exit promoter is missing: " << helperPath.string();
            return false;
        }

        const std::wstring helper = helperPath.wstring();
        const std::wstring staged = std::filesystem::path(stagedPath).wstring();
        const std::wstring target = std::filesystem::path(targetPath).wstring();
        std::wstring command      = L"\"" + helper + L"\" --wait-pid " + std::to_wstring(GetCurrentProcessId()) +
                               L" --source \"" + staged + L"\" --target \"" + target + L"\"" +
                               L" --game-root \"" + helperPath.parent_path().wstring() + L"\"";
        if (!operationsPath.empty())
        {
            command += L" --operations \"" + std::filesystem::path(operationsPath).wstring() + L"\"";
            command += L" --current \"" + std::filesystem::path(currentMarkerPath).wstring() + L"\"";
        }
        std::string launchError;
        if (!RuntimeUpdate::LaunchDetachedHelper(helperPath, command, helperPath.parent_path(), launchError))
        {
            {
                std::lock_guard<std::mutex> lock(g_promotionMutex);
                g_scheduledPromotions.erase(key);
            }
            LOG_ERROR << "ContentStreaming: could not start post-exit promotion: " << launchError;
            return false;
        }

        g_postExitPromotionScheduled = true;
        LOG_INFO << "ContentStreaming: scheduled post-exit promotion: " << stagedPath << " -> " << targetPath;
        return true;
    }

    std::string LocalHashKey(const std::string& path)
    {
        return Lowercase(std::filesystem::path(path).lexically_normal().string());
    }

    bool GetLocalFileState(const std::string& path, uint64_t& size, int64_t& modified)
    {
        std::error_code error;
        size = std::filesystem::file_size(path, error);
        if (error)
            return false;

        const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
        if (error)
            return false;

        modified = static_cast<int64_t>(writeTime.time_since_epoch().count());
        return true;
    }

    void SaveLocalHashes()
    {
        if (g_localHashCachePath.empty())
            return;

        std::ostringstream contents;
        for (const auto& [path, entry] : g_localHashes)
            contents << entry.size << '|' << entry.modified << '|' << entry.md5 << '|' << path << "\r\n";

        const std::filesystem::path cachePath(g_localHashCachePath);
        std::error_code filesystemError;
        std::filesystem::create_directories(cachePath.parent_path(), filesystemError);
        if (filesystemError)
            return;

        const std::string temporaryPath = g_localHashCachePath + ".tmp";
        std::string error;
        if (!Patcher::Filesystem::WriteTextFile(temporaryPath, contents.str(), error))
            return;

        if (!MoveFileExA(temporaryPath.c_str(), g_localHashCachePath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            DeleteFileA(temporaryPath.c_str());
    }

    void RememberLocalHash(const std::string& path, const std::string& md5)
    {
        uint64_t size    = 0;
        int64_t modified = 0;
        if (!GetLocalFileState(path, size, modified))
            return;

        g_localHashes[LocalHashKey(path)] = {size, modified, Lowercase(md5)};
        SaveLocalHashes();
    }

    void ForgetLocalHash(const std::string& path)
    {
        if (g_localHashes.erase(LocalHashKey(path)) != 0)
            SaveLocalHashes();
    }

    void LoadLocalHashes(const std::string& gameRoot)
    {
        g_localHashes.clear();
        g_localHashCachePath = (std::filesystem::path(gameRoot) / ".duskhaven-streaming-local.txt").string();

        std::ifstream input(g_localHashCachePath, std::ios::binary);
        bool migrateLegacyCache = false;
        if (!input)
        {
            const std::string legacyPath =
                (std::filesystem::path(gameRoot) / "Cache" / "duskhaven-stream-local.txt").string();
            input.clear();
            input.open(legacyPath, std::ios::binary);
            migrateLegacyCache = input.good();
        }
        for (std::string line; input && std::getline(input, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const size_t first  = line.find('|');
            const size_t second = first == std::string::npos ? first : line.find('|', first + 1);
            const size_t third  = second == std::string::npos ? second : line.find('|', second + 1);
            if (third == std::string::npos)
                continue;

            const std::string sizeText     = line.substr(0, first);
            const std::string modifiedText = line.substr(first + 1, second - first - 1);
            char* end                      = nullptr;
            const unsigned __int64 size    = _strtoui64(sizeText.c_str(), &end, 10);
            if (!end || *end != '\0')
                continue;

            const __int64 modified = _strtoi64(modifiedText.c_str(), &end, 10);
            if (!end || *end != '\0')
                continue;

            const std::string md5  = Lowercase(line.substr(second + 1, third - second - 1));
            const std::string path = line.substr(third + 1);
            if (!IsHexMd5(md5) || path.empty())
                continue;

            g_localHashes[LocalHashKey(path)] = {static_cast<uint64_t>(size), static_cast<int64_t>(modified), md5};
        }

        if (migrateLegacyCache)
            SaveLocalHashes();
    }

    void EnqueueMount(const std::string& path, const std::string& md5, bool refreshNativeDbcs,
                      bool reloadGlue = false, bool deferActivation = false)
    {
        const std::string key = LocalHashKey(path) + "|" + Lowercase(md5);
        std::lock_guard<std::mutex> lock(g_mountMutex);
        if (!g_queuedMounts.insert(key).second)
            return;

        g_mountQueue.push_back({path, key, false, refreshNativeDbcs, true, {}, reloadGlue, deferActivation});
        if (deferActivation)
            g_deferredFallbackActivations[LocalHashKey(path)] =
                {path, "fallback-ready|" + key, true, true, true, {}, reloadGlue};
        ++g_pendingMountCount;
        LOG_INFO << "ContentStreaming: queued archive mount: " << path;
    }

    void EnqueueRefresh(const std::string& path, const std::string& revision,
                        const std::vector<std::string>& changedMembers, bool refreshAllDbcs, bool reloadGlue)
    {
        const std::string key = "refresh|" + LocalHashKey(path) + "|" + Lowercase(revision);
        std::lock_guard<std::mutex> lock(g_mountMutex);
        const auto deferred = g_deferredFallbackActivations.find(LocalHashKey(path));
        if (deferred != g_deferredFallbackActivations.end())
        {
            deferred->second.reloadGlue = deferred->second.reloadGlue || reloadGlue;
            return; // Activate the entire seed only after catch-up succeeds.
        }
        if (!g_queuedMounts.insert(key).second)
            return;

        g_mountQueue.push_back({path, key, true, true, refreshAllDbcs, changedMembers, reloadGlue});
        ++g_pendingMountCount;
        const char* refreshMode = changedMembers.empty() ? "conservative" :
                                  (refreshAllDbcs ? "targeted+conservative-dbc" : "targeted");
        LOG_INFO << "ContentStreaming: queued mounted archive refresh: " << path
                 << " mode=" << refreshMode << " members=" << changedMembers.size();
    }

    int32_t RefreshUnitModel(WOWGUID guid, void*)
    {
        CGObject_C* unit = ClntObjMgrObjectPtr(guid, static_cast<TypeMask>(TYPEMASK_UNIT | TYPEMASK_PLAYER), __FILE__, __LINE__);
        if (unit)
            unit->UpdateDisplayInfo(1);
        return 1;
    }

    void RefreshLoadedModels()
    {
        // Login and character-select scenes populate the model cache before an active player exists.
        // Drop those parsed models after mounting replacement art so later loads use the new archive.
        M2CacheInvalidate();
        M2CachePurge();

        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        if (!activePlayer)
        {
            LOG_INFO << "ContentStreaming: art archive mounted before world entry; "
                        "model cache invalidated; skipping visible world-model rebuild";
            return;
        }

        LOG_INFO << "ContentStreaming: refreshing loaded world models activePlayer=" << (void*)activePlayer;
        ClntObjMgrEnumVisibleObjects(RefreshUnitModel, nullptr);
        LOG_INFO << "ContentStreaming: rebuilt world models after art mount";
    }

    void PumpMountQueue()
    {
        ClientArchive::PumpWritableRequests();

        std::vector<MountRequest> batch;
        {
            std::lock_guard<std::mutex> lock(g_mountMutex);
            batch.swap(g_mountQueue);
        }

        struct MountedActivation
        {
            const MountRequest* request = nullptr;
            void* archiveRecord         = nullptr;
        };
        std::vector<MountedActivation> mountedActivations;
        mountedActivations.reserve(batch.size());
        for (const MountRequest& request : batch)
        {
            SArchive* publicArchive = nullptr;
            void* archiveRecord     = nullptr;
            bool mounted            = false;
            int priority            = 0;
            if (request.refreshExisting)
            {
                ClientArchive::MountedArchive existing;
                mounted       = ClientArchive::FindMounted(request.path, existing);
                archiveRecord = existing.archiveRecord;
                LOG_INFO << "ContentStreaming: mounted archive refresh " << (mounted ? "succeeded" : "failed")
                         << " path=" << request.path;
            }
            else
            {
                priority = g_nextMountPriority.fetch_add(1);
                mounted  = ClientArchive::Mount(request.path.c_str(), priority, &publicArchive);
                archiveRecord = publicArchive ? publicArchive->m_archive : nullptr;
                LOG_INFO << "ContentStreaming: mount " << (mounted ? "succeeded" : "failed") << " priority=" << priority
                         << " path=" << request.path;
            }

            if (mounted && archiveRecord)
            {
                mountedActivations.push_back({&request, archiveRecord});
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(g_mountMutex);
                    g_queuedMounts.erase(request.dedupeKey);
                }
                g_activationFailed = true;
                SetStatus(Phase::Failed, request.path, "downloaded archive could not be mounted");
            }
        }

        bool artMounted = false;
        bool reloadGlue = false;
        if (!mountedActivations.empty())
        {
            const auto hashStarted = std::chrono::steady_clock::now();
            SFile::RebuildHash();
            const double hashMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hashStarted).count();
            LOG_INFO << "ContentStreaming: archive hash rebuild batch=" << mountedActivations.size()
                     << " ms=" << hashMilliseconds;

            size_t targetedArchives = 0;
            size_t targetedMembers  = 0;
            const auto dbcStarted   = std::chrono::steady_clock::now();
            for (const MountedActivation& activation : mountedActivations)
            {
                const MountRequest& request = *activation.request;
                if (request.deferActivation)
                {
                    // The fallback can provide the current Glue presentation as soon as it is
                    // mounted. Login remains blocked while the member catch-up pass runs, and
                    // DBC/CDBC activation stays deferred until that pass has verified the seed.
                    reloadGlue = reloadGlue || request.reloadGlue;
                    LOG_INFO << "ContentStreaming: refreshed fallback Glue; deferring data tables until member catch-up: "
                             << request.path;
                    continue;
                }
                DbcFromMpq::RefreshResult result;
                if (request.changedMembers.empty())
                {
                    result = DbcFromMpq::RefreshFromArchive(activation.archiveRecord, request.refreshNativeDbcs);
                }
                else
                {
                    ++targetedArchives;
                    targetedMembers += request.changedMembers.size();
                    result = DbcFromMpq::RefreshMembersFromArchive(activation.archiveRecord, request.changedMembers,
                                                                   request.refreshNativeDbcs,
                                                                   request.refreshAllDbcs);
                }
                artMounted = artMounted || result.artFiles;
                reloadGlue = reloadGlue || request.reloadGlue;
            }
            const double dbcMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - dbcStarted).count();
            LOG_INFO << "ContentStreaming: DBC/CDBC refresh batch=" << mountedActivations.size()
                     << " targetedArchives=" << targetedArchives << " targetedMembers=" << targetedMembers
                     << " ms=" << dbcMilliseconds;
        }

        for (size_t index = 0; index < batch.size(); ++index)
        {
            --g_pendingMountCount;
            if (g_downloadsFinished.load() && g_phase.load() == Phase::Installing)
            {
                const uint32_t total     = g_filesTotal.load();
                const uint32_t pending   = g_pendingMountCount.load();
                const uint32_t completed = total > pending ? total - pending : 0;
                g_completedBytes         = completed;
                g_filesDone              = completed;
            }
        }

        if (artMounted)
        {
            const auto modelStarted = std::chrono::steady_clock::now();
            RefreshLoadedModels();
            const double modelMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - modelStarted).count();
            LOG_INFO << "ContentStreaming: model refresh ms=" << modelMilliseconds;
        }

        if (reloadGlue && ClntObjMgrGetActivePlayerObj() == nullptr)
        {
            g_glueReloadRequired = true;
            LOG_INFO << "ContentStreaming: scheduled GlueXML reload after mounting the interface fallback";
        }

        if (g_downloadsFinished.load() && !g_running.load() && g_pendingMountCount.load() == 0)
        {
            if (!g_catchUpAfterFallbackMount.load() && !g_runtimeRestartRequired.load())
            {
                const std::string compatibilityError = CDBC::PendingCompatibilityError();
                if (!compatibilityError.empty())
                    SetStatus(Phase::Failed, {}, compatibilityError);
            }
            const bool mountFailed = g_activationFailed.load() || g_phase.load() == Phase::Failed;
            const bool catchUp     = g_catchUpAfterFallbackMount.exchange(false) && !mountFailed;
            const bool recheck     = g_recheckRequested.exchange(false);
            if (mountFailed)
                ClearPendingInstalledRelease();
            else if (!catchUp)
                CommitPendingInstalledRelease();
            if (catchUp || recheck)
            {
                const bool queuedForceRepair = g_forceRepairRequested.exchange(false);
                const bool forceRepair       = catchUp || queuedForceRepair;
                if (catchUp)
                    LOG_INFO << "ContentStreaming: fallback mounted; starting current-release member catch-up";
                else
                    LOG_INFO << "ContentStreaming: archive installation finished; starting queued recheck";
                ContentStreaming::Trigger(forceRepair);
            }
            else if (!mountFailed)
            {
                std::lock_guard<std::mutex> lock(g_statusMutex);
                Phase expected = Phase::Installing;
                const Phase completed = g_runtimeRestartRequired.load() ? Phase::RestartRequired : Phase::Complete;
                if (g_phase.compare_exchange_strong(expected, completed))
                {
                    g_currentFile.clear();
                    g_error = completed == Phase::RestartRequired
                        ? "Exit and reopen the game to finish installing the client update."
                        : std::string{};
                }
            }
        }
    }

    bool FileMatches(const std::string& path, const ManifestEntry& entry, bool useCache = true)
    {
        uint64_t localSize    = 0;
        int64_t localModified = 0;
        if (!GetLocalFileState(path, localSize, localModified) || localSize != entry.size)
            return false;

        const auto cached = g_localHashes.find(LocalHashKey(path));
        if (useCache && cached != g_localHashes.end() && cached->second.size == localSize &&
            cached->second.modified == localModified)
        {
            LOG_INFO << "ContentStreaming: reused verified archive hash: " << path;
            return Patcher::Filesystem::EqualsIgnoreCase(cached->second.md5, entry.md5);
        }

        std::string hash;
        std::string error;
        uint64_t size = 0;
        if (!Patcher::Filesystem::HashFileMd5(path, hash, size, error))
            return false;

        RememberLocalHash(path, hash);
        return size == entry.size && Patcher::Filesystem::EqualsIgnoreCase(hash, entry.md5);
    }

    bool PromoteOrStage(const std::string& temporaryPath, const std::string& targetPath, const ManifestEntry& entry,
                        bool reloadGlue, std::string& error)
    {
        if (g_cancelRequested.load())
        {
            error = "update canceled";
            return false;
        }
        const bool refreshNativeDbcs = true;
        if (MoveFileExA(temporaryPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            ForgetLocalHash(temporaryPath);
            RememberLocalHash(targetPath, entry.md5);
            EnqueueMount(targetPath, entry.md5, refreshNativeDbcs, reloadGlue,
                             entry.kind == ManifestEntry::Kind::FullMpqIfMissing);
            LOG_INFO << "ContentStreaming: installed verified archive: " << targetPath;
            return true;
        }

        const DWORD replaceError     = GetLastError();
        const std::string stagedPath = StagedPath(targetPath, entry.md5);
        if (!EnsureStagingDirectory(stagedPath, error))
            return false;
        if (!MoveFileExA(temporaryPath.c_str(), stagedPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::ostringstream stream;
            stream << "could not promote or stage " << entry.targetPath << " (replace=" << replaceError
                   << ", stage=" << GetLastError() << ")";
            error = stream.str();
            return false;
        }

        ForgetLocalHash(temporaryPath);
        RememberLocalHash(stagedPath, entry.md5);
        if (!SchedulePromotionAfterExit(stagedPath, targetPath))
        {
            error = "the downloaded game file could not be scheduled for installation";
            return false;
        }
        EnqueueMount(stagedPath, entry.md5, refreshNativeDbcs, reloadGlue,
                     entry.kind == ManifestEntry::Kind::FullMpqIfMissing);
        LOG_WARN << "ContentStreaming: target is in use; mounted staged verified archive: " << stagedPath;
        return true;
    }

    std::string FullChunkStateRoot(const std::string& targetPath)
    {
        return targetPath + ".dhchunks";
    }

    std::string ChunkDonePath(const std::string& targetPath, const ManifestEntry& entry, const PayloadChunk& chunk)
    {
        return (std::filesystem::path(FullChunkStateRoot(targetPath)) / entry.md5 /
                (std::to_string(chunk.index) + "-" + chunk.md5 + ".done"))
            .string();
    }

    std::string ChunkLayoutMetadata(const ManifestEntry& entry)
    {
        std::ostringstream metadata;
        metadata << "chunks:" << entry.size << ':' << entry.md5 << ':' << entry.chunks.size();
        for (const PayloadChunk& chunk : entry.chunks)
            metadata << '\n' << chunk.index << ':' << chunk.size << ':' << chunk.md5;
        return metadata.str();
    }

    bool ChunkIsComplete(const std::string& targetPath, const ManifestEntry& entry, const PayloadChunk& chunk)
    {
        std::string marker;
        std::string error;
        return Patcher::Filesystem::ReadTextFile(ChunkDonePath(targetPath, entry, chunk), marker, error) &&
               marker == std::to_string(chunk.size) + ":" + chunk.md5;
    }

    bool MarkChunkComplete(const std::string& targetPath, const ManifestEntry& entry, const PayloadChunk& chunk,
                           std::string& error)
    {
        const std::string markerPath = ChunkDonePath(targetPath, entry, chunk);
        std::error_code directoryError;
        std::filesystem::create_directories(std::filesystem::path(markerPath).parent_path(), directoryError);
        if (directoryError)
        {
            error = "could not create full-MPQ chunk state directory: " + directoryError.message();
            return false;
        }
        return Patcher::Filesystem::WriteTextFile(markerPath, std::to_string(chunk.size) + ":" + chunk.md5, error);
    }

    void RemoveFullChunkState(const std::string& targetPath)
    {
        std::error_code removeError;
        std::filesystem::remove_all(FullChunkStateRoot(targetPath), removeError);
        if (removeError)
            LOG_WARN << "ContentStreaming: could not remove full-MPQ chunk state: " << removeError.message();
    }

    bool PrepareChunkDestination(const std::string& temporaryPath, const std::string& metadataPath,
                                 const std::string& targetPath, const ManifestEntry& entry, std::string& error)
    {
        if (entry.size > static_cast<uint64_t>((std::numeric_limits<LONGLONG>::max)()))
        {
            error = "full-MPQ is too large for the local filesystem";
            return false;
        }
        const std::string expectedMetadata = ChunkLayoutMetadata(entry);
        std::string metadata;
        std::string metadataError;
        const bool matchingMetadata =
            Patcher::Filesystem::ReadTextFile(metadataPath, metadata, metadataError) && metadata == expectedMetadata;
        if (!matchingMetadata)
        {
            DeleteFileA(temporaryPath.c_str());
            DeleteFileA(metadataPath.c_str());
            RemoveFullChunkState(targetPath);
            if (!Patcher::Filesystem::WriteTextFile(metadataPath, expectedMetadata, error))
                return false;
        }

        HANDLE destination = CreateFileA(temporaryPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
        if (destination == INVALID_HANDLE_VALUE)
        {
            error = Patcher::Filesystem::LastErrorString(("could not open " + temporaryPath).c_str());
            return false;
        }

        LARGE_INTEGER size = {};
        bool sized = GetFileSizeEx(destination, &size) != FALSE && size.QuadPart >= 0 &&
                     static_cast<uint64_t>(size.QuadPart) == entry.size;
        if (!sized)
        {
            RemoveFullChunkState(targetPath);
            LARGE_INTEGER end = {};
            end.QuadPart      = static_cast<LONGLONG>(entry.size);
            sized = SetFilePointerEx(destination, end, nullptr, FILE_BEGIN) != FALSE &&
                    SetEndOfFile(destination) != FALSE;
        }
        CloseHandle(destination);
        if (!sized)
        {
            error = Patcher::Filesystem::LastErrorString(("could not size " + temporaryPath).c_str());
            return false;
        }
        return true;
    }

    bool VerifyChunkedArchive(const std::string& path, const ManifestEntry& entry, std::string& error)
    {
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = Patcher::Filesystem::LastErrorString(("could not open " + path).c_str());
            return false;
        }

        LARGE_INTEGER fileSize = {};
        if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 0 ||
            static_cast<uint64_t>(fileSize.QuadPart) != entry.size)
        {
            CloseHandle(file);
            error = "assembled full-MPQ size does not match the manifest";
            return false;
        }

        Patcher::Hash::Md5 hash;
        if (!hash.Begin(error))
        {
            CloseHandle(file);
            return false;
        }

        std::vector<unsigned char> buffer(1024 * 1024);
        uint64_t hashed = 0;
        while (hashed < entry.size)
        {
            if (g_cancelRequested.load())
            {
                CloseHandle(file);
                error = "update canceled";
                return false;
            }
            const DWORD requested =
                static_cast<DWORD>((std::min)(entry.size - hashed, static_cast<uint64_t>(buffer.size())));
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), requested, &read, nullptr) || read == 0)
            {
                error = Patcher::Filesystem::LastErrorString(("could not verify " + path).c_str());
                CloseHandle(file);
                return false;
            }
            if (!hash.Update(buffer.data(), read, error))
            {
                CloseHandle(file);
                return false;
            }
            hashed += read;
            g_completedBytes = hashed;
        }
        CloseHandle(file);

        std::string actualMd5;
        if (!hash.Finish(actualMd5, error) || !Patcher::Filesystem::EqualsIgnoreCase(actualMd5, entry.md5))
        {
            if (error.empty())
                error = "assembled full-MPQ MD5 does not match the manifest";
            return false;
        }
        g_filesDone = 1;
        return true;
    }

    bool DownloadFullChunks(const std::string& targetPath, const std::string& temporaryPath, const Manifest& manifest,
                            const ManifestEntry& entry, std::string& error)
    {
        std::vector<uint64_t> offsets(entry.chunks.size(), 0);
        std::vector<uint64_t> progress(entry.chunks.size(), 0);
        std::vector<uint8_t> completedThisRun(entry.chunks.size(), 0);
        uint64_t offset = 0;
        for (size_t index = 0; index < entry.chunks.size(); ++index)
        {
            offsets[index] = offset;
            offset += entry.chunks[index].size;
            if (ChunkIsComplete(targetPath, entry, entry.chunks[index]))
                progress[index] = entry.chunks[index].size;
        }

        BeginDownload(entry.targetPath + " :: full archive parts", 0);
        g_currentFileBytes = std::accumulate(progress.begin(), progress.end(), uint64_t{0});

        const bool inWorld = ClntObjMgrGetActivePlayerObj() != nullptr;
        const size_t workerLimit =
            inWorld ? 1 : (GetSeekPenalty(targetPath) == SeekPenalty::DoesNotIncur ? 4 : 2);
        const size_t workerCount = (std::min)(entry.chunks.size(), workerLimit);
        std::atomic<size_t> nextChunk{0};
        std::atomic<bool> downloadFailed{false};
        std::mutex progressMutex;
        std::mutex errorMutex;
        std::string downloadError;
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (size_t worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back(
                [&]()
                {
                    ScopedBackgroundThreadMode backgroundMode(inWorld);
                    while (!downloadFailed.load() && !g_cancelRequested.load())
                    {
                        const size_t index = nextChunk.fetch_add(1);
                        if (index >= entry.chunks.size())
                            return;
                        const PayloadChunk& chunk = entry.chunks[index];
                        if (ChunkIsComplete(targetPath, entry, chunk))
                            continue;

                        std::string url;
                        std::string workerError;
                        if (!Patcher::Http::ResolveUrl(manifest.sourceUrl, chunk.payloadPath, url, workerError))
                        {
                            std::lock_guard<std::mutex> lock(errorMutex);
                            if (downloadError.empty())
                                downloadError = std::move(workerError);
                            downloadFailed = true;
                            return;
                        }

                        Patcher::Http::ResumeOptions options;
                        options.expectedSize        = chunk.size;
                        options.expectedMd5         = chunk.md5;
                        options.destinationOffset   = offsets[index];
                        options.preserveDestination = true;
                        options.flushToDisk         = false;
                        options.onProgress = [&, index](uint64_t downloaded, uint64_t)
                        {
                            std::lock_guard<std::mutex> lock(progressMutex);
                            progress[index] = downloaded;
                            g_currentFileBytes = std::accumulate(progress.begin(), progress.end(), uint64_t{0});
                            return !downloadFailed.load() && !g_cancelRequested.load();
                        };
                        if (!Patcher::Http::DownloadFileResumable(url, temporaryPath, options, workerError))
                        {
                            std::lock_guard<std::mutex> lock(errorMutex);
                            if (downloadError.empty())
                                downloadError = std::move(workerError);
                            downloadFailed = true;
                            return;
                        }
                        completedThisRun[index] = 1;
                    }
                });
        }
        for (std::thread& worker : workers)
            worker.join();

        if (std::any_of(completedThisRun.begin(), completedThisRun.end(), [](uint8_t done) { return done != 0; }))
        {
            HANDLE destination = CreateFileA(temporaryPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (destination == INVALID_HANDLE_VALUE || !FlushFileBuffers(destination))
            {
                error = Patcher::Filesystem::LastErrorString(("could not flush " + temporaryPath).c_str());
                if (destination != INVALID_HANDLE_VALUE)
                    CloseHandle(destination);
                return false;
            }
            CloseHandle(destination);

            for (size_t index = 0; index < entry.chunks.size(); ++index)
            {
                if (completedThisRun[index] == 0)
                    continue;
                if (!MarkChunkComplete(targetPath, entry, entry.chunks[index], error))
                    return false;
            }
        }

        if (downloadFailed.load() || g_cancelRequested.load())
        {
            error = downloadError.empty() ? "update canceled" : downloadError;
            return false;
        }
        for (const PayloadChunk& chunk : entry.chunks)
            CompleteDownload(chunk.size);
        return true;
    }

    bool ProcessFullEntry(const std::string& gameRoot, const Manifest& manifest, const ManifestEntry& entry,
                          bool reloadGlue, std::string& error)
    {
        const bool refreshNativeDbcs    = true;
        const std::string targetPath    = LocalPath(gameRoot, entry.targetPath);
        const std::string temporaryPath = targetPath + ".dhdownload";
        const std::string metadataPath  = temporaryPath + ".meta";
        const std::string stagedPath    = ExistingStagedPath(targetPath, entry.md5);

        std::error_code filesystemError;
        std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path(), filesystemError);
        if (filesystemError)
        {
            error = "could not create destination directory for " + entry.targetPath;
            return false;
        }

        if (FileMatches(targetPath, entry))
        {
            DeleteFileA(temporaryPath.c_str());
            DeleteFileA(metadataPath.c_str());
            RemoveFullChunkState(targetPath);
            return true;
        }

        if (FileMatches(stagedPath, entry))
        {
            DeleteFileA(temporaryPath.c_str());
            DeleteFileA(metadataPath.c_str());
            RemoveFullChunkState(targetPath);
            BeginPhase(Phase::Installing, entry.targetPath, 1, 1);
            if (MoveFileExA(stagedPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                ForgetLocalHash(stagedPath);
                RememberLocalHash(targetPath, entry.md5);
                EnqueueMount(targetPath, entry.md5, refreshNativeDbcs, reloadGlue,
                             entry.kind == ManifestEntry::Kind::FullMpqIfMissing);
                LOG_INFO << "ContentStreaming: promoted staged archive: " << targetPath;
            }
            else
            {
                if (!SchedulePromotionAfterExit(stagedPath, targetPath))
                {
                    error = "the downloaded game file could not be scheduled for installation";
                    return false;
                }
                EnqueueMount(stagedPath, entry.md5, refreshNativeDbcs, reloadGlue,
                     entry.kind == ManifestEntry::Kind::FullMpqIfMissing);
                LOG_WARN << "ContentStreaming: staged archive remains locked; mounting staged copy: " << stagedPath;
            }
            return true;
        }

        if (!entry.chunks.empty())
        {
            if (!PrepareChunkDestination(temporaryPath, metadataPath, targetPath, entry, error) ||
                !DownloadFullChunks(targetPath, temporaryPath, manifest, entry, error))
                return false;

            BeginPhase(Phase::Verifying, entry.targetPath, entry.size, 1);
            if (!VerifyChunkedArchive(temporaryPath, entry, error))
            {
                if (!g_cancelRequested.load())
                {
                    DeleteFileA(temporaryPath.c_str());
                    DeleteFileA(metadataPath.c_str());
                    RemoveFullChunkState(targetPath);
                }
                return false;
            }

            BeginPhase(Phase::Installing, entry.targetPath, 1, 1);
            if (!PromoteOrStage(temporaryPath, targetPath, entry, reloadGlue, error))
                return false;
            g_completedBytes = 1;
            g_filesDone      = 1;
            DeleteFileA(metadataPath.c_str());
            RemoveFullChunkState(targetPath);
            return true;
        }

        const std::string expectedMetadata = std::to_string(entry.size) + ":" + entry.md5;
        uint64_t resumeFrom                = 0;
        std::string metadata;
        std::string metadataError;
        if (Patcher::Filesystem::ReadTextFile(metadataPath, metadata, metadataError) && metadata == expectedMetadata)
        {
            std::error_code sizeError;
            uint64_t partialSize = std::filesystem::file_size(temporaryPath, sizeError);
            if (!sizeError && partialSize > 0 && partialSize < entry.size)
                resumeFrom = partialSize;
        }
        if (resumeFrom == 0)
        {
            DeleteFileA(temporaryPath.c_str());
            if (!Patcher::Filesystem::WriteTextFile(metadataPath, expectedMetadata, error))
                return false;
        }

        std::string downloadUrl;
        if (!Patcher::Http::ResolveUrl(manifest.sourceUrl, entry.payloadPath, downloadUrl, error))
            return false;

        BeginDownload(entry.targetPath, resumeFrom);
        Patcher::Http::ResumeOptions options;
        options.resumeFrom   = resumeFrom;
        options.expectedSize = entry.size;
        options.expectedMd5  = entry.md5;
        options.onProgress   = [](uint64_t downloaded, uint64_t)
        {
            g_currentFileBytes = downloaded;
            return !g_cancelRequested.load();
        };
        if (!Patcher::Http::DownloadFileResumable(downloadUrl, temporaryPath, options, error))
            return false;

        BeginPhase(Phase::Installing, entry.targetPath, 1, 1);
        if (!PromoteOrStage(temporaryPath, targetPath, entry, reloadGlue, error))
            return false;
        g_completedBytes = 1;
        g_filesDone      = 1;
        DeleteFileA(metadataPath.c_str());
        return true;
    }

    bool PayloadFileMatches(const std::string& path, const ManifestEntry& entry)
    {
        std::string hash;
        std::string error;
        uint64_t size = 0;
        return Patcher::Filesystem::HashFileMd5(path, hash, size, error) && size == entry.payloadSize &&
               Patcher::Filesystem::EqualsIgnoreCase(hash, entry.payloadMd5);
    }

    bool CachedArchiveRevisionMatches(const std::string& archivePath, const std::string& revision)
    {
        uint64_t size    = 0;
        int64_t modified = 0;
        if (!GetLocalFileState(archivePath, size, modified))
            return false;
        const auto cached = g_localHashes.find(LocalHashKey(archivePath));
        return cached != g_localHashes.end() && cached->second.size == size && cached->second.modified == modified &&
               Patcher::Filesystem::EqualsIgnoreCase(cached->second.md5, revision);
    }

    bool MatchArchiveMembers(const std::string& archivePath, const std::vector<const ManifestEntry*>& entries,
                             std::vector<bool>& matches, std::string& error,
                             Phase progressPhase = Phase::Disabled)
    {
        std::vector<Patcher::Mpq::FileExpectation> expectations;
        expectations.reserve(entries.size());
        for (const ManifestEntry* entry : entries)
            expectations.push_back({entry->memberPath, entry->size, entry->md5});

        const bool reportProgress = progressPhase == Phase::Checking || progressPhase == Phase::Verifying;
        if (reportProgress)
        {
            g_completedBytes   = 0;
            g_currentFileBytes = 0;
            g_filesDone        = 0;
            g_filesTotal       = static_cast<uint32_t>(entries.size());
            SetStatus(progressPhase, archivePath);
        }

        const Patcher::Mpq::MatchProgressCallback onProgress =
            reportProgress ?
                [](uint64_t completedBytes, uint64_t totalBytes, size_t completedFiles, size_t)
        {
            g_totalBytes           = totalBytes;
            uint64_t previousBytes = g_completedBytes.load();
            while (previousBytes < completedBytes &&
                   !g_completedBytes.compare_exchange_weak(previousBytes, completedBytes))
            {
            }
            const uint32_t completed = static_cast<uint32_t>(completedFiles);
            uint32_t previousFiles   = g_filesDone.load();
            while (previousFiles < completed && !g_filesDone.compare_exchange_weak(previousFiles, completed))
            {
            }
        } :
                Patcher::Mpq::MatchProgressCallback{};
        return Patcher::Mpq::MatchArchiveFilesMd5(archivePath, expectations, matches, error, onProgress);
    }

    bool AllArchiveMembersMatch(const std::string& archivePath, const std::vector<const ManifestEntry*>& entries,
                                Phase progressPhase = Phase::Disabled)
    {
        std::vector<bool> matches;
        std::string error;
        return MatchArchiveMembers(archivePath, entries, matches, error, progressPhase) && !matches.empty() &&
               std::all_of(matches.begin(), matches.end(), [](bool match) { return match; });
    }

    std::string MemberDownloadPath(const std::string& targetPath, const ManifestEntry& entry)
    {
        const char* extension = entry.bundled ? ".dhbundle" : ".dhdownload";
        return (std::filesystem::path(targetPath + ".dhparts") / (entry.payloadMd5 + extension)).string();
    }

    void RemoveMemberDownloadCache(const std::string& targetPath)
    {
        const std::filesystem::path cachePath(targetPath + ".dhparts");
        std::error_code filesystemError;
        const uintmax_t removed = std::filesystem::remove_all(cachePath, filesystemError);
        if (filesystemError)
        {
            LOG_WARN << "ContentStreaming: could not remove verified member cache " << cachePath.string() << ": "
                     << filesystemError.message();
        }
        else if (removed != 0)
        {
            LOG_INFO << "ContentStreaming: removed verified member cache " << cachePath.string() << " (" << removed
                     << " entries)";
        }
    }

    std::string MemberPayloadKey(const ManifestEntry& entry)
    {
        return Lowercase(entry.payloadPath) + ":" + std::to_string(entry.payloadSize) + ":" +
               Lowercase(entry.payloadMd5);
    }

    std::vector<std::string> ActivationMemberPaths(const std::vector<const ManifestEntry*>& changedEntries,
                                                   const std::vector<const ManifestEntry*>& deletedEntries)
    {
        std::vector<std::string> paths;
        paths.reserve(changedEntries.size() + deletedEntries.size());
        for (const ManifestEntry* entry : changedEntries)
            paths.push_back(entry->memberPath);
        for (const ManifestEntry* entry : deletedEntries)
            paths.push_back(entry->memberPath);
        return paths;
    }

    bool DownloadMemberPayload(const std::string& targetPath, const Manifest& manifest, const ManifestEntry& entry,
                               std::string& downloadedPath, const std::function<void(uint64_t)>& onProgress,
                               std::string& error)
    {
        downloadedPath                 = MemberDownloadPath(targetPath, entry);
        const std::string metadataPath = downloadedPath + ".meta";
        std::error_code filesystemError;
        std::filesystem::create_directories(std::filesystem::path(downloadedPath).parent_path(), filesystemError);
        if (filesystemError)
        {
            error = "could not create member download directory for " + entry.containerPath;
            return false;
        }

        const std::string expectedMetadata = std::to_string(entry.payloadSize) + ":" + entry.payloadMd5;
        uint64_t resumeFrom                = 0;
        std::string metadata;
        std::string metadataError;
        if (Patcher::Filesystem::ReadTextFile(metadataPath, metadata, metadataError) && metadata == expectedMetadata)
        {
            std::error_code sizeError;
            const uint64_t partialSize = std::filesystem::file_size(downloadedPath, sizeError);
            if (!sizeError && partialSize > 0 && partialSize < entry.payloadSize)
                resumeFrom = partialSize;
            else if (!sizeError && partialSize == entry.payloadSize && PayloadFileMatches(downloadedPath, entry))
            {
                if (onProgress)
                    onProgress(entry.payloadSize);
                return true;
            }
        }
        if (resumeFrom == 0)
        {
            DeleteFileA(downloadedPath.c_str());
            if (!Patcher::Filesystem::WriteTextFile(metadataPath, expectedMetadata, error))
                return false;
        }

        std::string downloadUrl;
        if (!Patcher::Http::ResolveUrl(manifest.sourceUrl, entry.payloadPath, downloadUrl, error))
            return false;

        if (onProgress)
            onProgress(resumeFrom);
        Patcher::Http::ResumeOptions options;
        options.resumeFrom   = resumeFrom;
        options.expectedSize = entry.payloadSize;
        options.expectedMd5  = entry.payloadMd5;
        options.flushToDisk  = !entry.bundled;
        options.onProgress   = [&onProgress](uint64_t downloaded, uint64_t)
        {
            if (onProgress)
                onProgress(downloaded);
            return !g_cancelRequested.load();
        };
        if (!Patcher::Http::DownloadFileResumable(downloadUrl, downloadedPath, options, error))
            return false;

        return true;
    }

    bool ProcessMemberGroup(const std::string& gameRoot, const Manifest& manifest,
                            const std::vector<const ManifestEntry*>& allEntries,
                            const std::vector<const ManifestEntry*>& changedEntries,
                            const std::vector<const ManifestEntry*>& deletedEntries, std::string& error)
    {
        const ManifestEntry& first   = *(allEntries.empty() ? deletedEntries.front() : allEntries.front());
        const std::string targetPath = LocalPath(gameRoot, first.containerPath);

        std::vector<Patcher::Mpq::FileReplacement> replacements;
        replacements.reserve(changedEntries.size());
        std::vector<const ManifestEntry*> payloadEntries;
        std::map<std::string, size_t> payloadIndexes;
        for (const ManifestEntry* entry : changedEntries)
        {
            const std::string payloadKey = MemberPayloadKey(*entry);
            if (payloadIndexes.emplace(payloadKey, payloadEntries.size()).second)
                payloadEntries.push_back(entry);
        }

        std::vector<std::string> payloadPaths(payloadEntries.size());
        std::vector<uint64_t> payloadProgress(payloadEntries.size(), 0);
        std::atomic<size_t> nextPayload{0};
        std::atomic<bool> downloadFailed{false};
        std::mutex progressMutex;
        std::mutex downloadErrorMutex;
        std::string downloadError;

        if (!payloadEntries.empty())
        {
            BeginDownload(first.containerPath +
                              (std::any_of(payloadEntries.begin(), payloadEntries.end(),
                                           [](const ManifestEntry* entry) { return entry->bundled; }) ?
                                   " :: update bundles" :
                                   " :: updated files"),
                          0);
            const bool inWorld = ClntObjMgrGetActivePlayerObj() != nullptr;
            const size_t workerLimit =
                inWorld || GetSeekPenalty(targetPath) != SeekPenalty::DoesNotIncur ? 1 : 4;
            const size_t workerCount = (std::min)(payloadEntries.size(), workerLimit);
            std::vector<std::thread> workers;
            workers.reserve(workerCount);
            for (size_t worker = 0; worker < workerCount; ++worker)
            {
                workers.emplace_back(
                    [&]()
                    {
                        ScopedBackgroundThreadMode backgroundMode(inWorld);
                        while (!downloadFailed.load() && !g_cancelRequested.load())
                        {
                            const size_t index = nextPayload.fetch_add(1);
                            if (index >= payloadEntries.size())
                                return;
                            const ManifestEntry& entry = *payloadEntries[index];
                            std::string workerError;
                            const bool downloaded = DownloadMemberPayload(
                                targetPath, manifest, entry, payloadPaths[index],
                                [&, index](uint64_t bytes)
                                {
                                    std::lock_guard<std::mutex> lock(progressMutex);
                                    payloadProgress[index] = bytes;
                                    g_currentFileBytes =
                                        std::accumulate(payloadProgress.begin(), payloadProgress.end(), uint64_t{0});
                                },
                                workerError);
                            if (!downloaded)
                            {
                                downloadFailed = true;
                                std::lock_guard<std::mutex> lock(downloadErrorMutex);
                                if (downloadError.empty())
                                    downloadError = std::move(workerError);
                                return;
                            }
                        }
                    });
            }
            for (std::thread& worker : workers)
                worker.join();

            if (downloadFailed.load() || g_cancelRequested.load())
            {
                error = downloadError.empty() ? "update canceled" : downloadError;
                return false;
            }
        }

        std::map<std::string, std::string> downloadedPayloads;
        for (size_t index = 0; index < payloadEntries.size(); ++index)
        {
            downloadedPayloads.emplace(MemberPayloadKey(*payloadEntries[index]), payloadPaths[index]);
            CompleteDownload(payloadEntries[index]->payloadSize);
        }

        for (const ManifestEntry* entry : changedEntries)
        {
            if (g_cancelRequested.load())
                return false;

            const std::string payloadKey = MemberPayloadKey(*entry);
            const auto downloaded        = downloadedPayloads.find(payloadKey);
            if (downloaded == downloadedPayloads.end())
            {
                error = "downloaded payload was not available for " + entry->memberPath;
                return false;
            }
            const std::string& downloadedPath = downloaded->second;

            Patcher::Mpq::FileReplacement replacement;
            replacement.logicalPath = entry->memberPath;
            if (entry->bundled)
                replacement.bundlePath = downloadedPath;
            else
                replacement.downloadedPath = downloadedPath;
            replacement.size = entry->size;
            replacement.md5  = entry->md5;
            replacements.push_back(std::move(replacement));
        }

        g_completedBytes   = 0;
        g_currentFileBytes = 0;
        g_filesDone        = 0;
        g_totalBytes       = 0;
        g_filesTotal       = static_cast<uint32_t>(replacements.size());
        SetStatus(Phase::Installing, first.containerPath);
        if (g_cancelRequested.load())
        {
            error = "update canceled";
            return false;
        }
        std::vector<std::string> deletions;
        deletions.reserve(deletedEntries.size());
        for (const ManifestEntry* entry : deletedEntries)
            deletions.push_back(entry->memberPath);

        const Patcher::Mpq::ProgressCallback onProgress =
            [](uint64_t completedBytes, uint64_t totalBytes, size_t completedFiles, size_t totalFiles)
        {
            g_completedBytes = completedBytes;
            g_totalBytes     = totalBytes;
            g_filesDone      = static_cast<uint32_t>(completedFiles);
            g_filesTotal     = static_cast<uint32_t>(totalFiles);
        };
        if (!Patcher::Mpq::InstallChangesInMountedArchive(targetPath, replacements, deletions, onProgress, error))
            return false;

        if (g_cancelRequested.load())
        {
            error = "update canceled";
            return false;
        }

        if (!changedEntries.empty() && !AllArchiveMembersMatch(targetPath, changedEntries, Phase::Verifying))
        {
            error = "patched archive failed member verification: " + first.containerPath;
            return false;
        }

        const bool deletionFailed =
            std::any_of(deletedEntries.begin(), deletedEntries.end(), [&](const ManifestEntry* entry)
                        { return Patcher::Mpq::ArchiveContainsFile(targetPath, entry->memberPath); });
        if (deletionFailed)
        {
            error = "patched archive failed deletion verification: " + first.containerPath;
            return false;
        }

        if (g_cancelRequested.load())
        {
            error = "update canceled";
            return false;
        }

        RememberLocalHash(targetPath, first.archiveRevision);
        const std::vector<std::string> activationMembers =
            ActivationMemberPaths(changedEntries, deletedEntries);
        const bool conservativeRefresh = !deletedEntries.empty() || activationMembers.empty();
        const bool reloadGlue =
            std::any_of(changedEntries.begin(), changedEntries.end(),
                        [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); }) ||
            std::any_of(deletedEntries.begin(), deletedEntries.end(),
                        [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); });
        EnqueueRefresh(targetPath, first.archiveRevision, activationMembers, conservativeRefresh, reloadGlue);

        for (const auto& [payloadKey, downloadedPath] : downloadedPayloads)
        {
            DeleteFileA(downloadedPath.c_str());
            DeleteFileA((downloadedPath + ".meta").c_str());
        }
        RemoveMemberDownloadCache(targetPath);
        return true;
    }

    struct FullWork
    {
        const ManifestEntry* entry = nullptr;
        bool needsDownload         = false;
        bool reloadGlue            = false;
    };

    struct MemberWork
    {
        std::string containerPath;
        std::string revision;
        std::vector<const ManifestEntry*> allEntries;
        std::vector<const ManifestEntry*> changedEntries;
        std::vector<const ManifestEntry*> deletedEntries;
        bool interfaceReloadRequired = false;
    };

    struct InstalledArchive
    {
        std::string revision;
        std::map<std::string, const ManifestEntry*> operations;
    };

    std::string MemberOperationKey(const ManifestEntry& entry)
    {
        std::string member = Lowercase(entry.memberPath);
        std::replace(member.begin(), member.end(), '/', '\\');
        return member;
    }

    bool EquivalentMemberOperation(const ManifestEntry& previous, const ManifestEntry& current)
    {
        if (previous.kind != current.kind)
            return false;
        if (current.kind == ManifestEntry::Kind::MpqDelete)
            return true;
        return current.kind == ManifestEntry::Kind::MpqMember && previous.size == current.size &&
               Patcher::Filesystem::EqualsIgnoreCase(previous.md5, current.md5);
    }

    std::map<std::string, InstalledArchive> IndexInstalledArchives(const Manifest& manifest)
    {
        std::map<std::string, InstalledArchive> archives;
        for (const ManifestEntry& entry : manifest.entries)
        {
            if (entry.kind != ManifestEntry::Kind::MpqMember && entry.kind != ManifestEntry::Kind::MpqDelete)
                continue;

            InstalledArchive& archive = archives[Lowercase(entry.containerPath)];
            archive.revision          = entry.archiveRevision;
            archive.operations[MemberOperationKey(entry)] = &entry;
        }
        return archives;
    }

    void SelectManifestDelta(const InstalledArchive& installed, MemberWork& current)
    {
        for (const ManifestEntry* entry : current.allEntries)
        {
            const auto previous = installed.operations.find(MemberOperationKey(*entry));
            if (previous == installed.operations.end() || !EquivalentMemberOperation(*previous->second, *entry))
                current.changedEntries.push_back(entry);
        }

        std::vector<const ManifestEntry*> pendingDeletions;
        pendingDeletions.reserve(current.deletedEntries.size());
        for (const ManifestEntry* entry : current.deletedEntries)
        {
            const auto previous = installed.operations.find(MemberOperationKey(*entry));
            if (previous == installed.operations.end() || !EquivalentMemberOperation(*previous->second, *entry))
                pendingDeletions.push_back(entry);
        }
        current.deletedEntries = std::move(pendingDeletions);
    }

    bool InstalledArchiveMatches(const InstalledArchive& installed, const MemberWork& current)
    {
        const size_t operationCount = current.allEntries.size() + current.deletedEntries.size();
        if (installed.operations.size() != operationCount)
            return false;

        const auto matches = [&](const ManifestEntry* entry)
        {
            const auto previous = installed.operations.find(MemberOperationKey(*entry));
            return previous != installed.operations.end() && EquivalentMemberOperation(*previous->second, *entry);
        };
        return std::all_of(current.allEntries.begin(), current.allEntries.end(), matches) &&
               std::all_of(current.deletedEntries.begin(), current.deletedEntries.end(), matches);
    }

    bool JsonString(const Patcher::Json::Value& object, const char* key, bool required, std::string& value,
                    std::string& error)
    {
        const Patcher::Json::Value* field = object.Find(key);
        if (!field)
        {
            if (!required)
            {
                value.clear();
                return true;
            }
            error = std::string("latest.json is missing ") + key;
            return false;
        }
        if (field->type != Patcher::Json::Type::String)
        {
            error = std::string("latest.json field must be text: ") + key;
            return false;
        }
        value = field->string;
        return true;
    }

    std::string InstalledVersionPath(const std::string& gameRoot)
    {
        return (std::filesystem::path(gameRoot) / ".duskhaven-streaming-version").string();
    }

    std::string CachedManifestPath(const std::string& gameRoot)
    {
        return (std::filesystem::path(gameRoot) / "Cache" / "duskhaven-streaming-current.mfil").string();
    }

    std::string InstalledManifestPath(const std::string& gameRoot)
    {
        return (std::filesystem::path(gameRoot) / ".duskhaven-streaming-installed.mfil").string();
    }

    bool InstalledVersionMatches(const std::string& gameRoot, const std::string& releaseVersion)
    {
        if (releaseVersion.empty() || !IsRegularFile(InstalledManifestPath(gameRoot)))
            return false;

        std::string installedVersion;
        std::string error;
        return Patcher::Filesystem::ReadTextFile(InstalledVersionPath(gameRoot), installedVersion, error) &&
               Trim(installedVersion) == releaseVersion;
    }

    bool WriteTextFileAtomically(const std::string& path, const std::string& contents, std::string& error)
    {
        const std::filesystem::path destination(path);
        std::error_code directoryError;
        std::filesystem::create_directories(destination.parent_path(), directoryError);
        if (directoryError)
        {
            error = "could not create state directory: " + directoryError.message();
            return false;
        }

        const std::string temporaryPath = path + ".tmp";
        HANDLE file = CreateFileA(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = Patcher::Filesystem::LastErrorString(("could not open " + temporaryPath).c_str());
            return false;
        }

        size_t offset = 0;
        while (offset < contents.size())
        {
            const DWORD requested = static_cast<DWORD>((std::min)(contents.size() - offset,
                                                                  static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD written = 0;
            if (!WriteFile(file, contents.data() + offset, requested, &written, nullptr) || written == 0)
            {
                error = Patcher::Filesystem::LastErrorString(("could not write " + temporaryPath).c_str());
                CloseHandle(file);
                DeleteFileA(temporaryPath.c_str());
                return false;
            }
            offset += written;
        }
        if (!FlushFileBuffers(file))
        {
            error = Patcher::Filesystem::LastErrorString(("could not flush " + temporaryPath).c_str());
            CloseHandle(file);
            DeleteFileA(temporaryPath.c_str());
            return false;
        }
        CloseHandle(file);
        if (MoveFileExA(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;

        error = Patcher::Filesystem::LastErrorString(("could not replace " + path).c_str());
        DeleteFileA(temporaryPath.c_str());
        return false;
    }

    bool LoadInstalledManifest(const std::string& gameRoot, Manifest& manifest, std::string& contents)
    {
        const std::string path = InstalledManifestPath(gameRoot);
        if (!IsRegularFile(path))
            return false;

        std::string error;
        if (!Patcher::Filesystem::ReadTextFile(path, contents, error) || !ParseManifest(contents, manifest, error))
        {
            LOG_WARN << "ContentStreaming: ignored invalid installed manifest: " << error;
            return false;
        }
        return true;
    }

    void RecordInstalledRelease(const std::string& gameRoot, const std::string& manifestContents,
                                const std::string& releaseVersion)
    {
        std::string error;
        if (!manifestContents.empty() &&
            !WriteTextFileAtomically(InstalledManifestPath(gameRoot), manifestContents, error))
        {
            LOG_WARN << "ContentStreaming: could not record installed manifest: " << error;
            return;
        }

        if (!releaseVersion.empty())
        {
            const std::string versionContents = releaseVersion + "\n";
            std::string installedVersion;
            std::string readError;
            if (Patcher::Filesystem::ReadTextFile(InstalledVersionPath(gameRoot), installedVersion, readError) &&
                installedVersion == versionContents)
                return;
            if (!WriteTextFileAtomically(InstalledVersionPath(gameRoot), versionContents, error))
                LOG_WARN << "ContentStreaming: could not record installed release: " << error;
        }
    }

    void StageInstalledRelease(const std::string& gameRoot, const std::string& manifestContents,
                               const std::string& releaseVersion)
    {
        std::lock_guard<std::mutex> lock(g_installedReleaseMutex);
        g_pendingInstalledRelease = {gameRoot, manifestContents, releaseVersion, true};
    }

    void ClearPendingInstalledRelease()
    {
        std::lock_guard<std::mutex> lock(g_installedReleaseMutex);
        g_pendingInstalledRelease = {};
    }

    void CommitPendingInstalledRelease()
    {
        PendingInstalledRelease pending;
        {
            std::lock_guard<std::mutex> lock(g_installedReleaseMutex);
            if (!g_pendingInstalledRelease.ready)
                return;
            pending = std::move(g_pendingInstalledRelease);
            g_pendingInstalledRelease = {};
        }
        RecordInstalledRelease(pending.gameRoot, pending.manifestContents, pending.releaseVersion);
    }

    bool HashTextSha256(const std::string& text, std::string& hash, std::string& error)
    {
        Patcher::Hash::Sha256 sha;
        return sha.Begin(error) && sha.Update(text.data(), static_cast<uint32_t>(text.size()), error) &&
               sha.Finish(hash, error);
    }

    bool FetchCurrentManifest(const std::string& url, const std::string& gameRoot, std::string& contents,
                              std::string& releaseVersion, std::string& resolvedManifestUrl,
                              RuntimeUpdate::ReleasePointer& runtimeRelease, std::string& error)
    {
        if (!Patcher::Http::IsSecureOrLoopbackUrl(url))
        {
            error = "content streaming requires HTTPS (HTTP is allowed only for localhost testing)";
            return false;
        }

        const bool latestIndex = EndsWithIgnoreCase(url, "/latest.json");
        std::string first;
        if (!Patcher::Http::DownloadString(
                url, latestIndex ? LATEST_INDEX_DOWNLOAD_LIMIT : RELEASE_MANIFEST_DOWNLOAD_LIMIT, first, error))
            return false;
        const std::string trimmed = Trim(first);
        if (trimmed.empty() || trimmed.front() != '{')
        {
            if (latestIndex)
            {
                error = "latest.json did not contain a JSON release index";
                return false;
            }
            resolvedManifestUrl = url;
            contents = std::move(first);
            return true;
        }

        Patcher::Json::Value root;
        if (!Patcher::Json::Parse(trimmed, root, error) || root.type != Patcher::Json::Type::Object)
        {
            error = "latest.json could not be read: " + error;
            return false;
        }
        std::string manifestReference;
        std::string expectedSha256;
        if (!JsonString(root, "latest", true, releaseVersion, error) ||
            !JsonString(root, "manifest", true, manifestReference, error) ||
            !JsonString(root, "manifestSha256", true, expectedSha256, error))
            return false;
        if (!IsSafeReleaseId(releaseVersion) || !IsSafeManifestReference(manifestReference) ||
            !IsHexSha256(expectedSha256))
        {
            error = "latest.json contains invalid release metadata";
            return false;
        }

        // A realm can serve its tracked pointer while immutable content stays on a CDN.
        // Existing CDN-only pointers keep their original same-origin behavior.
        std::string contentBase;
        if (!JsonString(root, "baseUrl", false, contentBase, error))
            return false;
        std::string contentIndexUrl = url;
        if (!contentBase.empty())
        {
            if (contentBase.back() != '/' || contentBase.find_first_of("?#") != std::string::npos ||
                !Patcher::Http::IsSecureOrLoopbackUrl(contentBase) ||
                !Patcher::Http::ResolveUrl(contentBase, "latest.json", contentIndexUrl, error))
            {
                error = "latest.json baseUrl must be a secure directory URL";
                return false;
            }
        }

        std::string runtimeManifestReference;
        std::string runtimeManifestSha256;
        if (!JsonString(root, "runtimeManifest", false, runtimeManifestReference, error) ||
            !JsonString(root, "runtimeManifestSha256", false, runtimeManifestSha256, error))
            return false;
        if (runtimeManifestReference.empty() != runtimeManifestSha256.empty())
        {
            error = "latest.json contains an incomplete runtime release";
            return false;
        }
        if (!runtimeManifestReference.empty())
        {
            if (!IsSafeManifestReference(runtimeManifestReference) || !IsHexSha256(runtimeManifestSha256) ||
                !Patcher::Http::ResolveUrl(contentIndexUrl, runtimeManifestReference, runtimeRelease.manifestUrl, error) ||
                !Patcher::Http::IsSecureOrLoopbackUrl(runtimeRelease.manifestUrl) ||
                !Patcher::Http::HaveSameOrigin(contentIndexUrl, runtimeRelease.manifestUrl) ||
                !EndsWithIgnoreCase(runtimeRelease.manifestUrl, "/runtime.json"))
            {
                error = "latest.json runtime manifest must be a same-origin HTTPS runtime.json URL";
                return false;
            }
            runtimeRelease.indexUrl      = contentIndexUrl;
            runtimeRelease.manifestSha256 = Lowercase(runtimeManifestSha256);
        }

        if (!Patcher::Http::ResolveUrl(contentIndexUrl, manifestReference, resolvedManifestUrl, error) ||
            !Patcher::Http::IsSecureOrLoopbackUrl(resolvedManifestUrl) ||
            !Patcher::Http::HaveSameOrigin(contentIndexUrl, resolvedManifestUrl) ||
            !EndsWithIgnoreCase(resolvedManifestUrl, "/streaming.mfil"))
        {
            error = "latest.json manifest must be a same-origin HTTPS streaming.mfil URL";
            return false;
        }

        // A cached immutable manifest lets startup validate local archives without
        // downloading the large catalog again.
        const std::string cachedManifestPath = CachedManifestPath(gameRoot);
        std::string cachedContents;
        std::string cacheError;
        if (Patcher::Filesystem::ReadTextFile(cachedManifestPath, cachedContents, cacheError))
        {
            std::string cachedSha256;
            if (HashTextSha256(cachedContents, cachedSha256, cacheError) &&
                Patcher::Filesystem::EqualsIgnoreCase(cachedSha256, expectedSha256))
            {
                contents = std::move(cachedContents);
                LOG_INFO << "ContentStreaming: reused cached release manifest for " << releaseVersion;
                return true;
            }

            LOG_WARN << "ContentStreaming: ignored stale cached release manifest";
        }

        if (!Patcher::Http::DownloadString(resolvedManifestUrl, RELEASE_MANIFEST_DOWNLOAD_LIMIT, contents, error))
            return false;
        std::string actualSha256;
        if (!HashTextSha256(contents, actualSha256, error) ||
            !Patcher::Filesystem::EqualsIgnoreCase(actualSha256, expectedSha256))
        {
            error = "downloaded release manifest failed SHA-256 verification";
            return false;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(std::filesystem::path(cachedManifestPath).parent_path(), directoryError);
        std::string writeError;
        if (directoryError || !Patcher::Filesystem::WriteTextFile(cachedManifestPath, contents, writeError))
            LOG_WARN << "ContentStreaming: could not cache release manifest: " << writeError;
        return true;
    }

    void Run(const std::string manifestUrl, bool forceRepair)
    {
        ScopedBackgroundThreadMode backgroundMode(ClntObjMgrGetActivePlayerObj() != nullptr);
        SetStatus(Phase::Checking);
        const std::string gameRoot = Patcher::Filesystem::GetExeDirectory();
        if (gameRoot.empty())
        {
            SetStatus(Phase::Failed, {}, "client root unavailable");
            return;
        }

        std::string contents;
        std::string releaseVersion;
        std::string resolvedManifestUrl;
        RuntimeUpdate::ReleasePointer runtimeRelease;
        std::string error;
        if (!g_updateLock.Acquire(std::filesystem::path(gameRoot), error))
        {
            SetStatus(Phase::Failed, {}, error);
            return;
        }
        for (const char* stateName : {".duskhaven-streaming-local.txt", ".duskhaven-streaming-local.txt.tmp",
                                      ".duskhaven-streaming-installed.mfil", ".duskhaven-streaming-installed.mfil.tmp",
                                      ".duskhaven-streaming-version", ".duskhaven-streaming-version.tmp",
                                      "Cache/duskhaven-streaming-current.mfil"})
        {
            if (!ValidateUpdatePath(std::filesystem::path(gameRoot) / stateName, false, error))
            {
                SetStatus(Phase::Failed, {}, error);
                return;
            }
        }
        LoadLocalHashes(gameRoot);
        if (!FetchCurrentManifest(manifestUrl, gameRoot, contents, releaseVersion, resolvedManifestUrl,
                                  runtimeRelease, error))
        {
            SetStatus(Phase::Failed, {}, error);
            LOG_ERROR << "ContentStreaming: " << error;
            return;
        }
        if (g_runtimeRestartRequired.load())
        {
            SetStatus(Phase::RestartRequired, {},
                      "Specific game files have been downloaded that require a client restart to take effect.");
            return;
        }

        if (g_cancelRequested.load())
        {
            SetStatus(Phase::Canceled);
            return;
        }

        // Loose patch directories identify a local development client. Its
        // runtime binaries must stay paired with the developer's source build.
        bool developmentClient = false;
        if (!ValidateUpdatePath(std::filesystem::path(gameRoot) / "Data", true, error))
        {
            SetStatus(Phase::Failed, {}, error);
            return;
        }
        for (char letter = 'A'; letter <= 'Z'; ++letter)
        {
            const std::string archive = std::string("Data/patch-") + letter + ".MPQ";
            const DWORD attributes = GetFileAttributesA(LocalPath(gameRoot, archive).c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                developmentClient = true;
                break;
            }
        }
        if (developmentClient)
            LOG_INFO << "ContentStreaming: preserving locally built runtime in development client";

        if (runtimeRelease.Available() && !developmentClient)
        {
            RuntimeUpdate::ProgressCallbacks runtimeProgress;
            runtimeProgress.beginDownloads = [](uint64_t bytes, uint32_t files)
            {
                g_downloadCompletedBytes = 0;
                g_downloadTotalBytes     = bytes;
                g_downloadFilesDone      = 0;
                g_downloadFilesTotal     = files;
                BeginPhase(Phase::Downloading, "Downloading client runtime", bytes, files);
            };
            runtimeProgress.beginFile = [](const std::string& name, uint64_t resumeFrom)
            {
                BeginDownload("Client runtime :: " + name, resumeFrom);
            };
            runtimeProgress.updateFile = [](uint64_t completed, uint64_t)
            {
                g_currentFileBytes = completed;
                return !g_cancelRequested.load();
            };
            runtimeProgress.completeFile = [](uint64_t bytes) { CompleteDownload(bytes); };

            RuntimeUpdate::Result runtimeResult = RuntimeUpdate::Result::Current;
            if (!RuntimeUpdate::Prepare(runtimeRelease, gameRoot, forceRepair, runtimeProgress, runtimeResult,
                                        error))
            {
                if (g_cancelRequested.load())
                    SetStatus(Phase::Canceled);
                else
                    SetStatus(Phase::Failed, {}, error);
                LOG_ERROR << "ContentStreaming: runtime update failed: " << error;
                return;
            }
            if (runtimeResult == RuntimeUpdate::Result::RestartRequired)
            {
                g_runtimeRestartRequired = true;
                LOG_INFO << "ContentStreaming: staged a client runtime update for installation after exit";
                SetStatus(Phase::RestartRequired, {},
                          "Specific game files have been downloaded that require a client restart to take effect.");
                return;
            }
        }

        Manifest manifest;
        if (!ParseManifest(contents, manifest, error))
        {
            SetStatus(Phase::Failed, {},
                      "The game update could not be checked. Please restart the client and try again.");
            LOG_ERROR << "ContentStreaming: " << error;
            return;
        }
        if (!Patcher::Http::HaveSameOrigin(resolvedManifestUrl, manifest.sourceUrl))
        {
            SetStatus(Phase::Failed, {}, "The game update source did not match the trusted manifest host.");
            LOG_ERROR << "ContentStreaming: hosted manifest source must use the manifest origin";
            return;
        }

        // Loose development patches belong to the developer, not the updater.
        // Validate the parent first; only the archive itself may be a directory/link.
        if (!ValidateUpdatePath(std::filesystem::path(gameRoot) / "Data", true, error))
        {
            SetStatus(Phase::Failed, {}, error);
            return;
        }
        std::set<std::string> looseArchives;
        for (const ManifestEntry& entry : manifest.entries)
        {
            const std::string& archive = entry.containerPath.empty() ? entry.targetPath : entry.containerPath;
            const DWORD attributes = GetFileAttributesA(LocalPath(gameRoot, archive).c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                if (looseArchives.insert(Lowercase(archive)).second)
                    LOG_INFO << "ContentStreaming: preserving loose development patch " << archive;
            }
        }
        manifest.entries.erase(std::remove_if(manifest.entries.begin(), manifest.entries.end(),
            [&](const ManifestEntry& entry)
            {
                const std::string& archive = entry.containerPath.empty() ? entry.targetPath : entry.containerPath;
                return looseArchives.count(Lowercase(archive)) != 0;
            }), manifest.entries.end());

        std::set<std::string> checkedArchives;
        for (const ManifestEntry& entry : manifest.entries)
        {
            const std::string& archive = entry.containerPath.empty() ? entry.targetPath : entry.containerPath;
            if (!checkedArchives.insert(Lowercase(archive)).second)
                continue;
            const std::string target = LocalPath(gameRoot, archive);
            for (const char* suffix : {"", ".dhdownload", ".dhdownload.meta"})
            {
                if (!ValidateUpdatePath(std::filesystem::path(target + suffix), false, error))
                {
                    SetStatus(Phase::Failed, archive, error);
                    return;
                }
            }
            if (!entry.md5.empty() && entry.containerPath.empty() &&
                (!ValidateUpdatePath(std::filesystem::path(FlatNonMountableStagedPath(target, entry.md5)), false, error) ||
                 !ValidateUpdatePath(std::filesystem::path(FlatLegacyStagedPath(target, entry.md5)), false, error)))
            {
                SetStatus(Phase::Failed, archive, error);
                return;
            }
            if (!ValidateUpdateCache(std::filesystem::path(target + ".dhchunks"), error) ||
                !ValidateUpdateCache(std::filesystem::path(target + ".dhparts"), error))
            {
                SetStatus(Phase::Failed, archive, error);
                return;
            }
        }
        if (!ValidateUpdateCache(std::filesystem::path(gameRoot) / "Data" / ".dhstaged", error))
        {
            SetStatus(Phase::Failed, {}, error);
            return;
        }

        Manifest installedManifest;
        std::string installedManifestContents;
        std::map<std::string, InstalledArchive> installedArchives;
        if (!forceRepair && InstalledVersionMatches(gameRoot, releaseVersion))
        {
            // This exact release already owns the durable baseline. The archive
            // revision cache can prove each MPQ without rereading the large file.
            installedManifestContents = contents;
        }
        else if (!forceRepair && LoadInstalledManifest(gameRoot, installedManifest, installedManifestContents))
        {
            installedArchives = IndexInstalledArchives(installedManifest);
            LOG_INFO << "ContentStreaming: loaded the last successfully installed manifest";
        }

        std::vector<FullWork> fullWork;
        std::map<std::string, const ManifestEntry*> missingFallbacks;
        std::map<std::string, MemberWork> memberWorkByArchive;
        bool installedFallback = false;
        for (const ManifestEntry& entry : manifest.entries)
        {
            if (entry.kind == ManifestEntry::Kind::FullMpq)
            {
                const std::string targetPath = LocalPath(gameRoot, entry.targetPath);
                if (FileMatches(targetPath, entry, !forceRepair))
                    continue;
                std::string lockMessage;
                if (DescribeExternalArchiveLock(targetPath, entry.targetPath, gameRoot, lockMessage))
                {
                    SetStatus(Phase::Failed, entry.targetPath, lockMessage);
                    return;
                }
                const bool stagedReady =
                    FileMatches(ExistingStagedPath(targetPath, entry.md5), entry, !forceRepair);
                fullWork.push_back({&entry, !stagedReady, false});
                continue;
            }

            if (entry.kind == ManifestEntry::Kind::FullMpqIfMissing)
            {
                missingFallbacks[Lowercase(entry.targetPath)] = &entry;
                continue;
            }

            const std::string key = Lowercase(entry.containerPath);
            MemberWork& work      = memberWorkByArchive[key];
            work.containerPath    = entry.containerPath;
            work.revision         = entry.archiveRevision;
            if (entry.kind == ManifestEntry::Kind::MpqDelete)
                work.deletedEntries.push_back(&entry);
            else
                work.allEntries.push_back(&entry);
        }

        uint64_t totalBytes      = 0;
        uint32_t filesToDownload = 0;
        std::set<std::string> uniqueMemberPayloads;
        for (const FullWork& work : fullWork)
        {
            if (work.needsDownload)
            {
                totalBytes += work.entry->size;
                filesToDownload += static_cast<uint32_t>(
                    work.entry->chunks.empty() ? 1 : work.entry->chunks.size());
            }
        }

        std::vector<MemberWork*> memberWork;
        for (auto& [key, work] : memberWorkByArchive)
        {
            const std::string targetPath = LocalPath(gameRoot, work.containerPath);
            const DWORD targetAttributes = GetFileAttributesA(targetPath.c_str());
            const bool targetExists =
                targetAttributes != INVALID_FILE_ATTRIBUTES && (targetAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
            const auto fallback = missingFallbacks.find(key);
            if (!targetExists && fallback != missingFallbacks.end())
            {
                const ManifestEntry* entry = fallback->second;
                installedFallback          = true;
                const bool stagedReady =
                    FileMatches(ExistingStagedPath(targetPath, entry->md5), *entry, !forceRepair);
                const bool reloadGlue =
                    std::any_of(work.allEntries.begin(), work.allEntries.end(),
                                [](const ManifestEntry* member) { return IsInterfaceReloadMember(*member); }) ||
                    std::any_of(work.deletedEntries.begin(), work.deletedEntries.end(),
                                [](const ManifestEntry* member) { return IsInterfaceReloadMember(*member); });
                fullWork.push_back({entry, !stagedReady, reloadGlue});
                if (!stagedReady)
                {
                    totalBytes += entry->size;
                    filesToDownload +=
                        static_cast<uint32_t>(entry->chunks.empty() ? 1 : entry->chunks.size());
                }
                LOG_INFO << "ContentStreaming: " << work.containerPath << " is missing; selected full-MPQ fallback";
                continue;
            }

            if (g_cancelRequested.load())
            {
                SetStatus(Phase::Canceled);
                return;
            }

            const auto installed = installedArchives.find(key);
            const bool hasInstalledArchive = installed != installedArchives.end();
            const bool installedMatchesCurrent =
                hasInstalledArchive && InstalledArchiveMatches(installed->second, work);
            if (!forceRepair && CachedArchiveRevisionMatches(targetPath, work.revision))
            {
                if (!hasInstalledArchive || installedMatchesCurrent)
                {
                    RemoveMemberDownloadCache(targetPath);
                    continue;
                }

                if (!Patcher::Filesystem::EqualsIgnoreCase(installed->second.revision, work.revision))
                {
                    // The files were already written and verified, but the prior run
                    // did not finish activating the mounted archive.
                    MemberWork activationDelta = work;
                    SelectManifestDelta(installed->second, activationDelta);
                    const std::vector<std::string> activationMembers =
                        ActivationMemberPaths(activationDelta.changedEntries, activationDelta.deletedEntries);
                    const bool conservativeRefresh =
                        !activationDelta.deletedEntries.empty() || activationMembers.empty();
                    const bool reloadGlue =
                        std::any_of(activationDelta.changedEntries.begin(), activationDelta.changedEntries.end(),
                                    [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); }) ||
                        std::any_of(activationDelta.deletedEntries.begin(), activationDelta.deletedEntries.end(),
                                    [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); });
                    EnqueueRefresh(targetPath, work.revision, activationMembers, conservativeRefresh, reloadGlue);
                    RemoveMemberDownloadCache(targetPath);
                    continue;
                }

                LOG_WARN << "ContentStreaming: archive revision was reused with different member metadata: "
                         << work.containerPath;
            }

            bool usedManifestDelta = false;
            if (!forceRepair && hasInstalledArchive &&
                CachedArchiveRevisionMatches(targetPath, installed->second.revision))
            {
                SelectManifestDelta(installed->second, work);
                usedManifestDelta = !work.changedEntries.empty() || !work.deletedEntries.empty();
                if (usedManifestDelta)
                {
                    LOG_INFO << "ContentStreaming: selected manifest delta for " << work.containerPath << " ("
                             << work.changedEntries.size() << " replacement(s), " << work.deletedEntries.size()
                             << " deletion(s))";
                }
                else if (!Patcher::Filesystem::EqualsIgnoreCase(installed->second.revision, work.revision))
                {
                    LOG_WARN << "ContentStreaming: revision changed without a member delta; scanning "
                             << work.containerPath;
                }
            }

            if (!usedManifestDelta)
            {
                std::vector<bool> targetMatches;
                if (!work.allEntries.empty() &&
                    !MatchArchiveMembers(targetPath, work.allEntries, targetMatches, error, Phase::Checking))
                {
                    SetStatus(Phase::Failed, work.containerPath,
                              "The game files could not be checked. Please restart the client and try again.");
                    LOG_ERROR << "ContentStreaming: " << error;
                    return;
                }

                const bool allCurrentMembersMatch =
                    work.allEntries.empty() ||
                    (targetMatches.size() == work.allEntries.size() &&
                     std::all_of(targetMatches.begin(), targetMatches.end(), [](bool match) { return match; }));
                std::vector<const ManifestEntry*> pendingDeletions;
                pendingDeletions.reserve(work.deletedEntries.size());
                for (const ManifestEntry* entry : work.deletedEntries)
                {
                    if (Patcher::Mpq::ArchiveContainsFile(targetPath, entry->memberPath))
                        pendingDeletions.push_back(entry);
                }
                work.deletedEntries = std::move(pendingDeletions);

                if (allCurrentMembersMatch && work.deletedEntries.empty())
                {
                    RememberLocalHash(targetPath, work.revision);
                    RemoveMemberDownloadCache(targetPath);
                    continue;
                }

                for (size_t index = 0; index < work.allEntries.size(); ++index)
                {
                    if (index >= targetMatches.size() || !targetMatches[index])
                        work.changedEntries.push_back(work.allEntries[index]);
                }
            }

            ClientArchive::MountedArchive mountedArchive;
            std::string writableError;
            if (!ClientArchive::EnsureMountedWritable(targetPath, mountedArchive, writableError))
            {
                SetStatus(Phase::Failed, work.containerPath, writableError);
                return;
            }

            for (const ManifestEntry* entry : work.changedEntries)
            {
                const std::string payloadKey = MemberPayloadKey(*entry);
                if (uniqueMemberPayloads.insert(payloadKey).second)
                {
                    totalBytes += entry->payloadSize;
                    ++filesToDownload;
                }
            }
            work.interfaceReloadRequired =
                std::any_of(work.changedEntries.begin(), work.changedEntries.end(),
                            [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); }) ||
                std::any_of(work.deletedEntries.begin(), work.deletedEntries.end(),
                            [](const ManifestEntry* entry) { return IsInterfaceReloadMember(*entry); });
            memberWork.push_back(&work);
        }

        if (!fullWork.empty() || !memberWork.empty())
            g_contentUpdateRequired = true;

        std::stable_partition(fullWork.begin(), fullWork.end(),
                              [](const FullWork& work) { return work.reloadGlue; });

        g_downloadCompletedBytes = 0;
        g_downloadTotalBytes     = totalBytes;
        g_downloadFilesDone      = 0;
        g_downloadFilesTotal     = filesToDownload;
        g_completedBytes         = 0;
        g_currentFileBytes       = 0;
        g_filesDone              = 0;
        g_totalBytes             = totalBytes;
        g_filesTotal             = filesToDownload;
        LOG_INFO << "ContentStreaming: plan contains " << filesToDownload << " payload download(s), "
                 << memberWork.size() << " file-level MPQ update(s), and " << fullWork.size() << " full-MPQ update(s)";

        for (const FullWork& work : fullWork)
        {
            if (g_cancelRequested)
            {
                SetStatus(Phase::Canceled);
                return;
            }

            if (!ProcessFullEntry(gameRoot, manifest, *work.entry, work.reloadGlue, error))
            {
                if (g_cancelRequested)
                    SetStatus(Phase::Canceled);
                else
                    SetStatus(Phase::Failed, work.entry->targetPath, error);
                LOG_ERROR << "ContentStreaming: " << error;
                return;
            }

            RemoveMemberDownloadCache(LocalPath(gameRoot, work.entry->targetPath));

            if (work.needsDownload && work.entry->chunks.empty())
                CompleteDownload(work.entry->size);
        }

        for (MemberWork* work : memberWork)
        {
            if (g_cancelRequested)
            {
                SetStatus(Phase::Canceled);
                return;
            }
            if (!ProcessMemberGroup(gameRoot, manifest, work->allEntries, work->changedEntries, work->deletedEntries,
                                    error))
            {
                if (g_cancelRequested)
                    SetStatus(Phase::Canceled);
                else
                    SetStatus(Phase::Failed, work->containerPath, error);
                LOG_ERROR << "ContentStreaming: " << error;
                return;
            }
            if (work->interfaceReloadRequired)
                g_interfaceReloadRequired = true;
        }

        if (g_cancelRequested.load())
        {
            SetStatus(Phase::Canceled);
            return;
        }
        if (g_activationFailed.load() || g_phase.load() == Phase::Failed)
        {
            SetStatus(Phase::Failed, {}, "An updated archive could not be activated. Please restart the client.");
            return;
        }
        const bool catchUpAfterFallback = installedFallback && !g_postExitPromotionScheduled.load();
        if (catchUpAfterFallback)
        {
            // Mount the fallback before applying release deltas in a second pass.
            g_catchUpAfterFallbackMount = true;
            LOG_INFO << "ContentStreaming: installed missing-MPQ fallback; deferring release completion "
                        "until member catch-up";
        }
        if (!g_postExitPromotionScheduled.load() && !catchUpAfterFallback)
        {
            // Even an already-current seed needs its pending tables loaded.
            // Do this only after every member group has passed verification.
            {
                std::lock_guard<std::mutex> lock(g_mountMutex);
                for (const auto& [path, request] : g_deferredFallbackActivations)
                {
                    if (g_queuedMounts.insert(request.dedupeKey).second)
                    {
                        g_mountQueue.push_back(request);
                        ++g_pendingMountCount;
                    }
                }
                g_deferredFallbackActivations.clear();
            }
            // A development checkout did not install the complete release.
            if (looseArchives.empty())
                StageInstalledRelease(gameRoot, contents == installedManifestContents ? std::string{} : contents,
                                      releaseVersion);
        }
        g_downloadsFinished          = true;
        const uint32_t pendingMounts = g_pendingMountCount.load();
        if (pendingMounts == 0)
        {
            if (g_catchUpAfterFallbackMount.exchange(false))
            {
                // Start the catch-up pass after this Run invocation releases g_running.
                g_forceRepairRequested = true;
                g_recheckRequested     = true;
            }
            else
            {
                const std::string compatibilityError = CDBC::PendingCompatibilityError();
                if (!g_runtimeRestartRequired.load() && !compatibilityError.empty())
                {
                    ClearPendingInstalledRelease();
                    SetStatus(Phase::Failed, {}, compatibilityError);
                    return;
                }
                CommitPendingInstalledRelease();
                if (g_runtimeRestartRequired.load())
                    SetStatus(Phase::RestartRequired, {},
                              "Specific game files have been downloaded that require a client restart to take effect.");
                else
                    SetStatus(Phase::Complete);
            }
        }
        else
            BeginPhase(Phase::Installing, "Activating updated archives", pendingMounts, pendingMounts);
    }

    double Progress()
    {
        const uint64_t total = g_totalBytes.load();
        if (total == 0)
        {
            const Phase phase = g_phase.load();
            return phase == Phase::Complete || phase == Phase::RestartRequired ? 1.0 : 0.0;
        }
        const uint64_t done = (std::min)(total, g_completedBytes.load() + g_currentFileBytes.load());
        return static_cast<double>(done) / static_cast<double>(total);
    }

    void ShowStockDownloadScreen(bool preserveTerminalState = false)
    {
        if (!preserveTerminalState && !IsNativeLoginRejectedPhase(g_phase.load()))
        {
            g_stockDownloadGlueState = nullptr;
            return;
        }

        lua_State* state = ClientLua::State();
        if (!state)
            return;

        const auto now = std::chrono::steady_clock::now();
        // GlueXML reload completion is a one-shot handoff. Never drop it to
        // the normal refresh throttle or the freshly loaded Patch A screen
        // can return to the login form instead of preserving terminal state.
        if (!preserveTerminalState && now < g_nextStockDownloadAttempt)
            return;
        g_nextStockDownloadAttempt = now + std::chrono::milliseconds(250);

        if (state != g_stockDownloadGlueState &&
            !ClientLua::DoString(ContentStreaming::kStockPatchDownloadLua, "StockPatchDownload.lua"))
            return;

        constexpr int LUA_GLOBALS_INDEX = -10002;
        ClientLua::PushNil(state);
        ClientLua::SetField(state, LUA_GLOBALS_INDEX, "DuskhavenStockContentStreamingScreenReady");

        const char* script = preserveTerminalState ? R"LUA(
DuskhavenStockContentStreamingScreenReady =
    type(Duskhaven_ShowStockContentStreamingScreen) == "function" and
    Duskhaven_ShowStockContentStreamingScreen(true) == true;
)LUA"
                                                   : R"LUA(
DuskhavenStockContentStreamingScreenReady =
    type(Duskhaven_ShowStockContentStreamingScreen) == "function" and
    Duskhaven_ShowStockContentStreamingScreen(false) == true;
)LUA";
        ClientLua::DoString(script, "ContentStreamingStockGlue");
        const int stackTop = ClientLua::GetTop(state);
        ClientLua::GetField(state, LUA_GLOBALS_INDEX, "DuskhavenStockContentStreamingScreenReady");
        const bool ready = ClientLua::ToBoolean(state, -1) != 0;
        ClientLua::SetTop(state, stackTop);
        g_stockDownloadGlueState = ready ? state : nullptr;
    }

    bool IsLoginBlocked()
    {
        return IsNativeLoginRejectedPhase(g_phase.load());
    }

    void OnGlueIdle()
    {
        bool glueReloadCompleted = false;

        if (g_glueReloadPending && CGlueMgr::m_reload == 0)
        {
            if (CGlueMgr::m_suspended != 0)
            {
                g_glueReloadPending = false;
                g_glueReloadRequired = true;
            }
            else
            {
                g_glueReloadPending = false;
                g_stockDownloadGlueState = nullptr;
                glueReloadCompleted = true;
            }
        }

        PumpMountQueue();
        if (g_glueReloadRequired && !g_glueReloadPending && CGlueMgr::m_suspended == 0 &&
            ClntObjMgrGetActivePlayerObj() == nullptr)
        {
            g_glueReloadRequired = false;
            g_glueReloadPending = true;
            CGlueMgr::m_reload = 1;
            LOG_INFO << "ContentStreaming: requested GlueXML reload after mounting the interface fallback";
        }
        ShowStockDownloadScreen(glueReloadCompleted);
    }
} // namespace

namespace ContentStreaming
{
    void Trigger(bool forceRepair)
    {
        if (!g_started.load())
            return;

        if (g_runtimeRestartRequired.load())
        {
            SetStatus(Phase::RestartRequired, {},
                      "Specific game files have been downloaded that require a client restart to take effect.");
            return;
        }

        if (g_pendingMountCount.load() != 0)
        {
            if (forceRepair)
                g_forceRepairRequested = true;
            g_recheckRequested = true;
            LOG_INFO << "ContentStreaming: queued recheck while archive installation is pending";
            return;
        }

        bool expected = false;
        if (!g_running.compare_exchange_strong(expected, true))
        {
            if (forceRepair)
                g_forceRepairRequested = true;
            g_recheckRequested = true;
            if (IsLoginBlockedPhase(g_phase.load()))
            {
                LOG_INFO << "ContentStreaming: queued recheck after the active update";
            }
            else
            {
                LOG_INFO << "ContentStreaming: queued recheck while the previous worker finishes";
            }
            return;
        }
        g_recheckRequested   = false;
        const bool runRepair = forceRepair || g_forceRepairRequested.exchange(false);

        const StreamingBootstrap::Status bootstrap = StreamingBootstrap::Inspect("Duskhaven.mfil");
        if (!bootstrap.exists)
        {
            SetStatus(Phase::Disabled);
            LOG_INFO << "ContentStreaming: disabled; " << bootstrap.detail;
            g_running = false;
            return;
        }

        if (!bootstrap.valid)
        {
            SetStatus(Phase::Failed, {}, bootstrap.detail);
            LOG_ERROR << "ContentStreaming: " << bootstrap.detail;
            g_running = false;
            return;
        }

        g_cancelRequested            = false;
        g_downloadsFinished          = false;
        g_completedBytes             = 0;
        g_currentFileBytes           = 0;
        g_totalBytes                 = 0;
        g_filesDone                  = 0;
        g_filesTotal                 = 0;
        g_downloadCompletedBytes     = 0;
        g_downloadTotalBytes         = 0;
        g_downloadFilesDone          = 0;
        g_downloadFilesTotal         = 0;
        g_catchUpAfterFallbackMount  = false;
        g_activationFailed          = false;
        ClearPendingInstalledRelease();

        SetStatus(Phase::Checking);
        LOG_INFO << "ContentStreaming: checking for updates from " << bootstrap.manifestUrl;
        std::thread(
            [manifestUrl = bootstrap.manifestUrl, runRepair]()
            {
                Run(manifestUrl, runRepair);
                g_running = false;
                if (g_recheckRequested.exchange(false))
                {
                    LOG_INFO << "ContentStreaming: starting queued recheck";
                    Trigger(g_forceRepairRequested.exchange(false));
                }
            })
            .detach();
    }

    void Initialize()
    {
        if (g_started.exchange(true))
            return;

        Trigger(false);
    }

} // namespace ContentStreaming

int32_t Duskhaven_GetContentStreamingProgress(lua_State* L)
{
    Phase phase;
    std::string currentFile;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        phase       = g_phase.load();
        currentFile = g_currentFile;
        error       = g_error;
    }

    const bool busy     = IsLoginBlockedPhase(phase);
    const bool complete = phase == Phase::Complete;
    ClientLua::PushNumber(L, Progress());
    ClientLua::PushBoolean(L, busy);
    ClientLua::PushBoolean(L, complete);
    // Streamed MPQs are mounted live, but Lua/XML/TOC changes need /reload so
    // the interface executes their updated definitions.
    ClientLua::PushBoolean(L, g_interfaceReloadRequired.load());
    ClientLua::PushNumber(L, static_cast<double>(g_filesDone.load()));
    ClientLua::PushNumber(L, static_cast<double>(g_filesTotal.load()));
    ClientLua::PushNumber(L, static_cast<double>(g_completedBytes.load() + g_currentFileBytes.load()));
    ClientLua::PushNumber(L, static_cast<double>(g_totalBytes.load()));
    ClientLua::PushString(L, currentFile.c_str());
    ClientLua::PushString(L, PhaseName(phase));
    ClientLua::PushString(L, error.c_str());
    // Append to preserve the existing progress tuple for older GlueXML.
    ClientLua::PushBoolean(L, g_contentUpdateRequired.load());
    return 12;
}

int32_t Duskhaven_AcknowledgeContentStreamingInterfaceUpdate(lua_State* L)
{
    g_interfaceReloadRequired = false;
    return 0;
}

int32_t Duskhaven_CancelContentStreaming(lua_State* L)
{
    if (g_runtimeRestartRequired.load())
    {
        SetStatus(Phase::RestartRequired, {},
                  "Specific game files have been downloaded that require a client restart to take effect.");
        ClientLua::PushBoolean(L, 0);
        return 1;
    }
    // Lua calls can arrive even when the cancel button is hidden. Do not
    // cancel native archive writes or remove the login gate before the worker
    // acknowledges a safe cancellation boundary.
    std::lock_guard<std::mutex> lock(g_statusMutex);
    const Phase phase = g_phase.load();
    if (phase != Phase::Checking && phase != Phase::Downloading)
    {
        ClientLua::PushBoolean(L, 0);
        return 1;
    }
    g_cancelRequested = true;
    ClientLua::PushBoolean(L, 1);
    return 1;
}

namespace ContentStreaming
{
    bool IsLoginBlocked()
    {
        return ::IsLoginBlocked();
    }

    void OnGlueIdle()
    {
        ::OnGlueIdle();
    }

    void PumpMountQueue()
    {
        ::PumpMountQueue();
    }

    void RegisterLuaFunctions()
    {
        FrameScript_RegisterFunction("Duskhaven_GetContentStreamingProgress", Duskhaven_GetContentStreamingProgress);
        FrameScript_RegisterFunction("Duskhaven_AcknowledgeContentStreamingInterfaceUpdate", Duskhaven_AcknowledgeContentStreamingInterfaceUpdate);
        FrameScript_RegisterFunction("Duskhaven_CancelContentStreaming", Duskhaven_CancelContentStreaming);
    }
}


