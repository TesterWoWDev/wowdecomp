-- ==============================================================================
-- Duskhaven MSDF Font Engine Configuration
-- ==============================================================================

local FontConfig = {
    msdfEnabled = true,
    atlasSize = 512,
    glyphCellSize = 48,
    pixelRange = 3.0,
    
    -- Drop shadow parameters for UI text
    dropShadow = {
        enabled = true,
        blurRadius = 1.5,
        offset = { x = 1.0, y = -1.0 },
        opacity = 0.85
    },

    -- Fonts configured to use MSDF rendering
    fonts = {
        ["Fonts\\FRIZQT__.TTF"] = { useMsdf = true, rangeMultiplier = 1.0 },
        ["Fonts\\ARIALN.TTF"]   = { useMsdf = true, rangeMultiplier = 1.0 },
        ["Fonts\\MORPHEUS.TTF"] = { useMsdf = true, rangeMultiplier = 1.2 },
        ["Fonts\\SKURR.TTF"]    = { useMsdf = true, rangeMultiplier = 1.0 }
    }
}

-- Hook into client font initialization
ModCore.Hooks.Register("OnEngineInit", function()
    ModCore.LogInfo("FontConfig: Applying MSDF distance field settings (Atlas: %dx%d, Range: %.1f)", 
                    FontConfig.atlasSize, FontConfig.atlasSize, FontConfig.pixelRange)
end, 100, "MSDF Font settings hook")

return FontConfig
