#include "WoW/Source/Duskhaven/Camera/CameraFade.h"
#include "WoW/Source/Duskhaven/CVars/DuskhavenCVars.h"
#include "WoW/Source/WorldClient/World.h"
#include "Engine/Source/Model2/M2Model.h"
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

namespace Duskhaven
{
    namespace CameraFade
    {
        static constexpr float FADE_SPEED = 0.25f;
        static constexpr float ALPHA_THRESHOLD = 0.01f;
        static constexpr int32_t CLEANUP_INTERVAL = 2;

        static std::unordered_map<CM2Model*, float> s_modelsOriginalAlphas;
        static std::unordered_set<CM2Model*> s_modelsCurrent;
        static std::unordered_set<CM2Model*> s_modelsBeingFaded;
        static int32_t s_cleanupTimer = 0;
        static bool s_collisionCheck = false;

        bool IsCollisionCheckActive()
        {
            return s_collisionCheck;
        }

        bool IsModelCurrent(CM2Model* model)
        {
            return s_modelsCurrent.find(model) != s_modelsCurrent.end();
        }

        void OnModelRelease(CM2Model* model)
        {
            s_modelsCurrent.erase(model);
            s_modelsBeingFaded.erase(model);
            s_modelsOriginalAlphas.erase(model);
        }

        int32_t IntersectCamera(const C3Vector& start, const C3Vector& end, C3Vector* hit, float* distance, uint32_t flags)
        {
            if (Duskhaven::CVars::g_modelFadeAlpha >= 1.0f)
            {
                return World::Intersect(start, end, hit, distance, flags, nullptr);
            }

            World::HitTestDebug debug = {};
            s_collisionCheck = true;
            int32_t fadeHit = World::Intersect(start, end, hit, distance, flags | 1, &debug);
            s_collisionCheck = false;

            if (fadeHit)
            {
                if (debug.m_type == 1 && debug.m_numModels > 0)
                {
                    CM2Model* model = debug.m_models[0].m_model;
                    if (model)
                    {
                        s_modelsCurrent.insert(model);
                        s_modelsBeingFaded.insert(model);

                        s_modelsOriginalAlphas.emplace(model, model->m_alpha);
                        const float targetAlpha = Duskhaven::CVars::g_modelFadeAlpha;
                        model->m_alpha += (targetAlpha - model->m_alpha) * FADE_SPEED;

                        return 0;
                    }
                }
            }

            if (++s_cleanupTimer > CLEANUP_INTERVAL)
            {
                s_cleanupTimer = 0;
                for (auto it = s_modelsBeingFaded.begin(); it != s_modelsBeingFaded.end();)
                {
                    CM2Model* model = *it;
                    if (!s_modelsCurrent.count(model))
                    {
                        auto itAlpha = s_modelsOriginalAlphas.find(model);
                        if (itAlpha != s_modelsOriginalAlphas.end())
                        {
                            const float originalAlpha = itAlpha->second;
                            model->m_alpha += (originalAlpha - model->m_alpha) * FADE_SPEED;

                            if (std::abs(model->m_alpha - originalAlpha) < ALPHA_THRESHOLD)
                            {
                                model->m_alpha = originalAlpha;
                                s_modelsOriginalAlphas.erase(itAlpha);
                                it = s_modelsBeingFaded.erase(it);
                                continue;
                            }
                        }
                    }
                    ++it;
                }
                s_modelsCurrent.clear();
            }

            return World::Intersect(start, end, hit, distance, flags & ~1u, nullptr);
        }
    }
}
