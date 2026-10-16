#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <windows.h>

#include "Engine/Source/Gxu/GxuFontPlugin.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"

// msdfgen headers
#include <msdfgen/msdfgen.h>
#include <msdfgen/msdfgen-ext.h>

// MSDF configuration constants
static constexpr uint32_t MSDF_ATLAS_SIZE = 512;
static constexpr uint32_t MSDF_GLYPH_SIZE = 48;
static constexpr double   MSDF_PIXEL_RANGE = 3.0;

static FontAllocFn s_allocFn = nullptr;
static FontFreeFn  s_freeFn  = nullptr;

static void* MsdfAlloc(size_t size) {
    if (s_allocFn) {
        return s_allocFn(size);
    }
    return malloc(size);
}

static void MsdfFree(void* ptr) {
    if (s_freeFn) {
        s_freeFn(ptr);
        return;
    }
    free(ptr);
}

static void CalculateYOffset(uint32_t pixelSize, int32_t baseline, FT_Face face, uint32_t glyphHeight, int32_t* yOffset, uint32_t* yStart) {
    if (glyphHeight > pixelSize) {
        glyphHeight = pixelSize;
    }
    int32_t offset = 0;
    int32_t start = 0;

    int32_t top = face->glyph->bitmap_top;
    if (top <= baseline) {
        start = baseline - top;
    } else {
        start = 0;
        offset = top - baseline;
    }

    start = start <= 0 ? 0 : start;
    int32_t available = static_cast<int32_t>(pixelSize) - static_cast<int32_t>(glyphHeight);

    if (available >= start) {
        *yOffset = offset;
        *yStart = static_cast<uint32_t>(start);
    } else {
        int32_t s = available <= 0 ? 0 : available;
        *yOffset = static_cast<int32_t>(pixelSize) - start - static_cast<int32_t>(glyphHeight);
        *yStart = static_cast<uint32_t>(s);
    }
}

// Embedded HLSL MSDF pixel shader source with 4-sample Rotated Grid Supersampling (RGSS)
// and distance field bias for crisp, smooth anti-aliased font rendering at any distance
static const char* s_msdfPixelShaderHlsl =
    "sampler2D s0 : register(s0);\n"
    "float4 u_outlineParams : register(c0);\n"
    "\n"
    "float median(float r, float g, float b) {\n"
    "    return max(min(r, g), min(max(r, g), b));\n"
    "}\n"
    "\n"
    "float evalSample(float sd, float range, float bias, float width) {\n"
    "    float dist = sd - 0.5f + bias;\n"
    "    float fade = saturate((sd - 0.02f) * 20.0f);\n"
    "    return saturate(dist * range + width + 0.5f) * fade;\n"
    "}\n"
    "\n"
    "float4 main(float4 color : COLOR0, float2 texcoord : TEXCOORD0) : COLOR0 {\n"
    "    float2 dx = ddx(texcoord);\n"
    "    float2 dy = ddy(texcoord);\n"
    "\n"
    "    float2 unitRange = float2(4.0f / 512.0f, 4.0f / 512.0f);\n"
    "    float2 screenTexSize = 1.0f / max(abs(dx) + abs(dy), 1e-6f);\n"
    "    float screenPxRange = max(0.5f * dot(unitRange, screenTexSize), 1.0f);\n"
    "\n"
    "    float fontSize = (u_outlineParams.y > 1.0f) ? u_outlineParams.y : 12.0f;\n"
    "    float screenFontSize = fontSize * (screenPxRange / 4.0f);\n"
    "\n"
    "    // Distance bias to maintain stroke weight and prevent thinning/breakup at small sizes\n"
    "    float bias = (screenFontSize < 14.0f) ? (14.0f - screenFontSize) * 0.005f : 0.0f;\n"
    "\n"
    "    // Detect if this is a shadow pass (shadowColor is black, r+g+b near 0)\n"
    "    // Boost shadow distance slightly to give rich, dark, high-contrast drop shadows\n"
    "    float isShadow = (dot(color.rgb, float3(1.0f, 1.0f, 1.0f)) < 0.05f) ? 1.0f : 0.0f;\n"
    "    bias += isShadow * 0.035f;\n"
    "\n"
    "    // Measure minification (how many atlas texels per screen pixel)\n"
    "    // In 2D UI at native scale, maxTexelDelta is ~1.0 -> subpixelScale is 0 (crisp, solid 100% contrast).\n"
    "    // Shadows never use subpixel offsets (keeps drop shadows 100% solid black).\n"
    "    // In 3D far-away text, maxTexelDelta is > 1.5 -> subpixelScale ramps to 1.0 (4-sample RGSS).\n"
    "    float maxTexelDelta = max(length(dx), length(dy)) * 512.0f;\n"
    "    float subpixelScale = saturate((maxTexelDelta - 1.15f) * 0.8f) * (1.0f - isShadow);\n"
    "\n"
    "    // 4-point Rotated Grid Supersampling (RGSS) offsets scaled by minification factor\n"
    "    // Offsets: (-3/8, -1/8), (1/8, -3/8), (3/8, 1/8), (-1/8, 3/8)\n"
    "    float2 off0 = (-0.375f * dx - 0.125f * dy) * subpixelScale;\n"
    "    float2 off1 = ( 0.125f * dx - 0.375f * dy) * subpixelScale;\n"
    "    float2 off2 = ( 0.375f * dx + 0.125f * dy) * subpixelScale;\n"
    "    float2 off3 = (-0.125f * dx + 0.375f * dy) * subpixelScale;\n"
    "\n"
    "    float3 s0_rgb = tex2D(s0, texcoord + off0).rgb;\n"
    "    float3 s1_rgb = tex2D(s0, texcoord + off1).rgb;\n"
    "    float3 s2_rgb = tex2D(s0, texcoord + off2).rgb;\n"
    "    float3 s3_rgb = tex2D(s0, texcoord + off3).rgb;\n"
    "\n"
    "    float sd0 = median(s0_rgb.r, s0_rgb.g, s0_rgb.b);\n"
    "    float sd1 = median(s1_rgb.r, s1_rgb.g, s1_rgb.b);\n"
    "    float sd2 = median(s2_rgb.r, s2_rgb.g, s2_rgb.b);\n"
    "    float sd3 = median(s3_rgb.r, s3_rgb.g, s3_rgb.b);\n"
    "\n"
    "    float width = u_outlineParams.x;\n"
    "    float o0 = evalSample(sd0, screenPxRange, bias, width);\n"
    "    float o1 = evalSample(sd1, screenPxRange, bias, width);\n"
    "    float o2 = evalSample(sd2, screenPxRange, bias, width);\n"
    "    float o3 = evalSample(sd3, screenPxRange, bias, width);\n"
    "    float opacity = (o0 + o1 + o2 + o3) * 0.25f;\n"
    "\n"
    "    return float4(color.rgb, color.a * opacity);\n"
    "}\n";

// Precompiled fallback D3D9 pixel shader bytecode (ps_2_a)
static const unsigned char s_msdfPs2aBytecode[] = {
    255, 254,   2,   0,  49,   0, 
      0,   5,   0,   0,  15, 160, 
      0,   0, 128,  60,   0,   0, 
    128,  60,   0,   0, 128,  60, 
      0,   0, 128,  60,  49,   0, 
      0,   5,   1,   0,  15, 160, 
      0,   0, 128,  63,   0,   0, 
      0,   0,   0,   0,   0,  63, 
      0,   0,   0,  65,  49,   0, 
      0,   5,   2,   0,  15, 160, 
      0,   0, 128,  62,   0,   0, 
      0,  61,   0,   0,   0,  63, 
      0,   0,   0,  65,  49,   0, 
      0,   5,   3,   0,  15, 160, 
      0,   0, 192, 190,   0,   0, 
      0, 190,   0,   0,   0,  62, 
      0,   0, 192,  62,  49,   0, 
      0,   5,   4,   0,  15, 160, 
      0,   0,   0, 191,   0,   0, 
    128,  63,   0,   0,   0,   0, 
      0,   0,   0,   0,  49,   0, 
      0,   5,   5,   0,  15, 160, 
      0,   0, 128,  63,   0,   0, 
      0,  63,   0,   0, 128,  63, 
      0,   0, 128,  63,  49,   0, 
      0,   5,   6,   0,  15, 160, 
      0,   0,   0,  68,   0,   0, 
    144,  64,   0,   0,  32,  65, 
      0,   0, 166,  63,  49,   0, 
      0,   5,   7,   0,  15, 160, 
      0,   0, 128,  63,   0,   0, 
      0,   0,   0,   0, 160,  64, 
      0,   0, 128,  62,  31,   0, 
      0,   2,   0,   0,   0, 128, 
      0,   0,  15, 176,  31,   0, 
      0,   2,   0,   0,   0, 128, 
      0,   0,  15, 144,  31,   0, 
      0,   2,   0,   0,   0, 144, 
      0,   8,  15, 160,  78,   0, 
      0,   2,   1,   0,   3, 128, 
      0,   0, 228, 176,  79,   0, 
      0,   2,   2,   0,   3, 128, 
      0,   0, 228, 176,   1,   0, 
      0,   2,   3,   0,   3, 128, 
      1,   0, 228, 128,   2,   0, 
      0,   3,   3,   0,   3, 128, 
      2,   0, 228, 128,   3,   0, 
    228, 128,   3,   0,   0,   3, 
      4,   0,   3, 128,   1,   0, 
    228, 128,   1,   0, 228, 128, 
      3,   0,   0,   3,   1,   0, 
      3, 128,   2,   0, 228, 128, 
      2,   0, 228, 128,   2,   0, 
      0,   3,   1,   0,   3, 128, 
      1,   0, 228, 128,   4,   0, 
    228, 128,  10,   0,   0,   3, 
      4,   0,   1, 128,   1,   0, 
    255, 128,   6,   0,   0, 160, 
      2,   0,   0,   3,   4,   0, 
      1, 128,   4,   0, 255, 128, 
      4,   0, 170, 128,   2,   0, 
      0,   3,   1,   0,   1, 128, 
      1,   0, 255, 128,   1,   0, 
      0, 128,   2,   0,   0,   3, 
      1,   0,   1, 128,   1,   0, 
    255, 128,   1,   0, 170, 128, 
      2,   0,   0,   3,   1,   0, 
      1, 128,   1,   0, 255, 128, 
      0,   0,  85, 160,  88,   0, 
      0,   4,   1,   0,   1, 128, 
      1,   0, 255, 128,   7,   0, 
    170, 160,   7,   0,  85, 160, 
     12,   0,   0,   3,   1,   0, 
      1, 128,   1,   0, 255, 128, 
      3,   0, 228, 128,   5,   0, 
      0,   3,   1,   0,   1, 128, 
      1,   0, 255, 128,   0,   0, 
     85, 160,  88,   0,   0,   4, 
      1,   0,   1, 128,   1,   0, 
    255, 128,   1,   0,   0, 160, 
      1,   0,  85, 160,  12,   0, 
      0,   3,   1,   0,   1, 128, 
      1,   0, 255, 128,   0,   0, 
     85, 176,  12,   0,   0,   3, 
      4,   0,   1, 128,   4,   0, 
    255, 128,   6,   0, 255, 160, 
      2,   0,   0,   3,   4,   0, 
      1, 128,   4,   0, 255, 128, 
      6,   0,  85, 160,  10,   0, 
      0,   3,   4,   0,   1, 128, 
      4,   0, 255, 128,   4,   0, 
    255, 160,   2,   0,   0,   3, 
      4,   0,   1, 128,   4,   0, 
    255, 128,   6,   0, 170, 160, 
     88,   0,   0,   4,   4,   0, 
      1, 128,   4,   0, 255, 128, 
      7,   0, 255, 160,   7,   0, 
     85, 160,  12,   0,   0,   3, 
      4,   0,   1, 128,   4,   0, 
    255, 128,   0,   0,  85, 160, 
      3,   0,   0,   3,   1,   0, 
      1, 128,   1,   0, 255, 128, 
      2,   0, 228, 160,   2,   0, 
      0,   3,   4,   0,   1, 128, 
      4,   0, 255, 128,   1,   0, 
    255, 128,   4,   0,   0,   4, 
      1,   0,   1, 128,   0,   0, 
     85, 160,   4,   0, 255, 128, 
      1,   0, 255, 128,   2,   0, 
      0,   3,   1,   0,   1, 128, 
      1,   0, 255, 128,   1,   0, 
      0, 160,   3,   0,   0,   3, 
      1,   0,   1, 128,   1,   0, 
    255, 128,   4,   0, 228, 160, 
      2,   0,   0,   3,   1,   0, 
      1, 128,   1,   0, 255, 128, 
      2,   0, 170, 160,   4,   0, 
      0,   4,   1,   0,   2, 128, 
      2,   0,  85, 128,   1,   0, 
    255, 128,   1,   0,   0, 176, 
      2,   0,   0,   3,   1,   0, 
      4, 128,   1,   0, 255, 128, 
      0,   0,   0, 176,  10,   0, 
      0,   3,   4,   0,   1, 128, 
      1,   0,   0, 128,   5,   0, 
     85, 160,  12,   0,   0,   3, 
      4,   0,   1, 128,   4,   0, 
    255, 128,   5,   0, 255, 160, 
      2,   0,   0,   3,   4,   0, 
      1, 128,   4,   0, 255, 128, 
      5,   0,   0, 160,   2,   0, 
      0,   3,   1,   0,   1, 128, 
      4,   0, 255, 128,   1,   0, 
    255, 160,   2,   0,   0,   3, 
      1,   0,   1, 128,   1,   0, 
    255, 128,   7,   0, 170, 160, 
      4,   0,   0,   4,   1,   0, 
      1, 128,   1,   0, 255, 128, 
      7,   0,   0, 160,   1,   0, 
    255, 128,  88,   0,   0,   4, 
      1,   0,   1, 128,   1,   0, 
    255, 128,   7,   0,  85, 160, 
      7,   0,  85, 160,   2,   0, 
      0,   3,   4,   0,   1, 128, 
      1,   0, 255, 128,   2,   0, 
      0, 128,   4,   0,   0,   4, 
      1,   0,   2, 128,   2,   0, 
      0, 128,   1,   0, 255, 128, 
      1,   0,  85, 128,   4,   0, 
      0,   4,   2,   0,   3, 128, 
      3,   0, 228, 128,   1,   0, 
      0, 128,   0,   0, 228, 176, 
      4,   0,   0,   4,   3,   0, 
      3, 128,   3,   0, 238, 128, 
      1,   0,   0, 128,   0,   0, 
    228, 176,  66,   0,   0,   3, 
      4,   0,  15, 128,   2,   0, 
    228, 128,   0,   8, 228, 160, 
     66,   0,   0,   3,   3,   0, 
     15, 128,   3,   0, 228, 128, 
      0,   8, 228, 160,   2,   0, 
      0,   3,   3,   0,   8, 128, 
      4,   0,  85, 129,   4,   0, 
      0, 128,  88,   0,   0,   4, 
      2,   0,   3, 128,   3,   0, 
    255, 128,   4,   0, 225, 128, 
      4,   0, 228, 128,  10,   0, 
      0,   3,   3,   0,   8, 128, 
      4,   0, 170, 128,   2,   0, 
     85, 128,  11,   0,   0,   3, 
      1,   0,   1, 128,   2,   0, 
      0, 128,   3,   0, 255, 128, 
      2,   0,   0,   3,   3,   0, 
      8, 128,   2,   0, 255, 128, 
      1,   0,   0, 128,   2,   0, 
      0,   3,   1,   0,   1, 128, 
      1,   0,   0, 128,   6,   0, 
      0, 160,   5,   0,   0,   3, 
      1,   0,  17, 128,   1,   0, 
      0, 128,   6,   0,  85, 160, 
      2,   0,   0,   3,   3,   0, 
      8, 128,   3,   0, 255, 128, 
      1,   0, 255, 161,   4,   0, 
      0,   4,   2,   0,  17, 128, 
      3,   0, 255, 128,   0,   0, 
      0, 128,   1,   0, 255, 160, 
      4,   0,   0,   4,   1,   0, 
      2, 128,   2,   0,   0, 128, 
      1,   0,   0, 128,   1,   0, 
     85, 128,   2,   0,   0,   3, 
      2,   0,   1, 128,   3,   0, 
     85, 129,   3,   0,   0, 128, 
     88,   0,   0,   4,   2,   0, 
      3, 128,   2,   0,   0, 128, 
      3,   0, 225, 128,   3,   0, 
    228, 128,  10,   0,   0,   3, 
      4,   0,   1, 128,   3,   0, 
    170, 128,   2,   0,  85, 128, 
     11,   0,   0,   3,   3,   0, 
      1, 128,   2,   0,   0, 128, 
      4,   0,   0, 128,   2,   0, 
      0,   3,   2,   0,   1, 128, 
      2,   0, 255, 128,   3,   0, 
      0, 128,   2,   0,   0,   3, 
      2,   0,   2, 128,   3,   0, 
      0, 128,   6,   0,   0, 160, 
      5,   0,   0,   3,   2,   0, 
     18, 128,   2,   0,  85, 128, 
      6,   0,  85, 160,   2,   0, 
      0,   3,   2,   0,   1, 128, 
      2,   0,   0, 128,   1,   0, 
    255, 161,   4,   0,   0,   4, 
      2,   0,  20, 128,   2,   0, 
      0, 128,   0,   0,   0, 128, 
      1,   0, 255, 160,   4,   0, 
      0,   4,   1,   0,   2, 128, 
      2,   0, 170, 128,   2,   0, 
     85, 128,   1,   0,  85, 128, 
      5,   0,   0,   3,   1,   0, 
      2, 128,   1,   0,  85, 128, 
      3,   0,   0, 160,   2,   0, 
      0,   3,   2,   0,  12, 128, 
      5,   0,   0, 128,   7,   0, 
     68, 160,   5,   0,   0,   3, 
      0,   0,   2, 128,   0,   0, 
     85, 128,   2,   0, 170, 128, 
     11,   0,   0,   3,   2,   0, 
      4, 128,   0,   0,   0, 128, 
      0,   0,  85, 128,   4,   0, 
      0,   4,   0,   0,   1, 128, 
      0,   0, 170, 128,   2,   0, 
    170, 128,   0,   0,   0, 160, 
      2,   0,   0,   3,   0,   0, 
     17, 128,   0,   0,   0, 128, 
      1,   0, 255, 160,   5,   0, 
      0,   3,   0,   0,   1, 128, 
      0,   0, 255, 128,   0,   0, 
      0, 128,   4,   0,   0,   4, 
      0,   0,   2, 128,   1,   0, 
    170, 128,   2,   0, 170, 128, 
      0,   0,   0, 160,   2,   0, 
      0,   3,   0,   0,  18, 128, 
      0,   0,  85, 128,   1,   0, 
    255, 160,   4,   0,   0,   4, 
      0,   0,   1, 128,   0,   0, 
     85, 128,   1,   0, 255, 128, 
      0,   0,   0, 128,   4,   0, 
      0,   4,   0,   0,   2, 128, 
      3,   0, 255, 128,   2,   0, 
    170, 128,   0,   0,   0, 160, 
      4,   0,   0,   4,   0,   0, 
      4, 128,   2,   0,   0, 128, 
      2,   0, 170, 128,   0,   0, 
      0, 160,   2,   0,   0,   3, 
      0,   0,  22, 128,   0,   0, 
    228, 128,   1,   0, 255, 160, 
      4,   0,   0,   4,   0,   0, 
      1, 128,   0,   0,  85, 128, 
      1,   0,   0, 128,   0,   0, 
      0, 128,   4,   0,   0,   4, 
      0,   0,   1, 128,   0,   0, 
    170, 128,   2,   0,  85, 128, 
      0,   0,   0, 128,   5,   0, 
      0,   3,   0,   0,   1, 128, 
      0,   0,   0, 128,   3,   0, 
      0, 160,  88,   0,   0,   4, 
      0,   0,   1, 128,   2,   0, 
    255, 128,   0,   0,   0, 128, 
      1,   0,  85, 128,   5,   0, 
      0,   3,   0,   0,  14, 128, 
      1,   0,  85, 128,   0,   0, 
    144, 144,  88,   0,   0,   4, 
      1,   0,   7, 128,   2,   0, 
    255, 128,   0,   0, 249, 128, 
      0,   0, 228, 144,   5,   0, 
      0,   3,   1,   0,   8, 128, 
      0,   0,   0, 128,   0,   0, 
    255, 144,   1,   0,   0,   2, 
      0,   8,  15, 128,   1,   0, 
    228, 128, 255, 255,   0,   0
};

class MsdfFontBackend : public IGxuFontBackend {
public:
    bool Initialize(FontAllocFn allocFn, FontFreeFn freeFn) override {
        s_allocFn = allocFn;
        s_freeFn = freeFn;
        return true;
    }

    void Shutdown() override {
        s_allocFn = nullptr;
        s_freeFn = nullptr;
    }

    float ComputeScreenPxRange(uint32_t renderedPixelSize) override {
        float scale = static_cast<float>(renderedPixelSize) / static_cast<float>(MSDF_GLYPH_SIZE);
        return scale * static_cast<float>(MSDF_PIXEL_RANGE);
    }

    const void* GetPixelShaderBytecode(uint32_t* outSize) override {
        if (outSize) {
            *outSize = sizeof(s_msdfPs2aBytecode);
        }
        return s_msdfPs2aBytecode;
    }

    const char* GetPixelShaderHlsl() override {
        return s_msdfPixelShaderHlsl;
    }

    bool GenerateGlyph(FT_Face face, uint32_t pixelSize, uint32_t code, uint32_t baseline, GLYPHBITMAPDATA* dataPtr, uint32_t padding) override {
        if (!face || !pixelSize || !dataPtr) {
            return false;
        }

        FT_UInt index = FT_Get_Char_Index(face, code);
        if (!index) {
            return false;
        }

        if (FT_Load_Glyph(face, index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) {
            return false;
        }

        msdfgen::Shape shape;
        FT_Error err = msdfgen::readFreetypeOutline(shape, &face->glyph->outline, 1.0 / 64.0);

        if (FT_Render_Glyph(face->glyph, static_cast<FT_Render_Mode>(0)) != 0) {
            return false;
        }

        uint32_t width = face->glyph->bitmap.width;
        uint32_t height = pixelSize >= static_cast<uint32_t>(face->glyph->bitmap.rows) ? face->glyph->bitmap.rows : pixelSize;

        if (err != 0 || shape.contours.empty() || shape.edgeCount() == 0 || !width || !height) {
            uint32_t emptyWidth = width ? (width + padding) : ((pixelSize + 3) >> 2);
            if (!emptyWidth) emptyWidth = pixelSize;
            uint32_t emptyHeight = height ? (height + padding) : pixelSize;
            if (!emptyHeight) emptyHeight = 1;

            uint32_t emptyPitch = emptyWidth * 3;
            uint32_t emptySize = emptyHeight * emptyPitch;

            void* data = MsdfAlloc(emptySize);
            if (data) {
                memset(data, 0, emptySize);
            }

            dataPtr->m_data = data;
            dataPtr->m_dataSize = emptySize;
            dataPtr->m_glyphWidth = emptyWidth;
            dataPtr->m_glyphHeight = emptyHeight;
            dataPtr->m_glyphCellWidth = emptyWidth;
            dataPtr->m_glyphPitch = emptyPitch;
            dataPtr->m_glyphAdvance = (face->glyph->metrics.horiAdvance / 64.0f) + 1.0f;
            dataPtr->m_glyphBearing = face->glyph->metrics.horiBearingX * (1.0f / 64.0f);
            dataPtr->m_yOffset = 0;
            dataPtr->m_yStart = 0;
            dataPtr->m_isMsdf = 1;
            return true;
        }

        shape.normalize();
        msdfgen::edgeColoringInkTrap(shape, 3.0, 0);

        uint32_t pad = padding / 2;
        uint32_t msdfWidth = width + padding;
        uint32_t msdfHeight = height + padding;

        if (msdfWidth > MSDF_ATLAS_SIZE || msdfHeight > MSDF_ATLAS_SIZE || !msdfWidth || !msdfHeight) {
            return false;
        }

        double translateX = -static_cast<double>(face->glyph->bitmap_left) + static_cast<double>(pad);
        double translateY = -static_cast<double>(face->glyph->bitmap_top - static_cast<int32_t>(face->glyph->bitmap.rows)) + static_cast<double>(pad);

        msdfgen::Shape::Bounds bounds = shape.getBounds();
        if (bounds.t + translateY > static_cast<double>(msdfHeight)) {
            translateY = static_cast<double>(msdfHeight) - bounds.t;
        }
        if (bounds.b + translateY < 0.0) {
            translateY = -bounds.b;
        }
        if (bounds.r + translateX > static_cast<double>(msdfWidth)) {
            translateX = static_cast<double>(msdfWidth) - bounds.r;
        }
        if (bounds.l + translateX < 0.0) {
            translateX = -bounds.l;
        }

        msdfgen::Bitmap<float, 3> msdf(msdfWidth, msdfHeight);
        msdfgen::Projection projection(msdfgen::Vector2(1.0, 1.0), msdfgen::Vector2(translateX, translateY));
        msdfgen::MSDFGeneratorConfig config(true);
        config.overlapSupport = true;
        config.errorCorrection.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
        msdfgen::generateMSDF(msdf, shape, projection, msdfgen::Range(MSDF_PIXEL_RANGE), config);

        uint32_t pitch = msdfWidth * 3;
        uint32_t size = pitch * msdfHeight;

        uint8_t* pixelData = static_cast<uint8_t*>(MsdfAlloc(size));
        if (!pixelData) {
            return false;
        }

        for (uint32_t y = 0; y < msdfHeight; y++) {
            for (uint32_t x = 0; x < msdfWidth; x++) {
                const float* pixel = msdf(x, msdfHeight - 1 - y);
                uint32_t idx = y * pitch + x * 3;
                pixelData[idx + 0] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, pixel[0] * 255.0f + 0.5f)));
                pixelData[idx + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, pixel[1] * 255.0f + 0.5f)));
                pixelData[idx + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, pixel[2] * 255.0f + 0.5f)));
            }
        }

        dataPtr->m_data = pixelData;
        dataPtr->m_dataSize = size;
        dataPtr->m_glyphWidth = msdfWidth;
        dataPtr->m_glyphHeight = msdfHeight;
        dataPtr->m_glyphCellWidth = msdfWidth;
        dataPtr->m_glyphPitch = pitch;
        dataPtr->m_glyphAdvance = (face->glyph->metrics.horiAdvance / 64.0f) + 1.0f;
        dataPtr->m_glyphBearing = face->glyph->metrics.horiBearingX * (1.0f / 64.0f);
        dataPtr->m_isMsdf = 1;

        CalculateYOffset(pixelSize, baseline, face, height, &dataPtr->m_yOffset, &dataPtr->m_yStart);
        return true;
    }
};

extern "C" __declspec(dllexport) IGxuFontBackend* CreateFontBackend() {
    static MsdfFontBackend s_backend;
    return &s_backend;
}
