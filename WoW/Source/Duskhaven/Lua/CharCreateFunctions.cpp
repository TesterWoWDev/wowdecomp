#include "WoW/Source/Duskhaven/Lua/CharCreateFunctions.h"
#include "WoW/Source/Duskhaven/Character/ExtendedAnimationNames.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/SoundEntriesRec.h"
#include "WoW/Source/DB/Rec/AnimationDataRec.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

namespace Duskhaven
{
    namespace Lua
    {
        namespace
        {
            constexpr uint32_t CHARACTER_CREATE_PREVIEW_ATTACHMENT = 0;

            namespace Auras
            {
                constexpr uint32_t PeriodicDamage                = 3;
                constexpr uint32_t PeriodicHeal                  = 8;
                constexpr uint32_t ModStealth                    = 16;
                constexpr uint32_t ModInvisibility               = 18;
                constexpr uint32_t PeriodicEnergize              = 24;
                constexpr uint32_t ModShapeshift                 = 36;
                constexpr uint32_t ProcTriggerDamage             = 43;
                constexpr uint32_t PeriodicLeech                 = 53;
                constexpr uint32_t Transform                     = 56;
                constexpr uint32_t ModScale                      = 61;
                constexpr uint32_t PeriodicHealthFunnel          = 62;
                constexpr uint32_t PeriodicManaLeech             = 64;
                constexpr uint32_t Mounted                       = 78;
                constexpr uint32_t PeriodicDamagePct             = 89;
                constexpr uint32_t Ghost                         = 95;
                constexpr uint32_t PeriodicDummy                 = 226;
                constexpr uint32_t PeriodicTriggerSpellWithValue = 227;
                constexpr uint32_t ModScale2                     = 239;
            }

            namespace SpellAttachment
            {
                constexpr uint32_t Chest     = 15;
                constexpr uint32_t Breath    = 17;
                constexpr uint32_t Base      = 19;
                constexpr uint32_t Head      = 20;
                constexpr uint32_t LeftHand  = 21;
                constexpr uint32_t RightHand = 22;
                constexpr uint32_t Special1  = 23;
                constexpr uint32_t Special2  = 24;
                constexpr uint32_t Special3  = 25;
            }

            struct CharacterCreateAnimationStep
            {
                int animationId;
                uint32_t duration;
                float speed;
                int32_t transitionLead;
            };

            struct AnimationSequenceInfo
            {
                uint32_t duration  = 0;
                uint32_t blendTime = 0;
            };

            struct CharacterCreateSpellPreview
            {
                CM2Model* parent  = nullptr;
                CM2Model* missile = nullptr;
            };

            std::vector<CharacterCreateAnimationStep> CharacterCreateAnimationChain;
            std::vector<uint32_t> CharacterCreateModelAttachments;
            std::vector<uint32_t> CharacterCreateSpellEffectAttachments;
            size_t CharacterCreateAnimationChainIndex = 0;
            DWORD CharacterCreateAnimationChainEndsAt = 0;
            CM2Model* CharacterCreateAnimationChainModel = nullptr;
            CharacterCreateSpellPreview CharacterCreatePreview;
            C3Vector CharacterCreatePreviewMissileStart{};
            C3Vector CharacterCreatePreviewMissileCurrent{};
            C3Vector CharacterCreatePreviewMissileEnd{};
            C3Vector CharacterCreatePreviewTargetOffset{5.0f, 0.0f, -0.25f};
            DWORD CharacterCreatePreviewMissileStartedAt   = 0;
            uint32_t CharacterCreatePreviewMissileDuration = 0;
            char CharacterCreatePreviewMissilePath[260]    = {};
            CM2Model* CharacterCreateAuraPreviewModel      = nullptr;
            float CharacterCreateAuraPreviewOriginalRed    = 1.0f;
            float CharacterCreateAuraPreviewOriginalGreen  = 1.0f;
            float CharacterCreateAuraPreviewOriginalBlue   = 1.0f;
            float CharacterCreateAuraPreviewOriginalAlpha  = 1.0f;

            CM2Model* GetCharacterCreateModel()
            {
                if (!CCharacterCreation::m_character)
                    return nullptr;
                return CCharacterCreation::m_character->m_data.m_model;
            }

            const char* GetAnimationName(uint32_t animationId)
            {
                AnimationDataRec* row = g_animationDataDB.GetRecord(animationId);
                if (row && row->m_name && row->m_name[0])
                    return row->m_name;

                return GetExtendedAnimationName(animationId);
            }

            std::string GetSoundEntryPath(uint32_t soundId)
            {
                SoundEntriesRec* row = g_soundEntriesDB.GetRecord(soundId);
                if (!row)
                    return {};

                for (int i = 0; i < 10; ++i)
                {
                    const char* file = row->m_file[i];
                    if (!file || !file[0])
                        continue;

                    return (row->m_directoryBase && row->m_directoryBase[0]) ?
                        std::string(row->m_directoryBase) + "\\" + file :
                        std::string(file);
                }

                return {};
            }

            void ClearCharacterCreateAuraPreview()
            {
                if (CharacterCreateAuraPreviewModel && CharacterCreateAuraPreviewModel == GetCharacterCreateModel() &&
                    CharacterCreateAuraPreviewModel->IsLoaded(0, 0))
                {
                    CharacterCreateAuraPreviewModel->m_diffuseTintR = CharacterCreateAuraPreviewOriginalRed;
                    CharacterCreateAuraPreviewModel->m_diffuseTintG = CharacterCreateAuraPreviewOriginalGreen;
                    CharacterCreateAuraPreviewModel->m_diffuseTintB = CharacterCreateAuraPreviewOriginalBlue;
                    CharacterCreateAuraPreviewModel->m_alpha = CharacterCreateAuraPreviewOriginalAlpha;
                }

                CharacterCreateAuraPreviewModel         = nullptr;
                CharacterCreateAuraPreviewOriginalRed   = 1.0f;
                CharacterCreateAuraPreviewOriginalGreen = 1.0f;
                CharacterCreateAuraPreviewOriginalBlue  = 1.0f;
                CharacterCreateAuraPreviewOriginalAlpha = 1.0f;
            }

            bool SetCharacterCreateAuraPresentation(float red, float green, float blue, float alpha)
            {
                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0))
                    return false;

                ClearCharacterCreateAuraPreview();
                CharacterCreateAuraPreviewModel = model;
                CharacterCreateAuraPreviewOriginalRed   = model->m_diffuseTintR;
                CharacterCreateAuraPreviewOriginalGreen = model->m_diffuseTintG;
                CharacterCreateAuraPreviewOriginalBlue  = model->m_diffuseTintB;
                CharacterCreateAuraPreviewOriginalAlpha = model->m_alpha;

                model->m_diffuseTintR = red;
                model->m_diffuseTintG = green;
                model->m_diffuseTintB = blue;
                model->m_alpha        = alpha;
                return true;
            }

            bool PreviewCharacterCreateSpellAuraInternal(uint32_t spellId)
            {
                SpellRec spellRec;
                if (!spellId || !g_spellDB.GetRecord(spellId, &spellRec))
                    return false;
                SpellRec* spell = &spellRec;

                bool hasStealth      = false;
                bool hasInvisibility = false;
                bool hasGhost        = false;
                for (uint32_t const aura : spell->m_effectAura)
                {
                    hasStealth      = hasStealth || aura == Auras::ModStealth;
                    hasInvisibility = hasInvisibility || aura == Auras::ModInvisibility;
                    hasGhost        = hasGhost || aura == Auras::Ghost;
                }

                if (hasGhost)
                    return SetCharacterCreateAuraPresentation(0.55f, 0.75f, 1.0f, 0.55f);
                if (hasInvisibility)
                    return SetCharacterCreateAuraPresentation(0.8f, 0.9f, 1.0f, 0.22f);
                if (hasStealth)
                    return SetCharacterCreateAuraPresentation(1.0f, 1.0f, 1.0f, 0.45f);
                return false;
            }

            void ClearCharacterCreateSpellPreview()
            {
                CM2Model* const currentModel = GetCharacterCreateModel();
                bool const ownsCurrentPreview = CharacterCreatePreview.parent &&
                                                CharacterCreatePreview.parent == currentModel &&
                                                CharacterCreatePreview.parent->IsLoaded(0, 0);
                if (ownsCurrentPreview)
                    CharacterCreatePreview.parent->DetachAllChildrenById(CHARACTER_CREATE_PREVIEW_ATTACHMENT);

                CharacterCreatePreview                 = {};
                CharacterCreatePreviewMissileStartedAt = 0;
                CharacterCreatePreviewMissileDuration  = 0;
                CharacterCreatePreviewMissilePath[0]   = '\0';
            }

            void ClearCharacterCreateSpellVisuals(CM2Model* model)
            {
                if (!model || !model->IsLoaded(0, 0))
                    return;

                for (uint32_t const attachment : CharacterCreateSpellEffectAttachments)
                    model->DetachAllChildrenById(attachment);

                CharacterCreateSpellEffectAttachments.clear();
            }

            bool ClearCharacterCreateModelAttachments()
            {
                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0))
                    return false;

                for (uint32_t const attachmentId : CharacterCreateModelAttachments)
                    if (model->HasAttachment(attachmentId))
                        model->DetachAllChildrenById(attachmentId);

                CharacterCreateModelAttachments.clear();
                return true;
            }

            bool SetCharacterCreateWeaponsHidden(bool hidden)
            {
                if (!hidden)
                    return true;

                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0))
                    return false;

                for (uint32_t const attachmentId : {1u, 2u})
                    if (model->HasAttachment(attachmentId))
                        model->DetachAllChildrenById(attachmentId);
                return true;
            }

            bool SetCharacterCreateEquipmentHidden(bool hidden)
            {
                if (!hidden)
                    return true;

                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0))
                    return false;

                for (uint32_t attachmentId = 0; attachmentId <= 12; ++attachmentId)
                    if (model->HasAttachment(attachmentId))
                        model->DetachAllChildrenById(attachmentId);

                CharacterCreateModelAttachments.clear();
                return true;
            }

            bool AttachCharacterCreateModel(std::string const& path, uint32_t attachmentId)
            {
                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0) || path.empty())
                    return false;

                CM2Scene* scene = model->m_scene;
                if (!scene)
                    return false;

                CM2Model* child = scene->CreateModel(path.c_str(), 0);
                if (!child)
                    return false;

                child->AttachToParent(model, attachmentId, nullptr, 0);
                child->Release();
                CharacterCreateModelAttachments.push_back(attachmentId);
                return true;
            }

            bool AttachCharacterCreateSpellEffect(CM2Model* model, uint32_t effectId, uint32_t attachmentId)
            {
                if (!model || !effectId)
                    return false;

                SpellVisualEffectNameRec* effect = g_spellVisualEffectNameDB.GetRecord(effectId);
                if (!effect || !effect->m_fileName || !effect->m_fileName[0])
                    return false;

                CM2Scene* scene = model->m_scene;
                if (!scene)
                    return false;

                CM2Model* child = scene->CreateModel(effect->m_fileName, 0);
                if (!child)
                    return false;

                child->AttachToParent(model, attachmentId, nullptr, 0);
                child->Release();
                if (std::find(CharacterCreateSpellEffectAttachments.begin(), CharacterCreateSpellEffectAttachments.end(),
                              attachmentId) == CharacterCreateSpellEffectAttachments.end())
                {
                    CharacterCreateSpellEffectAttachments.push_back(attachmentId);
                }

                return true;
            }

            uint32_t GetSpellVisualKitId(SpellVisualRec const& visual, uint32_t stage)
            {
                switch (stage)
                {
                    case 0: return visual.m_precastKit;
                    case 1: return visual.m_castKit;
                    case 2: return visual.m_channelKit;
                    case 3: return visual.m_stateKit;
                    case 4: return visual.m_impactKit;
                    default: return 0;
                }
            }

            SpellVisualKitRec* GetSpellVisualKit(SpellVisualRec const& visual, uint32_t stage)
            {
                uint32_t const kitId = GetSpellVisualKitId(visual, stage);
                return kitId ? g_spellVisualKitDB.GetRecord(kitId) : nullptr;
            }

            std::string ResolveCharacterCreateSpellStageSoundPath(uint32_t spellId, uint32_t stage)
            {
                SpellRec spellRec;
                if (!spellId || !g_spellDB.GetRecord(spellId, &spellRec))
                    return {};
                SpellRec* spell = &spellRec;

                uint32_t const visualId = spell->m_spellVisualID[0] ? spell->m_spellVisualID[0] : spell->m_spellVisualID[1];
                SpellVisualRec* visual = visualId ? g_spellVisualDB.GetRecord(visualId) : nullptr;
                SpellVisualKitRec* kit = visual ? GetSpellVisualKit(*visual, stage) : nullptr;
                if (!kit || !kit->m_soundID)
                    return {};

                return GetSoundEntryPath(kit->m_soundID);
            }

            uint32_t PlayCharacterCreateSpellVisualInternal(uint32_t spellId, uint32_t stage)
            {
                SpellRec spellRec;
                if (!spellId || !g_spellDB.GetRecord(spellId, &spellRec))
                    return 0;
                SpellRec* spell = &spellRec;

                uint32_t const visualId = spell->m_spellVisualID[0] ? spell->m_spellVisualID[0] : spell->m_spellVisualID[1];
                SpellVisualRec* visual = visualId ? g_spellVisualDB.GetRecord(visualId) : nullptr;
                if (!visual)
                    return 0;

                CM2Model* model = GetCharacterCreateModel();
                if (!model || !model->IsLoaded(0, 0))
                    return 0;

                ClearCharacterCreateSpellVisuals(model);

                SpellVisualKitRec* kit = GetSpellVisualKit(*visual, stage);
                if (!kit)
                    return 0;

                uint32_t attachedEffects = 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_headEffect, SpellAttachment::Head) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_chestEffect, SpellAttachment::Chest) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_baseEffect, SpellAttachment::Base) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_leftHandEffect, SpellAttachment::LeftHand) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_rightHandEffect, SpellAttachment::RightHand) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_breathEffect, SpellAttachment::Breath) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_specialEffect[0], SpellAttachment::Special1) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_specialEffect[1], SpellAttachment::Special2) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_specialEffect[2], SpellAttachment::Special3) ? 1 : 0;
                attachedEffects += AttachCharacterCreateSpellEffect(model, kit->m_worldEffect, SpellAttachment::Base) ? 1 : 0;
                return attachedEffects;
            }

            AnimationSequenceInfo GetAnimationSequenceInfo(CM2Model* model, uint32_t animationId)
            {
                if (!model || !model->m_shared || !model->m_shared->m_data)
                    return {};

                M2Data* data = model->m_shared->m_data;
                for (uint32_t i = 0; i < data->m_sequences.m_count; ++i)
                {
                    const M2Sequence& seq = data->m_sequences[i];
                    if (seq.m_id == animationId)
                        return { seq.m_duration, seq.m_blendTime };
                }

                return {};
            }

            void ClearCharacterCreateAnimationChain()
            {
                CharacterCreateAnimationChain.clear();
                CharacterCreateAnimationChainIndex  = 0;
                CharacterCreateAnimationChainEndsAt = 0;
                CharacterCreateAnimationChainModel  = nullptr;
            }

            bool PlayCharacterCreateAnimationInternal(CM2Model* model, int animationId, float speed = 1.0f)
            {
                if (!model || animationId < 0 || !model->IsLoaded(0, 0))
                    return false;

                model->SetBoneSequence(static_cast<uint32_t>(-1), animationId, 0, 0, speed, 1, 1);
                return true;
            }

            bool PlayCurrentCharacterCreateAnimationChainEntry()
            {
                if (CharacterCreateAnimationChainIndex >= CharacterCreateAnimationChain.size())
                    return false;

                CM2Model* model = GetCharacterCreateModel();
                CharacterCreateAnimationStep const& step = CharacterCreateAnimationChain[CharacterCreateAnimationChainIndex];
                if (!PlayCharacterCreateAnimationInternal(model, step.animationId, step.speed))
                    return false;

                CharacterCreateAnimationChainModel = model;
                if (CharacterCreateAnimationChainIndex + 1 < CharacterCreateAnimationChain.size() || step.duration > 0)
                {
                    AnimationSequenceInfo const sequence =
                        GetAnimationSequenceInfo(model, static_cast<uint32_t>(step.animationId));
                    uint32_t const sequenceDuration =
                        step.duration > 0 ? step.duration : static_cast<uint32_t>(sequence.duration / step.speed);
                    uint32_t const duration             = sequenceDuration > 0 ? sequenceDuration : 1000;
                    uint32_t const blendTime            = static_cast<uint32_t>(sequence.blendTime / step.speed);
                    uint32_t const transitionLead       = step.transitionLead >= 0 ?
                                                              (std::min)(static_cast<uint32_t>(step.transitionLead), duration) :
                                                              (std::min)(blendTime, (std::min)(duration / 2, 250u));
                    CharacterCreateAnimationChainEndsAt = GetTickCount() + duration - transitionLead;
                }
                else
                    CharacterCreateAnimationChainEndsAt = 0;

                return true;
            }
        } // namespace

        static int PlayCharacterCreateAnimation(lua_State* L)
        {
            const int animationId = static_cast<int>(ClientLua::GetNumber(L, 1, -1));
            ClearCharacterCreateAnimationChain();
            CM2Model* model = GetCharacterCreateModel();
            ClientLua::PushBoolean(L, PlayCharacterCreateAnimationInternal(model, animationId));
            return 1;
        }

        static int PlayCharacterCreateAnimationChain(lua_State* L)
        {
            ClearCharacterCreateAnimationChain();

            int const count = ClientLua::GetTop(L);
            CharacterCreateAnimationChain.reserve(count / 4);
            for (int index = 1; index <= count; index += 4)
            {
                int const animationId = static_cast<int>(ClientLua::GetNumber(L, index, -1));
                if (animationId >= 0)
                {
                    uint32_t const duration      = static_cast<uint32_t>(ClientLua::GetNumber(L, index + 1, 0));
                    float const speed            = static_cast<float>(ClientLua::GetNumber(L, index + 2, 1));
                    int32_t const transitionLead = static_cast<int32_t>(ClientLua::GetNumber(L, index + 3, -1));
                    CharacterCreateAnimationChain.push_back(
                        {animationId, duration, speed > 0.0f ? speed : 1.0f, transitionLead});
                }
            }

            bool const started = !CharacterCreateAnimationChain.empty() && PlayCurrentCharacterCreateAnimationChainEntry();
            if (!started)
                ClearCharacterCreateAnimationChain();

            ClientLua::PushBoolean(L, started);
            return 1;
        }

        static int UpdateCharacterCreateAnimationChain(lua_State* L)
        {
            if (CharacterCreateAnimationChainEndsAt == 0)
                return 0;

            CM2Model* model = GetCharacterCreateModel();
            if (model != CharacterCreateAnimationChainModel)
            {
                ClearCharacterCreateAnimationChain();
                return 0;
            }

            if (static_cast<int32_t>(GetTickCount() - CharacterCreateAnimationChainEndsAt) >= 0)
            {
                ++CharacterCreateAnimationChainIndex;
                if (!PlayCurrentCharacterCreateAnimationChainEntry())
                {
                    PlayCharacterCreateAnimationInternal(model, 0);
                    ClearCharacterCreateAnimationChain();
                }
            }

            return 0;
        }

        static int GetCharacterCreateAnimations(lua_State* L)
        {
            CM2Model* model = GetCharacterCreateModel();
            if (!model || !model->IsLoaded(0, 0) || !model->m_shared || !model->m_shared->m_data)
            {
                ClientLua::CreateTable(L, 0, 0);
                return 1;
            }

            const unsigned int maximumAnimationId = static_cast<unsigned int>(ClientLua::GetNumber(L, 1, 4096));
            const int selectedAnimationId         = static_cast<int>(ClientLua::GetNumber(L, 2, -1));
            ClientLua::CreateTable(L, 0, 0);

            int resultIndex = 1;
            for (unsigned int animationId = 0; animationId <= maximumAnimationId; ++animationId)
            {
                if (!model->HasSequence(animationId) && static_cast<int>(animationId) != selectedAnimationId)
                    continue;

                char const* animationName = GetAnimationName(animationId);

                ClientLua::PushNumber(L, resultIndex++);
                ClientLua::CreateTable(L, 0, 2);
                ClientLua::PushNumber(L, animationId);
                ClientLua::SetField(L, -2, "id");
                ClientLua::PushString(L, animationName ? animationName : "Unknown");
                ClientLua::SetField(L, -2, "name");
                ClientLua::RawSet(L, -3);
            }

            return 1;
        }

        static int PlayCharacterCreateSpellVisual(lua_State* L)
        {
            uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t const stage   = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 1));
            ClientLua::PushNumber(L, spellId ? PlayCharacterCreateSpellVisualInternal(spellId, stage) : 0);
            return 1;
        }

        static int GetCharacterCreateSpellStageSoundPath(lua_State* L)
        {
            uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t const stage   = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 1));
            ClientLua::PushString(L, spellId ? ResolveCharacterCreateSpellStageSoundPath(spellId, stage).c_str() : "");
            return 1;
        }

        static int GetCharacterCreateSpellCastInfo(lua_State* L)
        {
            uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            SpellRec spellRec;
            SpellRec* spell = (spellId && g_spellDB.GetRecord(spellId, &spellRec)) ? &spellRec : nullptr;
            if (!spell)
            {
                ClientLua::CreateTable(L, 0, 0);
                return 1;
            }

            uint32_t const visualId = spell->m_spellVisualID[0] ? spell->m_spellVisualID[0] : spell->m_spellVisualID[1];
            SpellVisualRec* visual = visualId ? g_spellVisualDB.GetRecord(visualId) : nullptr;

            ClientLua::CreateTable(L, 0, 24);
            int32_t const castTime = SpellRec::GetCastTime(spell, 0, 0, 1);
            ClientLua::PushNumber(L, castTime > 0 ? castTime : 0);
            ClientLua::SetField(L, -2, "castTime");
            constexpr uint32_t ATTR1_CHANNELED_1 = 0x4;
            constexpr uint32_t ATTR1_CHANNELED_2 = 0x8;
            ClientLua::PushBoolean(
                L, (spell->m_attributesEx & (ATTR1_CHANNELED_1 | ATTR1_CHANNELED_2)) != 0);
            ClientLua::SetField(L, -2, "channeled");
            ClientLua::PushNumber(L, spell->m_speed);
            ClientLua::SetField(L, -2, "missileSpeed");

            struct StageInfo
            {
                char const* name;
                uint32_t stage;
            };
            StageInfo const stages[] = {
                {"precast", 0},
                {"cast", 1},
                {"channel", 2},
                {"state", 3},
            };

            if (!visual)
            {
                for (StageInfo const& stage : stages)
                {
                    std::string const startName = std::string(stage.name) + "StartAnim";
                    std::string const animName  = std::string(stage.name) + "Anim";
                    ClientLua::PushNumber(L, 0);
                    ClientLua::SetField(L, -2, startName.c_str());
                    ClientLua::PushNumber(L, 0);
                    ClientLua::SetField(L, -2, animName.c_str());
                }
                return 1;
            }

            ClientLua::PushBoolean(L, visual->m_hasMissile != 0 && visual->m_missileModel != 0);
            ClientLua::SetField(L, -2, "hasMissile");
            ClientLua::PushNumber(L, visual->m_missileAttachment);
            ClientLua::SetField(L, -2, "missileAttachment");
            ClientLua::PushNumber(L, visual->m_missileDestinationAttachment);
            ClientLua::SetField(L, -2, "missileDestinationAttachment");
            ClientLua::PushBoolean(L, visual->m_targetImpactKit != 0);
            ClientLua::SetField(L, -2, "hasTargetImpact");

            SpellVisualEffectNameRec* missileEffect = visual->m_missileModel ?
                g_spellVisualEffectNameDB.GetRecord(visual->m_missileModel) : nullptr;
            ClientLua::PushString(L, missileEffect && missileEffect->m_fileName ? missileEffect->m_fileName : "");
            ClientLua::SetField(L, -2, "missileModelPath");

            for (StageInfo const& stage : stages)
            {
                SpellVisualKitRec* kit      = GetSpellVisualKit(*visual, stage.stage);
                std::string const startName = std::string(stage.name) + "StartAnim";
                std::string const animName  = std::string(stage.name) + "Anim";
                ClientLua::PushNumber(L, kit ? kit->m_startAnimID : 0);
                ClientLua::SetField(L, -2, startName.c_str());
                ClientLua::PushNumber(L, kit ? kit->m_animID : 0);
                ClientLua::SetField(L, -2, animName.c_str());
            }
            return 1;
        }

        static int GetCharacterCreateSpellPreviewInfo(lua_State* L)
        {
            uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            ClientLua::CreateTable(L, 0, 8);
            SpellRec spellRec;
            SpellRec* spell = (spellId && g_spellDB.GetRecord(spellId, &spellRec)) ? &spellRec : nullptr;
            if (!spell)
                return 1;

            uint32_t const visualId = spell->m_spellVisualID[0] ? spell->m_spellVisualID[0] : spell->m_spellVisualID[1];
            SpellVisualRec* visual = visualId ? g_spellVisualDB.GetRecord(visualId) : nullptr;

            bool hasStealth      = false;
            bool hasInvisibility = false;
            bool hasShapeshift   = false;
            bool hasTransform    = false;
            bool hasScale        = false;
            bool hasMounted      = false;
            bool hasGhost        = false;
            ClientLua::CreateTable(L, 0, 3);
            int auraResultIndex = 1;
            for (uint32_t effectIndex = 0; effectIndex < 3; ++effectIndex)
            {
                uint32_t const aura = spell->m_effectAura[effectIndex];
                if (!aura)
                    continue;

                hasStealth      = hasStealth || aura == Auras::ModStealth;
                hasInvisibility = hasInvisibility || aura == Auras::ModInvisibility;
                hasShapeshift   = hasShapeshift || aura == Auras::ModShapeshift;
                hasTransform    = hasTransform || aura == Auras::Transform;
                hasScale        = hasScale || aura == Auras::ModScale || aura == Auras::ModScale2;
                hasMounted      = hasMounted || aura == Auras::Mounted;
                hasGhost        = hasGhost || aura == Auras::Ghost;

                ClientLua::PushNumber(L, auraResultIndex++);
                ClientLua::CreateTable(L, 0, 5);
                ClientLua::PushNumber(L, effectIndex);
                ClientLua::SetField(L, -2, "effectIndex");
                ClientLua::PushNumber(L, aura);
                ClientLua::SetField(L, -2, "auraType");
                ClientLua::PushNumber(L, static_cast<int32_t>(spell->m_effectBasePoints[effectIndex]) + 1);
                ClientLua::SetField(L, -2, "amount");
                ClientLua::PushNumber(L, spell->m_effectMiscValue[effectIndex]);
                ClientLua::SetField(L, -2, "miscValue");
                ClientLua::PushNumber(L, spell->m_effectMiscValueB[effectIndex]);
                ClientLua::SetField(L, -2, "miscValueB");
                ClientLua::RawSet(L, -3);
            }
            ClientLua::SetField(L, -2, "auraEffects");

            auto setAuraFlag = [L](char const* field, bool value)
            {
                ClientLua::PushBoolean(L, value);
                ClientLua::SetField(L, -2, field);
            };
            setAuraFlag("hasStealth", hasStealth);
            setAuraFlag("hasInvisibility", hasInvisibility);
            setAuraFlag("hasShapeshift", hasShapeshift);
            setAuraFlag("hasTransform", hasTransform);
            setAuraFlag("hasScale", hasScale);
            setAuraFlag("hasMounted", hasMounted);
            setAuraFlag("hasGhost", hasGhost);
            ClientLua::PushNumber(L, visual ? visual->m_animEventSoundID : 0);
            ClientLua::SetField(L, -2, "animationEventSound");
            ClientLua::PushNumber(L, visual ? visual->m_missileSound : 0);
            ClientLua::SetField(L, -2, "missileSound");
            return 1;
        }

        static int PreviewCharacterCreateSpellAura(lua_State* L)
        {
            uint32_t const spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            ClientLua::PushBoolean(L, spellId && PreviewCharacterCreateSpellAuraInternal(spellId));
            return 1;
        }

        static int ClearCharacterCreateAuraPreviewLua(lua_State* L)
        {
            ClearCharacterCreateAuraPreview();
            return 0;
        }

        static int SetCharacterCreateSpellPreviewTargetOffset(lua_State* L)
        {
            CharacterCreatePreviewTargetOffset = {
                static_cast<float>(ClientLua::GetNumber(L, 1, CharacterCreatePreviewTargetOffset.x)),
                static_cast<float>(ClientLua::GetNumber(L, 2, CharacterCreatePreviewTargetOffset.y)),
                static_cast<float>(ClientLua::GetNumber(L, 3, CharacterCreatePreviewTargetOffset.z)),
            };
            return 0;
        }

        static int GetCharacterCreateSceneDebugInfo(lua_State* L)
        {
            ClientLua::CreateTable(L, 0, 32);

            auto setNumber = [L](char const* field, double value)
            {
                ClientLua::PushNumber(L, value);
                ClientLua::SetField(L, -2, field);
            };

            setNumber("targetOffsetX", CharacterCreatePreviewTargetOffset.x);
            setNumber("targetOffsetY", CharacterCreatePreviewTargetOffset.y);
            setNumber("targetOffsetZ", CharacterCreatePreviewTargetOffset.z);
            setNumber("missileStartX", CharacterCreatePreviewMissileStart.x);
            setNumber("missileStartY", CharacterCreatePreviewMissileStart.y);
            setNumber("missileStartZ", CharacterCreatePreviewMissileStart.z);
            setNumber("missileCurrentX", CharacterCreatePreviewMissileCurrent.x);
            setNumber("missileCurrentY", CharacterCreatePreviewMissileCurrent.y);
            setNumber("missileCurrentZ", CharacterCreatePreviewMissileCurrent.z);
            setNumber("missileEndX", CharacterCreatePreviewMissileEnd.x);
            setNumber("missileEndY", CharacterCreatePreviewMissileEnd.y);
            setNumber("missileEndZ", CharacterCreatePreviewMissileEnd.z);
            ClientLua::PushBoolean(L, CharacterCreatePreview.missile != nullptr);
            ClientLua::SetField(L, -2, "missileActive");

            return 1;
        }

        static int ShowCharacterCreateSpellPreviewPath(lua_State* L)
        {
            ClearCharacterCreateSpellPreview();
            ClientLua::PushNumber(L, 0);
            return 1;
        }

        static int StartCharacterCreateSpellMissile(lua_State* L)
        {
            ClearCharacterCreateSpellPreview();
            ClientLua::PushNumber(L, 0);
            return 1;
        }

        static int UpdateCharacterCreateSpellPreview(lua_State* L)
        {
            ClearCharacterCreateSpellPreview();
            ClientLua::PushBoolean(L, false);
            return 1;
        }

        static int ClearCharacterCreateSpellPreviewLua(lua_State* L)
        {
            ClearCharacterCreateSpellPreview();
            return 0;
        }

        static int ClearCharacterCreateSpellVisualsLua(lua_State* L)
        {
            ClearCharacterCreateSpellVisuals(GetCharacterCreateModel());
            return 0;
        }

        static int ClearCharacterCreateModelAttachmentsLua(lua_State* L)
        {
            ClientLua::PushBoolean(L, ClearCharacterCreateModelAttachments());
            return 1;
        }

        static int SetCharacterCreateWeaponsHiddenLua(lua_State* L)
        {
            ClientLua::PushBoolean(L, SetCharacterCreateWeaponsHidden(ClientLua::ToBoolean(L, 1) != 0));
            return 1;
        }

        static int SetCharacterCreateEquipmentHiddenLua(lua_State* L)
        {
            ClientLua::PushBoolean(L, SetCharacterCreateEquipmentHidden(ClientLua::ToBoolean(L, 1) != 0));
            return 1;
        }

        static int AttachCharacterCreateModelLua(lua_State* L)
        {
            std::string const path      = ClientLua::GetString(L, 1, "");
            uint32_t const attachmentId = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            ClientLua::PushBoolean(L, AttachCharacterCreateModel(path, attachmentId));
            return 1;
        }

        static int GetHairStyleSelection(lua_State* L)
        {
            if (!CCharacterCreation::m_character) return 0;
            ClientLua::PushNumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId);
            return 1;
        }

        static int GetHairColorSelection(lua_State* L)
        {
            if (!CCharacterCreation::m_character) return 0;
            ClientLua::PushNumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId);
            return 1;
        }

        static int GetFacialHairSelection(lua_State* L)
        {
            if (!CCharacterCreation::m_character) return 0;
            ClientLua::PushNumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId);
            return 1;
        }

        static int GetSkinColorSelection(lua_State* L)
        {
            if (!CCharacterCreation::m_character) return 0;
            ClientLua::PushNumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_skinID);
            return 1;
        }

        static int GetFaceSelection(lua_State* L)
        {
            if (!CCharacterCreation::m_character) return 0;
            ClientLua::PushNumber(L, CCharacterCreation::m_character->m_data.m_preferences.m_faceId);
            return 1;
        }

        void RegisterCharCreateFunctions()
        {
            ClientLua::RegisterFunction("PlayCharacterCreateAnimation", PlayCharacterCreateAnimation);
            ClientLua::RegisterFunction("PlayCharacterCreateAnimationChain", PlayCharacterCreateAnimationChain);
            ClientLua::RegisterFunction("UpdateCharacterCreateAnimationChain", UpdateCharacterCreateAnimationChain);
            ClientLua::RegisterFunction("GetCharacterCreateAnimations", GetCharacterCreateAnimations);
            ClientLua::RegisterFunction("PlayCharacterCreateSpellVisual", PlayCharacterCreateSpellVisual);
            ClientLua::RegisterFunction("GetCharacterCreateSpellStageSoundPath", GetCharacterCreateSpellStageSoundPath);
            ClientLua::RegisterFunction("GetCharacterCreateSpellCastInfo", GetCharacterCreateSpellCastInfo);
            ClientLua::RegisterFunction("GetCharacterCreateSpellPreviewInfo", GetCharacterCreateSpellPreviewInfo);
            ClientLua::RegisterFunction("PreviewCharacterCreateSpellAura", PreviewCharacterCreateSpellAura);
            ClientLua::RegisterFunction("ClearCharacterCreateAuraPreview", ClearCharacterCreateAuraPreviewLua);
            ClientLua::RegisterFunction("SetCharacterCreateSpellPreviewTargetOffset", SetCharacterCreateSpellPreviewTargetOffset);
            ClientLua::RegisterFunction("GetCharacterCreateSceneDebugInfo", GetCharacterCreateSceneDebugInfo);
            ClientLua::RegisterFunction("ShowCharacterCreateSpellPreviewPath", ShowCharacterCreateSpellPreviewPath);
            ClientLua::RegisterFunction("StartCharacterCreateSpellMissile", StartCharacterCreateSpellMissile);
            ClientLua::RegisterFunction("UpdateCharacterCreateSpellPreview", UpdateCharacterCreateSpellPreview);
            ClientLua::RegisterFunction("ClearCharacterCreateSpellPreview", ClearCharacterCreateSpellPreviewLua);
            ClientLua::RegisterFunction("ClearCharacterCreateSpellVisuals", ClearCharacterCreateSpellVisualsLua);
            ClientLua::RegisterFunction("ClearCharacterCreateModelAttachments", ClearCharacterCreateModelAttachmentsLua);
            ClientLua::RegisterFunction("SetCharacterCreateWeaponsHidden", SetCharacterCreateWeaponsHiddenLua);
            ClientLua::RegisterFunction("SetCharacterCreateEquipmentHidden", SetCharacterCreateEquipmentHiddenLua);
            ClientLua::RegisterFunction("AttachCharacterCreateModel", AttachCharacterCreateModelLua);
            ClientLua::RegisterFunction("GetHairStyleSelection", GetHairStyleSelection);
            ClientLua::RegisterFunction("GetHairColorSelection", GetHairColorSelection);
            ClientLua::RegisterFunction("GetFacialHairSelection", GetFacialHairSelection);
            ClientLua::RegisterFunction("GetSkinColorSelection", GetSkinColorSelection);
            ClientLua::RegisterFunction("GetFaceSelection", GetFaceSelection);
        }
    } // namespace Lua
} // namespace Duskhaven
