#include "WeaponTrailPlayback.h"
#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrail.h"
#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrailParam.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/UIUtil/Camera.h"
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Duskhaven
{
namespace WeaponTrailPlayback
{
namespace
{
    constexpr std::size_t MAX_ACTIVE_TRAILS             = 16;
    constexpr std::size_t MAX_TRAIL_SAMPLES             = 128;
    constexpr std::size_t MAX_RENDER_SAMPLES            = 512;
    constexpr std::size_t MAX_TRAIL_TEXTURES            = 3;
    constexpr std::size_t TRAIL_SMOOTHING_SUBDIVISIONS  = 16;
    constexpr uint32_t CHARACTER_MAIN_HAND_ATTACHMENT   = 1;
    constexpr uint32_t CHARACTER_OFF_HAND_ATTACHMENT    = 2;
    constexpr uint32_t WEAPON_TRAIL_BOTTOM_EVENT        = 0x42545724; // $WTB
    constexpr uint32_t WEAPON_TRAIL_TOP_EVENT           = 0x54545724; // $WTT
    constexpr uint32_t ATTACH_TIMEOUT_MS                = 1500;
    constexpr uint32_t DEFAULT_DURATION_MS              = 200;
    constexpr uint32_t DEFAULT_EDGE_LIFESPAN_MS         = 150;
    constexpr uint32_t MAX_LIFETIME_MS                  = 10000;

    struct TrailSample
    {
        C3Vector bottom{};
        C3Vector top{};
        uint32_t capturedAtMs = 0;
    };

    struct TrailInstance
    {
        uint64_t objectGuid         = 0;
        uint32_t weaponAttachmentId = 0;
        uint32_t overrideAttachTop  = 0;
        uint32_t overrideAttachBot  = 0;
        uint32_t createdAtMs        = 0;
        uint32_t activateAtMs       = 0;
        uint32_t emitUntilMs        = 0;
        uint32_t expireAtMs         = 0;
        uint32_t attachDeadlineMs   = 0;
        uint32_t edgeLifeSpanMs     = DEFAULT_EDGE_LIFESPAN_MS;
        uint32_t fadeOutMs          = 0;
        uint32_t lastSampleAtMs     = 0;
        float smoothSampleAngle     = 0.0f;
        std::array<std::string, MAX_TRAIL_TEXTURES> texturePaths;
        std::array<float, MAX_TRAIL_TEXTURES> textureScrollU{};
        std::array<float, MAX_TRAIL_TEXTURES> textureScrollV{};
        std::array<float, MAX_TRAIL_TEXTURES> textureScaleU{};
        std::array<float, MAX_TRAIL_TEXTURES> textureScaleV{};
        std::vector<TrailSample> samples;
    };

    struct TrailVertex
    {
        float x;
        float y;
        float z;
        D3DCOLOR color;
        float u;
        float v;
    };

    struct TextureResource
    {
        IDirect3DTexture9* texture = nullptr;
        bool loadAttempted         = false;
    };

    struct HandTarget
    {
        uint32_t attachmentId = 0;
    };

    std::vector<TrailInstance> s_instances;
    std::unordered_map<std::string, TextureResource> s_textures;

    IDirect3DDevice9* GetDirect3DDevice()
    {
        if (!g_theGxDevicePtr)
            return nullptr;

        EGxApi api = g_theGxDevicePtr->DeviceApi();
        if (api == GxApi_D3d9)
            return static_cast<CGxDeviceD3d*>(g_theGxDevicePtr)->m_d3dDevice;
        if (api == GxApi_D3d9Ex)
            return static_cast<CGxDeviceD3d9Ex*>(g_theGxDevicePtr)->m_d3dDevice;

        return nullptr;
    }

    bool IsFinite(C3Vector const& position)
    {
        return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
    }

    bool HasElapsed(uint32_t now, uint32_t deadline)
    {
        return static_cast<int32_t>(now - deadline) >= 0;
    }

    uint32_t ElapsedSince(uint32_t now, uint32_t then)
    {
        return now - then;
    }

    uint32_t SecondsToMilliseconds(float seconds, uint32_t fallback)
    {
        if (!std::isfinite(seconds) || seconds <= 0.0f)
            return fallback;

        double const milliseconds = static_cast<double>(seconds) * 1000.0;
        if (milliseconds >= MAX_LIFETIME_MS)
            return MAX_LIFETIME_MS;
        return static_cast<uint32_t>(milliseconds + 0.5);
    }

    std::string NormalizePath(char const* path)
    {
        if (!path)
            return {};

        std::string normalized(path);
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        return normalized;
    }

    CGObject_C* ResolveObject(uint64_t guid)
    {
        if (!guid)
            return nullptr;
        return ClntObjMgrObjectPtr(WOWGUID{ guid }, static_cast<TypeMask>(TYPEMASK_UNIT | TYPEMASK_PLAYER), __FILE__, __LINE__);
    }

    bool GetEventPosition(CM2Model* model, uint32_t eventId, C3Vector& position)
    {
        if (!model->HasEvent(eventId))
            return false;

        position = model->GetEventPosition(eventId);
        return IsFinite(position);
    }

    bool GetAttachmentPosition(CM2Model* model, uint32_t attachmentId, C3Vector& position)
    {
        if (!model->HasAttachment(attachmentId))
            return false;

        C44Matrix transform = model->GetAttachmentWorldTransform(attachmentId);
        position = C3Vector(transform.d0, transform.d1, transform.d2);
        return IsFinite(position);
    }

    bool GetTrailEndpoints(TrailInstance const& instance, CM2Model* weaponModel, C3Vector& bottom, C3Vector& top)
    {
        bool const hasBottom = instance.overrideAttachBot
                                   ? GetAttachmentPosition(weaponModel, instance.overrideAttachBot, bottom)
                                   : GetEventPosition(weaponModel, WEAPON_TRAIL_BOTTOM_EVENT, bottom);
        bool const hasTop = instance.overrideAttachTop
                                ? GetAttachmentPosition(weaponModel, instance.overrideAttachTop, top)
                                : GetEventPosition(weaponModel, WEAPON_TRAIL_TOP_EVENT, top);
        if (!hasBottom || !hasTop)
            return false;

        float const x = top.x - bottom.x;
        float const y = top.y - bottom.y;
        float const z = top.z - bottom.z;
        return x * x + y * y + z * z > 0.000001f;
    }

    CM2Model* FindWeaponModel(TrailInstance& instance, CM2Model* characterModel)
    {
        if (!characterModel)
            return nullptr;

        CM2Model* preferredMatch = nullptr;
        uint32_t preferredCount  = 0;

        for (CM2Model* current = characterModel->m_attachList; current; current = current->m_attachNext)
        {
            if (current->IsLoaded(0, 0))
            {
                C3Vector bottom{};
                C3Vector top{};
                if (GetTrailEndpoints(instance, current, bottom, top))
                {
                    if (current->m_attachmentId == instance.weaponAttachmentId)
                    {
                        preferredMatch = current;
                        ++preferredCount;
                    }
                }
            }
        }

        if (preferredCount != 1)
            return nullptr;

        return preferredMatch;
    }

    bool GetHandTargets(uint32_t hand, std::array<HandTarget, 2>& targets, std::size_t& count)
    {
        count = 0;
        switch (hand)
        {
            case 0:
                targets[count++] = {CHARACTER_MAIN_HAND_ATTACHMENT};
                return true;
            case 1:
                targets[count++] = {CHARACTER_OFF_HAND_ATTACHMENT};
                return true;
            case 2:
                targets[count++] = {CHARACTER_MAIN_HAND_ATTACHMENT};
                targets[count++] = {CHARACTER_OFF_HAND_ATTACHMENT};
                return true;
            default:
                return false;
        }
    }

    bool DecodeBLP(std::string const& path, std::vector<CImVector>& pixels, uint32_t& width, uint32_t& height)
    {
        CBLPFile blp;
        std::string filename = path;
        if (filename.find('.') == std::string::npos)
            filename += ".blp";

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
        std::memcpy(pixels.data(), data, pixelCount * sizeof(CImVector));

        blp.Unlock(0);
        blp.Close();
        return true;
    }

    IDirect3DTexture9* GetTexture(IDirect3DDevice9* device, std::string const& path)
    {
        TextureResource& resource = s_textures[path];
        if (resource.texture || resource.loadAttempted)
            return resource.texture;

        resource.loadAttempted = true;
        std::vector<CImVector> pixels;
        uint32_t width  = 0;
        uint32_t height = 0;
        if (!DecodeBLP(path, pixels, width, height))
            return nullptr;

        if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                         &resource.texture, nullptr)) ||
            !resource.texture)
            return nullptr;

        D3DLOCKED_RECT locked{};
        if (FAILED(resource.texture->LockRect(0, &locked, nullptr, 0)))
        {
            resource.texture->Release();
            resource.texture = nullptr;
            return nullptr;
        }

        uint8_t const* source = reinterpret_cast<uint8_t const*>(pixels.data());
        uint8_t* destination  = static_cast<uint8_t*>(locked.pBits);
        std::size_t const rowBytes = static_cast<std::size_t>(width) * sizeof(CImVector);
        for (uint32_t row = 0; row < height; ++row)
            std::memcpy(destination + static_cast<std::size_t>(row) * locked.Pitch,
                        source + static_cast<std::size_t>(row) * rowBytes, rowBytes);
        resource.texture->UnlockRect(0);

        return resource.texture;
    }

    void TrimExpiredSamples(TrailInstance& instance, uint32_t now)
    {
        while (!instance.samples.empty() &&
               HasElapsed(now, instance.samples.front().capturedAtMs) &&
               ElapsedSince(now, instance.samples.front().capturedAtMs) > instance.edgeLifeSpanMs)
            instance.samples.erase(instance.samples.begin());
    }

    float Dot(C3Vector const& a, C3Vector const& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Length(C3Vector const& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    C3Vector NormalizeVec(C3Vector const& v)
    {
        float len = Length(v);
        return len > 0.000001f ? v * (1.0f / len) : C3Vector(0.0f, 0.0f, 0.0f);
    }

    bool CaptureSample(TrailInstance& instance, uint32_t now)
    {
        CGObject_C* object   = ResolveObject(instance.objectGuid);
        if (!object)
            return false;

        CM2Model* characterModel = object->GetObjectModel();
        if (!characterModel || !characterModel->IsLoaded(0, 0))
            return false;

        CM2Model* weaponModel = FindWeaponModel(instance, characterModel);
        if (!weaponModel)
            return false;

        TrailSample sample{};
        if (!GetTrailEndpoints(instance, weaponModel, sample.bottom, sample.top))
            return false;
        sample.capturedAtMs = now;

        if (!instance.samples.empty())
        {
            TrailSample const& previous = instance.samples.back();
            C3Vector const previousWidth = previous.top - previous.bottom;
            C3Vector const currentWidth  = sample.top - sample.bottom;
            if (Dot(previousWidth, currentWidth) < 0.0f)
                std::swap(sample.bottom, sample.top);

            float const bottomX = sample.bottom.x - previous.bottom.x;
            float const bottomY = sample.bottom.y - previous.bottom.y;
            float const bottomZ = sample.bottom.z - previous.bottom.z;
            float const topX    = sample.top.x - previous.top.x;
            float const topY    = sample.top.y - previous.top.y;
            float const topZ    = sample.top.z - previous.top.z;
            float const movementSquared = bottomX * bottomX + bottomY * bottomY + bottomZ * bottomZ +
                                          topX * topX + topY * topY + topZ * topZ;
            if (movementSquared < 0.000001f)
                return true;
        }

        if (instance.samples.size() == MAX_TRAIL_SAMPLES)
            instance.samples.erase(instance.samples.begin());
        instance.samples.push_back(sample);
        instance.lastSampleAtMs = now;

        return true;
    }

    float GetGlobalAlpha(TrailInstance const& instance, uint32_t now)
    {
        if (!HasElapsed(now, instance.emitUntilMs))
            return 1.0f;
        if (!instance.fadeOutMs)
            return 1.0f;

        float const elapsed = static_cast<float>(ElapsedSince(now, instance.emitUntilMs));
        return (std::max)(0.0f, 1.0f - elapsed / static_cast<float>(instance.fadeOutMs));
    }

    bool HasDrawableTrail(TrailInstance const& instance)
    {
        return instance.samples.size() >= 2 &&
               std::any_of(instance.texturePaths.begin(), instance.texturePaths.end(),
                           [](std::string const& path) { return !path.empty(); });
    }

    C3Vector InterpolatePosition(C3Vector const& start, C3Vector const& end, float amount)
    {
        return C3Vector(
            start.x + (end.x - start.x) * amount,
            start.y + (end.y - start.y) * amount,
            start.z + (end.z - start.z) * amount);
    }

    C3Vector CatmullRomPosition(C3Vector const& first, C3Vector const& second,
                                C3Vector const& third, C3Vector const& fourth, float amount)
    {
        float const amountSquared = amount * amount;
        float const amountCubed   = amountSquared * amount;
        C3Vector const constantTerm = second * 2.0f;
        C3Vector const linearTerm   = (third - first) * amount;
        C3Vector const squareTerm   = ((first * 2.0f - second * 5.0f) + (third * 4.0f - fourth)) * amountSquared;
        C3Vector const cubicTerm    = ((second * 3.0f - first) + (fourth - third * 3.0f)) * amountCubed;
        return ((constantTerm + linearTerm) + (squareTerm + cubicTerm)) * 0.5f;
    }

    TrailSample InterpolateStableSample(TrailSample const& first, TrailSample const& second,
                                        TrailSample const& third, TrailSample const& fourth, float amount)
    {
        C3Vector const firstCenter  = (first.bottom + first.top) * 0.5f;
        C3Vector const secondCenter = (second.bottom + second.top) * 0.5f;
        C3Vector const thirdCenter  = (third.bottom + third.top) * 0.5f;
        C3Vector const fourthCenter = (fourth.bottom + fourth.top) * 0.5f;
        C3Vector const center =
            CatmullRomPosition(firstCenter, secondCenter, thirdCenter, fourthCenter, amount);

        C3Vector const secondWidth = second.top - second.bottom;
        C3Vector thirdWidth        = third.top - third.bottom;
        if (Dot(secondWidth, thirdWidth) < 0.0f)
            thirdWidth = thirdWidth * -1.0f;

        float const widthLength = Length(secondWidth) + (Length(thirdWidth) - Length(secondWidth)) * amount;
        C3Vector widthDirection =
            InterpolatePosition(NormalizeVec(secondWidth), NormalizeVec(thirdWidth), amount);
        if (Length(widthDirection) <= 0.000001f)
            widthDirection = secondWidth;

        C3Vector const halfWidth = NormalizeVec(widthDirection) * (widthLength * 0.5f);
        TrailSample sample{};
        sample.bottom       = center - halfWidth;
        sample.top          = center + halfWidth;
        sample.capturedAtMs = second.capturedAtMs + static_cast<uint32_t>(
            static_cast<float>(third.capturedAtMs - second.capturedAtMs) * amount);
        return sample;
    }

    void BuildSmoothedSamples(TrailInstance const& instance, std::vector<TrailSample>& samples)
    {
        samples.assign(instance.samples.begin(), instance.samples.end());
        if (!std::isfinite(instance.smoothSampleAngle) || instance.smoothSampleAngle <= 0.0f || samples.size() < 3)
            return;

        std::vector<TrailSample> smoothed;
        smoothed.reserve((std::min)(MAX_RENDER_SAMPLES,
                                    (samples.size() - 1) * TRAIL_SMOOTHING_SUBDIVISIONS + 1));
        for (std::size_t index = 0; index + 1 < samples.size(); ++index)
        {
            TrailSample const& first  = index > 0 ? samples[index - 1] : samples[index];
            TrailSample const& second = samples[index];
            TrailSample const& third  = samples[index + 1];
            TrailSample const& fourth = index + 2 < samples.size() ? samples[index + 2] : samples[index + 1];
            for (std::size_t step = 0; step < TRAIL_SMOOTHING_SUBDIVISIONS; ++step)
            {
                if (smoothed.size() == MAX_RENDER_SAMPLES - 1)
                    break;
                float const amount = static_cast<float>(step) /
                                     static_cast<float>(TRAIL_SMOOTHING_SUBDIVISIONS);
                smoothed.push_back(InterpolateStableSample(first, second, third, fourth, amount));
            }
            if (smoothed.size() == MAX_RENDER_SAMPLES - 1)
                break;
        }
        smoothed.push_back(samples.back());
        samples.swap(smoothed);
    }

    void BuildVertices(TrailInstance const& instance, uint32_t now, std::size_t textureIndex,
                       C3Vector const& cameraPosition, std::vector<TrailVertex>& vertices)
    {
        vertices.clear();
        std::vector<TrailSample> samples;
        BuildSmoothedSamples(instance, samples);
        vertices.reserve(samples.size() * 2);

        float const globalAlpha = GetGlobalAlpha(instance, now);
        float const elapsedSeconds = static_cast<float>(ElapsedSince(now, instance.createdAtMs)) / 1000.0f;
        float const scaleU = std::isfinite(instance.textureScaleU[textureIndex]) &&
                                     instance.textureScaleU[textureIndex] != 0.0f
                                 ? instance.textureScaleU[textureIndex]
                                 : 1.0f;
        float const scaleV = std::isfinite(instance.textureScaleV[textureIndex]) &&
                                     instance.textureScaleV[textureIndex] != 0.0f
                                 ? instance.textureScaleV[textureIndex]
                                 : 1.0f;
        float const scrollU = std::isfinite(instance.textureScrollU[textureIndex])
                                  ? instance.textureScrollU[textureIndex] * elapsedSeconds
                                  : 0.0f;
        float const scrollV = std::isfinite(instance.textureScrollV[textureIndex])
                                  ? instance.textureScrollV[textureIndex] * elapsedSeconds
                                  : 0.0f;

        for (TrailSample const& sample : samples)
        {
            float const normalizedAge = (std::min)(1.0f, static_cast<float>(ElapsedSince(now, sample.capturedAtMs)) /
                                                            static_cast<float>(instance.edgeLifeSpanMs));
            float const edgeAlpha = 1.0f - normalizedAge;
            uint8_t const alpha = static_cast<uint8_t>(
                (std::max)(0.0f, (std::min)(1.0f, globalAlpha * edgeAlpha)) * 255.0f);
            D3DCOLOR const color = D3DCOLOR_ARGB(alpha, 255, 255, 255);
            float const u = normalizedAge * scaleU + scrollU;

            vertices.push_back({sample.bottom.x - cameraPosition.x, sample.bottom.y - cameraPosition.y,
                                sample.bottom.z - cameraPosition.z, color, u, scaleV + scrollV});
            vertices.push_back({sample.top.x - cameraPosition.x, sample.top.y - cameraPosition.y,
                                sample.top.z - cameraPosition.z, color, u, scrollV});
        }
    }

    D3DMATRIX ToD3DMatrix(C44Matrix const& source)
    {
        D3DMATRIX result{};
        result._11 = source.a0; result._12 = source.a1; result._13 = source.a2; result._14 = source.a3;
        result._21 = source.b0; result._22 = source.b1; result._23 = source.b2; result._24 = source.b3;
        result._31 = source.c0; result._32 = source.c1; result._33 = source.c2; result._34 = source.c3;
        result._41 = source.d0; result._42 = source.d1; result._43 = source.d2; result._44 = source.d3;
        return result;
    }

    void ConfigureRenderState(IDirect3DDevice9* device, C44Matrix const& view, C44Matrix const& projection)
    {
        D3DMATRIX identity{};
        identity._11 = 1.0f;
        identity._22 = 1.0f;
        identity._33 = 1.0f;
        identity._44 = 1.0f;

        device->SetTransform(D3DTS_WORLD, &identity);
        D3DMATRIX const d3dView       = ToD3DMatrix(view);
        D3DMATRIX const d3dProjection = ToD3DMatrix(projection);
        device->SetTransform(D3DTS_VIEW, &d3dView);
        device->SetTransform(D3DTS_PROJECTION, &d3dProjection);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    }

    void DrawTrails(IDirect3DDevice9* device, CGWorldFrame* worldFrame, uint32_t now,
                    C44Matrix const& view, C44Matrix const& projection)
    {
        bool const hasDrawable = std::any_of(s_instances.begin(), s_instances.end(), HasDrawableTrail);
        if (!hasDrawable)
            return;

        IDirect3DStateBlock9* state = nullptr;
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) || !state)
            return;

        ConfigureRenderState(device, view, projection);
        std::vector<TrailVertex> vertices;
        for (TrailInstance& instance : s_instances)
        {
            if (!HasElapsed(now, instance.activateAtMs))
                continue;

            if (!HasDrawableTrail(instance))
                continue;

            for (std::size_t textureIndex = 0; textureIndex < MAX_TRAIL_TEXTURES; ++textureIndex)
            {
                std::string const& path = instance.texturePaths[textureIndex];
                if (path.empty())
                    continue;

                IDirect3DTexture9* texture = GetTexture(device, path);
                if (!texture)
                    continue;

                BuildVertices(instance, now, textureIndex, worldFrame->m_camera->m_position, vertices);
                if (vertices.size() < 4)
                    continue;

                bool const scrolls = instance.textureScrollU[textureIndex] != 0.0f ||
                                     instance.textureScrollV[textureIndex] != 0.0f;
                device->SetSamplerState(0, D3DSAMP_ADDRESSU, scrolls ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
                device->SetSamplerState(0, D3DSAMP_ADDRESSV, scrolls ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
                device->SetTexture(0, texture);
                device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, static_cast<UINT>(vertices.size() - 2),
                                        vertices.data(), sizeof(TrailVertex));
            }
        }

        state->Apply();
        state->Release();
    }
} // namespace

void ReleaseResources()
{
    for (auto& entry : s_textures)
    {
        if (entry.second.texture)
            entry.second.texture->Release();
    }
    s_textures.clear();
    s_instances.clear();
}

void OnWorldRender(CGWorldFrame* worldFrame)
{
    if (s_instances.empty())
        return;

    IDirect3DDevice9* device = GetDirect3DDevice();
    if (!device || device->TestCooperativeLevel() != D3D_OK)
        return;

    D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetViewport(&viewport)) || viewport.Width == 0 || viewport.Height == 0)
        return;

    if (!worldFrame || !worldFrame->m_camera || !g_theGxDevicePtr)
        return;

    uint32_t const now = OsGetAsyncTimeMs();
    for (TrailInstance& instance : s_instances)
    {
        TrimExpiredSamples(instance, now);

        if (!HasElapsed(now, instance.emitUntilMs) && instance.lastSampleAtMs != now)
        {
            bool const captured = CaptureSample(instance, now);
            if (!captured && HasElapsed(now, instance.attachDeadlineMs))
                instance.expireAtMs = now;
        }
    }

    C44Matrix savedProjection = g_theGxDevicePtr->m_projection;
    C44Matrix savedView;
    GxXformView(savedView);

    worldFrame->m_camera->SetupWorldProjection(worldFrame->m_viewportDDC);
    C44Matrix const worldProjection = g_theGxDevicePtr->m_projection;
    C44Matrix worldView;
    GxXformView(worldView);

    DrawTrails(device, worldFrame, now, worldView, worldProjection);

    g_theGxDevicePtr->XformSetProjection(savedProjection);
    g_theGxDevicePtr->XformSetView(savedView);

    s_instances.erase(std::remove_if(s_instances.begin(), s_instances.end(), [now](TrailInstance const& instance) {
                          return HasElapsed(now, instance.expireAtMs) && instance.samples.empty();
                      }),
                      s_instances.end());
}

void Play(CGObject_C* object, uint32_t spellVisualKitId, WeaponTrailRow const& trail,
          WeaponTrailParamRow const& param)
{
    if (!object)
        return;

    std::array<HandTarget, 2> handTargets{};
    std::size_t handTargetCount = 0;
    if (!GetHandTargets(param.Hand, handTargets, handTargetCount))
        return;

    if (!object->m_obj)
        return;

    uint64_t const guid = object->m_obj->m_guid.m_guid;
    if (!guid)
        return;

    std::array<std::string, MAX_TRAIL_TEXTURES> texturePaths;
    bool hasTexture = false;
    for (std::size_t index = 0; index < MAX_TRAIL_TEXTURES; ++index)
    {
        texturePaths[index] = NormalizePath(trail.TexturePath[index]);
        hasTexture |= !texturePaths[index].empty();
    }
    if (!hasTexture)
        return;

    uint32_t const durationMs     = SecondsToMilliseconds(param.Duration, DEFAULT_DURATION_MS);
    uint32_t const fadeOutMs      = SecondsToMilliseconds(param.FadeOutTime, 0);
    uint32_t const edgeLifeSpanMs = SecondsToMilliseconds(param.EdgeLifeSpan, DEFAULT_EDGE_LIFESPAN_MS);
    uint32_t const initialDelayMs = SecondsToMilliseconds(param.InitialDelay, 0);
    uint32_t const now            = OsGetAsyncTimeMs();

    for (std::size_t targetIndex = 0; targetIndex < handTargetCount; ++targetIndex)
    {
        HandTarget const& target = handTargets[targetIndex];
        s_instances.erase(
            std::remove_if(s_instances.begin(), s_instances.end(), [guid, &target](TrailInstance const& item) {
                return item.objectGuid == guid && item.weaponAttachmentId == target.attachmentId;
            }),
            s_instances.end());
        if (s_instances.size() >= MAX_ACTIVE_TRAILS)
            s_instances.erase(s_instances.begin());

        TrailInstance instance{};
        instance.objectGuid         = guid;
        instance.weaponAttachmentId = target.attachmentId;
        instance.overrideAttachTop  = param.OverrideAttachTop;
        instance.overrideAttachBot  = param.OverrideAttachBot;
        instance.createdAtMs        = now;
        instance.activateAtMs       = now + initialDelayMs;
        instance.emitUntilMs        = instance.activateAtMs + durationMs;
        instance.expireAtMs         = instance.emitUntilMs + (std::max)(fadeOutMs, edgeLifeSpanMs);
        instance.attachDeadlineMs   = now + ATTACH_TIMEOUT_MS;
        instance.edgeLifeSpanMs     = edgeLifeSpanMs;
        instance.fadeOutMs          = fadeOutMs;
        instance.smoothSampleAngle  = param.SmoothSampleAngle;
        instance.texturePaths       = texturePaths;
        for (std::size_t index = 0; index < MAX_TRAIL_TEXTURES; ++index)
        {
            instance.textureScrollU[index] = trail.TextureScrollRateU[index];
            instance.textureScrollV[index] = trail.TextureScrollRateV[index];
            instance.textureScaleU[index]  = trail.TextureScaleU[index];
            instance.textureScaleV[index]  = trail.TextureScaleV[index];
        }
        instance.samples.reserve(MAX_TRAIL_SAMPLES);
        s_instances.push_back(std::move(instance));
    }
}

} // namespace WeaponTrailPlayback
} // namespace Duskhaven
