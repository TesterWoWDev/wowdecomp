-- ==============================================================================
-- Duskhaven Player Stats & Combat Rating Extensions (Pure Lua)
-- ==============================================================================

local Stats = {}

function Stats.GetShapeshiftFormID()
    if GetShapeshiftForm then
        return GetShapeshiftForm() or 0
    end
    return 0
end

function Stats.GetCombatRatingMult(rating)
    rating = tonumber(rating) or 0
    if rating >= 1 and GetCombatRatingBonus then
        return GetCombatRatingBonus(rating) or 0.0
    end
    return 0.0
end

function Stats.GetCombatRatingScalar(rating)
    return 1.0
end

function Stats.GetModCastSpeed()
    if UnitSpellHaste then
        return UnitSpellHaste("player") or 0.0
    end
    return 0.0
end

function Stats.GetWeaponAP(attackType)
    if UnitAttackPower then
        local base, pos, neg = UnitAttackPower("player")
        return (base or 0) + (pos or 0) + (neg or 0)
    end
    return 0.0
end

function Stats.GetPetSP()
    if GetSpellBonusDamage then
        return GetSpellBonusDamage(1) or 0.0
    end
    return 0.0
end

function Stats.GetSpeedPercent()
    if GetUnitSpeed then
        local currentSpeed, runSpeed = GetUnitSpeed("player")
        if runSpeed and runSpeed > 0 then
            return (currentSpeed / runSpeed) * 100.0
        end
    end
    return 100.0
end

function Stats.GetLeechPercent()
    return 0.0
end

function Stats.GetAvoidancePercent()
    return 0.0
end

-- Export to global Lua environment (_G)
_G.GetShapeshiftFormID   = Stats.GetShapeshiftFormID
_G.GetCombatRatingMult   = Stats.GetCombatRatingMult
_G.GetCombatRatingScalar = Stats.GetCombatRatingScalar
_G.GetModCastSpeed       = Stats.GetModCastSpeed
_G.GetWeaponAP           = Stats.GetWeaponAP
_G.GetPetSP              = Stats.GetPetSP
_G.GetSpeedPercent       = Stats.GetSpeedPercent
_G.GetLeechPercent       = Stats.GetLeechPercent
_G.GetAvoidancePercent   = Stats.GetAvoidancePercent

ModCore.LogInfo("Duskhaven Player Stats module registered.")
return Stats
