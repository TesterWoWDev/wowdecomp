#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdio>

#include "Engine/Source/BlizzardCore/SFile2-Core.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace StorageStreamer
{
    static std::atomic<bool> s_initialized{false};
    static std::atomic<bool> s_loginBlocked{false};
    static std::atomic<float> s_progress{1.0f};
    static std::string s_cdnUrl = "https://cdn.duskhaven.is/client/live";
    static HINTERNET s_httpSession = nullptr;
    static BCRYPT_ALG_HANDLE s_sha256Provider = nullptr;

    bool VfsHookCallback(const char* filename, void** outStreamHandle)
    {
        if (!filename) {
            return false;
        }

        // Check if file is already cached locally; if not, queue on-demand stream
        return false;
    }

    bool HashSha256(const void* data, size_t size, uint8_t outHash[32])
    {
        if (!s_sha256Provider || !data || !size) {
            return false;
        }

        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS status = BCryptCreateHash(s_sha256Provider, &hHash, nullptr, 0, nullptr, 0, 0);
        if (status < 0) {
            return false;
        }

        BCryptHashData(hHash, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), 0);
        BCryptFinishHash(hHash, outHash, 32, 0);
        BCryptDestroyHash(hHash);
        return true;
    }
}

extern "C" __declspec(dllexport) bool StorageStreamer_Init(const char* cdnUrl)
{
    if (StorageStreamer::s_initialized) {
        return true;
    }

    if (cdnUrl && *cdnUrl) {
        StorageStreamer::s_cdnUrl = cdnUrl;
    }

    // Open WinHTTP session
    StorageStreamer::s_httpSession = WinHttpOpen(
        L"DuskhavenStreamer/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    // Open BCrypt SHA-256 algorithm provider
    BCryptOpenAlgorithmProvider(&StorageStreamer::s_sha256Provider, BCRYPT_SHA256_ALGORITHM, nullptr, 0);

    // Register clean VFS hook with Storm
    Storm_RegisterStorageHook(StorageStreamer::VfsHookCallback);

    StorageStreamer::s_initialized = true;
    return true;
}

extern "C" __declspec(dllexport) void StorageStreamer_Shutdown()
{
    if (!StorageStreamer::s_initialized) {
        return;
    }

    Storm_RegisterStorageHook(nullptr);

    if (StorageStreamer::s_sha256Provider) {
        BCryptCloseAlgorithmProvider(StorageStreamer::s_sha256Provider, 0);
        StorageStreamer::s_sha256Provider = nullptr;
    }

    if (StorageStreamer::s_httpSession) {
        WinHttpCloseHandle(StorageStreamer::s_httpSession);
        StorageStreamer::s_httpSession = nullptr;
    }

    StorageStreamer::s_initialized = false;
}

extern "C" __declspec(dllexport) bool StorageStreamer_IsBlocked()
{
    return StorageStreamer::s_loginBlocked.load();
}

extern "C" __declspec(dllexport) float StorageStreamer_GetProgress()
{
    return StorageStreamer::s_progress.load();
}
