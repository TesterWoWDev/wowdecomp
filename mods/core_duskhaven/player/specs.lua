-- ==============================================================================
-- Duskhaven Player Active Spec & Mastery System
-- ==============================================================================

local Specs = {}

local activeSpec = 0
local masteryAmount = 0
local masteryPct = 0.0
local masteryRatings = { 1.0, 1.0, 1.0, 1.0 }

local spec1 = { [1]=true, [4]=true, [7]=true, [10]=true, [13]=true, [16]=true, [19]=true, [22]=true, [25]=true, [28]=true, [34]=true }
local spec2 = { [2]=true, [5]=true, [8]=true, [11]=true, [14]=true, [17]=true, [20]=true, [23]=true, [26]=true, [29]=true, [35]=true }
local spec3 = { [3]=true, [6]=true, [9]=true, [12]=true, [15]=true, [18]=true, [21]=true, [24]=true, [27]=true, [30]=true, [36]=true }
local spec4 = { [31]=true, [32]=true, [33]=true }

local function SpecToIndex(specId)
    if spec1[specId] then return 1 end
    if spec2[specId] then return 2 end
    if spec3[specId] then return 3 end
    if spec4[specId] then return 4 end
    return 1
end

function Specs.GetActiveSpec()
    return activeSpec
end

function Specs.SetActiveSpec(specId)
    activeSpec = tonumber(specId) or 0
end

function Specs.SetMasteryRatings(r1, r2, r3, r4)
    local n1 = tonumber(r1) or 1.0
    local n2 = tonumber(r2) or 1.0
    local n3 = tonumber(r3) or 1.0
    local n4 = tonumber(r4) or 1.0

    masteryRatings[1] = (n1 ~= 0) and n1 or 1.0
    masteryRatings[2] = (n2 ~= 0) and n2 or 1.0
    masteryRatings[3] = (n3 ~= 0) and n3 or 1.0
    masteryRatings[4] = (n4 ~= 0) and n4 or 1.0
end

function Specs.UpdateMasteryAmount(amount, pct)
    masteryAmount = tonumber(amount) or 0
    masteryPct = tonumber(pct) or 0.0
end

function Specs.GetMasteryRating()
    local idx = SpecToIndex(activeSpec)
    local rating = masteryRatings[idx] or 1.0
    if rating <= 0.0 then rating = 1.0 end

    -- CR_MASTERY is rating index 26 in 3.3.5/Cata combat ratings
    local crMastery = (GetCombatRating and GetCombatRating(26)) or 0
    local totalAmount = masteryAmount + crMastery

    local basePct = (masteryAmount / rating) + masteryPct
    local totalPct = basePct + (crMastery / rating)

    return totalAmount, totalPct
end

-- Export to global Lua environment (_G)
_G.GetActiveSpec = function()
    return Specs.GetActiveSpec()
end

_G.SetActiveSpec = function(specId)
    Specs.SetActiveSpec(specId)
end

_G.GetMasteryRating = function()
    return Specs.GetMasteryRating()
end

_G.SetMasteryRatings = function(r1, r2, r3, r4)
    Specs.SetMasteryRatings(r1, r2, r3, r4)
end

_G.UpdateMasteryAmount = function(amount, pct)
    Specs.UpdateMasteryAmount(amount, pct)
end

ModCore.LogInfo("Duskhaven Specs & Mastery module registered.")
return Specs
