#include "SpellFunctions.h"
#include "ClientLua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellIconRec.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/DB/Rec/SpellFocusObjectRec.h"
#include "WoW/Source/DB/Rec/AreaGroupRec.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/TotemCategoryRec.h"
#include "WoW/Source/DB/Rec/ItemSubClassRec.h"
#include "WoW/Source/DB/Rec/ItemClassRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/QuestTextParser.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Duskhaven/Spells/SpellCustomAttributes.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellAdditionalAttributes.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Duskhaven
{
    namespace Lua
    {
        namespace
        {
            std::unordered_map<uint32_t, std::vector<uint32_t>> s_professionSpells;
            std::unordered_map<uint32_t, SkillLineAbilityRec> s_professionAbilities;
            std::unordered_map<uint32_t, std::vector<uint32_t>> s_knownProfessionSpells;
            bool s_professionIndexLoaded = false;
            WOWGUID s_professionPlayerGuid = {0};

            void LoadProfessionIndex()
            {
                if (s_professionIndexLoaded)
                    return;

                s_professionSpells.clear();
                s_professionAbilities.clear();

                for (int32_t i = 0; i < g_skillLineAbilityDB.m_numRecords; ++i)
                {
                    const SkillLineAbilityRec& ability = g_skillLineAbilityDB.m_records[i];
                    s_professionSpells[ability.m_skillLine].push_back(ability.m_spell);
                    s_professionAbilities[ability.m_spell] = ability;
                }

                for (auto& pair : s_professionSpells)
                {
                    auto& spells = pair.second;
                    std::sort(spells.begin(), spells.end());
                    spells.erase(std::unique(spells.begin(), spells.end()), spells.end());
                }

                s_professionIndexLoaded = true;
            }

            void InvalidateProfessionRecipes()
            {
                s_knownProfessionSpells.clear();
                s_professionPlayerGuid = ClntObjMgrGetActivePlayer();
            }

            const std::vector<uint32_t>& GetKnownProfessionRecipes(uint32_t skillLine)
            {
                if (s_professionPlayerGuid != ClntObjMgrGetActivePlayer())
                {
                    InvalidateProfessionRecipes();
                }

                auto found = s_knownProfessionSpells.find(skillLine);
                if (found != s_knownProfessionSpells.end())
                {
                    return found->second;
                }

                LoadProfessionIndex();
                auto& recipes = s_knownProfessionSpells[skillLine];
                CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
                if (!player || !skillLine)
                {
                    return recipes;
                }

                auto abilities = s_professionSpells.find(skillLine);
                if (abilities == s_professionSpells.end())
                {
                    return recipes;
                }

                for (uint32_t spellId : abilities->second)
                {
                    if (player->IsSpellKnown(spellId))
                    {
                        recipes.push_back(spellId);
                    }
                }

                return recipes;
            }

            uint32_t GetProfessionRecipeCountForSkillLine(uint32_t skillLineId)
            {
                return static_cast<uint32_t>(GetKnownProfessionRecipes(skillLineId).size());
            }

            uint32_t GetProfessionRecipeSpellId(uint32_t skillLineId, uint32_t oneBasedIndex)
            {
                const auto& recipes = GetKnownProfessionRecipes(skillLineId);
                if (oneBasedIndex == 0 || oneBasedIndex > recipes.size())
                {
                    return 0;
                }
                return recipes[oneBasedIndex - 1];
            }

            uint32_t GetCraftedItemId(uint32_t spellId, const SpellRec& spell)
            {
                for (uint32_t i = 0; i < 3; ++i)
                {
                    if (spell.m_effectItemType[i] > 0)
                    {
                        return static_cast<uint32_t>(spell.m_effectItemType[i]);
                    }
                }

                for (uint32_t i = 0; i < 32; ++i)
                {
                    auto* effect = SpellEffectCacheDB::Get(SpellEffectCacheKey(spellId, i));
                    if (effect && effect->effectItemType > 0)
                    {
                        return static_cast<uint32_t>(effect->effectItemType);
                    }
                }

                return 0;
            }

            uint32_t GetPlayerItemCount(uint32_t itemId)
            {
                CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
                if (!player || !itemId)
                {
                    return 0;
                }
                return player->m_bag.GetItemTypeCount(itemId, 0);
            }

            uint32_t GetCraftableCount(const SpellRec& row)
            {
                uint32_t craftable = 0xFFFFFFFF;
                bool hasReagent = false;

                for (uint32_t i = 0; i < 8; ++i)
                {
                    uint32_t reagentId = row.m_reagent[i];
                    uint32_t required = row.m_reagentCount[i];
                    if (!reagentId || !required)
                    {
                        continue;
                    }

                    hasReagent = true;
                    uint32_t available = GetPlayerItemCount(reagentId) / required;
                    if (available < craftable)
                    {
                        craftable = available;
                    }
                }

                if (!hasReagent)
                {
                    return 1;
                }

                return craftable == 0xFFFFFFFF ? 0 : craftable;
            }

            ItemStats_C* GetItemStats(uint32_t itemId)
            {
                if (!itemId)
                    return nullptr;
                return g_itemDBCache.GetRecord(itemId, WOWGUID(), nullptr, nullptr, false);
            }

            const char* GetItemName(uint32_t itemId)
            {
                ItemStats_C* stats = GetItemStats(itemId);
                return (stats && stats->GetName(0)) ? stats->GetName(0) : "";
            }

            std::string GetItemIcon(uint32_t itemId)
            {
                ItemStats_C* stats = GetItemStats(itemId);
                if (!stats || !stats->m_displayId)
                    return "";

                ItemDisplayInfoRec displayRec{};
                if (g_itemDisplayInfoDB.GetRecord(stats->m_displayId, &displayRec))
                {
                    if (displayRec.m_inventoryIcon[0] && displayRec.m_inventoryIcon[0][0])
                    {
                        return displayRec.m_inventoryIcon[0];
                    }
                }
                return "";
            }

            const char* GetItemCategory(uint32_t itemId)
            {
                ItemStats_C* stats = GetItemStats(itemId);
                if (!stats)
                    return "";

                for (int32_t i = 0; i < g_itemSubClassDB.m_numRecords; ++i)
                {
                    const ItemSubClassRec& row = g_itemSubClassDB.m_records[i];
                    if (static_cast<uint32_t>(row.m_classID) == stats->m_class &&
                        static_cast<uint32_t>(row.m_subClassID) == stats->m_subClass)
                    {
                        if (row.m_displayName && row.m_displayName[0])
                            return row.m_displayName;
                        if (row.m_verboseName && row.m_verboseName[0])
                            return row.m_verboseName;
                        break;
                    }
                }

                ItemClassRec* itemClass = g_itemClassDB.GetRecord(stats->m_class);
                return (itemClass && itemClass->m_className) ? itemClass->m_className : "";
            }

            struct ProfessionRequirement
            {
                std::string name;
                bool met;
            };

            std::string BuildProfessionRequirements(const SpellRec& row)
            {
                std::vector<ProfessionRequirement> requirements;
                requirements.reserve(7);

                if (row.m_requiresSpellFocus)
                {
                    SpellFocusObjectRec* focus = g_spellFocusObjectDB.GetRecord(row.m_requiresSpellFocus);
                    if (focus && focus->m_name)
                    {
                        requirements.push_back({focus->m_name, true});
                    }
                }

                if (row.m_requiredAreasID > 0)
                {
                    AreaGroupRec* group = g_areaGroupDB.GetRecord(row.m_requiredAreasID);
                    if (group)
                    {
                        AreaTableRec* area = g_areaTableDB.GetRecord(group->m_areaID[0]);
                        if (area && area->m_areaName)
                        {
                            requirements.push_back({area->m_areaName, true});
                        }
                    }
                }

                for (uint32_t i = 0; i < 2; ++i)
                {
                    uint32_t itemId = row.m_totem[i];
                    if (!itemId)
                        continue;

                    const char* itemName = GetItemName(itemId);
                    if (itemName && itemName[0])
                    {
                        requirements.push_back({itemName, GetPlayerItemCount(itemId) > 0});
                    }
                }

                CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
                for (uint32_t i = 0; i < 2; ++i)
                {
                    uint32_t categoryId = row.m_requiredTotemCategoryID[i];
                    if (!categoryId)
                        continue;

                    TotemCategoryRec* category = g_totemCategoryDB.GetRecord(categoryId);
                    if (category && category->m_name)
                    {
                        bool hasTotem = player && player->m_bag.FindTotemCategory(categoryId, 0);
                        requirements.push_back({category->m_name, hasTotem});
                    }
                }

                std::string text;
                for (const auto& req : requirements)
                {
                    if (!text.empty())
                    {
                        text += ", ";
                    }
                    if (req.met)
                    {
                        text += req.name;
                    }
                    else
                    {
                        text += "|cffff2020";
                        text += req.name;
                        text += "|r";
                    }
                }
                return text;
            }

            uint32_t GetNonzeroReagentSlot(const SpellRec& row, uint32_t oneBasedReagentIndex)
            {
                uint32_t seen = 0;
                for (uint32_t i = 0; i < 8; ++i)
                {
                    if (!row.m_reagent[i])
                        continue;

                    ++seen;
                    if (seen == oneBasedReagentIndex)
                        return i;
                }
                return 8;
            }

            uint32_t GetNonzeroReagentCount(const SpellRec& row)
            {
                uint32_t count = 0;
                for (uint32_t i = 0; i < 8; ++i)
                {
                    if (row.m_reagent[i])
                        ++count;
                }
                return count;
            }
        } // namespace

        int Script_GetSpellDescription(lua_State* L)
        {
            if (ClientLua::IsNumber(L, 1))
            {
                uint32_t spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
                SpellRec spell{};
                if (g_spellDB.GetRecord(spellId, &spell))
                {
                    char dest[1024] = {};
                    SpellParserParseText(&spell, dest, sizeof(dest), 0, 0, 0, 0, 1, 0);
                    ClientLua::PushString(L, dest);
                    return 1;
                }
            }

            ClientLua::PushString(L, "");
            return 1;
        }

        int Script_GetSpellNameById(lua_State* L)
        {
            if (ClientLua::IsNumber(L, 1))
            {
                uint32_t spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
                SpellRec spell{};
                if (g_spellDB.GetRecord(spellId, &spell))
                {
                    ClientLua::PushString(L, spell.m_name ? spell.m_name : "");
                    ClientLua::PushString(L, spell.m_nameSubtext ? spell.m_nameSubtext : "");
                    return 2;
                }
            }

            ClientLua::PushNil(L);
            ClientLua::PushNil(L);
            return 2;
        }

        int Script_GetSpellIconById(lua_State* L)
        {
            if (ClientLua::IsNumber(L, 1))
            {
                uint32_t spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
                SpellRec spell{};
                if (g_spellDB.GetRecord(spellId, &spell))
                {
                    SpellIconRec* iconRow = g_spellIconDB.GetRecord(spell.m_spellIconID);
                    if (iconRow && iconRow->m_textureFilename)
                    {
                        ClientLua::PushString(L, iconRow->m_textureFilename);
                        return 1;
                    }
                }
            }

            ClientLua::PushNil(L);
            return 1;
        }

        int Script_SpellRequiresComboPoints(lua_State* L)
        {
            if (ClientLua::IsNumber(L, 1))
            {
                uint32_t spellId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1));
                auto* attr = SpellAdditionalAttributesDB::Get(spellId);
                bool requiresCombo = attr && ((attr->customAttr1 & SPELL_ATTR1_CU_COMBODAMAGE) ||
                                              (attr->customAttr1 & SPELL_ATTR1_CU_COMBODURATION));
                ClientLua::PushBoolean(L, requiresCombo ? 1 : 0);
                return 1;
            }

            ClientLua::PushBoolean(L, 0);
            return 1;
        }

        int Script_UnitCustomCastingData(lua_State* L)
        {
            if (!ClientLua::IsString(L, 1))
            {
                ClientLua::DisplayError(L, "Usage: UnitCustomCastingData(\"unit\")");
                return 0;
            }

            CGUnit_C* unit = GetUnitFromName(ClientLua::ToString(L, 1));
            if (!unit)
                return 0;

            uint32_t currentCast = 0;
            if (unit->m_castingSpellId)
                currentCast = unit->m_castingSpellId;
            else if (unit->m_channelSpellId)
                currentCast = unit->m_channelSpellId;

            if (!currentCast)
                return 0;

            SpellRec spell{};
            if (!g_spellDB.GetRecord(currentCast, &spell))
                return 0;

            float spellId = static_cast<float>(spell.m_id);
            int32_t const castTime = SpellRec::GetCastTime(&spell, 0, 0, 1);
            auto* customAttributesRow = SpellAdditionalAttributesDB::Get(spell.m_id);

            bool hideCastbar = false;
            bool invertCastbar = false;

            if (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_FORCE_HIDE_CASTBAR))
                hideCastbar = true;

            if (castTime <= 250 &&
                (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_LOW_TIME_FORCE_HIDE_CASTBAR)))
                hideCastbar = true;

            if (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_INVERT_CASTBAR))
                invertCastbar = true;

            ClientLua::PushNumber(L, spellId);
            ClientLua::PushBoolean(L, hideCastbar ? 1 : 0);
            ClientLua::PushBoolean(L, invertCastbar ? 1 : 0);
            return 3;
        }

        int Script_PreloadProfessionItemCache(lua_State* L)
        {
            uint32_t itemId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            if (itemId)
            {
                g_itemDBCache.GetRecord(itemId, WOWGUID(), nullptr, nullptr, false);
                ClientLua::PushBoolean(L, 1);
                return 1;
            }
            ClientLua::PushBoolean(L, 0);
            return 1;
        }

        int Script_GetProfessionRecipeCount(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            ClientLua::PushNumber(L, static_cast<double>(GetProfessionRecipeCountForSkillLine(skillLineId)));
            return 1;
        }

        int Script_GetProfessionRecipeSpellId(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t index = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);
            if (!spellId)
            {
                ClientLua::PushNil(L);
                return 1;
            }

            ClientLua::PushNumber(L, static_cast<double>(spellId));
            return 1;
        }

        int Script_GetProfessionRecipeInfo(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t index = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);
            if (!spellId)
            {
                ClientLua::PushNil(L);
                return 1;
            }

            SpellRec row{};
            bool cached = g_spellDB.GetRecord(spellId, &row);

            LoadProfessionIndex();
            auto abilityIt = s_professionAbilities.find(spellId);
            const SkillLineAbilityRec* ability = abilityIt != s_professionAbilities.end() ? &abilityIt->second : nullptr;

            uint32_t minRank = ability ? ability->m_minSkillLineRank : 0;
            uint32_t trivialHigh = ability ? ability->m_trivialSkillLineRankHigh : 0;
            uint32_t trivialLow = ability ? ability->m_trivialSkillLineRankLow : 0;

            ClientLua::PushNumber(L, static_cast<double>(spellId));
            ClientLua::PushString(L, cached && row.m_name ? row.m_name : "");
            ClientLua::PushString(L, cached && row.m_nameSubtext ? row.m_nameSubtext : "");

            SpellIconRec* iconRow = cached ? g_spellIconDB.GetRecord(row.m_spellIconID) : nullptr;
            ClientLua::PushString(L, (iconRow && iconRow->m_textureFilename) ? iconRow->m_textureFilename : "");

            ClientLua::PushNumber(L, static_cast<double>(minRank));
            ClientLua::PushNumber(L, static_cast<double>(trivialHigh));
            ClientLua::PushNumber(L, static_cast<double>(trivialLow));
            ClientLua::PushNumber(L, cached ? static_cast<double>(GetCraftableCount(row)) : 0.0);
            ClientLua::PushBoolean(L, cached ? 1 : 0);

            uint32_t craftedItemId = cached ? GetCraftedItemId(spellId, row) : 0;
            ClientLua::PushNumber(L, static_cast<double>(craftedItemId));
            ClientLua::PushString(L, GetItemName(craftedItemId));
            ClientLua::PushString(L, GetItemIcon(craftedItemId).c_str());
            ClientLua::PushString(L, GetItemCategory(craftedItemId));
            return 13;
        }

        int Script_GetProfessionRecipeRequirements(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t index = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

            SpellRec row{};
            if (!spellId || !g_spellDB.GetRecord(spellId, &row))
            {
                ClientLua::PushNil(L);
                ClientLua::PushNumber(L, 0);
                ClientLua::PushNumber(L, 0);
                return 3;
            }

            std::string requirements = BuildProfessionRequirements(row);
            ClientLua::PushString(L, requirements.c_str());
            ClientLua::PushNumber(L, static_cast<double>(row.m_totem[0]));
            ClientLua::PushNumber(L, static_cast<double>(row.m_totem[1]));
            return 3;
        }

        int Script_GetProfessionRecipeReagentCount(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t index = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

            SpellRec row{};
            if (!spellId || !g_spellDB.GetRecord(spellId, &row))
            {
                ClientLua::PushNumber(L, 0);
                return 1;
            }

            ClientLua::PushNumber(L, static_cast<double>(GetNonzeroReagentCount(row)));
            return 1;
        }

        int Script_GetProfessionRecipeReagentInfo(lua_State* L)
        {
            uint32_t skillLineId = static_cast<uint32_t>(ClientLua::GetNumber(L, 1, 0));
            uint32_t index = static_cast<uint32_t>(ClientLua::GetNumber(L, 2, 0));
            uint32_t reagentIndex = static_cast<uint32_t>(ClientLua::GetNumber(L, 3, 0));
            uint32_t spellId = GetProfessionRecipeSpellId(skillLineId, index);

            SpellRec row{};
            if (!spellId || !reagentIndex || !g_spellDB.GetRecord(spellId, &row))
            {
                ClientLua::PushNil(L);
                return 1;
            }

            uint32_t reagentSlot = GetNonzeroReagentSlot(row, reagentIndex);
            if (reagentSlot >= 8)
            {
                ClientLua::PushNil(L);
                return 1;
            }

            uint32_t itemId = row.m_reagent[reagentSlot];
            uint32_t required = row.m_reagentCount[reagentSlot];
            uint32_t have = GetPlayerItemCount(itemId);

            ClientLua::PushNumber(L, static_cast<double>(itemId));
            ClientLua::PushString(L, GetItemName(itemId));
            ClientLua::PushString(L, GetItemIcon(itemId).c_str());
            ClientLua::PushNumber(L, static_cast<double>(required));
            ClientLua::PushNumber(L, static_cast<double>(have));
            return 5;
        }

        void RegisterSpellFunctions()
        {
            ClientLua::RegisterFunction("GetSpellDescription", Script_GetSpellDescription);
            ClientLua::RegisterFunction("GetSpellNameById", Script_GetSpellNameById);
            ClientLua::RegisterFunction("GetSpellIconById", Script_GetSpellIconById);
            ClientLua::RegisterFunction("SpellRequiresComboPoints", Script_SpellRequiresComboPoints);
            ClientLua::RegisterFunction("UnitCustomCastingData", Script_UnitCustomCastingData);
            ClientLua::RegisterFunction("PreloadProfessionItemCache", Script_PreloadProfessionItemCache);
            ClientLua::RegisterFunction("GetProfessionRecipeCount", Script_GetProfessionRecipeCount);
            ClientLua::RegisterFunction("GetProfessionRecipeSpellId", Script_GetProfessionRecipeSpellId);
            ClientLua::RegisterFunction("GetProfessionRecipeInfo", Script_GetProfessionRecipeInfo);
            ClientLua::RegisterFunction("GetProfessionRecipeRequirements", Script_GetProfessionRecipeRequirements);
            ClientLua::RegisterFunction("GetProfessionRecipeReagentCount", Script_GetProfessionRecipeReagentCount);
            ClientLua::RegisterFunction("GetProfessionRecipeReagentInfo", Script_GetProfessionRecipeReagentInfo);
        }
    } // namespace Lua
} // namespace Duskhaven
