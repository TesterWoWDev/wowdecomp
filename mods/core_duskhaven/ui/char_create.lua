-- ==============================================================================
-- Duskhaven Character Creation Screen Staging & Customization Logic
-- ==============================================================================

local CharCreate = {}

-- Play a character animation sequence in the glue character create screen
function CharCreate.PlayAnimation(animIdOrName, loop)
    if PlayCharacterCreateAnimation then
        return PlayCharacterCreateAnimation(animIdOrName, loop or false)
    end
    return false
end

-- Play a chained sequence of animations (e.g. ready -> cast -> release -> idle)
function CharCreate.PlayAnimationChain(chainTable)
    if PlayCharacterCreateAnimationChain then
        return PlayCharacterCreateAnimationChain(chainTable)
    end
    return false
end

-- Preview a spell visual on the character create model
function CharCreate.PreviewSpellVisual(spellId)
    if PlayCharacterCreateSpellVisual then
        return PlayCharacterCreateSpellVisual(spellId)
    end
    return false
end

-- Preview a continuous spell aura (e.g. shadowform, stealth, stealth glow)
function CharCreate.PreviewSpellAura(spellId)
    if PreviewCharacterCreateSpellAura then
        return PreviewCharacterCreateSpellAura(spellId)
    end
    return false
end

-- Clear all active preview visual effects
function CharCreate.ClearSpellVisuals()
    if ClearCharacterCreateSpellVisuals then
        ClearCharacterCreateSpellVisuals()
    end
    if ClearCharacterCreateAuraPreview then
        ClearCharacterCreateAuraPreview()
    end
end

-- Control weapon visibility during class preview
function CharCreate.SetWeaponsHidden(hidden)
    if SetCharacterCreateWeaponsHidden then
        SetCharacterCreateWeaponsHidden(hidden)
    end
end

-- Control equipment visibility (e.g. for previewing tattoo/skin customizations)
function CharCreate.SetEquipmentHidden(hidden)
    if SetCharacterCreateEquipmentHidden then
        SetCharacterCreateEquipmentHidden(hidden)
    end
end

-- Query current customization indices
function CharCreate.GetCustomizationIndices()
    return {
        hairStyle = GetHairStyleSelection and GetHairStyleSelection() or 0,
        hairColor = GetHairColorSelection and GetHairColorSelection() or 0,
        facialHair = GetFacialHairSelection and GetFacialHairSelection() or 0,
        skinColor = GetSkinColorSelection and GetSkinColorSelection() or 0,
        face = GetFaceSelection and GetFaceSelection() or 0
    }
end

-- Hook into character create glue screen events
ModCore.Hooks.Register("OnCharacterCreateClassSelect", function(classId)
    ModCore.LogInfo("CharCreate: Class selected: %d", classId)
    -- Custom class animation choreography:
    -- e.g. Monks play martial arts ready stance, Demon Hunters play warglaive unsheath
    if classId == 10 then -- Monk
        CharCreate.SetWeaponsHidden(true)
        CharCreate.PlayAnimation("ReadyUnarmed", true)
    else
        CharCreate.SetWeaponsHidden(false)
    end
end, 100, "Character creation class staging hook")

ModCore.LogInfo("Duskhaven Character Creation staging module registered.")
return CharCreate
