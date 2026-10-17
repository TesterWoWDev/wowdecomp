-- ==============================================================================
-- Duskhaven Spell Charges & Cooldown Tracking (Pure Lua)
-- ==============================================================================

local Charges = {}
local spellCharges = {}

function Charges.Update(spellId, currentCharges, maxCharges, remainingCooldown, cooldown)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return end

    local existing = spellCharges[spellId]
    local cd = tonumber(cooldown) or 0
    if cd == 0 and existing and existing.cooldown then
        cd = existing.cooldown
    end

    local curCharges = tonumber(currentCharges) or 0
    local maxC = tonumber(maxCharges) or 0
    local remCd = tonumber(remainingCooldown) or 0

    spellCharges[spellId] = {
        currentCharges    = curCharges,
        maxCharges        = maxC,
        remainingCooldown = remCd,
        cooldown          = cd,
        updateTime        = (GetTime and GetTime()) or 0
    }

    if ModCore and ModCore.Hooks then
        ModCore.Hooks.Fire("OnSpellChargesUpdated", spellId, curCharges, maxC, remCd, cd)
    end
end

function Charges.Get(spellId)
    spellId = tonumber(spellId)
    if not spellId or spellId == 0 then return nil end

    local row = spellCharges[spellId]
    if not row then return nil end

    local remaining = row.remainingCooldown
    if GetTime and row.updateTime and row.updateTime > 0 and remaining > 0 then
        local elapsed = (GetTime() - row.updateTime) * 1000
        if elapsed > 0 then
            remaining = math.max(0, math.floor(remaining - elapsed))
        end
    end

    return row.currentCharges, row.maxCharges, remaining, row.cooldown
end

function Charges.Clear()
    spellCharges = {}
end

-- Export to global Lua environment (_G) for AddOns and FrameXML
_G.GetChargeInfoForSpell = Charges.Get
_G.UpdateChargeInfoForSpell = Charges.Update

ModCore.LogInfo("Duskhaven Spell Charges module registered.")
return Charges
