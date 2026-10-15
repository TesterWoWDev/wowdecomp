#pragma once
struct lua_State;
class CSimpleTexture;

namespace Duskhaven
{
    namespace Lua
    {
        void RegisterUIFunctions();

        int Script_FireActionBarSlotUpdateEvent(lua_State* L);
        int Script_FireTalentUpdateEvent(lua_State* L);
        int Script_FlashGameWindow(lua_State* L);
        int Script_FindSpellActionBarSlot(lua_State* L);
        int Script_ReplaceActionBarSpell(lua_State* L);
        int Script_SetSpellInActionBarSlot(lua_State* L);
        int Script_SetFrameChildClipping(lua_State* L);
        int Script_GetFrameChildClipping(lua_State* L);
        int Script_SetTextureWithMask(lua_State* L);
        int Script_SetPortraitToTexture(lua_State* L);

        bool ApplyMaskedTexture(CSimpleTexture* texture, const char* source, const char* mask);
    } // namespace Lua
} // namespace Duskhaven
