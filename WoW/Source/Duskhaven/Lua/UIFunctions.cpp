#include "WoW/Source/Duskhaven/Lua/UIFunctions.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/Types.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/Images/tga.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "WoW/Source/UIUtil/PortraitButton.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace Duskhaven
{
    namespace Lua
    {
        namespace
        {
            struct MaskedTextureKey
            {
                std::string source;
                std::string mask;
                uint32_t width  = 0;
                uint32_t height = 0;

                bool operator<(MaskedTextureKey const& other) const
                {
                    if (width != other.width)
                        return width < other.width;
                    if (height != other.height)
                        return height < other.height;
                    if (source != other.source)
                        return source < other.source;
                    return mask < other.mask;
                }
            };

            struct MaskedTextureCacheEntry
            {
                CTexture* texture = nullptr;
                std::vector<CImVector> pixels;
                std::string debugName;
            };

            std::map<MaskedTextureKey, std::unique_ptr<MaskedTextureCacheEntry>> s_maskedTextureCache;

            std::string NormalizeTexturePath(std::string path)
            {
                std::replace(path.begin(), path.end(), '/', '\\');
                std::transform(path.begin(), path.end(), path.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return path;
            }

            std::string AppendExtensionIfMissing(std::string path, std::string const& extension)
            {
                if (path.size() >= extension.size() &&
                    path.compare(path.size() - extension.size(), extension.size(), extension) == 0)
                {
                    return path;
                }
                return path + extension;
            }

            uint8_t SelectMaskByte(uint8_t const* px, bool preferAlpha)
            {
                if (preferAlpha)
                    return px[3];

                uint8_t b = px[0];
                uint8_t g = px[1];
                uint8_t r = px[2];
                return static_cast<uint8_t>((static_cast<uint32_t>(r) + static_cast<uint32_t>(g) + static_cast<uint32_t>(b)) / 3u);
            }

            bool LoadBLPToRGBA(std::string const& path, std::vector<CImVector>& pixels, uint32_t& width, uint32_t& height)
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

            bool LoadMaskAlphaFromTga(std::string const& path, uint32_t pixelCount, std::vector<uint8_t>& alphaOut)
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

            bool LoadMaskAlphaFromBlp(std::string const& path, uint32_t targetWidth, uint32_t targetHeight, std::vector<uint8_t>& alphaOut)
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

            bool LoadMaskAlpha(std::string const& maskBase, uint32_t width, uint32_t height, std::vector<uint8_t>& alphaOut)
            {
                uint32_t pixelCount = width * height;
                bool useSmall       = width <= 64 && height <= 64;

                std::array<std::string, 4> candidates = {
                    useSmall ? maskBase + "Small" : maskBase,
                    useSmall ? maskBase : maskBase + "Small",
                    maskBase,
                    maskBase + "Small"
                };

                for (std::string const& candidate : candidates)
                {
                    if (LoadMaskAlphaFromTga(candidate, pixelCount, alphaOut))
                        return true;
                    if (LoadMaskAlphaFromBlp(candidate, width, height, alphaOut))
                        return true;
                }

                return false;
            }

            void MaskedTextureUpdate(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, void* userArg, uint32_t& texelStrideInBytes, const void*& texels)
            {
                auto* entry = static_cast<MaskedTextureCacheEntry*>(userArg);
                if (!entry)
                    return;

                switch (command)
                {
                    case GxTex_Latch:
                    {
                        texelStrideInBytes = 4 * width;
                        texels = entry->pixels.data();
                        break;
                    }
                    default:
                        break;
                }
            }

            CTexture* CreateMaskedTexture(std::string const& sourceBase, std::string const& maskBase)
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
        } // namespace

        bool ApplyMaskedTexture(CSimpleTexture* texture, const char* source, const char* mask)
        {
            if (!texture || !source || !source[0] || !mask || !mask[0])
                return false;

            CTexture* masked = CreateMaskedTexture(source, mask);
            if (!masked)
                return false;

            texture->SetTexture(masked);
            return true;
        }

        int Script_SetTextureWithMask(lua_State* L)
        {
            CSimpleTexture* texture = ResolveTextureObject(L, 1);
            if (!texture)
            {
                ClientLua::DisplayError(L, "SetTextureWithMask(): expected texture object or texture name");
                return 0;
            }

            std::string source = ClientLua::GetString(L, 2, "");
            std::string mask   = ClientLua::GetString(L, 3, "");
            if (source.empty() || mask.empty())
            {
                ClientLua::DisplayError(L, "SetTextureWithMask(): expected source texture and mask texture");
                return 0;
            }

            if (!ApplyMaskedTexture(texture, source.c_str(), mask.c_str()))
            {
                std::string err = "SetTextureWithMask(): failed to build masked texture from '" + source + "' with mask '" + mask + "'";
                ClientLua::DisplayError(L, err.c_str());
            }

            return 0;
        }

        int Script_SetPortraitToTexture(lua_State* L)
        {
            CSimpleTexture* texture = ResolveTextureObject(L, 1);
            if (!texture)
            {
                ClientLua::DisplayError(L, "SetPortraitToTexture(): expected texture object or texture name");
                return 0;
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

        int Script_SetFrameChildClipping(lua_State* L)
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
                ClientLua::DisplayError(L, "Usage: SetFrameChildClipping(frame, enabled)");
                return 0;
            }

            frame->SetChildClipping(enabled);
            return 0;
        }

        int Script_GetFrameChildClipping(lua_State* L)
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

            ClientLua::PushBoolean(L, frame && frame->GetChildClipping());
            return 1;
        }

        int Script_FireActionBarSlotUpdateEvent(lua_State* L)
        {
            uint8_t slotID = static_cast<uint8_t>(ClientLua::GetNumber(L, 1));
            if (slotID < 144)
                FrameScript_SignalEvent(SCRIPT_EVENT_ACTIONBAR_SLOT_CHANGED, "%d", slotID + 1);

            return 0;
        }

        int Script_FireTalentUpdateEvent(lua_State* L)
        {
            FrameScript_SignalEvent(SCRIPT_EVENT_PLAYER_TALENT_UPDATE, nullptr);
            return 0;
        }

        int Script_FlashGameWindow(lua_State* L)
        {
            if (g_theGxDevicePtr)
            {
                HWND activeWindow = static_cast<HWND>(g_theGxDevicePtr->DeviceWindow());
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
            }

            return 0;
        }

        int Script_FindSpellActionBarSlot(lua_State* L)
        {
            uint32_t spellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            uint8_t count = 0;

            for (uint8_t i = 0; i < 144; i++)
            {
                if (CGActionBar::m_slotActions[i] == spellID)
                {
                    ClientLua::PushNumber(L, i);
                    count++;
                }
            }

            if (!count)
            {
                ClientLua::PushNil(L);
                return 1;
            }

            return count;
        }

        int Script_ReplaceActionBarSpell(lua_State* L)
        {
            uint32_t oldSpellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            uint32_t newSpellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 2));

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

        int Script_SetSpellInActionBarSlot(lua_State* L)
        {
            uint32_t spellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            uint8_t slotID = static_cast<uint8_t>(ClientLua::GetNumber(L, 2));

            if (slotID < 144)
            {
                CGActionBar::SetAction(slotID, spellID, 1, 0);
            }

            return 0;
        }

        static int Script_EmitFrameEventByName(lua_State* L)
        {
            std::string eventName = ClientLua::GetString(L, 1, "");
            if (!eventName.empty())
            {
                FrameScript_SignalEventByName(eventName.c_str());
            }
            return 0;
        }

        void RegisterUIFunctions()
        {
            ClientLua::AddFunction("FireActionBarSlotUpdateEvent", Script_FireActionBarSlotUpdateEvent);
            ClientLua::AddFunction("FireTalentUpdateEvent", Script_FireTalentUpdateEvent);
            ClientLua::AddFunction("FlashGameWindow", Script_FlashGameWindow);
            ClientLua::AddFunction("FindSpellActionBarSlot", Script_FindSpellActionBarSlot);
            ClientLua::AddFunction("ReplaceActionBarSpell", Script_ReplaceActionBarSpell);
            ClientLua::AddFunction("SetSpellInActionBarSlot", Script_SetSpellInActionBarSlot);
            ClientLua::AddFunction("SetFrameChildClipping", Script_SetFrameChildClipping);
            ClientLua::AddFunction("GetFrameChildClipping", Script_GetFrameChildClipping);
            ClientLua::AddFunction("SetTextureWithMask", Script_SetTextureWithMask);
            ClientLua::AddFunction("SetPortraitToTexture", Script_SetPortraitToTexture);
            ClientLua::AddFunction("EmitFrameEventByName", Script_EmitFrameEventByName);
        }
    } // namespace Lua
} // namespace Duskhaven
