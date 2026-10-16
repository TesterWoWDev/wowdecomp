---@meta wowdecomp
-- ==============================================================================
-- wowdecomp Modding Framework Typed API Definitions (EmmyLua / LuaLS)
-- Auto-generated for AI and IDE Development
-- ==============================================================================

---@class C3Vector
---@field x number World X coordinate
---@field y number World Y coordinate
---@field z number World Z coordinate
local C3Vector = {}

---@class Quat
---@field w number Real scalar component
---@field x number X imaginary axis
---@field y number Y imaginary axis
---@field z number Z imaginary axis
local Quat = {}

---@class WOWGUID
---@field low integer Low 32 bits of entity GUID
---@field high integer High 32 bits of entity GUID
local WOWGUID = {}

---Returns true if the GUID is non-zero and represents a valid game entity
---@return boolean
function WOWGUID:IsValid() end

---Returns true if this entity is a player character
---@return boolean
function WOWGUID:IsPlayer() end

---Returns true if this entity is a creature/NPC
---@return boolean
function WOWGUID:IsUnit() end

---Returns true if this entity is a game object (door, chest, housing item)
---@return boolean
function WOWGUID:IsGameObject() end

---@class CGObject_C
---@field m_guid WOWGUID
local CGObject_C = {}

---Returns the entity's 64-bit GUID
---@return WOWGUID
function CGObject_C:GetGUID() end

---Returns the entity's current world position
---@return C3Vector
function CGObject_C:GetPosition() end

---Returns the entity's current facing yaw in radians
---@return number
function CGObject_C:GetFacing() end

---Sets the entity's world position (Housing / Editor only)
---@param pos C3Vector
function CGObject_C:SetPosition(pos) end

---Sets the entity's 3D orientation (Housing / Editor only)
---@param rotation Quat
function CGObject_C:SetOrientation(rotation) end

---@class CGUnit_C : CGObject_C
local CGUnit_C = {}

---Returns the unit's current health points
---@return integer
function CGUnit_C:GetHealth() end

---Returns the unit's maximum health points
---@return integer
function CGUnit_C:GetMaxHealth() end

---Returns the unit's current resource power (Mana=0, Rage=1, Focus=2, Energy=3, Runic=6)
---@param powerType integer
---@return integer
function CGUnit_C:GetPower(powerType) end

---Returns the unit's maximum resource power
---@param powerType integer
---@return integer
function CGUnit_C:GetMaxPower(powerType) end

---Returns true if this unit is friendly to the active player
---@return boolean
function CGUnit_C:IsFriendly() end

---Returns true if this unit is hostile to the active player
---@return boolean
function CGUnit_C:IsHostile() end

---Returns the unit's active target GUID
---@return WOWGUID
function CGUnit_C:GetTargetGuid() end

---@class CGPlayer_C : CGUnit_C
local CGPlayer_C = {}

---@class ClntObjMgr
ClntObjMgr = {}

---Returns the active local player object pointer
---@return CGPlayer_C
function ClntObjMgr.GetActivePlayer() end

---Finds any world entity by its 64-bit GUID
---@param guid WOWGUID
---@return CGObject_C?
function ClntObjMgr.GetObject(guid) end

---@class ModCore
---@field Version string Framework version string
---@field BasePath string Path to /mods directory
---@field LoadedMods table<string, table> Table of mounted mods
ModCore = {}

---Logs a debug string to the client console
---@param fmt string Format string
---@vararg any
function ModCore.LogDebug(fmt, ...) end

---Logs an informational string to the client console
---@param fmt string Format string
---@vararg any
function ModCore.LogInfo(fmt, ...) end

---Logs a warning string to the client console
---@param fmt string Format string
---@vararg any
function ModCore.LogWarn(fmt, ...) end

---Logs an error string to the client console
---@param fmt string Format string
---@vararg any
function ModCore.LogError(fmt, ...) end

---@class Hooks
ModCore.Hooks = {}

---Registers a callback for an engine or gameplay lifecycle hook
---@param hookName string Name of hook event (e.g. "OnEngineInit", "OnWorldRender", "OnWorldUpdate", "OnPacketReceived", "OnSpellQuery")
---@param callback fun(...): any?
---@param priority? integer Higher priorities run first (default: 100)
---@param doc? string Documentation string for AI auto-export
function ModCore.Hooks.Register(hookName, callback, priority, doc) end

---Unregisters a callback from a hook
---@param hookName string
---@param callback fun(...)
function ModCore.Hooks.Unregister(hookName, callback) end

---Fires a hook across all registered listeners
---@param hookName string
---@vararg any
---@return any?
function ModCore.Hooks.Fire(hookName, ...) end

---@class NetEngine
Net = {}

---Registers a custom network packet definition with schema
---@param name string Unique packet name
---@param def { opcode: integer, direction: "CLIENT_TO_SERVER"|"SERVER_TO_CLIENT", schema: table, handler?: fun(packet: table) }
function Net.DefinePacket(name, def) end

---Sends a registered custom packet to the server
---@param name string Unique packet name
---@param payload table Data table matching schema
function Net.Send(name, payload) end

---Registers a raw opcode handler
---@param opcode integer Opcode number (e.g. 0x7001)
---@param handler fun(reader: any)
function Net.OnCustomPacket(opcode, handler) end

---@class VisualsEngine
Visuals = {}

---Spawns an animated ground telegraph decal (circle, donut, cone, rectangle)
---@param params { shape: "CIRCLE"|"DONUT"|"CONE"|"RECTANGLE", position: C3Vector, radius: number, innerRadius?: number, coneAngle?: number, duration?: number, color: { r: number, g: number, b: number, a: number } }
---@return any
function Visuals.CreateDecal(params) end

---Attaches a weapon ribbon trail to a unit's weapon bone
---@param unit CGUnit_C
---@param hand integer 0 for Mainhand, 1 for Offhand
---@param params { texture: string, startColor: table, endColor: table, lifetime: number }
function Visuals.AttachWeaponTrail(unit, hand, params) end

---@class DbcEngine
DBC = {}

---Returns a mutable handle to a client DBC table
---@param tableName string (e.g. "Spell", "SpellEffect", "ItemDisplayInfo")
---@return any
function DBC.GetTable(tableName) end

return ModCore
