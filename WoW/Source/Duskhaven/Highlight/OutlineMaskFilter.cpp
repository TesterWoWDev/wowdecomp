#include "WoW/Source/Duskhaven/Highlight/OutlineMaskFilter.h"
#include "WoW/Source/Duskhaven/Highlight/DrawBatch.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include <cstdint>
#include <cstring>

namespace Duskhaven::Highlight
{
    namespace
    {
        bool HasPathPrefix(const char* path, const char* prefix)
        {
            return _strnicmp(path, prefix, strlen(prefix)) == 0;
        }

        DWORD GetRenderState(IDirect3DDevice9* device, D3DRENDERSTATETYPE state)
        {
            DWORD value = 0xFFFFFFFF;
            if (!device || FAILED(device->GetRenderState(state, &value)))
                return 0xFFFFFFFF;
            return value;
        }
    } // namespace

    const char* GetModelPathForDebug(CM2Model* model)
    {
        if (!model)
            return "<null>";

        __try
        {
            if (!model->m_shared)
                return "<no-shared>";

            return model->m_shared->m_fileName;
        }
        __except (1)
        {
        }

        return "<unreadable>";
    }

    bool IsExcludedOutlineAttachment(CM2Model* model)
    {
        const char* path = GetModelPathForDebug(model);
        if (!path)
            return false;

        return HasPathPrefix(path, "interface\\") || HasPathPrefix(path, "spells\\");
    }

    bool IsModelOrAllowedChildOf(CM2Model* model, CM2Model* root)
    {
        if (!IsModelOrChildOf(model, root))
            return false;

        return model == root || !IsExcludedOutlineAttachment(model);
    }

    bool IsTransparentNoDepthWriteDraw(IDirect3DDevice9* device)
    {
        return GetRenderState(device, D3DRS_ALPHABLENDENABLE) == TRUE &&
               GetRenderState(device, D3DRS_ZWRITEENABLE) == FALSE;
    }
}
