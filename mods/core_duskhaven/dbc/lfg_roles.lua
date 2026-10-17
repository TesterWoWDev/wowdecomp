-- ==============================================================================
-- Duskhaven CDBC: LFGRoles
-- ==============================================================================

local DBC = require("system.dbc")

local LFGRoles = {}

LFGRoles.Roles = {
    LEADER = 0x01,
    TANK   = 0x02,
    HEALER = 0x04,
    DAMAGE = 0x08,
}

local schema = { "ClassID", "Roles" }

local defaultRows = {
    [1]  = { ClassID = 1,  Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.DAMAGE },                         -- Warrior
    [2]  = { ClassID = 2,  Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.HEALER + LFGRoles.Roles.DAMAGE }, -- Paladin
    [3]  = { ClassID = 3,  Roles = LFGRoles.Roles.DAMAGE },                                               -- Hunter
    [4]  = { ClassID = 4,  Roles = LFGRoles.Roles.DAMAGE },                                               -- Rogue
    [5]  = { ClassID = 5,  Roles = LFGRoles.Roles.HEALER + LFGRoles.Roles.DAMAGE },                       -- Priest
    [6]  = { ClassID = 6,  Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.DAMAGE },                         -- Death Knight
    [7]  = { ClassID = 7,  Roles = LFGRoles.Roles.HEALER + LFGRoles.Roles.DAMAGE },                       -- Shaman
    [8]  = { ClassID = 8,  Roles = LFGRoles.Roles.DAMAGE },                                               -- Mage
    [9]  = { ClassID = 9,  Roles = LFGRoles.Roles.DAMAGE },                                               -- Warlock
    [10] = { ClassID = 10, Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.HEALER + LFGRoles.Roles.DAMAGE }, -- Monk (Custom Class)
    [11] = { ClassID = 11, Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.HEALER + LFGRoles.Roles.DAMAGE }, -- Druid
    [12] = { ClassID = 12, Roles = LFGRoles.Roles.TANK + LFGRoles.Roles.DAMAGE },                         -- Demon Hunter (Custom Class)
}

DBC.RegisterTable("LFGRoles", schema, defaultRows)

function LFGRoles.GetClassRoles(classId)
    classId = tonumber(classId) or 0
    local row = DBC.GetRow("LFGRoles", classId)
    if row and row.Roles then
        return row.Roles
    end
    return 0
end

function LFGRoles.HasRole(classId, role)
    local roles = LFGRoles.GetClassRoles(classId)
    return bit and bit.band and (bit.band(roles, role) ~= 0) or false
end

_G.LFGRoles = LFGRoles
return LFGRoles
