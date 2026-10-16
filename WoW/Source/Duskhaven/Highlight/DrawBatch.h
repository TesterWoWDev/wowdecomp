#pragma once

class CM2Model;

namespace Duskhaven::Highlight
{
    using DrawBatchCallback = void (*)(CM2Model* model);

    CM2Model* GetModelParent(CM2Model* model);
    bool IsModelOrChildOf(CM2Model* model, CM2Model* root);
    void ApplyDrawBatch();
    void RegisterDrawBatchBegin(DrawBatchCallback callback);
    void RegisterDrawBatchEnd(DrawBatchCallback callback);
}
