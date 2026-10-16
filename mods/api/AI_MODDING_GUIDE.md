# wowdecomp AI Modding Guide & API Reference

> **Purpose**: This file provides an AI assistant or human developer with full contextual knowledge of how to build and extend mods for `wowdecomp` without needing access to the C++ source code.

---

## 1. Directory Layout & Mod Structure
Every mod resides in `mods/<mod_name>/`:
- `mod.json`: Manifest declaring id, name, version, and entry point.
- `init.lua`: Main entry point executed when the mod is mounted.

### Standard `mod.json` Template:
```json
{
  "id": "my-custom-mechanic",
  "name": "Custom Encounter Mechanics",
  "version": "1.0.0",
  "author": "Modder",
  "entryPoint": "init.lua",
  "priority": 100
}
```

---

## 2. Core Hook System
Register lifecycle callbacks via `ModCore.Hooks.Register(hookName, callback, priority)`:

```lua
-- Hook into engine init
ModCore.Hooks.Register("OnEngineInit", function()
    ModCore.LogInfo("My custom mod initialized!")
end)

-- Hook into per-frame 3D world rendering (danger zones, decals, trails)
ModCore.Hooks.Register("OnWorldRender", function(worldFrame)
    -- Render custom telegraphs, decals, or 3D gizmos
end)

-- Hook into spell attribute queries (replaces hardcoded LocalSpellData)
ModCore.Hooks.Register("OnSpellQuery", function(queryType, spellId, caster)
    if queryType == "PowerCost" and spellId == 98100 then
        return 25 -- Custom power cost
    end
    return nil -- Return nil to use stock DBC value
end)
```

---

## 3. Custom Networking & Packets
Define declarative network packets:
```lua
-- Register a custom packet schema
Net.DefinePacket("CMSG_CUSTOM_ACTION", {
    opcode = 0x7100,
    direction = "CLIENT_TO_SERVER",
    schema = {
        { name = "actionId", type = "uint32" },
        { name = "targetGuid", type = "guid" }
    }
})

-- Send the packet
Net.Send("CMSG_CUSTOM_ACTION", {
    actionId = 1,
    targetGuid = ClntObjMgr.GetActivePlayer():GetTargetGuid()
})
```

---

## 4. Safe Scripting Invariants
1. **Never throw uncaught errors**: Always guard entity pointer operations with `if object and object:IsValid()`.
2. **Coordinate conventions**:
   - World coordinates use `C3Vector` (`x`, `y`, `z`).
   - Angles use radians or degrees matching DBC units.
3. **Hot Reloading**:
   - In developer mode, saving any `.lua` file automatically compiles and updates in-memory bytecode without restarting the client.
