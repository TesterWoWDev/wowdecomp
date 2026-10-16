#pragma once

#include <d3d9.h>

class CM2Model;

namespace Duskhaven::Highlight::FindYourself::Outline
{
    void Apply();
    bool IsEnabled();
    void OnWorldRender();
    void BeginDrawBatch(CM2Model* model);
    void EndDrawBatch(CM2Model* model);
    void CapturePlayerDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex,
                           UINT numVertices, UINT startIndex, UINT primCount);
    void FinishOutlineFrame(IDirect3DDevice9* device = nullptr);
    void CleanupOutlineResources();
}
