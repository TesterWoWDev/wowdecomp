# wowdecomp Universal Modding Framework - AI & Developer Reference Guide

> **Target Audience**: AI assistants (Antigravity, Claude, GPT) and human mod developers.
> **Purpose**: Complete reference for extending `wowdecomp` client logic externally without recompiling C++.

---

## 1. Quick Start & Mod Structure

Mods live in `mods/<mod_id>/` with a `mod.json` manifest and an `init.lua` entry point:

```json
{
  "id": "my-custom-mod",
  "name": "My Custom Mod",
  "version": "1.0.0",
  "entryPoint": "init.lua",
  "priority": 100
}
```

In `init.lua`:
```lua
local MyMod = {}
function MyMod.Init(context)
    ModCore.LogInfo("Initialized MyMod!")
    ModCore.On("OnWorldUpdate", function(dt)
        -- per-frame logic
    end)
end
MyMod.Init(...)
return MyMod
```

---

## 2. Exhaustive Hook Catalog (42 Hooks)

| Hook Name | Category | Signature | Description |
| :--- | :--- | :--- | :--- |
| `OnEngineInit` | **Lifecycle** | `fun(): void` | Fired when the client engine completes low-level initialization |
| `OnEngineShutdown` | **Lifecycle** | `fun(): void` | Fired when the client engine begins shutdown sequence |
| `OnGameInitialize` | **Lifecycle** | `fun(): void` | Fired when CGGameUI initializes the active gameplay world session |
| `OnGameDestroy` | **Lifecycle** | `fun(): void` | Fired when CGGameUI shuts down the active gameplay world session |
| `OnFrameTick` | **Lifecycle** | `fun(dt: number): void` | Fired every main engine loop iteration with frame delta time |
| `OnEnterWorld` | **World** | `fun(): void` | Fired when the player character finishes loading and enters the 3D world |
| `OnLeaveWorld` | **World** | `fun(): void` | Fired when the player character leaves the 3D world (logout or disconnect) |
| `OnMapChange` | **World** | `fun(mapName: string, zoneId: integer): void` | Fired when CWorld loads or changes the active map |
| `OnWorldUpdate` | **World** | `fun(dt: number): void` | Fired every game tick during world update loop (ideal for visual updates and animation) |
| `OnWorldRender` | **World** | `fun(worldFrame: any): void` | Fired each frame during the 3D world rendering pass (for decals, ribbons, gizmos) |
| `OnWorldHitTest` | **World** | `fun(guid: number, point: any, hitObject: boolean): void` | Fired when the mouse raycast hits an entity or world surface |
| `OnTerrainTrack` | **World** | `fun(point: any): boolean?` | Fired during mouse terrain tracking queries |
| `OnTerrainClick` | **World** | `fun(button: integer, x: number, y: number, z: number): boolean?` | Fired when user clicks on terrain ground |
| `OnGlueResume` | **Glue** | `fun(): void` | Fired when GlueXML screen resumes execution |
| `OnGlueIdle` | **Glue** | `fun(): void` | Fired each frame while on GlueXML screens |
| `OnGlueScreenChange` | **Glue** | `fun(screenName: string): void` | Fired when transitioning between Glue screens (login, charselect, charcreate) |
| `IsLoginBlocked` | **Glue** | `fun(): boolean?` | Queried by client before allowing user to login to realm |
| `OnCharacterSelect` | **Glue** | `fun(charIndex: integer): void` | Fired when a character is selected on the character selection screen |
| `OnCharacterCreate` | **Glue** | `fun(name: string, race: integer, classId: integer, gender: integer): void` | Fired when a character creation request is submitted |
| `OnPacketReceived` | **Network** | `fun(opcode: integer, time: integer, reader: any): boolean?` | Fired when a network packet arrives from the server |
| `OnPacketSend` | **Network** | `fun(opcode: integer, size: integer, dataStore: any): boolean?` | Fired before a packet is transmitted to the server |
| `OnDisconnect` | **Network** | `fun(): void` | Fired when the client network connection disconnects |
| `OnSpellQuery` | **Spells** | `fun(queryType: string, spellId: integer, caster: any): any?` | Fired when client queries dynamic spell attributes (PowerCost, IsPassive, Range, CastTime) |
| `OnSpellCast` | **Spells** | `fun(spellId: integer, casterGuid: number, targetGuid: number): void` | Fired when a spell cast begins |
| `OnChannelStart` | **Spells** | `fun(msg: any): void` | Fired when a channeled spell begins casting |
| `OnChannelUpdate` | **Spells** | `fun(msg: any): void` | Fired on channeled spell duration updates |
| `OnChannelStop` | **Spells** | `fun(): void` | Fired when a channeled spell finishes or is interrupted |
| `OnSpellDestroy` | **Spells** | `fun(): void` | Fired when a client spell instance is destroyed |
| `OnTooltipEvaluate` | **Tooltips** | `fun(spellId: integer, caster: any, defaultText: string): string?` | Fired when spell tooltip description text is rendered for dynamic formulas and scaling |
| `OnTooltipSetSpell` | **Tooltips** | `fun(tooltip: any, spellId: integer): void` | Fired when CGTooltip::SetSpell populates a spell tooltip |
| `OnTooltipSetItem` | **Tooltips** | `fun(tooltip: any, itemId: integer): void` | Fired when CGTooltip::SetItem populates an item tooltip |
| `OnTooltipSetUnit` | **Tooltips** | `fun(tooltip: any, unitGuid: number): void` | Fired when CGTooltip::SetUnit populates a unit tooltip |
| `OnObjectCreate` | **Objects** | `fun(guid: number, typeId: integer, objectPtr: any): void` | Fired when an entity is spawned in the client object manager |
| `OnObjectDestroy` | **Objects** | `fun(guid: number, typeId: integer, objectPtr: any): void` | Fired when an entity is despawned or deleted from client memory |
| `OnTargetChanged` | **Objects** | `fun(oldGuid: number, newGuid: number): void` | Fired when the active player changes target entity |
| `OnKeyDown` | **Input** | `fun(key: integer, modifiers: integer, keyString: string): boolean?` | Fired on keyboard key down events |
| `OnKeyUp` | **Input** | `fun(key: integer, modifiers: integer, keyString: string): boolean?` | Fired on keyboard key up events |
| `OnMouseDown` | **Input** | `fun(button: integer, x: integer, y: integer): boolean?` | Fired on mouse button press |
| `OnMouseUp` | **Input** | `fun(button: integer, x: integer, y: integer): boolean?` | Fired on mouse button release |
| `OnMouseWheel` | **Input** | `fun(delta: integer): boolean?` | Fired on mouse wheel scroll |
| `OnChatMessage` | **Chat** | `fun(type: integer, sender: string, message: string, channel: string): boolean?` | Fired when an incoming chat message is received from server |
| `OnSendChatMessage` | **Chat** | `fun(message: string, type: integer, language: integer, target: string): boolean?` | Fired before a chat message is sent from the client |

---

## 3. Key Subsystem Recipes

### 3.1 Overriding Spell Attributes & Passives
```lua
ModCore.On("OnSpellQuery", function(queryType, spellId, caster)
    if queryType == "IsPassive" and spellId == 48266 then
        return false -- Render Blood Presence on action bar even if passive in DBC
    elseif queryType == "PowerCost" and spellId == 98100 then
        return 35 -- Custom energy cost
    end
    return nil -- Fallback to stock DBC
end)
```

### 3.2 Dynamic Tooltip Scaling Formulas
```lua
ModCore.On("OnTooltipEvaluate", function(spellId, caster, defaultText)
    if spellId == 98100 then
        local ap = caster and caster:GetAttackPower() or 0
        return string.format("Strike enemies for %.0f Shadowflame damage.", 300 + ap * 0.45)
    end
    return nil
end)
```

### 3.3 Intercepting Network Packets
```lua
local Net = require("system.net")
Net.DefinePacket("SMSG_CUSTOM_ALERT", {
    opcode = 0x7005,
    direction = "SERVER_TO_CLIENT",
    schema = { { name = "message", type = "string" } },
    handler = function(packet)
        ModCore.LogInfo("Alert from server: %s", packet.message)
    end
})
```

### 3.4 Capturing Keyboard & Mouse Input
```lua
ModCore.On("OnKeyDown", function(key, modifiers, keyString)
    if keyString == "H" and (modifiers & 1) ~= 0 then -- Shift+H
        ModCore.LogInfo("Toggled Custom UI Frame!")
        return true -- Consumes input
    end
end)
```

---

## 4. Frontend Scripting Invariants

1. **Entity Safety**: Always check `if entity and entity:IsValid()` before calling methods on pointers.
2. **Hook Return Values**: Returning a non-nil value from a query/filter hook acts as an override and halts further listener propagation.
3. **Hot-Reloading**: Use `ModCore.On(...)` or `ModCore.Hooks.Register(...)`. When scripts are modified on disk, the engine reloads them on the next frame tick without restarting the client.
