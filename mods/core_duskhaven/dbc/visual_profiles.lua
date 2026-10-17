-- ==============================================================================
-- Duskhaven CDBC: Visual, Missile, Trail & Zone Lighting Profiles
-- ==============================================================================

local DBC = require("system.dbc")

local VisualProfiles = {}

-- 1. DangerZoneVisualProfile
local dangerZoneSchema = {
    "id",
    "schoolMask",
    "shapeMask",
    "flags",
    "modelPath",
    "modelRadius",
    "radiusScale",
    "zOffset",
    "minDurationMs",
    "maxDurationMs",
    "red",
    "green",
    "blue",
    "alpha",
    "debugSpellId"
}
DBC.RegisterTable("DangerZoneVisualProfile", dangerZoneSchema, {})

-- 2. ExtraSpellVisualKitEffect
local extraVisualKitSchema = {
    "ID",
    "ParentSpellVisualKitID",
    "EffectType",
    "Effect"
}
DBC.RegisterTable("ExtraSpellVisualKitEffect", extraVisualKitSchema, {})

-- 3. ScriptedMissileMotion
local missileMotionSchema = {
    "id",
    "motion",
    "count",
    "durationMs",
    "tickMs",
    "flags",
    "collisionRadius",
    "radius",
    "radiusVelocity",
    "startDistance",
    "height",
    "verticalVelocity",
    "forwardSpeed",
    "angularSpeed",
    "sineAmplitude",
    "sineFrequency",
    "modelPath",
    "visualFlags",
    "attachMode",
    "attachPoint",
    "visualScale"
}
DBC.RegisterTable("ScriptedMissileMotion", missileMotionSchema, {})

-- 4. WeaponTrail
local weaponTrailSchema = {
    "ID",
    "FilePath",
    "Roll",
    "Pitch",
    "Yaw",
    "TexturePath1",
    "TexturePath2",
    "TexturePath3",
    "TextureScrollRateU1",
    "TextureScrollRateU2",
    "TextureScrollRateU3",
    "TextureScrollRateV1",
    "TextureScrollRateV2",
    "TextureScrollRateV3",
    "TextureScaleU1",
    "TextureScaleU2",
    "TextureScaleU3",
    "TextureScaleV1",
    "TextureScaleV2",
    "TextureScaleV3"
}
DBC.RegisterTable("WeaponTrail", weaponTrailSchema, {})

-- 5. WeaponTrailParam
local weaponTrailParamSchema = {
    "ID",
    "Hand",
    "Duration",
    "FadeOutTime",
    "EdgeLifeSpan",
    "InitialDelay",
    "SmoothSampleAngle",
    "OverrideAttachTop",
    "OverrideAttachBot",
    "Flags",
    "WeaponTrailID"
}
DBC.RegisterTable("WeaponTrailParam", weaponTrailParamSchema, {})

-- 6. ZoneLight & ZoneLightPoint
local zoneLightSchema = {
    "ID",
    "name",
    "mapID",
    "lightID"
}
DBC.RegisterTable("ZoneLight", zoneLightSchema, {})

local zoneLightPointSchema = {
    "ID",
    "zoneLightID",
    "positionX",
    "positionY",
    "pointOrder"
}
DBC.RegisterTable("ZoneLightPoint", zoneLightPointSchema, {})

_G.VisualProfiles = VisualProfiles
return VisualProfiles
