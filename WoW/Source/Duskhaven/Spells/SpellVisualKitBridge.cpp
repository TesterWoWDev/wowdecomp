#include "SpellVisualKitBridge.h"
#include "WeaponTrailPlayback.h"
#include "WoW/Source/Duskhaven/DBC/Defs/ExtraSpellVisualKitEffect.h"
#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrail.h"
#include "WoW/Source/Duskhaven/DBC/Defs/WeaponTrailParam.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"

namespace Duskhaven
{
namespace SpellVisualKitBridge
{
    namespace
    {
        constexpr uint32_t EXTRA_EFFECT_TYPE_WEAPON_TRAIL = 6;

        void PlayWeaponTrail(CGObject_C* object, uint32_t kitId, ExtraSpellVisualKitEffectRow const& extra)
        {
            WeaponTrailParamRow* param = WeaponTrailParamDB::Get(extra.Effect);
            if (!param)
                return;

            WeaponTrailRow* trail = WeaponTrailDB::Get(param->WeaponTrailID);
            if (!trail)
                return;

            WeaponTrailPlayback::Play(object, kitId, *trail, *param);
        }
    } // namespace

    void PlayExtraEffects(CGObject_C* object, uint32_t kitId)
    {
        for (uint32_t index = 0; index < ExtraSpellVisualKitEffectDB::GetNumRows(); ++index)
        {
            ExtraSpellVisualKitEffectRow* extra = ExtraSpellVisualKitEffectDB::GetRowByIndex(index);
            if (!extra || extra->ParentSpellVisualKitID != kitId ||
                extra->EffectType != EXTRA_EFFECT_TYPE_WEAPON_TRAIL)
                continue;

            PlayWeaponTrail(object, kitId, *extra);
        }
    }
} // namespace SpellVisualKitBridge
} // namespace Duskhaven
