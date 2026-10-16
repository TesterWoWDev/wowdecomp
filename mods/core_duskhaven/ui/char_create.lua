-- ==============================================================================
-- Duskhaven Character Creation Screen Staging & Customization Logic
-- ==============================================================================

local CharCreate = {}

-- Export global Character Creation APIs
_G.PlayCharacterCreateAnimation = _G.PlayCharacterCreateAnimation or function(animIdOrName, loop)
    return false
end

_G.PlayCharacterCreateAnimationChain = _G.PlayCharacterCreateAnimationChain or function(chainTable)
    return true
end

_G.UpdateCharacterCreateAnimationChain = _G.UpdateCharacterCreateAnimationChain or function()
    return 0
end

_G.GetCharacterCreateAnimations = _G.GetCharacterCreateAnimations or function(maximumAnimationId, selectedAnimationId)
    return {}
end

_G.PlayCharacterCreateSpellVisual = _G.PlayCharacterCreateSpellVisual or function(spellId)
    return 0
end

_G.GetCharacterCreateSpellStageSoundPath = _G.GetCharacterCreateSpellStageSoundPath or function(soundId)
    return ""
end

_G.GetCharacterCreateSpellCastInfo = _G.GetCharacterCreateSpellCastInfo or function(spellId)
    return {
        castTime = 0,
        channeled = false,
        missileSpeed = 0
    }
end

_G.GetCharacterCreateSpellPreviewInfo = _G.GetCharacterCreateSpellPreviewInfo or function(spellId)
    return {}
end

_G.PreviewCharacterCreateSpellAura = _G.PreviewCharacterCreateSpellAura or function(spellId)
    return false
end

_G.ClearCharacterCreateAuraPreview = _G.ClearCharacterCreateAuraPreview or function()
    return 0
end

_G.SetCharacterCreateSpellPreviewTargetOffset = _G.SetCharacterCreateSpellPreviewTargetOffset or function(x, y, z)
    return 0
end

_G.GetCharacterCreateSceneDebugInfo = _G.GetCharacterCreateSceneDebugInfo or function()
    return {}
end

_G.ShowCharacterCreateSpellPreviewPath = _G.ShowCharacterCreateSpellPreviewPath or function(p1, p2, p3)
    return 0
end

_G.StartCharacterCreateSpellMissile = _G.StartCharacterCreateSpellMissile or function(p1, p2, p3)
    return 0
end

_G.UpdateCharacterCreateSpellPreview = _G.UpdateCharacterCreateSpellPreview or function()
    return false
end

_G.ClearCharacterCreateSpellPreview = _G.ClearCharacterCreateSpellPreview or function()
    return 0
end

_G.ClearCharacterCreateSpellVisuals = _G.ClearCharacterCreateSpellVisuals or function()
    return 0
end

_G.ClearCharacterCreateModelAttachments = _G.ClearCharacterCreateModelAttachments or function()
    return true
end

_G.SetCharacterCreateWeaponsHidden = _G.SetCharacterCreateWeaponsHidden or function(hidden)
    return true
end

_G.SetCharacterCreateEquipmentHidden = _G.SetCharacterCreateEquipmentHidden or function(hidden)
    return true
end

_G.AttachCharacterCreateModel = _G.AttachCharacterCreateModel or function(path, attachmentId)
    return false
end

_G.GetHairStyleSelection = _G.GetHairStyleSelection or function()
    return 0
end

_G.GetHairColorSelection = _G.GetHairColorSelection or function()
    return 0
end

_G.GetFacialHairSelection = _G.GetFacialHairSelection or function()
    return 0
end

_G.GetSkinColorSelection = _G.GetSkinColorSelection or function()
    return 0
end

_G.GetFaceSelection = _G.GetFaceSelection or function()
    return 0
end

-- Play a character animation sequence in the glue character create screen
function CharCreate.PlayAnimation(animIdOrName, loop)
    return _G.PlayCharacterCreateAnimation(animIdOrName, loop or false)
end

-- Play a chained sequence of animations (e.g. ready -> cast -> release -> idle)
function CharCreate.PlayAnimationChain(chainTable)
    return _G.PlayCharacterCreateAnimationChain(chainTable)
end

-- Preview a spell visual on the character create model
function CharCreate.PreviewSpellVisual(spellId)
    return _G.PlayCharacterCreateSpellVisual(spellId)
end

-- Preview a continuous spell aura (e.g. shadowform, stealth, stealth glow)
function CharCreate.PreviewSpellAura(spellId)
    return _G.PreviewCharacterCreateSpellAura(spellId)
end

-- Clear all active preview visual effects
function CharCreate.ClearSpellVisuals()
    _G.ClearCharacterCreateSpellVisuals()
    _G.ClearCharacterCreateAuraPreview()
end

-- Control weapon visibility during class preview
function CharCreate.SetWeaponsHidden(hidden)
    _G.SetCharacterCreateWeaponsHidden(hidden)
end

-- Control equipment visibility (e.g. for previewing tattoo/skin customizations)
function CharCreate.SetEquipmentHidden(hidden)
    _G.SetCharacterCreateEquipmentHidden(hidden)
end

-- Query current customization indices
function CharCreate.GetCustomizationIndices()
    return {
        hairStyle = _G.GetHairStyleSelection(),
        hairColor = _G.GetHairColorSelection(),
        facialHair = _G.GetFacialHairSelection(),
        skinColor = _G.GetSkinColorSelection(),
        face = _G.GetFaceSelection()
    }
end

-- Hook into character create glue screen events
ModCore.Hooks.Register("OnCharacterCreateClassSelect", function(classId)
    ModCore.LogInfo("CharCreate: Class selected: %d", classId)
    if classId == 10 then -- Monk
        CharCreate.SetWeaponsHidden(true)
        CharCreate.PlayAnimation("ReadyUnarmed", true)
    else
        CharCreate.SetWeaponsHidden(false)
    end
end, 100, "Character creation class staging hook")

ModCore.LogInfo("Duskhaven Character Creation staging module registered.")
return CharCreate
