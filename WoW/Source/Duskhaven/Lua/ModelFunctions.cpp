#include "ModelFunctions.h"
#include "ClientLua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "WoW/Source/Ui/CharacterModelBase.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"

#include <cstdint>

namespace Duskhaven
{
    namespace Lua
    {
        namespace
        {
            bool LooksLikePath(const char* text)
            {
                if (!text)
                    return false;
                for (uint32_t i = 0; i < 240; ++i)
                {
                    char const c = text[i];
                    if (c == '\0')
                        return i > 0;
                    if (c < 0x20 || c > 0x7E)
                        return false;
                }
                return false;
            }

            void PushOptionalString(lua_State* L, const char* key, const char* value)
            {
                if (!LooksLikePath(value))
                    return;
                ClientLua::PushString(L, value);
                ClientLua::SetField(L, -2, key);
            }
        } // namespace

        int Script_Duskhaven_GetCreatureDisplayInfo(lua_State* L)
        {
            uint32_t const displayId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            if (displayId == 0)
                return 0;

            auto* display = g_creatureDisplayInfoDB.GetRecord(displayId);
            if (!display || display->m_modelID == 0)
                return 0;

            auto* model = g_creatureModelDataDB.GetRecord(display->m_modelID);
            if (!model || !model->m_modelName || !LooksLikePath(model->m_modelName))
                return 0;

            ClientLua::CreateTable(L, 0, 6);
            PushOptionalString(L, "ModelPath", model->m_modelName);
            PushOptionalString(L, "Texture1", display->m_textureVariation[0]);
            PushOptionalString(L, "Texture2", display->m_textureVariation[1]);
            PushOptionalString(L, "Texture3", display->m_textureVariation[2]);
            ClientLua::PushNumber(L, display->m_creatureModelScale);
            ClientLua::SetField(L, -2, "Scale");
            ClientLua::PushNumber(L, display->m_modelID);
            ClientLua::SetField(L, -2, "ModelId");
            return 1;
        }

        int Script_Duskhaven_GetModelPath(lua_State* L)
        {
            if (ClientLua::Type(L, 1) != LUA_TTABLE)
                return 0;

            ClientLua::RawGetI(L, 1, 0);
            CScriptObject* object = static_cast<CScriptObject*>(ClientLua::ToUserdata(L, -1));
            ClientLua::SetTop(L, -2);
            if (!object)
                return 0;

            if (object->IsA(CSimpleModel::GetObjectType()))
            {
                CSimpleModel* modelFrame = static_cast<CSimpleModel*>(object);
                if (modelFrame->m_model && modelFrame->m_model->m_shared)
                {
                    const char* path = modelFrame->m_model->m_shared->m_fileName;
                    if (path && LooksLikePath(path))
                    {
                        ClientLua::PushString(L, path);
                        return 1;
                    }
                }
            }
            return 0;
        }

        int Script_Duskhaven_SetModelDisplay(lua_State* L)
        {
            uint32_t const displayId = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            if (displayId == 0 || ClientLua::Type(L, 1) != LUA_TTABLE)
                return 0;

            ClientLua::RawGetI(L, 1, 0);
            CScriptObject* object = static_cast<CScriptObject*>(ClientLua::ToUserdata(L, -1));
            ClientLua::SetTop(L, -2);
            if (!object)
                return 0;

            bool applied = false;
            if (object->IsA(CGCharacterModelBase::GetObjectType()))
            {
                CGCharacterModelBase* charModel = static_cast<CGCharacterModelBase*>(object);
                CreatureStats stats{};
                stats.m_displayId[0] = displayId;
                charModel->SetUpModel(reinterpret_cast<CreatureStats_C*>(&stats));
                applied = true;
            }

            ClientLua::CreateTable(L, 0, 1);
            ClientLua::PushBoolean(L, applied ? 1 : 0);
            ClientLua::SetField(L, -2, "Applied");
            return 1;
        }

        void RegisterModelFunctions()
        {
            ClientLua::RegisterFunction("Duskhaven_GetCreatureDisplayInfo", Script_Duskhaven_GetCreatureDisplayInfo);
            ClientLua::RegisterFunction("Duskhaven_GetModelPath", Script_Duskhaven_GetModelPath);
            ClientLua::RegisterFunction("Duskhaven_SetModelDisplay", Script_Duskhaven_SetModelDisplay);
        }
    } // namespace Lua
} // namespace Duskhaven
