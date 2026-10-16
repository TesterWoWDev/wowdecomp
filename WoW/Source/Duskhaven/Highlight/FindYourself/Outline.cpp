#include "WoW/Source/Duskhaven/Highlight/FindYourself/Outline.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/FindYourself.h"
#include "WoW/Source/Duskhaven/Highlight/OutlineMaskFilter.h"
#include "WoW/Source/Duskhaven/Highlight/D3DShaderHelper.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h"
#include "Engine/Source/Gx/Gx.h"
#include <cstdlib>

namespace Duskhaven::Highlight::FindYourself::Outline
{
    namespace
    {
        struct ScreenVertex
        {
            float x;
            float y;
            float z;
            float rhw;
            float u;
            float v;
        };

        bool s_enabled                         = false;
        bool s_drawingPlayerBatch              = false;
        bool s_maskValid                       = false;
        bool s_maskNeedsClear                  = true;
        IDirect3DSurface9* s_playerWorldTarget = nullptr;
        IDirect3DTexture9* s_maskTexture       = nullptr;
        IDirect3DSurface9* s_maskSurface       = nullptr;
        IDirect3DPixelShader9* s_maskShader    = nullptr;
        IDirect3DPixelShader9* s_outlineShader = nullptr;
        UINT s_maskWidth                       = 0;
        UINT s_maskHeight                      = 0;

        bool IsDeviceReady(IDirect3DDevice9* device)
        {
            if (!device)
                return false;

            __try
            {
                return device->TestCooperativeLevel() == D3D_OK;
            }
            __except (1)
            {
                return false;
            }
        }

        constexpr char FindYourselfMaskPixelShader[] = R"(
            sampler2D modelSampler : register(s0);

            float4 main(float2 uv : TEXCOORD0) : COLOR
            {
                clip(tex2D(modelSampler, uv).a - 0.5);
                return float4(1.0, 1.0, 1.0, 1.0);
            }
        )";

        constexpr char FindYourselfOutlinePixelShader[] = R"(
            sampler2D maskSampler : register(s0);
            float4 outlineInfo : register(c0);

            float4 main(float2 uv : TEXCOORD0) : COLOR
            {
                float center = tex2D(maskSampler, uv).a;
                float2 pixel = outlineInfo.xy;
                float ring1 = 0.0;
                float ring2 = 0.0;
                float ring3 = 0.0;

                float2 offsets[8] = {
                    float2( 1.0,  0.0), float2(-1.0,  0.0),
                    float2( 0.0,  1.0), float2( 0.0, -1.0),
                    float2( 0.707,  0.707), float2(-0.707,  0.707),
                    float2( 0.707, -0.707), float2(-0.707, -0.707)
                };

                for (int i = 0; i < 8; ++i)
                {
                    ring1 = max(ring1, tex2D(maskSampler, uv + offsets[i] * pixel * 1.25).a);
                    ring2 = max(ring2, tex2D(maskSampler, uv + offsets[i] * pixel * 2.75).a);
                    ring3 = max(ring3, tex2D(maskSampler, uv + offsets[i] * pixel * 4.50).a);
                }

                float core = saturate(ring1 - center);
                float middle = saturate(ring2 - max(center, ring1));
                float outer = saturate(ring3 - max(center, ring2));
                float alpha = saturate(core * 0.56 + middle * 0.28 + outer * 0.11);
                return float4(0.92, 1.0, 0.48, alpha);
            }
        )";

        void EnsureOutlineResources(IDirect3DDevice9* device)
        {
            if (!IsDeviceReady(device))
                return;

            IDirect3DSurface9* backBuffer = nullptr;
            D3DSURFACE_DESC desc{};
            if (!device || FAILED(device->GetRenderTarget(0, &backBuffer)) || !backBuffer ||
                FAILED(backBuffer->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0)
            {
                if (backBuffer)
                    backBuffer->Release();
                return;
            }
            backBuffer->Release();

            if (s_maskTexture && (s_maskWidth != desc.Width || s_maskHeight != desc.Height))
            {
                if (s_maskSurface)
                {
                    s_maskSurface->Release();
                    s_maskSurface = nullptr;
                }
                s_maskTexture->Release();
                s_maskTexture    = nullptr;
                s_maskValid      = false;
                s_maskNeedsClear = true;
            }

            if (!s_maskTexture)
            {
                s_maskWidth  = desc.Width;
                s_maskHeight = desc.Height;
                if (FAILED(device->CreateTexture(s_maskWidth, s_maskHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, &s_maskTexture, nullptr)))
                {
                    s_maskWidth  = 0;
                    s_maskHeight = 0;
                    return;
                }
                if (FAILED(s_maskTexture->GetSurfaceLevel(0, &s_maskSurface)) || !s_maskSurface)
                    CleanupOutlineResources();
            }

            if (!s_maskShader)
            {
                s_maskShader = CompilePixelShaderFromHlsl(device, FindYourselfMaskPixelShader, "main", "ps_3_0");
            }
            if (!s_outlineShader)
            {
                s_outlineShader = CompilePixelShaderFromHlsl(device, FindYourselfOutlinePixelShader, "main", "ps_3_0");
            }
        }

        bool IsActivePlayerBatch(CM2Model* model)
        {
            if (!model)
                return false;

            CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));
            if (!player)
                return false;

            return IsModelOrAllowedChildOf(model, player->GetObjectModel());
        }

        void DrawPlayerOutline(IDirect3DDevice9* device, IDirect3DSurface9* worldTarget)
        {
            if (!FindYourself::ShouldRender() || !s_maskValid || !s_maskTexture || !s_outlineShader ||
                !IsDeviceReady(device) || !worldTarget || s_maskWidth == 0 || s_maskHeight == 0)
                return;

            IDirect3DSurface9* previousTarget = nullptr;
            IDirect3DSurface9* previousDepth  = nullptr;
            D3DVIEWPORT9 previousViewport{};
            D3DSURFACE_DESC worldTargetDesc{};
            IDirect3DStateBlock9* state = nullptr;
            if (FAILED(device->GetRenderTarget(0, &previousTarget)) || !previousTarget ||
                FAILED(device->GetDepthStencilSurface(&previousDepth)) || FAILED(device->GetViewport(&previousViewport)) ||
                previousViewport.Width == 0 || previousViewport.Height == 0 ||
                FAILED(worldTarget->GetDesc(&worldTargetDesc)) || worldTargetDesc.Width == 0 ||
                worldTargetDesc.Height == 0 || FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) || !state)
            {
                if (state)
                    state->Release();
                if (previousTarget)
                    previousTarget->Release();
                if (previousDepth)
                    previousDepth->Release();
                return;
            }
            if (FAILED(state->Capture()))
            {
                state->Release();
                previousTarget->Release();
                if (previousDepth)
                    previousDepth->Release();
                return;
            }

            device->SetRenderTarget(0, worldTarget);
            device->SetDepthStencilSurface(nullptr);
            D3DVIEWPORT9 viewport{0, 0, worldTargetDesc.Width, worldTargetDesc.Height, 0.0f, 1.0f};
            device->SetViewport(&viewport);

            const float width       = static_cast<float>(worldTargetDesc.Width);
            const float height      = static_cast<float>(worldTargetDesc.Height);
            ScreenVertex vertices[] = {
                {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
                {width - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f},
                {-0.5f, height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
                {width - 0.5f, height - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f},
            };
            const float outlineInfo[] = {1.0f / width, 1.0f / height, 0.0f, 0.0f};

            device->SetVertexShader(nullptr);
            device->SetPixelShader(s_outlineShader);
            device->SetPixelShaderConstantF(0, outlineInfo, 1);
            device->SetTexture(0, s_maskTexture);
            device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
            device->SetRenderState(D3DRS_ZENABLE, FALSE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));

            state->Apply();
            state->Release();
            if (previousTarget)
            {
                device->SetRenderTarget(0, previousTarget);
                previousTarget->Release();
            }
            if (previousDepth)
            {
                device->SetDepthStencilSurface(previousDepth);
                previousDepth->Release();
            }
            device->SetViewport(&previousViewport);
        }

        bool FindYourselfOutlineCVarCallback(CVar*, const char*, const char* value, void*)
        {
            s_enabled = value && std::atoi(value) != 0;

            if (!s_enabled)
            {
                CleanupOutlineResources();
            }

            FindYourself::RefreshPlayerNameColor();
            return true;
        }
    } // namespace

    void CleanupOutlineResources()
    {
        s_drawingPlayerBatch = false;
        s_maskValid          = false;
        s_maskNeedsClear     = true;
        s_maskWidth          = 0;
        s_maskHeight         = 0;

        if (s_playerWorldTarget)
        {
            s_playerWorldTarget->Release();
            s_playerWorldTarget = nullptr;
        }
        if (s_maskSurface)
        {
            s_maskSurface->Release();
            s_maskSurface = nullptr;
        }
        if (s_maskTexture)
        {
            s_maskTexture->Release();
            s_maskTexture = nullptr;
        }
        if (s_maskShader)
        {
            s_maskShader->Release();
            s_maskShader = nullptr;
        }
        if (s_outlineShader)
        {
            s_outlineShader->Release();
            s_outlineShader = nullptr;
        }
    }

    void Apply()
    {
        CVar::Register("findYourselfModeOutline", "Draws an outline around the active player character", 1, "0",
                       FindYourselfOutlineCVarCallback, CATEGORY_GRAPHICS, false, nullptr, false);
    }

    bool IsEnabled()
    {
        return s_enabled;
    }

    void BeginDrawBatch(CM2Model* model)
    {
        s_drawingPlayerBatch = s_enabled && FindYourself::ShouldRender() && IsActivePlayerBatch(model);
    }

    void EndDrawBatch(CM2Model*)
    {
        s_drawingPlayerBatch = false;
    }

    void CapturePlayerDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex,
                           UINT numVertices, UINT startIndex, UINT primCount)
    {
        if (!s_drawingPlayerBatch || !IsDeviceReady(device))
            return;

        if (IsTransparentNoDepthWriteDraw(device))
            return;

        EnsureOutlineResources(device);
        if (!s_maskSurface || !s_maskShader || s_maskWidth == 0 || s_maskHeight == 0)
            return;

        IDirect3DSurface9* previousTarget = nullptr;
        IDirect3DSurface9* previousDepth  = nullptr;
        D3DVIEWPORT9 previousViewport{};
        IDirect3DStateBlock9* state = nullptr;
        if (FAILED(device->GetRenderTarget(0, &previousTarget)) || !previousTarget ||
            FAILED(device->GetDepthStencilSurface(&previousDepth)) || FAILED(device->GetViewport(&previousViewport)) ||
            previousViewport.Width == 0 || previousViewport.Height == 0 ||
            FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) || !state)
        {
            if (state)
                state->Release();
            if (previousTarget)
                previousTarget->Release();
            if (previousDepth)
                previousDepth->Release();
            return;
        }
        if (FAILED(state->Capture()))
        {
            state->Release();
            previousTarget->Release();
            if (previousDepth)
                previousDepth->Release();
            return;
        }

        if (!s_playerWorldTarget && previousTarget)
        {
            s_playerWorldTarget = previousTarget;
            s_playerWorldTarget->AddRef();
        }

        device->SetRenderTarget(0, s_maskSurface);
        device->SetDepthStencilSurface(nullptr);
        D3DVIEWPORT9 viewport{0, 0, s_maskWidth, s_maskHeight, 0.0f, 1.0f};
        device->SetViewport(&viewport);
        if (s_maskNeedsClear)
        {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
            s_maskNeedsClear = false;
            s_maskValid      = true;
        }

        device->SetPixelShader(s_maskShader);
        device->DrawIndexedPrimitive(type, baseVertexIndex, minVertexIndex, numVertices, startIndex, primCount);

        if (state)
        {
            state->Apply();
            state->Release();
        }
        if (previousTarget)
            device->SetRenderTarget(0, previousTarget);
        if (previousDepth)
            device->SetDepthStencilSurface(previousDepth);
        device->SetViewport(&previousViewport);
        if (previousTarget)
            previousTarget->Release();
        if (previousDepth)
            previousDepth->Release();
    }

    void OnWorldRender()
    {
        if (!s_enabled)
            return;

        if (!s_playerWorldTarget)
            return;

        IDirect3DDevice9* device = nullptr;
        if (g_theGxDevicePtr)
        {
            EGxApi api = g_theGxDevicePtr->DeviceApi();
            if (api == GxApi_D3d9)
                device = static_cast<CGxDeviceD3d*>(g_theGxDevicePtr)->m_d3dDevice;
            else if (api == GxApi_D3d9Ex)
                device = static_cast<CGxDeviceD3d9Ex*>(g_theGxDevicePtr)->m_d3dDevice;
        }

        if (device)
        {
            DrawPlayerOutline(device, s_playerWorldTarget);
        }

        s_playerWorldTarget->Release();
        s_playerWorldTarget = nullptr;
    }

    void FinishOutlineFrame(IDirect3DDevice9*)
    {
        if (s_playerWorldTarget)
        {
            s_playerWorldTarget->Release();
            s_playerWorldTarget = nullptr;
        }
        s_maskNeedsClear = true;
    }
}
