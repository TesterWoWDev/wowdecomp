#pragma once

#include <d3d9.h>

class CM2Model;

namespace Duskhaven::Highlight
{
    const char* GetModelPathForDebug(CM2Model* model);
    bool IsExcludedOutlineAttachment(CM2Model* model);
    bool IsModelOrAllowedChildOf(CM2Model* model, CM2Model* root);
    bool IsTransparentNoDepthWriteDraw(IDirect3DDevice9* device);
}
