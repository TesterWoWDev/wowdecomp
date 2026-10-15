#include "WoW/Source/Duskhaven/Lua/PlayerFunctions.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "WoW/Source/Duskhaven/Character/CharacterDefines.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "Engine/Source/Os/TimeManager.h"

namespace Duskhaven
{
    namespace Lua
    {
        int Script_GetShapeshiftFormID(lua_State* L)
        {
            CGPlayer_C* activePlayer = ClntObjMgrGetActivePlayerObj();
            if (activePlayer)
            {
                ClientLua::PushNumber(L, activePlayer->GetShapeshiftForm());
                return 1;
            }

            ClientLua::PushNumber(L, 0);
            return 1;
        }

        int Script_GetActiveSpec(lua_State* L)
        {
            ClientLua::PushNumber(L, Character::GetCharActiveSpec());
            return 1;
        }

        int Script_SetActiveSpec(lua_State* L)
        {
            if (ClientLua::IsNumber(L, 1))
            {
                uint32_t const newSpec = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
                Character::SetCharActiveSpec(newSpec);
            }

            return 0;
        }

        int Script_GetMasteryRating(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (!player || !player->m_player)
                return 0;

            int32_t crMastery = static_cast<int32_t>(player->m_player->m_playerFieldCombatRating1[24]);
            int activeSpecIndex = Character::SpecToIndex(Character::GetCharActiveSpec());
            uint32_t masteryAmount = Character::GetMasteryAmount() + crMastery;
            float rating = Character::GetMasteryRatingSpec(activeSpecIndex);
            if (rating <= 0.0f)
                rating = 1.0f;
            float pct = Character::GetMasteryForSpec(activeSpecIndex) + (static_cast<float>(crMastery) / rating);

            ClientLua::PushNumber(L, masteryAmount);
            ClientLua::PushNumber(L, pct);
            return 2;
        }

        int Script_SetMasteryRatings(lua_State* L)
        {
            float num1 = static_cast<float>(ClientLua::GetNumber(L, 1, 1.0));
            float num2 = static_cast<float>(ClientLua::GetNumber(L, 2, 1.0));
            float num3 = static_cast<float>(ClientLua::GetNumber(L, 3, 1.0));
            float num4 = static_cast<float>(ClientLua::GetNumber(L, 4, 1.0));

            Character::SetMasteryRatingSpec(0, num1 != 0.0f ? num1 : 1.0f);
            Character::SetMasteryRatingSpec(1, num2 != 0.0f ? num2 : 1.0f);
            Character::SetMasteryRatingSpec(2, num3 != 0.0f ? num3 : 1.0f);
            Character::SetMasteryRatingSpec(3, num4 != 0.0f ? num4 : 1.0f);

            return 0;
        }

        int Script_UpdateMasteryAmount(lua_State* L)
        {
            Character::SetMasteryAmount(static_cast<uint32_t>(ClientLua::GetNumber(L, 1)));
            Character::SetMasteryPct(static_cast<float>(ClientLua::GetNumber(L, 2)));

            return 0;
        }

        int Script_UpdateChargeInfoForSpell(lua_State* L)
        {
            uint32_t spellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            Character::SpellCharge temp;

            temp.currentCharges = static_cast<uint8_t>(ClientLua::GetNumber(L, 2));
            temp.maxCharges = static_cast<uint8_t>(ClientLua::GetNumber(L, 3));
            temp.async = OsGetAsyncTimeMs();
            temp.remainingCooldown = static_cast<uint32_t>(ClientLua::GetNumber(L, 4));
            temp.cooldown = static_cast<uint32_t>(ClientLua::GetNumber(L, 5));

            auto it = Character::s_spellChargeMap.find(spellID);
            if (it != Character::s_spellChargeMap.end())
            {
                if (!temp.cooldown)
                    temp.cooldown = it->second.cooldown;

                it->second = temp;
            }
            else
            {
                Character::s_spellChargeMap[spellID] = temp;
            }

            return 0;
        }

        int Script_GetChargeInfoForSpell(lua_State* L)
        {
            uint32_t spellID = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            auto it = Character::s_spellChargeMap.find(spellID);
            if (it == Character::s_spellChargeMap.end())
            {
                ClientLua::PushNil(L);
                return 1;
            }

            const Character::SpellCharge& row = it->second;
            ClientLua::PushNumber(L, row.currentCharges);
            ClientLua::PushNumber(L, row.maxCharges);
            ClientLua::PushNumber(L, row.remainingCooldown);
            ClientLua::PushNumber(L, row.cooldown);
            return 4;
        }

        int Script_GetCombatRatingMult(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            uint32_t rating = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            float value = 0.0f;

            if (player && player->m_unit && rating >= 1)
            {
                uint32_t level = player->m_unit->m_unitFieldLevel;
                value = GetGameTableValue(1, level - 1, rating - 1);
            }

            ClientLua::PushNumber(L, value);
            return 1;
        }

        int Script_GetCombatRatingScalar(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            uint32_t rating = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
            float value = 0.0f;

            if (player && player->m_unit && rating >= 1)
            {
                uint32_t classID = player->m_unit->m_unitFieldBytes0[1];
                value = GetGameTableValue(11, rating, classID - 1);
            }

            ClientLua::PushNumber(L, value);
            return 1;
        }

        int Script_GetModCastSpeed(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (player && player->m_unit)
                ClientLua::PushNumber(L, player->m_unit->m_unitModCastSpeed);
            else
                ClientLua::PushNumber(L, 0.0f);

            return 1;
        }

        int Script_GetWeaponAP(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            uint32_t attackType = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));

            if (player && player->m_player && attackType < 3)
            {
                const int32_t* weaponBonusAP = reinterpret_cast<const int32_t*>(&player->m_player->m_playerFieldGlyphs1[3]);
                ClientLua::PushNumber(L, weaponBonusAP[attackType]);
            }
            else
            {
                ClientLua::PushNumber(L, 0.0f);
            }

            return 1;
        }

        int Script_GetPetSP(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (player && player->m_player)
                ClientLua::PushNumber(L, player->m_player->m_playerPetSpellPower);
            else
                ClientLua::PushNumber(L, 0.0f);

            return 1;
        }

        int Script_GetSpeedPercent(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (player && player->m_player)
            {
                const float* speed = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[0]);
                ClientLua::PushNumber(L, *speed);
            }
            else
            {
                ClientLua::PushNumber(L, 0.0f);
            }

            return 1;
        }

        int Script_GetLeechPercent(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (player && player->m_player)
            {
                const float* leech = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[1]);
                ClientLua::PushNumber(L, *leech);
            }
            else
            {
                ClientLua::PushNumber(L, 0.0f);
            }

            return 1;
        }

        int Script_GetAvoidancePercent(lua_State* L)
        {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            if (player && player->m_player)
            {
                const float* avoidance = reinterpret_cast<const float*>(&player->m_player->m_playerFieldGlyphs1[2]);
                ClientLua::PushNumber(L, *avoidance);
            }
            else
            {
                ClientLua::PushNumber(L, 0.0f);
            }

            return 1;
        }

        void RegisterPlayerFunctions()
        {
            ClientLua::AddFunction("GetShapeshiftFormID", Script_GetShapeshiftFormID);
            ClientLua::AddFunction("GetActiveSpec", Script_GetActiveSpec);
            ClientLua::AddFunction("SetActiveSpec", Script_SetActiveSpec);
            ClientLua::AddFunction("GetMasteryRating", Script_GetMasteryRating);
            ClientLua::AddFunction("SetMasteryRatings", Script_SetMasteryRatings);
            ClientLua::AddFunction("UpdateMasteryAmount", Script_UpdateMasteryAmount);
            ClientLua::AddFunction("UpdateChargeInfoForSpell", Script_UpdateChargeInfoForSpell);
            ClientLua::AddFunction("GetChargeInfoForSpell", Script_GetChargeInfoForSpell);
            ClientLua::AddFunction("GetCombatRatingMult", Script_GetCombatRatingMult);
            ClientLua::AddFunction("GetCombatRatingScalar", Script_GetCombatRatingScalar);
            ClientLua::AddFunction("GetModCastSpeed", Script_GetModCastSpeed);
            ClientLua::AddFunction("GetWeaponAP", Script_GetWeaponAP);
            ClientLua::AddFunction("GetPetSP", Script_GetPetSP);
            ClientLua::AddFunction("GetSpeedPercent", Script_GetSpeedPercent);
            ClientLua::AddFunction("GetLeechPercent", Script_GetLeechPercent);
            ClientLua::AddFunction("GetAvoidancePercent", Script_GetAvoidancePercent);
        }
    } // namespace Lua
} // namespace Duskhaven
