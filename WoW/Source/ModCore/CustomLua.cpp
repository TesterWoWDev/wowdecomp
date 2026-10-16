#include "CustomLua.h"

#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/Images/tga.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C4Quaternion.h"

#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/Ui/QuestTextParser.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/Ui/WorldMap.h"
#include "WoW/Source/Ui/ChatFrame.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/UIUtil/PortraitButton.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellIconRec.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/DB/Rec/SpellFocusObjectRec.h"
#include "WoW/Source/DB/Rec/AreaGroupRec.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/TotemCategoryRec.h"
#include "WoW/Source/DB/Rec/ItemSubClassRec.h"
#include "WoW/Source/DB/Rec/ItemClassRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/DB/Rec/AnimationDataRec.h"
#include "WoW/Source/DB/Rec/SoundEntriesRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/Ui/CharacterModelBase.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"

#include <windows.h>
#ifdef GetObjectType
#undef GetObjectType
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ModCore
{
    // =========================================================================
    // CustomLuaManager Singleton & Methods
    // =========================================================================

    CustomLuaManager& CustomLuaManager::Get()
    {
        static CustomLuaManager s_instance;
        return s_instance;
    }

    void CustomLuaManager::UpdateSpellCharge(uint32_t spellId, const SpellCharge& charge)
    {
        auto it = m_spellCharges.find(spellId);
        if (it != m_spellCharges.end())
        {
            SpellCharge updated = charge;
            if (updated.cooldown == 0)
                updated.cooldown = it->second.cooldown;
            it->second = updated;
        }
        else
        {
            m_spellCharges[spellId] = charge;
        }
    }

    bool CustomLuaManager::GetSpellCharge(uint32_t spellId, SpellCharge& outCharge) const
    {
        auto it = m_spellCharges.find(spellId);
        if (it != m_spellCharges.end())
        {
            outCharge = it->second;
            return true;
        }
        return false;
    }

    void CustomLuaManager::ClearSpellCharges()
    {
        m_spellCharges.clear();
    }

    int CustomLuaManager::SpecToIndex(uint32_t specID) const
    {
        static const uint32_t spec1[] = { 1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 34 };
        static const uint32_t spec2[] = { 2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 35 };
        static const uint32_t spec3[] = { 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 36 };
        static const uint32_t spec4[] = { 31, 32, 33 };

        for (uint32_t s : spec1) { if (s == specID) return 0; }
        for (uint32_t s : spec2) { if (s == specID) return 1; }
        for (uint32_t s : spec3) { if (s == specID) return 2; }
        for (uint32_t s : spec4) { if (s == specID) return 3; }
        return -1;
    }

    // =========================================================================
    // 1. Player & Spec Lua Callbacks
    // =========================================================================

    static int Script_GetShapeshiftFormID(lua_State* L)
    {
        CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
        if (activePlayer)
        {
            lua_pushnumber(L, activePlayer->GetShapeshiftForm());
            return 1;
        }
        lua_pushnumber(L, 0);
        return 1;
    }

    static int Script_GetActiveSpec(lua_State* L)
    {
        lua_pushnumber(L, CustomLuaManager::Get().GetActiveSpec());
        return 1;
    }

    static int Script_SetActiveSpec(lua_State* L)
    {
        if (lua_isnumber(L, 1))
        {
            uint32_t const newSpec = static_cast<uint32_t>(lua_tonumber(L, 1));
            CustomLuaManager::Get().SetActiveSpec(newSpec);
        }
        return 0;
    }

    static int Script_GetMasteryRating(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (!player || !player->m_player)
            return 0;

        int32_t crMastery = static_cast<int32_t>(player->m_player->m_playerFieldCombatRating1[24]);
        int activeSpecIndex = CustomLuaManager::Get().SpecToIndex(CustomLuaManager::Get().GetActiveSpec());
        uint32_t masteryAmount = CustomLuaManager::Get().GetMasteryAmount() + crMastery;
        float rating = CustomLuaManager::Get().GetMasteryRatingSpec(activeSpecIndex);
        if (rating <= 0.0f)
            rating = 1.0f;
        float pct = CustomLuaManager::Get().GetMasteryForSpec(activeSpecIndex) + (static_cast<float>(crMastery) / rating);

        lua_pushnumber(L, masteryAmount);
        lua_pushnumber(L, pct);
        return 2;
    }

    static int Script_SetMasteryRatings(lua_State* L)
    {
        float num1 = static_cast<float>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 1.0);
        float num2 = static_cast<float>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 1.0);
        float num3 = static_cast<float>(lua_isnumber(L, 3) ? lua_tonumber(L, 3) : 1.0);
        float num4 = static_cast<float>(lua_isnumber(L, 4) ? lua_tonumber(L, 4) : 1.0);

        CustomLuaManager::Get().SetMasteryRatingSpec(0, num1 != 0.0f ? num1 : 1.0f);
        CustomLuaManager::Get().SetMasteryRatingSpec(1, num2 != 0.0f ? num2 : 1.0f);
        CustomLuaManager::Get().SetMasteryRatingSpec(2, num3 != 0.0f ? num3 : 1.0f);
        CustomLuaManager::Get().SetMasteryRatingSpec(3, num4 != 0.0f ? num4 : 1.0f);
        return 0;
    }

    static int Script_UpdateMasteryAmount(lua_State* L)
    {
        CustomLuaManager::Get().SetMasteryAmount(static_cast<uint32_t>(lua_tonumber(L, 1)));
        CustomLuaManager::Get().SetMasteryPct(static_cast<float>(lua_tonumber(L, 2)));
        return 0;
    }

    static int Script_UpdateChargeInfoForSpell(lua_State* L)
    {
        uint32_t spellID = static_cast<uint32_t>(lua_tonumber(L, 1));
        SpellCharge temp;
        temp.currentCharges = static_cast<uint8_t>(lua_tonumber(L, 2));
        temp.maxCharges = static_cast<uint8_t>(lua_tonumber(L, 3));
        temp.async = OsGetAsyncTimeMs();
        temp.remainingCooldown = static_cast<uint32_t>(lua_tonumber(L, 4));
        temp.cooldown = static_cast<uint32_t>(lua_tonumber(L, 5));

        CustomLuaManager::Get().UpdateSpellCharge(spellID, temp);
        return 0;
    }

    static int Script_GetChargeInfoForSpell(lua_State* L)
    {
        uint32_t spellID = static_cast<uint32_t>(lua_tonumber(L, 1));
        SpellCharge row;
        if (!CustomLuaManager::Get().GetSpellCharge(spellID, row))
        {
            lua_pushnil(L);
            return 1;
        }

        lua_pushnumber(L, row.currentCharges);
        lua_pushnumber(L, row.maxCharges);
        lua_pushnumber(L, row.remainingCooldown);
        lua_pushnumber(L, row.cooldown);
        return 4;
    }

    static int Script_GetCombatRatingMult(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        uint32_t rating = static_cast<uint32_t>(lua_tonumber(L, 1));
        float value = 0.0f;

        if (player && player->m_unit && rating >= 1)
        {
            uint32_t level = player->m_unit->m_unitFieldLevel;
            value = GetGameTableValue(1, level - 1, rating - 1);
        }

        lua_pushnumber(L, value);
        return 1;
    }

    static int Script_GetCombatRatingScalar(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        uint32_t rating = static_cast<uint32_t>(lua_tonumber(L, 1));
        float value = 0.0f;

        if (player && player->m_unit && rating >= 1)
        {
            uint32_t classID = player->m_unit->m_unitFieldBytes0[1];
            value = GetGameTableValue(11, rating, classID - 1);
        }

        lua_pushnumber(L, value);
        return 1;
    }

    static int Script_GetModCastSpeed(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (player && player->m_unit)
            lua_pushnumber(L, player->m_unit->m_unitModCastSpeed);
        else
            lua_pushnumber(L, 0.0f);
        return 1;
    }

    static int Script_GetWeaponAP(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        uint32_t attackType = static_cast<uint32_t>(lua_tonumber(L, 1));

        if (player && player->m_player && attackType < 3)
        {
            const int32_t* weaponBonusAP = reinterpret_cast<const int32_t*>(&player->m_player->m_playerFieldGlyphs1[3]);
            lua_pushnumber(L, weaponBonusAP[attackType]);
        }
        else
        {
            lua_pushnumber(L, 0.0f);
        }
        return 1;
    }

    static int Script_GetPetSP(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (player && player->m_player)
            lua_pushnumber(L, player->m_player->m_playerPetSpellPower);
        else
            lua_pushnumber(L, 0.0f);
        return 1;
    }

    static int Script_GetSpeedPercent(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (player && player->m_player)
        {
            const float* speed = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[0]);
            lua_pushnumber(L, *speed);
        }
        else
        {
            lua_pushnumber(L, 0.0f);
        }
        return 1;
    }

    static int Script_GetLeechPercent(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (player && player->m_player)
        {
            const float* leech = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[1]);
            lua_pushnumber(L, *leech);
        }
        else
        {
            lua_pushnumber(L, 0.0f);
        }
        return 1;
    }

    static int Script_GetAvoidancePercent(lua_State* L)
    {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        if (player && player->m_player)
        {
            const float* avoidance = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[2]);
            lua_pushnumber(L, *avoidance);
        }
        else
        {
            lua_pushnumber(L, 0.0f);
        }
        return 1;
    }

    // =========================================================================
    // 2. Action Bar & UI Callbacks
    // =========================================================================

    static int Script_FireActionBarSlotUpdateEvent(lua_State* L)
    {
        uint8_t slotID = static_cast<uint8_t>(lua_tonumber(L, 1));
        if (slotID < 144)
            FrameScript_SignalEvent(SCRIPT_EVENT_ACTIONBAR_SLOT_CHANGED, "%d", slotID + 1);
        return 0;
    }

    static int Script_FireTalentUpdateEvent(lua_State* L)
    {
        FrameScript_SignalEvent(SCRIPT_EVENT_PLAYER_TALENT_UPDATE, nullptr);
        return 0;
    }

    static int Script_FlashGameWindow(lua_State* L)
    {
        HWND activeWindow = static_cast<HWND>(OsGuiGetWindow(0));
        if (activeWindow && GetForegroundWindow() != activeWindow)
        {
            FLASHWINFO flashInfo{};
            flashInfo.cbSize = sizeof(flashInfo);
            flashInfo.hwnd = activeWindow;
            flashInfo.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
            flashInfo.uCount = static_cast<UINT>(-1);
            flashInfo.dwTimeout = 500;
            FlashWindowEx(&flashInfo);
        }
        return 0;
    }

    static int Script_FindSpellActionBarSlot(lua_State* L)
    {
        uint32_t spellID = static_cast<uint32_t>(lua_tonumber(L, 1));
        uint8_t count = 0;

        for (uint8_t i = 0; i < 144; i++)
        {
            if (CGActionBar::m_slotActions[i] == spellID)
            {
                lua_pushnumber(L, i);
                count++;
            }
        }

        if (!count)
        {
            lua_pushnil(L);
            return 1;
        }
        return count;
    }

    static int Script_ReplaceActionBarSpell(lua_State* L)
    {
        uint32_t oldSpellID = static_cast<uint32_t>(lua_tonumber(L, 1));
        uint32_t newSpellID = static_cast<uint32_t>(lua_tonumber(L, 2));

        for (uint8_t i = 0; i < 144; i++)
        {
            if (CGActionBar::m_slotActions[i] == oldSpellID)
            {
                CGActionBar::SetAction(i, newSpellID, 1, 0);

                for (uint8_t j = i + 72; j < 144; j += 12)
                {
                    if (!CGActionBar::m_slotUsable[j])
                    {
                        CGActionBar::SetAction(j, newSpellID, 1, 0);
                    }
                }
            }
        }
        return 0;
    }

    static int Script_SetSpellInActionBarSlot(lua_State* L)
    {
        uint32_t spellID = static_cast<uint32_t>(lua_tonumber(L, 1));
        uint8_t slotID = static_cast<uint8_t>(lua_tonumber(L, 2));

        if (slotID < 144)
        {
            CGActionBar::SetAction(slotID, spellID, 1, 0);
        }
        return 0;
    }

    static int Script_SetFrameChildClipping(lua_State* L)
    {
        CSimpleFrame* frame = nullptr;
        int32_t enabled = 1;

        if (lua_isnumber(L, 2) || lua_isboolean(L, 2))
            enabled = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : (lua_tonumber(L, 2) != 0.0);

        if (lua_isstring(L, 1))
            frame = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 1, nullptr), CSimpleFrame::GetObjectType()));
        else if (lua_type(L, 1) == LUA_TTABLE)
        {
            lua_rawgeti(L, 1, 0);
            frame = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
            lua_settop(L, -2);
            if (frame && !frame->IsA(CSimpleFrame::GetObjectType()))
                frame = nullptr;
        }

        if (!frame)
        {
            return luaL_error(L, "Usage: SetFrameChildClipping(frame, enabled)");
        }

        frame->SetChildClipping(enabled);
        return 0;
    }

    static int Script_GetFrameChildClipping(lua_State* L)
    {
        CSimpleFrame* frame = nullptr;

        if (lua_isstring(L, 1))
            frame = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 1, nullptr), CSimpleFrame::GetObjectType()));
        else if (lua_type(L, 1) == LUA_TTABLE)
        {
            lua_rawgeti(L, 1, 0);
            frame = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
            lua_settop(L, -2);
            if (frame && !frame->IsA(CSimpleFrame::GetObjectType()))
                frame = nullptr;
        }

        lua_pushboolean(L, frame && frame->GetChildClipping());
        return 1;
    }

    namespace
    {
        CSimpleTexture* ResolveTextureObject(lua_State* L, int index)
        {
            if (lua_isstring(L, index))
            {
                const char* name = lua_tolstring(L, index, nullptr);
                return name ? static_cast<CSimpleTexture*>(CScriptObject::GetScriptObjectByName(name, CSimpleTexture::GetObjectType())) : nullptr;
            }

            if (lua_type(L, index) != LUA_TTABLE)
                return nullptr;

            lua_rawgeti(L, index, 0);
            CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
            lua_settop(L, -2);

            return (texture && texture->IsA(CSimpleTexture::GetObjectType())) ? texture : nullptr;
        }

        struct MaskedTextureKey
        {
            std::string source;
            std::string mask;
            uint32_t width  = 0;
            uint32_t height = 0;

            bool operator<(MaskedTextureKey const& other) const
            {
                if (width != other.width) return width < other.width;
                if (height != other.height) return height < other.height;
                if (source != other.source) return source < other.source;
                return mask < other.mask;
            }
        };

        struct MaskedTextureCacheEntry
        {
            CTexture* texture = nullptr;
            std::vector<CImVector> pixels;
            std::string debugName;
        };

        static std::map<MaskedTextureKey, std::unique_ptr<MaskedTextureCacheEntry>> s_maskedTextureCache;

        static std::string NormalizeTexturePath(std::string path)
        {
            std::replace(path.begin(), path.end(), '/', '\\');
            std::transform(path.begin(), path.end(), path.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return path;
        }

        static std::string AppendExtensionIfMissing(std::string path, std::string const& extension)
        {
            if (path.size() >= extension.size() &&
                path.compare(path.size() - extension.size(), extension.size(), extension) == 0)
            {
                return path;
            }
            return path + extension;
        }

        static uint8_t SelectMaskByte(uint8_t const* px, bool preferAlpha)
        {
            if (preferAlpha) return px[3];
            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r = px[2];
            return static_cast<uint8_t>((static_cast<uint32_t>(r) + static_cast<uint32_t>(g) + static_cast<uint32_t>(b)) / 3u);
        }

        static bool LoadBLPToRGBA(std::string const& path, std::vector<CImVector>& pixels, uint32_t& width, uint32_t& height)
        {
            CBLPFile blp;
            std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".blp");
            if (!blp.Open(filename.c_str(), 1))
                return false;

            uint8_t* data = nullptr;
            uint32_t stride = 0;
            if (!blp.Lock(PIXEL_ARGB8888, 0, data, stride) || !data)
            {
                blp.Close();
                return false;
            }

            width = blp.Width(0);
            height = blp.Height(0);
            uint32_t pixelCount = width * height;
            pixels.resize(pixelCount);
            memcpy(pixels.data(), data, pixelCount * sizeof(CImVector));

            blp.Unlock(0);
            blp.Close();
            return true;
        }

        static bool LoadMaskAlphaFromTga(std::string const& path, uint32_t pixelCount, std::vector<uint8_t>& alphaOut)
        {
            CTgaFile tga;
            std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".tga");
            if (!tga.Open(filename.c_str(), 1))
                return false;

            bool ok = false;
            if (tga.LoadImageData(0))
            {
                uint8_t* src = tga.Image();
                if (src)
                {
                    alphaOut.resize(pixelCount);
                    memcpy(alphaOut.data(), src, pixelCount);
                    ok = true;
                }
            }
            tga.Close();
            return ok;
        }

        static bool LoadMaskAlphaFromBlp(std::string const& path, uint32_t targetWidth, uint32_t targetHeight, std::vector<uint8_t>& alphaOut)
        {
            CBLPFile blp;
            std::string filename = AppendExtensionIfMissing(NormalizeTexturePath(path), ".blp");
            if (!blp.Open(filename.c_str(), 1))
                return false;

            uint8_t* data = nullptr;
            uint32_t stride = 0;
            if (!blp.Lock(PIXEL_ARGB8888, 0, data, stride) || !data)
            {
                blp.Close();
                return false;
            }

            uint32_t width = blp.Width(0);
            uint32_t height = blp.Height(0);
            uint32_t pixelCount = targetWidth * targetHeight;

            if (width != 0 && height != 0)
            {
                uint8_t* src = data;
                uint32_t srcCount = width * height;
                uint8_t alphaMin = 255, alphaMax = 0;
                uint8_t rgbMin = 255, rgbMax = 0;

                for (uint32_t i = 0; i < srcCount; ++i)
                {
                    uint8_t const* px = src + i * 4;
                    uint8_t a = px[3];
                    uint8_t rgb = static_cast<uint8_t>((static_cast<uint32_t>(px[0]) + static_cast<uint32_t>(px[1]) + static_cast<uint32_t>(px[2])) / 3u);
                    if (a < alphaMin) alphaMin = a;
                    if (a > alphaMax) alphaMax = a;
                    if (rgb < rgbMin) rgbMin = rgb;
                    if (rgb > rgbMax) rgbMax = rgb;
                }
                bool preferAlpha = (alphaMax > alphaMin) && (alphaMin != 255 || alphaMax != 255);
                alphaOut.resize(pixelCount);

                if (width * height == pixelCount)
                {
                    for (uint32_t i = 0; i < pixelCount; ++i)
                        alphaOut[i] = SelectMaskByte(src + i * 4, preferAlpha);
                }
                else if (targetWidth != 0 && targetHeight != 0)
                {
                    for (uint32_t y = 0; y < targetHeight; ++y)
                    {
                        uint32_t srcY = (static_cast<uint64_t>(y) * height) / targetHeight;
                        for (uint32_t x = 0; x < targetWidth; ++x)
                        {
                            uint32_t srcX = (static_cast<uint64_t>(x) * width) / targetWidth;
                            alphaOut[y * targetWidth + x] = SelectMaskByte(src + ((srcY * width + srcX) * 4), preferAlpha);
                        }
                    }
                }
            }

            blp.Unlock(0);
            blp.Close();
            return true;
        }

        static bool LoadMaskAlpha(std::string const& maskBase, uint32_t width, uint32_t height, std::vector<uint8_t>& alphaOut)
        {
            uint32_t pixelCount = width * height;
            bool useSmall = width <= 64 && height <= 64;

            std::vector<std::string> candidates = {
                useSmall ? maskBase + "Small" : maskBase,
                useSmall ? maskBase : maskBase + "Small",
                maskBase,
                maskBase + "Small"
            };

            for (std::string const& candidate : candidates)
            {
                if (LoadMaskAlphaFromTga(candidate, pixelCount, alphaOut)) return true;
                if (LoadMaskAlphaFromBlp(candidate, width, height, alphaOut)) return true;
            }
            return false;
        }

        static void MaskedTextureUpdate(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, void* userArg, uint32_t& texelStrideInBytes, const void*& texels)
        {
            auto* entry = static_cast<MaskedTextureCacheEntry*>(userArg);
            if (!entry) return;

            if (command == GxTex_Latch)
            {
                texelStrideInBytes = 4 * width;
                texels = entry->pixels.data();
            }
        }

        static CTexture* CreateMaskedTexture(std::string const& sourceBase, std::string const& maskBase)
        {
            std::vector<CImVector> pixels;
            uint32_t width = 0, height = 0;
            if (!LoadBLPToRGBA(sourceBase, pixels, width, height))
                return nullptr;

            if (width == 0 || height == 0 || pixels.empty())
                return nullptr;

            std::vector<uint8_t> alpha;
            if (!LoadMaskAlpha(maskBase, width, height, alpha))
                return nullptr;

            if (alpha.size() != pixels.size())
                return nullptr;

            for (size_t i = 0; i < pixels.size(); ++i)
                pixels[i].a = alpha[i];

            MaskedTextureKey key{NormalizeTexturePath(sourceBase), NormalizeTexturePath(maskBase), width, height};
            auto it = s_maskedTextureCache.find(key);
            if (it != s_maskedTextureCache.end())
                return it->second->texture;

            auto entry = std::make_unique<MaskedTextureCacheEntry>();
            entry->pixels = std::move(pixels);
            entry->debugName = "MaskedTexture:" + NormalizeTexturePath(sourceBase) + "|" + NormalizeTexturePath(maskBase);

            CGxTexFlags flags(GxTex_Linear, 0, 0, 0, 0, 0, 1);
            entry->texture = TextureCreate(width, height, GxTex_Argb8888, GxTex_Argb8888, flags, entry.get(), MaskedTextureUpdate, entry->debugName.c_str(), 0);
            if (!entry->texture)
                return nullptr;

            CTexture* createdTex = entry->texture;
            s_maskedTextureCache.emplace(std::move(key), std::move(entry));
            return createdTex;
        }

        static bool ApplyMaskedTexture(CSimpleTexture* texture, const char* source, const char* mask)
        {
            if (!texture || !source || !source[0] || !mask || !mask[0])
                return false;

            CTexture* masked = CreateMaskedTexture(source, mask);
            if (!masked)
                return false;

            texture->SetTexture(masked);
            return true;
        }
    } // namespace

    static int Script_SetTextureWithMask(lua_State* L)
    {
        CSimpleTexture* texture = ResolveTextureObject(L, 1);
        if (!texture)
        {
            return luaL_error(L, "SetTextureWithMask(): expected texture object or texture name");
        }

        std::string source = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
        std::string mask   = lua_isstring(L, 3) ? lua_tostring(L, 3) : "";
        if (source.empty() || mask.empty())
        {
            return luaL_error(L, "SetTextureWithMask(): expected source texture and mask texture");
        }

        if (!ApplyMaskedTexture(texture, source.c_str(), mask.c_str()))
        {
            std::string err = "SetTextureWithMask(): failed to build masked texture from '" + source + "' with mask '" + mask + "'";
            return luaL_error(L, "%s", err.c_str());
        }
        return 0;
    }

    static int Script_SetPortraitToTexture(lua_State* L)
    {
        CSimpleTexture* texture = ResolveTextureObject(L, 1);
        if (!texture)
        {
            return luaL_error(L, "SetPortraitToTexture(): expected texture object or texture name");
        }

        if (lua_isstring(L, 3))
        {
            const char* mask = lua_tolstring(L, 3, nullptr);
            if (mask && mask[0] && lua_isstring(L, 2))
            {
                const char* source = lua_tolstring(L, 2, nullptr);
                if (ApplyMaskedTexture(texture, source, mask))
                {
                    return 0;
                }
            }
        }

        if (lua_isstring(L, 2))
        {
            SetPortraitTexture(texture, lua_tolstring(L, 2, nullptr));
        }
        return 0;
    }

    static int Script_EmitFrameEventByName(lua_State* L)
    {
        if (lua_isstring(L, 1))
        {
            const char* eventName = lua_tostring(L, 1);
            if (eventName && eventName[0])
            {
                FrameScript_SignalEventByName(eventName);
            }
        }
        return 0;
    }

    // =========================================================================
    // 3. Spell & Profession Callbacks
    // =========================================================================

    static int Script_GetSpellDescription(lua_State* L)
    {
        if (lua_isnumber(L, 1))
        {
            uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 1));
            SpellRec spell{};
            if (g_spellDB.GetRecord(spellId, &spell))
            {
                char dest[1024] = {};
                SpellParserParseText(&spell, dest, sizeof(dest), 0, 0, 0, 0, 1, 0);
                lua_pushstring(L, dest);
                return 1;
            }
        }

        lua_pushstring(L, "");
        return 1;
    }

    static int Script_GetSpellNameById(lua_State* L)
    {
        if (lua_isnumber(L, 1))
        {
            uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 1));
            SpellRec spell{};
            if (g_spellDB.GetRecord(spellId, &spell))
            {
                lua_pushstring(L, spell.m_name ? spell.m_name : "");
                lua_pushstring(L, spell.m_nameSubtext ? spell.m_nameSubtext : "");
                return 2;
            }
        }

        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }

    static int Script_GetSpellIconById(lua_State* L)
    {
        if (lua_isnumber(L, 1))
        {
            uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 1));
            SpellRec spell{};
            if (g_spellDB.GetRecord(spellId, &spell))
            {
                SpellIconRec* iconRow = g_spellIconDB.GetRecord(spell.m_spellIconID);
                if (iconRow && iconRow->m_textureFilename)
                {
                    lua_pushstring(L, iconRow->m_textureFilename);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    static int Script_SpellRequiresComboPoints(lua_State* L)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_UnitCustomCastingData(lua_State* L)
    {
        if (!lua_isstring(L, 1))
        {
            return luaL_error(L, "Usage: UnitCustomCastingData(\"unit\")");
        }

        CGUnit_C* unit = GetUnitFromName(lua_tostring(L, 1));
        if (!unit)
            return 0;

        uint32_t currentCast = 0;
        if (unit->m_castingSpellId)
            currentCast = unit->m_castingSpellId;
        else if (unit->m_channelSpellId)
            currentCast = unit->m_channelSpellId;

        if (!currentCast)
            return 0;

        SpellRec spell{};
        if (!g_spellDB.GetRecord(currentCast, &spell))
            return 0;

        float spellId = static_cast<float>(spell.m_id);
        lua_pushnumber(L, spellId);
        lua_pushboolean(L, 0);
        lua_pushboolean(L, 0);
        return 3;
    }

    // Profession index helpers
    namespace
    {
        static std::unordered_map<uint32_t, std::vector<uint32_t>> s_professionSpells;
        static std::unordered_map<uint32_t, SkillLineAbilityRec> s_professionAbilities;
        static std::unordered_map<uint32_t, std::vector<uint32_t>> s_knownProfessionSpells;
        static bool s_professionIndexLoaded = false;
        static WOWGUID s_professionPlayerGuid = {0};

        static void LoadProfessionIndex()
        {
            if (s_professionIndexLoaded)
                return;

            s_professionSpells.clear();
            s_professionAbilities.clear();

            for (int32_t i = 0; i < g_skillLineAbilityDB.m_numRecords; ++i)
            {
                const SkillLineAbilityRec& ability = g_skillLineAbilityDB.m_records[i];
                s_professionSpells[ability.m_skillLine].push_back(ability.m_spell);
                s_professionAbilities[ability.m_spell] = ability;
            }

            for (auto& pair : s_professionSpells)
            {
                auto& spells = pair.second;
                std::sort(spells.begin(), spells.end());
                spells.erase(std::unique(spells.begin(), spells.end()), spells.end());
            }

            s_professionIndexLoaded = true;
        }

        static void InvalidateProfessionRecipes()
        {
            s_knownProfessionSpells.clear();
            s_professionPlayerGuid = ClntObjMgrGetActivePlayer();
        }

        static const std::vector<uint32_t>& GetKnownProfessionRecipes(uint32_t skillLine)
        {
            if (s_professionPlayerGuid != ClntObjMgrGetActivePlayer())
            {
                InvalidateProfessionRecipes();
            }

            auto found = s_knownProfessionSpells.find(skillLine);
            if (found != s_knownProfessionSpells.end())
            {
                return found->second;
            }

            LoadProfessionIndex();
            auto& recipes = s_knownProfessionSpells[skillLine];
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (!player || !skillLine)
            {
                return recipes;
            }

            auto abilities = s_professionSpells.find(skillLine);
            if (abilities == s_professionSpells.end())
            {
                return recipes;
            }

            for (uint32_t spellId : abilities->second)
            {
                if (player->IsSpellKnown(spellId))
                {
                    recipes.push_back(spellId);
                }
            }
            return recipes;
        }

        static uint32_t GetProfessionRecipeCountForSkillLine(uint32_t skillLineId)
        {
            return static_cast<uint32_t>(GetKnownProfessionRecipes(skillLineId).size());
        }

        static uint32_t GetProfessionRecipeSpellId(uint32_t skillLineId, uint32_t oneBasedIndex)
        {
            const auto& recipes = GetKnownProfessionRecipes(skillLineId);
            if (oneBasedIndex == 0 || oneBasedIndex > recipes.size())
            {
                return 0;
            }
            return recipes[oneBasedIndex - 1];
        }

        static uint32_t GetCraftedItemId(uint32_t spellId, const SpellRec& spell)
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (spell.m_effectItemType[i] > 0)
                {
                    return static_cast<uint32_t>(spell.m_effectItemType[i]);
                }
            }
            return 0;
        }

        static uint32_t GetPlayerItemCount(uint32_t itemId)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (!player || !itemId)
            {
                return 0;
            }
            return player->m_bag.GetItemTypeCount(itemId, 0);
        }

        static uint32_t GetCraftableCount(const SpellRec& row)
        {
            uint32_t craftable = 0xFFFFFFFF;
            bool hasReagent = false;

            for (uint32_t i = 0; i < 8; ++i)
            {
                uint32_t reagentId = row.m_reagent[i];
                uint32_t required = row.m_reagentCount[i];
                if (!reagentId || !required)
                    continue;

                hasReagent = true;
                uint32_t available = GetPlayerItemCount(reagentId) / required;
                if (available < craftable)
                {
                    craftable = available;
                }
            }

            if (!hasReagent)
                return 1;

            return craftable == 0xFFFFFFFF ? 0 : craftable;
        }

        static ItemStats_C* GetItemStats(uint32_t itemId)
        {
            if (!itemId)
                return nullptr;
            return g_itemDBCache.GetRecord(itemId, WOWGUID(), nullptr, nullptr, false);
        }

        static const char* GetItemName(uint32_t itemId)
        {
            ItemStats_C* stats = GetItemStats(itemId);
            return (stats && stats->GetName(0)) ? stats->GetName(0) : "";
        }

        static std::string GetItemIcon(uint32_t itemId)
        {
            ItemStats_C* stats = GetItemStats(itemId);
            if (!stats || !stats->m_displayId)
                return "";

            ItemDisplayInfoRec displayRec{};
            if (g_itemDisplayInfoDB.GetRecord(stats->m_displayId, &displayRec))
            {
                if (displayRec.m_inventoryIcon[0] && displayRec.m_inventoryIcon[0][0])
                {
                    return displayRec.m_inventoryIcon[0];
                }
            }
            return "";
        }

        static const char* GetItemCategory(uint32_t itemId)
        {
            ItemStats_C* stats = GetItemStats(itemId);
            if (!stats)
                return "";

            for (int32_t i = 0; i < g_itemSubClassDB.m_numRecords; ++i)
            {
                const ItemSubClassRec& row = g_itemSubClassDB.m_records[i];
                if (static_cast<uint32_t>(row.m_classID) == stats->m_class &&
                    static_cast<uint32_t>(row.m_subClassID) == stats->m_subClass)
                {
                    if (row.m_displayName && row.m_displayName[0])
                        return row.m_displayName;
                    if (row.m_verboseName && row.m_verboseName[0])
                        return row.m_verboseName;
                    break;
                }
            }

            ItemClassRec* itemClass = g_itemClassDB.GetRecord(stats->m_class);
            return (itemClass && itemClass->m_className) ? itemClass->m_className : "";
        }

        struct ProfessionRequirement
        {
            std::string name;
            bool met;
        };

        static std::string BuildProfessionRequirements(const SpellRec& row)
        {
            std::vector<ProfessionRequirement> requirements;
            requirements.reserve(7);

            if (row.m_requiresSpellFocus)
            {
                SpellFocusObjectRec* focus = g_spellFocusObjectDB.GetRecord(row.m_requiresSpellFocus);
                if (focus && focus->m_name)
                {
                    requirements.push_back({focus->m_name, true});
                }
            }

            if (row.m_requiredAreasID > 0)
            {
                AreaGroupRec* group = g_areaGroupDB.GetRecord(row.m_requiredAreasID);
                if (group)
                {
                    AreaTableRec* area = g_areaTableDB.GetRecord(group->m_areaID[0]);
                    if (area && area->m_areaName)
                    {
                        requirements.push_back({area->m_areaName, true});
                    }
                }
            }

            for (uint32_t i = 0; i < 2; ++i)
            {
                uint32_t itemId = row.m_totem[i];
                if (!itemId)
                    continue;

                const char* itemName = GetItemName(itemId);
                if (itemName && itemName[0])
                {
                    requirements.push_back({itemName, GetPlayerItemCount(itemId) > 0});
                }
            }

            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            for (uint32_t i = 0; i < 2; ++i)
            {
                uint32_t categoryId = row.m_requiredTotemCategoryID[i];
                if (!categoryId)
                    continue;

                TotemCategoryRec* category = g_totemCategoryDB.GetRecord(categoryId);
                if (category && category->m_name)
                {
                    bool hasTotem = player && player->m_bag.FindTotemCategory(categoryId, 0);
                    requirements.push_back({category->m_name, hasTotem});
                }
            }

            std::string text;
            for (const auto& req : requirements)
            {
                if (!text.empty()) text += ", ";
                if (req.met) text += req.name;
                else text += "|cffff2020" + req.name + "|r";
            }
            return text;
        }

        static uint32_t GetNonzeroReagentSlot(const SpellRec& row, uint32_t oneBasedReagentIndex)
        {
            uint32_t seen = 0;
            for (uint32_t i = 0; i < 8; ++i)
            {
                if (!row.m_reagent[i])
                    continue;

                ++seen;
                if (seen == oneBasedReagentIndex)
                    return i;
            }
            return 8;
        }

        static uint32_t GetNonzeroReagentCount(const SpellRec& row)
        {
            uint32_t count = 0;
            for (uint32_t i = 0; i < 8; ++i)
            {
                if (row.m_reagent[i])
                    ++count;
            }
            return count;
        }
    } // namespace

    static int Script_PreloadProfessionItemCache(lua_State* L)
    {
        uint32_t itemId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        if (itemId)
        {
            g_itemDBCache.GetRecord(itemId, WOWGUID(), nullptr, nullptr, false);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_GetProfessionRecipeCount(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        lua_pushnumber(L, static_cast<double>(GetProfessionRecipeCountForSkillLine(skillLineId)));
        return 1;
    }

    static int Script_GetProfessionRecipeSpellId(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        uint32_t index = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);
        if (!spellId)
        {
            lua_pushnil(L);
            return 1;
        }

        lua_pushnumber(L, static_cast<double>(spellId));
        return 1;
    }

    static int Script_GetProfessionRecipeInfo(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        uint32_t index = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);
        if (!spellId)
        {
            lua_pushnil(L);
            return 1;
        }

        SpellRec row{};
        bool cached = g_spellDB.GetRecord(spellId, &row);

        LoadProfessionIndex();
        auto abilityIt = s_professionAbilities.find(spellId);
        const SkillLineAbilityRec* ability = abilityIt != s_professionAbilities.end() ? &abilityIt->second : nullptr;

        uint32_t minRank = ability ? ability->m_minSkillLineRank : 0;
        uint32_t trivialHigh = ability ? ability->m_trivialSkillLineRankHigh : 0;
        uint32_t trivialLow = ability ? ability->m_trivialSkillLineRankLow : 0;

        lua_pushnumber(L, static_cast<double>(spellId));
        lua_pushstring(L, cached && row.m_name ? row.m_name : "");
        lua_pushstring(L, cached && row.m_nameSubtext ? row.m_nameSubtext : "");

        SpellIconRec* iconRow = cached ? g_spellIconDB.GetRecord(row.m_spellIconID) : nullptr;
        lua_pushstring(L, (iconRow && iconRow->m_textureFilename) ? iconRow->m_textureFilename : "");

        lua_pushnumber(L, static_cast<double>(minRank));
        lua_pushnumber(L, static_cast<double>(trivialHigh));
        lua_pushnumber(L, static_cast<double>(trivialLow));
        lua_pushnumber(L, cached ? static_cast<double>(GetCraftableCount(row)) : 0.0);
        lua_pushboolean(L, cached ? 1 : 0);

        uint32_t craftedItemId = cached ? GetCraftedItemId(spellId, row) : 0;
        lua_pushnumber(L, static_cast<double>(craftedItemId));
        lua_pushstring(L, GetItemName(craftedItemId));
        lua_pushstring(L, GetItemIcon(craftedItemId).c_str());
        lua_pushstring(L, GetItemCategory(craftedItemId));
        return 13;
    }

    static int Script_GetProfessionRecipeRequirements(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        uint32_t index = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

        SpellRec row{};
        if (!spellId || !g_spellDB.GetRecord(spellId, &row))
        {
            lua_pushnil(L);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 3;
        }

        std::string requirements = BuildProfessionRequirements(row);
        lua_pushstring(L, requirements.c_str());
        lua_pushnumber(L, static_cast<double>(row.m_totem[0]));
        lua_pushnumber(L, static_cast<double>(row.m_totem[1]));
        return 3;
    }

    static int Script_GetProfessionRecipeReagentCount(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        uint32_t index = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

        SpellRec row{};
        if (!spellId || !g_spellDB.GetRecord(spellId, &row))
        {
            lua_pushnumber(L, 0);
            return 1;
        }

        lua_pushnumber(L, static_cast<double>(GetNonzeroReagentCount(row)));
        return 1;
    }

    static int Script_GetProfessionRecipeReagentInfo(lua_State* L)
    {
        uint32_t skillLineId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        uint32_t index = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        uint32_t reagentIndex = static_cast<uint32_t>(lua_isnumber(L, 3) ? lua_tonumber(L, 3) : 0);
        uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

        SpellRec row{};
        if (!spellId || !reagentIndex || !g_spellDB.GetRecord(spellId, &row))
        {
            lua_pushnil(L);
            return 1;
        }

        uint32_t reagentSlot = GetNonzeroReagentSlot(row, reagentIndex);
        if (reagentSlot >= 8)
        {
            lua_pushnil(L);
            return 1;
        }

        uint32_t itemId = row.m_reagent[reagentSlot];
        uint32_t required = row.m_reagentCount[reagentSlot];
        uint32_t have = GetPlayerItemCount(itemId);

        lua_pushnumber(L, static_cast<double>(itemId));
        lua_pushstring(L, GetItemName(itemId));
        lua_pushstring(L, GetItemIcon(itemId).c_str());
        lua_pushnumber(L, static_cast<double>(required));
        lua_pushnumber(L, static_cast<double>(have));
        return 5;
    }

    // =========================================================================
    // 4. Unit Callbacks
    // =========================================================================

    static int Script_Duskhaven_GetUnitDisplayID(lua_State* L)
    {
        if (!lua_isstring(L, 1))
            return 0;

        const char* name = lua_tostring(L, 1);
        if (!name || !name[0])
            return 0;

        CGUnit_C* unit = GetUnitFromName(name);
        if (!unit || !unit->m_unit)
            return 0;

        uint32_t displayId = unit->m_unit->m_unitFieldDisplayid;
        if (displayId == 0 || displayId >= 0x80000000u)
            return 0;

        lua_pushnumber(L, displayId);
        return 1;
    }

    // =========================================================================
    // 5. Config (DHConfig) System
    // =========================================================================

    namespace
    {
        struct DHConfigState
        {
            bool initialized = false;
            std::filesystem::path path;
            std::unordered_map<std::string, std::string> values;
            std::unordered_map<std::string, std::string> defaults;
            std::mutex mutex;
        };

        static DHConfigState& GetDHConfigState()
        {
            static DHConfigState state;
            return state;
        }

        static std::filesystem::path ResolveDHConfigPath()
        {
            char buffer[MAX_PATH] = {};
            DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
                return std::filesystem::path(buffer).parent_path() / "WTF" / "DHConfig";

            return std::filesystem::current_path() / "WTF" / "DHConfig";
        }

        static void LoadDHConfigUnlocked()
        {
            DHConfigState& state = GetDHConfigState();
            state.values.clear();
            state.path = ResolveDHConfigPath();

            std::ifstream input(state.path);
            if (!input.is_open())
                return;

            std::string line;
            while (std::getline(input, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty())
                    continue;

                size_t sep = line.find('=');
                if (sep != std::string::npos)
                {
                    std::string key = line.substr(0, sep);
                    std::string val = line.substr(sep + 1);
                    state.values[key] = val;
                }
            }
        }

        static void EnsureDHConfigInitialized()
        {
            DHConfigState& state = GetDHConfigState();
            if (state.initialized)
                return;

            LoadDHConfigUnlocked();
            state.defaults["allowDiscord"] = "1";
            state.defaults["terrainReplacementEnabled"] = "0";
            state.initialized = true;
        }

        static void PersistDHConfigUnlocked()
        {
            DHConfigState& state = GetDHConfigState();
            std::error_code ec;
            std::filesystem::create_directories(state.path.parent_path(), ec);

            std::ofstream output(state.path, std::ios::out | std::ios::trunc);
            if (!output.is_open())
                return;

            for (const auto& [k, v] : state.values)
            {
                output << k << "=" << v << "\n";
            }
        }

        static std::string DHConfigReadStringHelper(const std::string& key, const std::string& def)
        {
            std::lock_guard<std::mutex> lock(GetDHConfigState().mutex);
            EnsureDHConfigInitialized();

            auto it = GetDHConfigState().values.find(key);
            if (it != GetDHConfigState().values.end())
                return it->second;

            auto dit = GetDHConfigState().defaults.find(key);
            if (dit != GetDHConfigState().defaults.end())
                return dit->second;

            return def;
        }

        static void DHConfigWriteHelper(const std::string& key, const std::string& val)
        {
            std::lock_guard<std::mutex> lock(GetDHConfigState().mutex);
            EnsureDHConfigInitialized();
            GetDHConfigState().values[key] = val;
            PersistDHConfigUnlocked();
        }

        static void SerializeTableToLua(lua_State* L, int idx, std::string& out)
        {
            if (idx < 0) idx = lua_gettop(L) + idx + 1;
            out += "{";
            lua_pushnil(L);
            bool first = true;
            while (lua_next(L, idx) != 0)
            {
                if (!first) out += ",";
                first = false;

                int kt = lua_type(L, -2);
                if (kt == LUA_TSTRING)
                {
                    out += "[\"";
                    const char* ks = lua_tostring(L, -2);
                    for (; *ks; ++ks) {
                        if (*ks == '"' || *ks == '\\') out += '\\';
                        out += *ks;
                    }
                    out += "\"]=";
                }
                else if (kt == LUA_TNUMBER)
                {
                    out += "[";
                    out += std::to_string(lua_tonumber(L, -2));
                    out += "]=";
                }
                else
                {
                    lua_pop(L, 1);
                    continue;
                }

                int vt = lua_type(L, -1);
                if (vt == LUA_TBOOLEAN)
                {
                    out += lua_toboolean(L, -1) ? "true" : "false";
                }
                else if (vt == LUA_TNUMBER)
                {
                    out += std::to_string(lua_tonumber(L, -1));
                }
                else if (vt == LUA_TSTRING)
                {
                    out += "\"";
                    const char* vs = lua_tostring(L, -1);
                    for (; *vs; ++vs) {
                        if (*vs == '"' || *vs == '\\') out += '\\';
                        out += *vs;
                    }
                    out += "\"";
                }
                else if (vt == LUA_TTABLE)
                {
                    SerializeTableToLua(L, -1, out);
                }
                else
                {
                    out += "nil";
                }
                lua_pop(L, 1);
            }
            out += "}";
        }
    } // namespace

    static int Script_DHConfigReadString(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        const char* def = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
        std::string val = DHConfigReadStringHelper(key, def);
        lua_pushstring(L, val.c_str());
        return 1;
    }

    static int Script_DHConfigReadInt(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        int32_t def     = static_cast<int32_t>(lua_tonumber(L, 2));
        std::string val = DHConfigReadStringHelper(key, "");
        if (val.empty())
        {
            lua_pushnumber(L, def);
            return 1;
        }
        char* end = nullptr;
        long res = std::strtol(val.c_str(), &end, 10);
        lua_pushnumber(L, end != val.c_str() ? res : def);
        return 1;
    }

    static int Script_DHConfigReadFloat(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        float def       = static_cast<float>(lua_tonumber(L, 2));
        std::string val = DHConfigReadStringHelper(key, "");
        if (val.empty())
        {
            lua_pushnumber(L, def);
            return 1;
        }
        char* end = nullptr;
        float res = std::strtof(val.c_str(), &end);
        lua_pushnumber(L, end != val.c_str() ? res : def);
        return 1;
    }

    static int Script_DHConfigWrite(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        const char* val = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
        DHConfigWriteHelper(key, val);
        return 0;
    }

    static int Script_DHConfigWriteTable(lua_State* L)
    {
        if (lua_gettop(L) < 2 || !lua_isstring(L, 1) || !lua_istable(L, 2))
            return 0;

        std::string key = lua_tostring(L, 1);
        std::string serialized;
        SerializeTableToLua(L, 2, serialized);
        DHConfigWriteHelper(key, serialized);
        return 0;
    }

    static int Script_DHConfigReadTable(lua_State* L)
    {
        if (!lua_isstring(L, 1))
        {
            if (lua_gettop(L) >= 2)
                lua_pushvalue(L, 2);
            else
                lua_pushnil(L);
            return 1;
        }

        std::string key = lua_tostring(L, 1);
        std::string serialized = DHConfigReadStringHelper(key, "");
        if (serialized.empty())
        {
            if (lua_gettop(L) >= 2)
                lua_pushvalue(L, 2);
            else
                lua_pushnil(L);
            return 1;
        }

        std::string chunk = "return " + serialized;
        if (luaL_loadbuffer(L, chunk.c_str(), chunk.size(), key.c_str()) != 0 ||
            lua_pcall(L, 0, 1, 0) != 0)
        {
            if (lua_gettop(L) >= 2)
                lua_pushvalue(L, 2);
            else
                lua_pushnil(L);
            return 1;
        }

        return 1;
    }

    static int Script_DHConfigAddDefault(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        const char* def = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
        std::lock_guard<std::mutex> lock(GetDHConfigState().mutex);
        EnsureDHConfigInitialized();
        GetDHConfigState().defaults[key] = def;
        return 0;
    }

    static int Script_DHConfigWriteDefaults(lua_State* L)
    {
        std::lock_guard<std::mutex> lock(GetDHConfigState().mutex);
        EnsureDHConfigInitialized();
        bool changed = false;
        for (const auto& [k, v] : GetDHConfigState().defaults)
        {
            if (GetDHConfigState().values.find(k) == GetDHConfigState().values.end())
            {
                GetDHConfigState().values[k] = v;
                changed = true;
            }
        }
        if (changed) PersistDHConfigUnlocked();
        return 0;
    }

    static int Script_DHConfigHas(lua_State* L)
    {
        const char* key = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        std::lock_guard<std::mutex> lock(GetDHConfigState().mutex);
        EnsureDHConfigInitialized();
        bool has = GetDHConfigState().values.find(key) != GetDHConfigState().values.end() ||
                   GetDHConfigState().defaults.find(key) != GetDHConfigState().defaults.end();
        lua_pushboolean(L, has ? 1 : 0);
        return 1;
    }

    // =========================================================================
    // 6. Display, Camera & World Space
    // =========================================================================

    static int Script_Duskhaven_GetMonitorCount(lua_State* L)
    {
        lua_pushnumber(L, 1);
        return 1;
    }

    static int Script_Duskhaven_GetCurrentMonitorSelection(lua_State* L)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    static bool IsFinitePosition(float x, float y, float z)
    {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    static bool TryGetActivePlayerPosition(C3Vector& out)
    {
        WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
        if (!activePlayer)
            return false;

        CGUnit_C* activeObjectPtr = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(activePlayer, TYPEMASK_UNIT, __FILE__, __LINE__));
        if (!activeObjectPtr)
            return false;

        out = activeObjectPtr->GetPosition();
        return IsFinitePosition(out.x, out.y, out.z);
    }

    static int Script_ConvertCoordsToScreenSpace(lua_State* L)
    {
        float ox = static_cast<float>(lua_tonumber(L, 1));
        float oy = static_cast<float>(lua_tonumber(L, 2));
        float oz = static_cast<float>(lua_tonumber(L, 3));

        if (!CGWorldFrame::s_currentWorldFrame || !IsFinitePosition(ox, oy, oz))
        {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
            return 3;
        }

        C3Vector pos3d = { ox, oy, oz };
        C3Vector pos2d = {};
        uint32_t flags = 0;
        bool result = CGWorldFrame::s_currentWorldFrame->GetScreenCoordinates(pos3d, &pos2d, &flags);
        if (!result || !std::isfinite(pos2d.x) || !std::isfinite(pos2d.y) || !std::isfinite(pos2d.z))
        {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
            return 3;
        }

        float screenX = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * pos2d.x);
        float screenY = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * pos2d.y);

        lua_pushnumber(L, screenX);
        lua_pushnumber(L, screenY);
        lua_pushnumber(L, pos2d.z);
        return 3;
    }

    static int Script_GetActivePlayerWorldPosition(lua_State* L)
    {
        C3Vector position = {};
        if (!TryGetActivePlayerPosition(position))
        {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
            return 3;
        }

        lua_pushnumber(L, position.x);
        lua_pushnumber(L, position.y);
        lua_pushnumber(L, position.z);
        return 3;
    }

    static int Script_GetDistanceFromActivePlayerToWorldPosition(lua_State* L)
    {
        float ox = static_cast<float>(lua_tonumber(L, 1));
        float oy = static_cast<float>(lua_tonumber(L, 2));
        float oz = static_cast<float>(lua_tonumber(L, 3));

        if (!IsFinitePosition(ox, oy, oz))
        {
            lua_pushnil(L);
            return 1;
        }

        C3Vector playerPosition = {};
        if (!TryGetActivePlayerPosition(playerPosition))
        {
            lua_pushnil(L);
            return 1;
        }

        float dx = ox - playerPosition.x;
        float dy = oy - playerPosition.y;
        float dz = oz - playerPosition.z;
        lua_pushnumber(L, std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
        return 1;
    }

    static int Script_TranslateToMapCoords(lua_State* L)
    {
        uint32_t continentID = static_cast<uint32_t>(lua_tonumber(L, 1));
        C3Vector worldPos = {
            static_cast<float>(lua_tonumber(L, 2)),
            static_cast<float>(lua_tonumber(L, 3)),
            static_cast<float>(lua_tonumber(L, 4))
        };
        float mapX = 0.0f;
        float mapY = 0.0f;

        CGWorldMap::GetWorldPosition(worldPos, continentID, &mapX, &mapY, nullptr, false, 0);

        lua_pushnumber(L, mapX);
        lua_pushnumber(L, mapY);
        return 2;
    }

    static void PrintChatMessage(const char* text)
    {
        CGChat::AddChatMessage(text, CHAT_MSG_SYSTEM, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, nullptr);
    }

    static int Script_ReloadMap(lua_State* L)
    {
        char buffer[512];
        WOWGUID activePlayer = ClntObjMgrGetActivePlayer();
        if (activePlayer)
        {
            int32_t mapId = CGGameUI::m_iCurrentMapID;
            CGUnit_C* player = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(activePlayer, TYPEMASK_UNIT, __FILE__, __LINE__));
            if (player && mapId >= 0)
            {
                MapRec* row = g_mapDB.GetRecord(mapId);
                if (row)
                {
                    C3Vector pos = player->GetPosition();
                    World::UnloadMap();
                    CWorld::LoadMap(row->m_directory, &pos, mapId, 0);

                    SStrPrintf(buffer, sizeof(buffer), "Map ID: %d (Directory: \"%s\", x: %f, y: %f, z: %f) reloaded.",
                        mapId, row->m_directory, pos.x, pos.y, pos.z);
                    PrintChatMessage(buffer);
                    return 0;
                }
            }
        }
        PrintChatMessage("ReloadMap: Failed to reload map.");
        return 0;
    }

    static int Script_ToggleDisplayNormals(lua_State* L)
    {
        CWorld::enables ^= Enable_ShowNormals;
        bool on = (CWorld::enables & Enable_ShowNormals) != 0;
        PrintChatMessage(on ? "Normal display turned on." : "Normal display turned off.");
        return 0;
    }

    static int Script_ToggleGroundEffects(lua_State* L)
    {
        CWorld::enables ^= Enable_DetailDoodads;
        bool on = (CWorld::enables & Enable_DetailDoodads) != 0;
        PrintChatMessage(on ? "Ground clutter shown." : "Ground clutter hidden.");
        return 0;
    }

    static int Script_ToggleLiquids(lua_State* L)
    {
        CWorld::enables ^= (Enable_Water | Enable_Particulates);
        bool on = (CWorld::enables & (Enable_Water | Enable_Particulates)) != 0;
        PrintChatMessage(on ? "Liquids shown." : "Liquids hidden.");
        return 0;
    }

    static int Script_ToggleM2(lua_State* L)
    {
        CWorld::enables ^= Enable_Doodads;
        bool on = (CWorld::enables & Enable_Doodads) != 0;
        PrintChatMessage(on ? "Client-side M2s shown." : "Client-side M2s hidden.");
        return 0;
    }

    static int Script_ToggleTerrain(lua_State* L)
    {
        CWorld::enables ^= Enable_Chunks;
        bool on = (CWorld::enables & Enable_Chunks) != 0;
        PrintChatMessage(on ? "Terrain chunks shown." : "Terrain chunks hidden.");
        return 0;
    }

    static int Script_ToggleTerrainCulling(lua_State* L)
    {
        CWorld::enables ^= Enable_Culling;
        bool on = (CWorld::enables & Enable_Culling) != 0;
        PrintChatMessage(on ? "Terrain culling enabled." : "Terrain culling disabled.");
        return 0;
    }

    static int Script_ToggleWireframeMode(lua_State* L)
    {
        CWorld::enables ^= Enable_ShowTris;
        bool on = (CWorld::enables & Enable_ShowTris) != 0;
        PrintChatMessage(on ? "Wireframe mode on." : "Wireframe mode off.");
        return 0;
    }

    static int Script_ToggleWMO(lua_State* L)
    {
        CWorld::enables ^= Enable_MapObjs;
        bool on = (CWorld::enables & Enable_MapObjs) != 0;
        PrintChatMessage(on ? "WMOs shown." : "WMOs hidden.");
        return 0;
    }

    // =========================================================================
    // 7. Model & Creature Display Functions
    // =========================================================================

    namespace
    {
        static bool LooksLikePath(const char* text)
        {
            if (!text) return false;
            for (uint32_t i = 0; i < 240; ++i)
            {
                char const c = text[i];
                if (c == '\0') return i > 0;
                if (c < 0x20 || c > 0x7E) return false;
            }
            return false;
        }

        static void PushOptionalStringField(lua_State* L, const char* key, const char* value)
        {
            if (!LooksLikePath(value)) return;
            lua_pushstring(L, value);
            lua_setfield(L, -2, key);
        }
    } // namespace

    static int Script_Duskhaven_GetCreatureDisplayInfo(lua_State* L)
    {
        uint32_t const displayId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        if (displayId == 0)
            return 0;

        auto* display = g_creatureDisplayInfoDB.GetRecord(displayId);
        if (!display || display->m_modelID == 0)
            return 0;

        auto* model = g_creatureModelDataDB.GetRecord(display->m_modelID);
        if (!model || !model->m_modelName || !LooksLikePath(model->m_modelName))
            return 0;

        lua_createtable(L, 0, 6);
        PushOptionalStringField(L, "ModelPath", model->m_modelName);
        PushOptionalStringField(L, "Texture1", display->m_textureVariation[0]);
        PushOptionalStringField(L, "Texture2", display->m_textureVariation[1]);
        PushOptionalStringField(L, "Texture3", display->m_textureVariation[2]);
        lua_pushnumber(L, display->m_creatureModelScale);
        lua_setfield(L, -2, "Scale");
        lua_pushnumber(L, display->m_modelID);
        lua_setfield(L, -2, "ModelId");
        return 1;
    }

    static int Script_Duskhaven_GetModelPath(lua_State* L)
    {
        if (lua_type(L, 1) != LUA_TTABLE)
            return 0;

        lua_rawgeti(L, 1, 0);
        CScriptObject* object = static_cast<CScriptObject*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        if (!object)
            return 0;

        if (object->IsA(CSimpleModel::GetObjectType()))
        {
            CSimpleModel* modelFrame = static_cast<CSimpleModel*>(object);
            if (modelFrame->m_model && modelFrame->m_model->m_shared)
            {
                const char* path = modelFrame->m_model->m_shared->m_fileName;
                if (path && LooksLikePath(path))
                {
                    lua_pushstring(L, path);
                    return 1;
                }
            }
        }
        return 0;
    }

    static int Script_Duskhaven_SetModelDisplay(lua_State* L)
    {
        uint32_t const displayId = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);
        if (displayId == 0 || lua_type(L, 1) != LUA_TTABLE)
            return 0;

        lua_rawgeti(L, 1, 0);
        CScriptObject* object = static_cast<CScriptObject*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        if (!object)
            return 0;

        bool applied = false;
        if (object->IsA(CGCharacterModelBase::GetObjectType()))
        {
            CGCharacterModelBase* charModel = static_cast<CGCharacterModelBase*>(object);
            CreatureStats stats{};
            stats.m_displayId[0] = displayId;
            charModel->SetUpModel(reinterpret_cast<CreatureStats_C*>(&stats));
            applied = true;
        }

        lua_createtable(L, 0, 1);
        lua_pushboolean(L, applied ? 1 : 0);
        lua_setfield(L, -2, "Applied");
        return 1;
    }

    // =========================================================================
    // 8. Housing Functions
    // =========================================================================

    static WOWGUID s_selectedGobGuid{0};

    static int Script_GetMouseWorldPosition(lua_State* L)
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        CustomLuaManager::Get().GetLastHitPoint(x, y, z);
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        lua_pushnumber(L, z);
        return 3;
    }

    static int Script_GetLastMouseoverGUID(lua_State* L)
    {
        uint64_t full = CustomLuaManager::Get().GetLastHitGuid();
        uint16_t type = uint16_t(full >> 48);
        uint32_t entry = uint32_t((full >> 24) & 0xFFFFFF);
        uint32_t low = uint32_t(full & 0xFFFFFF);

        lua_pushstring(L, std::to_string(full).c_str());
        lua_pushnumber(L, entry);
        lua_pushnumber(L, low);
        lua_pushnumber(L, type);
        return 4;
    }

    static int Script_ClearSelectedGob(lua_State* L)
    {
        s_selectedGobGuid = WOWGUID{0};
        return 0;
    }

    static int Script_SelectEditorGobByMouse(lua_State* L)
    {
        uint64_t guid = CustomLuaManager::Get().GetLastHitGuid();
        if (guid == 0) return 0;

        s_selectedGobGuid = WOWGUID{guid};
        CGObject_C* obj = ClntObjMgrObjectPtr(s_selectedGobGuid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
        if (!obj) return 0;

        CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
        C3Vector pos = gob->GetPosition();
        lua_pushstring(L, std::to_string(guid).c_str());
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 4;
    }

    static int Script_GetSelectedGobGUID(lua_State* L)
    {
        lua_pushstring(L, std::to_string(s_selectedGobGuid.m_guid).c_str());
        return 1;
    }

    static int Script_GetSelectedGobPosition(lua_State* L)
    {
        CGObject_C* obj = ClntObjMgrObjectPtr(s_selectedGobGuid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
        if (!obj) return 0;

        CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
        C3Vector pos = gob->GetPosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3;
    }

    static int Script_GetSelectedGobRotation(lua_State* L)
    {
        CGObject_C* obj = ClntObjMgrObjectPtr(s_selectedGobGuid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
        if (!obj) return 0;

        CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
        C4Quaternion q = gob->GetRotation();
        lua_pushnumber(L, q.x);
        lua_pushnumber(L, q.y);
        lua_pushnumber(L, q.z);
        lua_pushnumber(L, q.w);
        return 4;
    }

    static int Script_SnapSelectedGobToGround(lua_State* L)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_SetGobPositionByGUID(lua_State* L)
    {
        uint64_t guidVal = 0;
        if (lua_isstring(L, 1))
        {
            guidVal = std::strtoull(lua_tolstring(L, 1, 0), nullptr, 10);
        }
        else if (lua_isnumber(L, 1))
        {
            guidVal = static_cast<uint64_t>(lua_tonumber(L, 1));
        }

        WOWGUID guid{guidVal};
        CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
        if (!obj)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
        float x = static_cast<float>(lua_tonumber(L, 2));
        float y = static_cast<float>(lua_tonumber(L, 3));
        float z = static_cast<float>(lua_tonumber(L, 4));

        gob->m_passenger.m_position = {x, y, z};
        gob->UpdateWorldObject(0);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Script_SetGobRotationByGUID(lua_State* L)
    {
        uint64_t guidVal = 0;
        if (lua_isstring(L, 1))
        {
            guidVal = std::strtoull(lua_tolstring(L, 1, 0), nullptr, 10);
        }
        else if (lua_isnumber(L, 1))
        {
            guidVal = static_cast<uint64_t>(lua_tonumber(L, 1));
        }

        WOWGUID guid{guidVal};
        CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__);
        if (!obj)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        CGGameObject_C* gob = static_cast<CGGameObject_C*>(obj);
        float x = static_cast<float>(lua_tonumber(L, 2));
        float y = static_cast<float>(lua_tonumber(L, 3));
        float z = static_cast<float>(lua_tonumber(L, 4));
        float w = static_cast<float>(lua_tonumber(L, 5));

        C4Quaternion q;
        q.x = x;
        q.y = y;
        q.z = z;
        q.w = w;
        *reinterpret_cast<C4QuaternionCompressed*>(&gob->m_passenger.m_facing) = q;
        gob->UpdateWorldObject(0);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Script_LogMouseoverGobValues(lua_State* L)
    {
        return 0;
    }

    // =========================================================================
    // 9. Character Creation Functions
    // =========================================================================

    namespace
    {
        static CM2Model* GetCharacterCreateModel()
        {
            if (!CCharacterCreation::m_character)
                return nullptr;
            return CCharacterCreation::m_character->m_data.m_model;
        }

        static const char* GetAnimationName(uint32_t animationId)
        {
            AnimationDataRec* row = g_animationDataDB.GetRecord(animationId);
            if (row && row->m_name && row->m_name[0])
                return row->m_name;
            return "Unknown";
        }

        static std::string GetSoundEntryPath(uint32_t soundId)
        {
            SoundEntriesRec* row = g_soundEntriesDB.GetRecord(soundId);
            if (!row) return {};

            for (int i = 0; i < 10; ++i)
            {
                const char* file = row->m_file[i];
                if (!file || !file[0]) continue;
                return (row->m_directoryBase && row->m_directoryBase[0]) ?
                    std::string(row->m_directoryBase) + "\\" + file : std::string(file);
            }
            return {};
        }

        static std::vector<uint32_t> s_charCreateModelAttachments;
        static std::vector<uint32_t> s_charCreateSpellEffectAttachments;

        static void ClearCharacterCreateSpellVisuals(CM2Model* model)
        {
            if (!model || !model->IsLoaded(0, 0)) return;
            for (uint32_t const att : s_charCreateSpellEffectAttachments)
                model->DetachAllChildrenById(att);
            s_charCreateSpellEffectAttachments.clear();
        }

        static bool ClearCharacterCreateModelAttachmentsInternal()
        {
            CM2Model* model = GetCharacterCreateModel();
            if (!model || !model->IsLoaded(0, 0)) return false;
            for (uint32_t const att : s_charCreateModelAttachments)
                if (model->HasAttachment(att))
                    model->DetachAllChildrenById(att);
            s_charCreateModelAttachments.clear();
            return true;
        }
    } // namespace

    static int Script_GetHairStyleSelection(lua_State* L)
    {
        if (!CCharacterCreation::m_character) return 0;
        lua_pushnumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId);
        return 1;
    }

    static int Script_GetHairColorSelection(lua_State* L)
    {
        if (!CCharacterCreation::m_character) return 0;
        lua_pushnumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId);
        return 1;
    }

    static int Script_GetFacialHairSelection(lua_State* L)
    {
        if (!CCharacterCreation::m_character) return 0;
        lua_pushnumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId);
        return 1;
    }

    static int Script_GetSkinColorSelection(lua_State* L)
    {
        if (!CCharacterCreation::m_character) return 0;
        lua_pushnumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_skinID);
        return 1;
    }

    static int Script_GetFaceSelection(lua_State* L)
    {
        if (!CCharacterCreation::m_character) return 0;
        lua_pushnumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_faceId);
        return 1;
    }

    static int Script_PlayCharacterCreateAnimation(lua_State* L)
    {
        const int animationId = static_cast<int>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : -1);
        CM2Model* model = GetCharacterCreateModel();
        if (model && animationId >= 0 && model->IsLoaded(0, 0))
        {
            model->SetBoneSequence(static_cast<uint32_t>(-1), animationId, 0, 0, 1.0f, 1, 1);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_PlayCharacterCreateAnimationChain(lua_State* L)
    {
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Script_UpdateCharacterCreateAnimationChain(lua_State* L)
    {
        return 0;
    }

    static int Script_GetCharacterCreateAnimations(lua_State* L)
    {
        CM2Model* model = GetCharacterCreateModel();
        if (!model || !model->IsLoaded(0, 0) || !model->m_shared || !model->m_shared->m_data)
        {
            lua_createtable(L, 0, 0);
            return 1;
        }

        const unsigned int maximumAnimationId = static_cast<unsigned int>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 4096);
        const int selectedAnimationId         = static_cast<int>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : -1);
        lua_createtable(L, 0, 0);

        int resultIndex = 1;
        for (unsigned int animationId = 0; animationId <= maximumAnimationId; ++animationId)
        {
            if (!model->HasSequence(animationId) && static_cast<int>(animationId) != selectedAnimationId)
                continue;

            char const* animationName = GetAnimationName(animationId);

            lua_pushnumber(L, resultIndex++);
            lua_createtable(L, 0, 2);
            lua_pushnumber(L, animationId);
            lua_setfield(L, -2, "id");
            lua_pushstring(L, animationName ? animationName : "Unknown");
            lua_setfield(L, -2, "name");
            lua_rawset(L, -3);
        }

        return 1;
    }

    static int Script_PlayCharacterCreateSpellVisual(lua_State* L)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    static int Script_GetCharacterCreateSpellStageSoundPath(lua_State* L)
    {
        lua_pushstring(L, "");
        return 1;
    }

    static int Script_GetCharacterCreateSpellCastInfo(lua_State* L)
    {
        uint32_t const spellId = static_cast<uint32_t>(lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0);
        SpellRec spellRec;
        SpellRec* spell = (spellId && g_spellDB.GetRecord(spellId, &spellRec)) ? &spellRec : nullptr;
        if (!spell)
        {
            lua_createtable(L, 0, 0);
            return 1;
        }

        lua_createtable(L, 0, 24);
        int32_t const castTime = SpellRec::GetCastTime(spell, 0, 0, 1);
        lua_pushnumber(L, castTime > 0 ? castTime : 0);
        lua_setfield(L, -2, "castTime");
        lua_pushboolean(L, (spell->m_attributesEx & 0xC) != 0);
        lua_setfield(L, -2, "channeled");
        lua_pushnumber(L, spell->m_speed);
        lua_setfield(L, -2, "missileSpeed");
        return 1;
    }

    static int Script_GetCharacterCreateSpellPreviewInfo(lua_State* L)
    {
        lua_createtable(L, 0, 0);
        return 1;
    }

    static int Script_PreviewCharacterCreateSpellAura(lua_State* L)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_ClearCharacterCreateAuraPreview(lua_State* L)
    {
        return 0;
    }

    static int Script_SetCharacterCreateSpellPreviewTargetOffset(lua_State* L)
    {
        return 0;
    }

    static int Script_GetCharacterCreateSceneDebugInfo(lua_State* L)
    {
        lua_createtable(L, 0, 0);
        return 1;
    }

    static int Script_ShowCharacterCreateSpellPreviewPath(lua_State* L)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    static int Script_StartCharacterCreateSpellMissile(lua_State* L)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    static int Script_UpdateCharacterCreateSpellPreview(lua_State* L)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_ClearCharacterCreateSpellPreview(lua_State* L)
    {
        return 0;
    }

    static int Script_ClearCharacterCreateSpellVisuals(lua_State* L)
    {
        ClearCharacterCreateSpellVisuals(GetCharacterCreateModel());
        return 0;
    }

    static int Script_ClearCharacterCreateModelAttachments(lua_State* L)
    {
        lua_pushboolean(L, ClearCharacterCreateModelAttachmentsInternal() ? 1 : 0);
        return 1;
    }

    static int Script_SetCharacterCreateWeaponsHidden(lua_State* L)
    {
        bool hidden = lua_toboolean(L, 1) != 0;
        if (!hidden)
        {
            lua_pushboolean(L, 1);
            return 1;
        }

        CM2Model* model = GetCharacterCreateModel();
        if (model && model->IsLoaded(0, 0))
        {
            for (uint32_t const attachmentId : {1u, 2u})
                if (model->HasAttachment(attachmentId))
                    model->DetachAllChildrenById(attachmentId);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Script_SetCharacterCreateEquipmentHidden(lua_State* L)
    {
        bool hidden = lua_toboolean(L, 1) != 0;
        if (!hidden)
        {
            lua_pushboolean(L, 1);
            return 1;
        }

        CM2Model* model = GetCharacterCreateModel();
        if (model && model->IsLoaded(0, 0))
        {
            for (uint32_t attachmentId = 0; attachmentId <= 12; ++attachmentId)
                if (model->HasAttachment(attachmentId))
                    model->DetachAllChildrenById(attachmentId);
        }
        s_charCreateModelAttachments.clear();
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Script_AttachCharacterCreateModel(lua_State* L)
    {
        std::string const path = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
        uint32_t const attachmentId = static_cast<uint32_t>(lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0);

        CM2Model* model = GetCharacterCreateModel();
        if (!model || !model->IsLoaded(0, 0) || path.empty() || !model->m_scene)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        CM2Model* child = model->m_scene->CreateModel(path.c_str(), 0);
        if (!child)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        child->AttachToParent(model, attachmentId, nullptr, 0);
        child->Release();
        s_charCreateModelAttachments.push_back(attachmentId);
        lua_pushboolean(L, 1);
        return 1;
    }

    // =========================================================================
    // 10. Fallbacks / Stubs
    // =========================================================================

    static int Script_CDBC_Stub(lua_State* L)
    {
        return 0;
    }

    static int Script_DiscordStub(lua_State* L)
    {
        return 0;
    }

    static int Script_StreamingStub(lua_State* L)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    static int Script_GetMPQHashResults(lua_State* L)
    {
        lua_pushstring(L, "");
        return 1;
    }

    // =========================================================================
    // Register All Bindings
    // =========================================================================

    void CustomLuaManager::RegisterLuaBindings(lua_State* L)
    {
        int top = L ? lua_gettop(L) : 0;

        // Player & Spec & Stats
        FrameScript_RegisterFunction("GetShapeshiftFormID", Script_GetShapeshiftFormID);
        FrameScript_RegisterFunction("GetActiveSpec", Script_GetActiveSpec);
        FrameScript_RegisterFunction("SetActiveSpec", Script_SetActiveSpec);
        FrameScript_RegisterFunction("GetMasteryRating", Script_GetMasteryRating);
        FrameScript_RegisterFunction("SetMasteryRatings", Script_SetMasteryRatings);
        FrameScript_RegisterFunction("UpdateMasteryAmount", Script_UpdateMasteryAmount);
        FrameScript_RegisterFunction("UpdateChargeInfoForSpell", Script_UpdateChargeInfoForSpell);
        FrameScript_RegisterFunction("GetChargeInfoForSpell", Script_GetChargeInfoForSpell);
        FrameScript_RegisterFunction("GetCombatRatingMult", Script_GetCombatRatingMult);
        FrameScript_RegisterFunction("GetCombatRatingScalar", Script_GetCombatRatingScalar);
        FrameScript_RegisterFunction("GetModCastSpeed", Script_GetModCastSpeed);
        FrameScript_RegisterFunction("GetWeaponAP", Script_GetWeaponAP);
        FrameScript_RegisterFunction("GetPetSP", Script_GetPetSP);
        FrameScript_RegisterFunction("GetSpeedPercent", Script_GetSpeedPercent);
        FrameScript_RegisterFunction("GetLeechPercent", Script_GetLeechPercent);
        FrameScript_RegisterFunction("GetAvoidancePercent", Script_GetAvoidancePercent);

        // Action Bar & UI
        FrameScript_RegisterFunction("FireActionBarSlotUpdateEvent", Script_FireActionBarSlotUpdateEvent);
        FrameScript_RegisterFunction("FireTalentUpdateEvent", Script_FireTalentUpdateEvent);
        FrameScript_RegisterFunction("FlashGameWindow", Script_FlashGameWindow);
        FrameScript_RegisterFunction("FindSpellActionBarSlot", Script_FindSpellActionBarSlot);
        FrameScript_RegisterFunction("ReplaceActionBarSpell", Script_ReplaceActionBarSpell);
        FrameScript_RegisterFunction("SetSpellInActionBarSlot", Script_SetSpellInActionBarSlot);
        FrameScript_RegisterFunction("SetFrameChildClipping", Script_SetFrameChildClipping);
        FrameScript_RegisterFunction("GetFrameChildClipping", Script_GetFrameChildClipping);
        FrameScript_RegisterFunction("SetTextureWithMask", Script_SetTextureWithMask);
        FrameScript_RegisterFunction("SetPortraitToTexture", Script_SetPortraitToTexture);
        FrameScript_RegisterFunction("EmitFrameEventByName", Script_EmitFrameEventByName);

        // Spell & Profession
        FrameScript_RegisterFunction("GetSpellDescription", Script_GetSpellDescription);
        FrameScript_RegisterFunction("GetSpellNameById", Script_GetSpellNameById);
        FrameScript_RegisterFunction("GetSpellIconById", Script_GetSpellIconById);
        FrameScript_RegisterFunction("SpellRequiresComboPoints", Script_SpellRequiresComboPoints);
        FrameScript_RegisterFunction("UnitCustomCastingData", Script_UnitCustomCastingData);
        FrameScript_RegisterFunction("PreloadProfessionItemCache", Script_PreloadProfessionItemCache);
        FrameScript_RegisterFunction("GetProfessionRecipeCount", Script_GetProfessionRecipeCount);
        FrameScript_RegisterFunction("GetProfessionRecipeSpellId", Script_GetProfessionRecipeSpellId);
        FrameScript_RegisterFunction("GetProfessionRecipeInfo", Script_GetProfessionRecipeInfo);
        FrameScript_RegisterFunction("GetProfessionRecipeRequirements", Script_GetProfessionRecipeRequirements);
        FrameScript_RegisterFunction("GetProfessionRecipeReagentCount", Script_GetProfessionRecipeReagentCount);
        FrameScript_RegisterFunction("GetProfessionRecipeReagentInfo", Script_GetProfessionRecipeReagentInfo);

        // Unit
        FrameScript_RegisterFunction("Duskhaven_GetUnitDisplayID", Script_Duskhaven_GetUnitDisplayID);

        // Config (DHConfig)
        FrameScript_RegisterFunction("DHConfigReadString", Script_DHConfigReadString);
        FrameScript_RegisterFunction("DHConfigRead", Script_DHConfigReadString);
        FrameScript_RegisterFunction("DHConfigReadInt", Script_DHConfigReadInt);
        FrameScript_RegisterFunction("DHConfigReadFloat", Script_DHConfigReadFloat);
        FrameScript_RegisterFunction("DHConfigWrite", Script_DHConfigWrite);
        FrameScript_RegisterFunction("DHConfigWriteTable", Script_DHConfigWriteTable);
        FrameScript_RegisterFunction("DHConfigReadTable", Script_DHConfigReadTable);
        FrameScript_RegisterFunction("DHConfigAddDefault", Script_DHConfigAddDefault);
        FrameScript_RegisterFunction("DHConfigWriteDefaults", Script_DHConfigWriteDefaults);
        FrameScript_RegisterFunction("DHConfigHas", Script_DHConfigHas);

        // Display, Camera & World
        FrameScript_RegisterFunction("Duskhaven_GetMonitorCount", Script_Duskhaven_GetMonitorCount);
        FrameScript_RegisterFunction("Duskhaven_GetCurrentMonitorSelection", Script_Duskhaven_GetCurrentMonitorSelection);
        FrameScript_RegisterFunction("ConvertCoordsToScreenSpace", Script_ConvertCoordsToScreenSpace);
        FrameScript_RegisterFunction("GetActivePlayerWorldPosition", Script_GetActivePlayerWorldPosition);
        FrameScript_RegisterFunction("GetDistanceFromActivePlayerToWorldPosition", Script_GetDistanceFromActivePlayerToWorldPosition);
        FrameScript_RegisterFunction("TranslateToMapCoords", Script_TranslateToMapCoords);
        FrameScript_RegisterFunction("ReloadMap", Script_ReloadMap);
        FrameScript_RegisterFunction("ToggleDisplayNormals", Script_ToggleDisplayNormals);
        FrameScript_RegisterFunction("ToggleGroundEffects", Script_ToggleGroundEffects);
        FrameScript_RegisterFunction("ToggleLiquids", Script_ToggleLiquids);
        FrameScript_RegisterFunction("ToggleM2", Script_ToggleM2);
        FrameScript_RegisterFunction("ToggleTerrain", Script_ToggleTerrain);
        FrameScript_RegisterFunction("ToggleTerrainCulling", Script_ToggleTerrainCulling);
        FrameScript_RegisterFunction("ToggleWireframeMode", Script_ToggleWireframeMode);
        FrameScript_RegisterFunction("ToggleWMO", Script_ToggleWMO);

        // Model & Creature
        FrameScript_RegisterFunction("Duskhaven_GetCreatureDisplayInfo", Script_Duskhaven_GetCreatureDisplayInfo);
        FrameScript_RegisterFunction("Duskhaven_GetModelPath", Script_Duskhaven_GetModelPath);
        FrameScript_RegisterFunction("Duskhaven_SetModelDisplay", Script_Duskhaven_SetModelDisplay);

        // Housing
        FrameScript_RegisterFunction("GetMouseWorldPosition", Script_GetMouseWorldPosition);
        FrameScript_RegisterFunction("GetLastMouseoverGUID", Script_GetLastMouseoverGUID);
        FrameScript_RegisterFunction("ClearSelectedGob", Script_ClearSelectedGob);
        FrameScript_RegisterFunction("SelectEditorGobByMouse", Script_SelectEditorGobByMouse);
        FrameScript_RegisterFunction("GetSelectedGobGUID", Script_GetSelectedGobGUID);
        FrameScript_RegisterFunction("GetSelectedGobPosition", Script_GetSelectedGobPosition);
        FrameScript_RegisterFunction("SnapSelectedGobToGround", Script_SnapSelectedGobToGround);
        FrameScript_RegisterFunction("GetSelectedGobRotation", Script_GetSelectedGobRotation);
        FrameScript_RegisterFunction("SetGobPositionByGUID", Script_SetGobPositionByGUID);
        FrameScript_RegisterFunction("SetGobRotationByGUID", Script_SetGobRotationByGUID);
        FrameScript_RegisterFunction("LogMouseoverGobValues", Script_LogMouseoverGobValues);

        // Character Creation
        FrameScript_RegisterFunction("PlayCharacterCreateAnimation", Script_PlayCharacterCreateAnimation);
        FrameScript_RegisterFunction("PlayCharacterCreateAnimationChain", Script_PlayCharacterCreateAnimationChain);
        FrameScript_RegisterFunction("UpdateCharacterCreateAnimationChain", Script_UpdateCharacterCreateAnimationChain);
        FrameScript_RegisterFunction("GetCharacterCreateAnimations", Script_GetCharacterCreateAnimations);
        FrameScript_RegisterFunction("PlayCharacterCreateSpellVisual", Script_PlayCharacterCreateSpellVisual);
        FrameScript_RegisterFunction("GetCharacterCreateSpellStageSoundPath", Script_GetCharacterCreateSpellStageSoundPath);
        FrameScript_RegisterFunction("GetCharacterCreateSpellCastInfo", Script_GetCharacterCreateSpellCastInfo);
        FrameScript_RegisterFunction("GetCharacterCreateSpellPreviewInfo", Script_GetCharacterCreateSpellPreviewInfo);
        FrameScript_RegisterFunction("PreviewCharacterCreateSpellAura", Script_PreviewCharacterCreateSpellAura);
        FrameScript_RegisterFunction("ClearCharacterCreateAuraPreview", Script_ClearCharacterCreateAuraPreview);
        FrameScript_RegisterFunction("SetCharacterCreateSpellPreviewTargetOffset", Script_SetCharacterCreateSpellPreviewTargetOffset);
        FrameScript_RegisterFunction("GetCharacterCreateSceneDebugInfo", Script_GetCharacterCreateSceneDebugInfo);
        FrameScript_RegisterFunction("ShowCharacterCreateSpellPreviewPath", Script_ShowCharacterCreateSpellPreviewPath);
        FrameScript_RegisterFunction("StartCharacterCreateSpellMissile", Script_StartCharacterCreateSpellMissile);
        FrameScript_RegisterFunction("UpdateCharacterCreateSpellPreview", Script_UpdateCharacterCreateSpellPreview);
        FrameScript_RegisterFunction("ClearCharacterCreateSpellPreview", Script_ClearCharacterCreateSpellPreview);
        FrameScript_RegisterFunction("ClearCharacterCreateSpellVisuals", Script_ClearCharacterCreateSpellVisuals);
        FrameScript_RegisterFunction("ClearCharacterCreateModelAttachments", Script_ClearCharacterCreateModelAttachments);
        FrameScript_RegisterFunction("SetCharacterCreateWeaponsHidden", Script_SetCharacterCreateWeaponsHidden);
        FrameScript_RegisterFunction("SetCharacterCreateEquipmentHidden", Script_SetCharacterCreateEquipmentHidden);
        FrameScript_RegisterFunction("AttachCharacterCreateModel", Script_AttachCharacterCreateModel);
        FrameScript_RegisterFunction("GetHairStyleSelection", Script_GetHairStyleSelection);
        FrameScript_RegisterFunction("GetHairColorSelection", Script_GetHairColorSelection);
        FrameScript_RegisterFunction("GetFacialHairSelection", Script_GetFacialHairSelection);
        FrameScript_RegisterFunction("GetSkinColorSelection", Script_GetSkinColorSelection);
        FrameScript_RegisterFunction("GetFaceSelection", Script_GetFaceSelection);

        // CDBC & Stubs
        FrameScript_RegisterFunction("_CDBC_GET", Script_CDBC_Stub);
        FrameScript_RegisterFunction("GetCDBCRow", Script_CDBC_Stub);
        FrameScript_RegisterFunction("DiscordSetGamePresence", Script_DiscordStub);
        FrameScript_RegisterFunction("DiscordClearGamePresence", Script_DiscordStub);
        FrameScript_RegisterFunction("Duskhaven_GetContentStreamingProgress", Script_StreamingStub);
        FrameScript_RegisterFunction("Duskhaven_AcknowledgeContentStreamingInterfaceUpdate", Script_StreamingStub);
        FrameScript_RegisterFunction("Duskhaven_CancelContentStreaming", Script_StreamingStub);
        FrameScript_RegisterFunction("GetMPQHashResults", Script_GetMPQHashResults);

        if (L)
        {
            lua_settop(L, top);
        }
    }
}
