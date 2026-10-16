-- ==============================================================================
-- Duskhaven Asset Streaming & Patch Policy Configuration
-- ==============================================================================

local StreamingConfig = {
    enabled = true,
    cdnBaseUrl = "https://cdn.duskhaven.is/client/live",
    manifestPath = "latest.json",
    maxConcurrentDownloads = 8,
    cacheDirectory = "Data/Cache/Streaming",
    
    -- Verification policy
    verifyHashesOnStartup = false,
    resumePartialDownloads = true,

    -- Remote dynamic assets
    mountPriority = {
        "Data/patch-D.MPQ",
        "Data/patch-Duskhaven.MPQ",
        "Data/patch-CustomAudio.MPQ"
    }
}

ModCore.Hooks.Register("OnEngineInit", function()
    ModCore.LogInfo("StreamingConfig: Asset streaming initialized (CDN: %s)", StreamingConfig.cdnBaseUrl)
end, 100, "Streaming policy hook")

return StreamingConfig
