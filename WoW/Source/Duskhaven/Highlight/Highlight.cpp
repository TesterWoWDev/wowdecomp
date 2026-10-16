#include "WoW/Source/Duskhaven/Highlight/Highlight.h"
#include "WoW/Source/Duskhaven/Highlight/DrawBatch.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/FindYourself.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/Outline.h"
#include "WoW/Source/Duskhaven/Highlight/NPCHighlight/NPCHighlight.h"
#include "WoW/Source/Duskhaven/Highlight/NPCHighlight/Outline.h"
#include "WoW/Source/Duskhaven/Highlight/TargetSelection.h"
#include "Engine/Source/Model2/CM2SceneRender.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"

namespace Duskhaven::Highlight
{
    void Initialize()
    {
        ApplyDrawBatch();
        FindYourself::Apply();
        NPCHighlight::Apply();
        TargetSelection::Apply();

        CM2SceneRender::RegisterDrawBatchBegin(BeginDrawBatch);
        CM2SceneRender::RegisterDrawBatchEnd(EndDrawBatch);
        CGxDeviceD3d::RegisterDrawIndexedPrimitiveCallback(OnDrawIndexedPrimitive);
    }

    void BeginDrawBatch(CM2Model* model)
    {
        FindYourself::Outline::BeginDrawBatch(model);
        NPCHighlight::Outline::BeginDrawBatch(model);
    }

    void EndDrawBatch(CM2Model* model)
    {
        FindYourself::Outline::EndDrawBatch(model);
        NPCHighlight::Outline::EndDrawBatch(model);
    }

    void OnDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex,
                                UINT numVertices, UINT startIndex, UINT primCount)
    {
        FindYourself::Outline::CapturePlayerDraw(device, type, baseVertexIndex, minVertexIndex, numVertices, startIndex, primCount);
        NPCHighlight::Outline::CaptureHoveredDraw(device, type, baseVertexIndex, minVertexIndex, numVertices, startIndex, primCount);
    }

    void OnWorldRender()
    {
        FindYourself::OnWorldRender();
        NPCHighlight::OnWorldRender();
        FinishFrame();
    }

    void FinishFrame()
    {
        FindYourself::Outline::FinishOutlineFrame();
        NPCHighlight::Outline::FinishOutlineFrame();
    }

    void CleanupResources()
    {
        FindYourself::Outline::CleanupOutlineResources();
        NPCHighlight::Outline::CleanupOutlineResources();
    }
}
