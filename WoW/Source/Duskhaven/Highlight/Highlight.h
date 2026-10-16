#pragma once

#include <d3d9.h>

class CM2Model;

namespace Duskhaven::Highlight
{
    void Initialize();
    void OnWorldRender();
    void BeginDrawBatch(CM2Model* model);
    void EndDrawBatch(CM2Model* model);
    void OnDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertexIndex, UINT minVertexIndex,
                                UINT numVertices, UINT startIndex, UINT primCount);
    void FinishFrame();
    void CleanupResources();
}
