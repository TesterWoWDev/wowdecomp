#include "WoW/Source/Duskhaven/Highlight/DrawBatch.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Storm/H/stpl.h"

namespace Duskhaven::Highlight
{
    namespace
    {
        TSGrowableArray<DrawBatchCallback> s_beginCallbacks;
        TSGrowableArray<DrawBatchCallback> s_endCallbacks;
    } // namespace

    CM2Model* GetModelParent(CM2Model* model)
    {
        if (!model)
            return nullptr;

        return model->m_attachParent;
    }

    bool IsModelOrChildOf(CM2Model* model, CM2Model* root)
    {
        if (!root)
            return false;

        __try
        {
            for (int hop = 0; model && hop < 8; ++hop, model = GetModelParent(model))
            {
                if (model == root)
                    return true;
            }
        }
        __except (1)
        {
        }

        return false;
    }

    void ApplyDrawBatch()
    {
    }

    void RegisterDrawBatchBegin(DrawBatchCallback callback)
    {
        if (callback)
            *s_beginCallbacks.New() = callback;
    }

    void RegisterDrawBatchEnd(DrawBatchCallback callback)
    {
        if (callback)
            *s_endCallbacks.New() = callback;
    }
}
