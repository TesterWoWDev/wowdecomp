#include "NativeDbcPatch.h"
#include "StreamingLogger.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Common/ComponentCore/ComponentUtils.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/WorldClient/DetailDoodad.h"
#include "WoW/Source/WorldClient/CDetailDoodadData.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

struct NativeClientDB {
    void* vtableCommon;
    int32_t m_loaded;
    int32_t m_numRecords;
    int32_t m_maxID;
    int32_t m_minID;
    const char* m_strings;
    void* vtableDatabase;
    void* m_records;
    void** m_recordsById;
};

std::vector<DBCPatch::PatchRange>& DBCPatch::PatchedRanges()
{
    static std::vector<PatchRange> v;
    return v;
}

void DBCPatch::AddPatchedRange(const void* begin, const void* end)
{
    auto& v = PatchedRanges();
    PatchRange r{reinterpret_cast<uintptr_t>(begin), reinterpret_cast<uintptr_t>(end)};
    auto it =
        std::lower_bound(v.begin(), v.end(), r.begin, [](const PatchRange& e, uintptr_t val) { return e.begin < val; });
    v.insert(it, r);
}

bool DBCPatch::IsPatchedRow(const void* p)
{
    auto& v     = PatchedRanges();
    uintptr_t a = reinterpret_cast<uintptr_t>(p);
    auto it = std::upper_bound(v.begin(), v.end(), a, [](uintptr_t val, const PatchRange& e) { return val < e.begin; });
    if (it == v.begin())
        return false;
    --it;
    return a >= it->begin && a < it->end;
}

struct DBCEntry
{
    const char* name;
    NativeClientDB* db;
};

#define DBC_ENTRY(name, dbVar) { name, reinterpret_cast<NativeClientDB*>(&(dbVar)) }

static const DBCEntry kStorages[] = {
    DBC_ENTRY("achievement", g_achievementDB),
    DBC_ENTRY("achievement_Category", g_achievement_CategoryDB),
    DBC_ENTRY("achievement_Criteria", g_achievement_CriteriaDB),
    DBC_ENTRY("animationData", g_animationDataDB),
    DBC_ENTRY("areaGroup", g_areaGroupDB),
    DBC_ENTRY("areaPOI", g_areaPOIDB),
    DBC_ENTRY("areaTable", g_areaTableDB),
    DBC_ENTRY("areaTrigger", g_areaTriggerDB),
    DBC_ENTRY("attackAnimKits", g_attackAnimKitsDB),
    DBC_ENTRY("attackAnimTypes", g_attackAnimTypesDB),
    DBC_ENTRY("auctionHouse", g_auctionHouseDB),
    DBC_ENTRY("bankBagSlotPrices", g_bankBagSlotPricesDB),
    DBC_ENTRY("bannedAddOns", g_bannedAddOnsDB),
    DBC_ENTRY("barberShopStyle", g_barberShopStyleDB),
    DBC_ENTRY("battlemasterList", g_battlemasterListDB),
    DBC_ENTRY("cameraShakes", g_cameraShakesDB),
    DBC_ENTRY("cfg_Categories", g_cfg_CategoriesDB),
    DBC_ENTRY("cfg_Configs", g_cfg_ConfigsDB),
    DBC_ENTRY("charBaseInfo", g_charBaseInfoDB),
    DBC_ENTRY("charHairGeosets", g_charHairGeosetsDB),
    DBC_ENTRY("charSections", g_charSectionsDB),
    DBC_ENTRY("charStartOutfit", g_charStartOutfitDB),
    DBC_ENTRY("charTitles", g_charTitlesDB),
    DBC_ENTRY("characterFacialHairStyles", g_characterFacialHairStylesDB),
    DBC_ENTRY("chatChannels", g_chatChannelsDB),
    DBC_ENTRY("chatProfanity", g_chatProfanityDB),
    DBC_ENTRY("chrClasses", g_chrClassesDB),
    DBC_ENTRY("chrRaces", g_chrRacesDB),
    DBC_ENTRY("cinematicCamera", g_cinematicCameraDB),
    DBC_ENTRY("cinematicSequences", g_cinematicSequencesDB),
    DBC_ENTRY("creatureDisplayInfo", g_creatureDisplayInfoDB),
    DBC_ENTRY("creatureDisplayInfoExtra", g_creatureDisplayInfoExtraDB),
    DBC_ENTRY("creatureFamily", g_creatureFamilyDB),
    DBC_ENTRY("creatureModelData", g_creatureModelDataDB),
    DBC_ENTRY("creatureMovementInfo", g_creatureMovementInfoDB),
    DBC_ENTRY("creatureSoundData", g_creatureSoundDataDB),
    DBC_ENTRY("creatureSpellData", g_creatureSpellDataDB),
    DBC_ENTRY("creatureType", g_creatureTypeDB),
    DBC_ENTRY("currencyCategory", g_currencyCategoryDB),
    DBC_ENTRY("currencyTypes", g_currencyTypesDB),
    DBC_ENTRY("danceMoves", g_danceMovesDB),
    DBC_ENTRY("deathThudLookups", g_deathThudLookupsDB),
    DBC_ENTRY("declinedWord", g_declinedWordDB),
    DBC_ENTRY("declinedWordCases", g_declinedWordCasesDB),
    DBC_ENTRY("destructibleModelData", g_destructibleModelDataDB),
    DBC_ENTRY("dungeonEncounter", g_dungeonEncounterDB),
    DBC_ENTRY("dungeonMap", g_dungeonMapDB),
    DBC_ENTRY("dungeonMapChunk", g_dungeonMapChunkDB),
    DBC_ENTRY("durabilityCosts", g_durabilityCostsDB),
    DBC_ENTRY("durabilityQuality", g_durabilityQualityDB),
    DBC_ENTRY("emotes", g_emotesDB),
    DBC_ENTRY("emotesText", g_emotesTextDB),
    DBC_ENTRY("emotesTextData", g_emotesTextDataDB),
    DBC_ENTRY("emotesTextSound", g_emotesTextSoundDB),
    DBC_ENTRY("environmentalDamage", g_environmentalDamageDB),
    DBC_ENTRY("exhaustion", g_exhaustionDB),
    DBC_ENTRY("faction", g_factionDB),
    DBC_ENTRY("factionGroup", g_factionGroupDB),
    DBC_ENTRY("factionTemplate", g_factionTemplateDB),
    DBC_ENTRY("fileData", g_fileDataDB),
    DBC_ENTRY("footprintTextures", g_footprintTexturesDB),
    DBC_ENTRY("footstepTerrainLookup", g_footstepTerrainLookupDB),
    DBC_ENTRY("gMSurveyAnswers", g_gmSurveyAnswersDB),
    DBC_ENTRY("gMSurveyCurrentSurvey", g_gmSurveyCurrentSurveyDB),
    DBC_ENTRY("gMSurveyQuestions", g_gmSurveyQuestionsDB),
    DBC_ENTRY("gMSurveySurveys", g_gmSurveySurveysDB),
    DBC_ENTRY("gMTicketCategory", g_gmTicketCategoryDB),
    DBC_ENTRY("gameObjectArtKit", g_gameObjectArtKitDB),
    DBC_ENTRY("gameObjectDisplayInfo", g_gameObjectDisplayInfoDB),
    DBC_ENTRY("gameTables", g_gameTablesDB),
    DBC_ENTRY("gameTips", g_gameTipsDB),
    DBC_ENTRY("gemProperties", g_gemPropertiesDB),
    DBC_ENTRY("glyphProperties", g_glyphPropertiesDB),
    DBC_ENTRY("glyphSlot", g_glyphSlotDB),
    DBC_ENTRY("groundEffectDoodad", g_groundEffectDoodadDB),
    DBC_ENTRY("groundEffectTexture", g_groundEffectTextureDB),
    DBC_ENTRY("gtBarberShopCostBase", g_gtBarberShopCostBaseDB),
    DBC_ENTRY("gtChanceToMeleeCrit", g_gtChanceToMeleeCritDB),
    DBC_ENTRY("gtChanceToMeleeCritBase", g_gtChanceToMeleeCritBaseDB),
    DBC_ENTRY("gtChanceToSpellCrit", g_gtChanceToSpellCritDB),
    DBC_ENTRY("gtChanceToSpellCritBase", g_gtChanceToSpellCritBaseDB),
    DBC_ENTRY("gtCombatRatings", g_gtCombatRatingsDB),
    DBC_ENTRY("gtNPCManaCostScaler", g_gtNPCManaCostScalerDB),
    DBC_ENTRY("gtOCTClassCombatRatingScalar", g_gtOCTClassCombatRatingScalarDB),
    DBC_ENTRY("gtOCTRegenHP", g_gtOCTRegenHPDB),
    DBC_ENTRY("gtOCTRegenMP", g_gtOCTRegenMPDB),
    DBC_ENTRY("gtRegenHPPerSpt", g_gtRegenHPPerSptDB),
    DBC_ENTRY("gtRegenMPPerSpt", g_gtRegenMPPerSptDB),
    DBC_ENTRY("helmetGeosetVisData", g_helmetGeosetVisDataDB),
    DBC_ENTRY("holidayDescriptions", g_holidayDescriptionsDB),
    DBC_ENTRY("holidayNames", g_holidayNamesDB),
    DBC_ENTRY("holidays", g_holidaysDB),
    DBC_ENTRY("item", g_itemDB),
    DBC_ENTRY("itemBagFamily", g_itemBagFamilyDB),
    DBC_ENTRY("itemClass", g_itemClassDB),
    DBC_ENTRY("itemCondExtCosts", g_itemCondExtCostsDB),
    DBC_ENTRY("itemDisplayInfo", g_itemDisplayInfoDB),
    DBC_ENTRY("itemExtendedCost", g_itemExtendedCostDB),
    DBC_ENTRY("itemGroupSounds", g_itemGroupSoundsDB),
    DBC_ENTRY("itemLimitCategory", g_itemLimitCategoryDB),
    DBC_ENTRY("itemPetFood", g_itemPetFoodDB),
    DBC_ENTRY("itemPurchaseGroup", g_itemPurchaseGroupDB),
    DBC_ENTRY("itemRandomProperties", g_itemRandomPropertiesDB),
    DBC_ENTRY("itemRandomSuffix", g_itemRandomSuffixDB),
    DBC_ENTRY("itemSet", g_itemSetDB),
    DBC_ENTRY("itemSubClass", g_itemSubClassDB),
    DBC_ENTRY("itemSubClassMask", g_itemSubClassMaskDB),
    DBC_ENTRY("itemVisualEffects", g_itemVisualEffectsDB),
    DBC_ENTRY("itemVisuals", g_itemVisualsDB),
    DBC_ENTRY("languageWords", g_languageWordsDB),
    DBC_ENTRY("languages", g_languagesDB),
    DBC_ENTRY("lfgDungeonExpansion", g_lfgDungeonExpansionDB),
    DBC_ENTRY("lfgDungeonGroup", g_lfgDungeonGroupDB),
    DBC_ENTRY("lfgDungeons", g_lfgDungeonsDB),
    DBC_ENTRY("light", g_lightDB),
    DBC_ENTRY("lightFloatBand", g_lightFloatBandDB),
    DBC_ENTRY("lightIntBand", g_lightIntBandDB),
    DBC_ENTRY("lightParams", g_lightParamsDB),
    DBC_ENTRY("lightSkybox", g_lightSkyboxDB),
    DBC_ENTRY("liquidMaterial", g_liquidMaterialDB),
    DBC_ENTRY("liquidType", g_liquidTypeDB),
    DBC_ENTRY("loadingScreenTaxiSplines", g_loadingScreenTaxiSplinesDB),
    DBC_ENTRY("loadingScreens", g_loadingScreensDB),
    DBC_ENTRY("lock", g_lockDB),
    DBC_ENTRY("lockType", g_lockTypeDB),
    DBC_ENTRY("mailTemplate", g_mailTemplateDB),
    DBC_ENTRY("map", g_mapDB),
    DBC_ENTRY("mapDifficulty", g_mapDifficultyDB),
    DBC_ENTRY("material", g_materialDB),
    DBC_ENTRY("movie", g_movieDB),
    DBC_ENTRY("movieFileData", g_movieFileDataDB),
    DBC_ENTRY("movieVariation", g_movieVariationDB),
    DBC_ENTRY("nameGen", g_nameGenDB),
    DBC_ENTRY("namesProfanity", g_namesProfanityDB),
    DBC_ENTRY("namesReserved", g_namesReservedDB),
    DBC_ENTRY("nPCSounds", g_npcSoundsDB),
    DBC_ENTRY("objectEffect", g_objectEffectDB),
    DBC_ENTRY("objectEffectGroup", g_objectEffectGroupDB),
    DBC_ENTRY("objectEffectModifier", g_objectEffectModifierDB),
    DBC_ENTRY("objectEffectPackage", g_objectEffectPackageDB),
    DBC_ENTRY("objectEffectPackageElem", g_objectEffectPackageElemDB),
    DBC_ENTRY("overrideSpellData", g_overrideSpellDataDB),
    DBC_ENTRY("package", g_packageDB),
    DBC_ENTRY("pageTextMaterial", g_pageTextMaterialDB),
    DBC_ENTRY("paperDollItemFrame", g_paperDollItemFrameDB),
    DBC_ENTRY("particleColor", g_particleColorDB),
    DBC_ENTRY("petPersonality", g_petPersonalityDB),
    DBC_ENTRY("powerDisplay", g_powerDisplayDB),
    DBC_ENTRY("pvpDifficulty", g_pvpDifficultyDB),
    DBC_ENTRY("questFactionReward", g_questFactionRewardDB),
    DBC_ENTRY("questInfo", g_questInfoDB),
    DBC_ENTRY("questSort", g_questSortDB),
    DBC_ENTRY("questXP", g_questXPDB),
    DBC_ENTRY("randPropPoints", g_randPropPointsDB),
    DBC_ENTRY("resistances", g_resistancesDB),
    DBC_ENTRY("scalingStatDistribution", g_scalingStatDistributionDB),
    DBC_ENTRY("scalingStatValues", g_scalingStatValuesDB),
    DBC_ENTRY("screenEffect", g_screenEffectDB),
    DBC_ENTRY("serverMessages", g_serverMessagesDB),
    DBC_ENTRY("sheatheSoundLookups", g_sheatheSoundLookupsDB),
    DBC_ENTRY("skillCostsData", g_skillCostsDataDB),
    DBC_ENTRY("skillLine", g_skillLineDB),
    DBC_ENTRY("skillLineAbility", g_skillLineAbilityDB),
    DBC_ENTRY("skillLineCategory", g_skillLineCategoryDB),
    DBC_ENTRY("skillRaceClassInfo", g_skillRaceClassInfoDB),
    DBC_ENTRY("skillTiers", g_skillTiersDB),
    DBC_ENTRY("soundAmbience", g_soundAmbienceDB),
    DBC_ENTRY("soundEmitters", g_soundEmittersDB),
    DBC_ENTRY("soundEntries", g_soundEntriesDB),
    DBC_ENTRY("soundEntriesAdvanced", g_soundEntriesAdvancedDB),
    DBC_ENTRY("soundFilter", g_soundFilterDB),
    DBC_ENTRY("soundFilterElem", g_soundFilterElemDB),
    DBC_ENTRY("soundProviderPreferences", g_soundProviderPreferencesDB),
    DBC_ENTRY("soundSamplePreferences", g_soundSamplePreferencesDB),
    DBC_ENTRY("soundWaterType", g_soundWaterTypeDB),
    DBC_ENTRY("spamMessages", g_spamMessagesDB),
    DBC_ENTRY("spell", g_spellDB),
    DBC_ENTRY("spellCastTimes", g_spellCastTimesDB),
    DBC_ENTRY("spellCategory", g_spellCategoryDB),
    DBC_ENTRY("spellChainEffects", g_spellChainEffectsDB),
    DBC_ENTRY("spellDescriptionVariables", g_spellDescriptionVariablesDB),
    DBC_ENTRY("spellDifficulty", g_spellDifficultyDB),
    DBC_ENTRY("spellDispelType", g_spellDispelTypeDB),
    DBC_ENTRY("spellDuration", g_spellDurationDB),
    DBC_ENTRY("spellEffectCameraShakes", g_spellEffectCameraShakesDB),
    DBC_ENTRY("spellFocusObject", g_spellFocusObjectDB),
    DBC_ENTRY("spellIcon", g_spellIconDB),
    DBC_ENTRY("spellItemEnchantment", g_spellItemEnchantmentDB),
    DBC_ENTRY("spellItemEnchantmentCondition", g_spellItemEnchantmentConditionDB),
    DBC_ENTRY("spellMechanic", g_spellMechanicDB),
    DBC_ENTRY("spellMissile", g_spellMissileDB),
    DBC_ENTRY("spellMissileMotion", g_spellMissileMotionDB),
    DBC_ENTRY("spellRadius", g_spellRadiusDB),
    DBC_ENTRY("spellRange", g_spellRangeDB),
    DBC_ENTRY("spellRuneCost", g_spellRuneCostDB),
    DBC_ENTRY("spellShapeshiftForm", g_spellShapeshiftFormDB),
    DBC_ENTRY("spellVisual", g_spellVisualDB),
    DBC_ENTRY("spellVisualEffectName", g_spellVisualEffectNameDB),
    DBC_ENTRY("spellVisualKit", g_spellVisualKitDB),
    DBC_ENTRY("spellVisualKitAreaModel", g_spellVisualKitAreaModelDB),
    DBC_ENTRY("spellVisualKitModelAttach", g_spellVisualKitModelAttachDB),
    DBC_ENTRY("stableSlotPrices", g_stableSlotPricesDB),
    DBC_ENTRY("stationery", g_stationeryDB),
    DBC_ENTRY("stringLookups", g_stringLookupsDB),
    DBC_ENTRY("summonProperties", g_summonPropertiesDB),
    DBC_ENTRY("talent", g_talentDB),
    DBC_ENTRY("talentTab", g_talentTabDB),
    DBC_ENTRY("taxiNodes", g_taxiNodesDB),
    DBC_ENTRY("taxiPath", g_taxiPathDB),
    DBC_ENTRY("taxiPathNode", g_taxiPathNodeDB),
    DBC_ENTRY("teamContributionPoints", g_teamContributionPointsDB),
    DBC_ENTRY("terrainType", g_terrainTypeDB),
    DBC_ENTRY("terrainTypeSounds", g_terrainTypeSoundsDB),
    DBC_ENTRY("totemCategory", g_totemCategoryDB),
    DBC_ENTRY("transportAnimation", g_transportAnimationDB),
    DBC_ENTRY("transportPhysics", g_transportPhysicsDB),
    DBC_ENTRY("transportRotation", g_transportRotationDB),
    DBC_ENTRY("uISoundLookups", g_uiSoundLookupsDB),
    DBC_ENTRY("unitBlood", g_unitBloodDB),
    DBC_ENTRY("unitBloodLevels", g_unitBloodLevelsDB),
    DBC_ENTRY("vehicle", g_vehicleDB),
    DBC_ENTRY("vehicleSeat", g_vehicleSeatDB),
    DBC_ENTRY("vehicleUIIndicator", g_vehicleUIIndicatorDB),
    DBC_ENTRY("vehicleUIIndSeat", g_vehicleUIIndSeatDB),
    DBC_ENTRY("vocalUISounds", g_vocalUISoundsDB),
    DBC_ENTRY("wMOAreaTable", g_wmoAreaTableDB),
    DBC_ENTRY("weaponImpactSounds", g_weaponImpactSoundsDB),
    DBC_ENTRY("weaponSwingSounds2", g_weaponSwingSounds2DB),
    DBC_ENTRY("weather", g_weatherDB),
    DBC_ENTRY("worldChunkSounds", g_worldChunkSoundsDB),
    DBC_ENTRY("worldMapArea", g_worldMapAreaDB),
    DBC_ENTRY("worldMapContinent", g_worldMapContinentDB),
    DBC_ENTRY("worldMapOverlay", g_worldMapOverlayDB),
    DBC_ENTRY("worldMapTransforms", g_worldMapTransformsDB),
    DBC_ENTRY("worldSafeLocs", g_worldSafeLocsDB),
    DBC_ENTRY("worldStateUI", g_worldStateUIDB),
    DBC_ENTRY("worldStateZoneSounds", g_worldStateZoneSoundsDB),
    DBC_ENTRY("zoneIntroMusicTable", g_zoneIntroMusicTableDB),
    DBC_ENTRY("zoneMusic", g_zoneMusicDB),
};

constexpr uint32_t kStrSentinelNull = 0xFFFFFFFFu;

static char tolower_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
}

static bool iequals(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (tolower_ascii(*a) != tolower_ascii(*b))
            return false;
    return *a == *b;
}

NativeClientDB* DBCPatch::FindStorage(const char* name)
{
    for (const DBCEntry& e : kStorages)
        if (iequals(e.name, name))
            return e.db;
    return nullptr;
}

struct DenseSortKey
{
    const char* name;
    uint16_t fields[6];
};

constexpr DenseSortKey kDenseSortKeys[] = {
    {"wMOAreaTable", {4, 8, 12, 0xFFFFu}},
    {"charSections", {4, 8, 12, 0x20, 0x24, 0xFFFFu}},
};

static const uint16_t* FindDenseSortKey(const char* name)
{
    for (const DenseSortKey& k : kDenseSortKeys)
        if (iequals(k.name, name))
            return k.fields;
    return nullptr;
}

void DBCPatch::FixupRowForClient(const char* dbcName, void* record)
{
    if (!iequals(dbcName, "light"))
        return;
    TransformLightMapCoord(reinterpret_cast<LightRec*>(record), true);
}

static void RefreshCharacterComponent()
{
    if (!CCharacterComponent::s_chrVarArray && !CCharacterComponent::s_characterFacialHairStylesList)
        return;

    if (g_chrRacesDB.m_maxID < 0)
        return;
    int length = g_chrRacesDB.m_maxID * 2 + 2;

    st_race* chrVar = nullptr;
    uint32_t* facial = nullptr;
    if (CCharacterComponent::s_chrVarArray)
        BuildComponentArray(length, &chrVar);
    if (CCharacterComponent::s_characterFacialHairStylesList)
        CountFacialFeatures(length, &facial);

    if (chrVar)
        CCharacterComponent::s_chrVarArray = chrVar;
    if (facial)
        CCharacterComponent::s_characterFacialHairStylesList = facial;

    LOG_DEBUG << "DBC character component rebuilt length=" << length << " chrVar=" << (chrVar ? 1 : 0)
              << " facial=" << (facial ? 1 : 0);
}

void DBCPatch::RefreshDerivedState(const char* dbcName, NativeClientDB* db)
{
    if (iequals(dbcName, "charSections") || iequals(dbcName, "characterFacialHairStyles") ||
        iequals(dbcName, "chrRaces"))
    {
        RefreshCharacterComponent();
        return;
    }

    if (!iequals(dbcName, "groundEffectDoodad"))
        return;
    if (CDetailDoodad::s_doodadList.Count() == 0)
        return;

    uint32_t want = db->m_maxID >= 0 ? static_cast<uint32_t>(db->m_maxID) + 1 : 0;
    if (want > CDetailDoodad::s_doodadList.Count())
    {
        uint32_t oldCount = CDetailDoodad::s_doodadList.Count();
        CDetailDoodad::s_doodadList.SetCount(want);
        for (uint32_t i = oldCount; i < want; ++i)
            CDetailDoodad::s_doodadList[i] = nullptr;
    }

    void** byId   = db->m_recordsById;
    int refreshed = 0, added = 0;
    for (uint32_t i = 0; i < CDetailDoodad::s_doodadList.Count(); ++i)
    {
        int id    = static_cast<int>(i);
        void* rec = (byId && id >= db->m_minID && id <= db->m_maxID) ? byId[id - db->m_minID] : nullptr;
        if (!rec)
            continue;
        if (CDetailDoodad::s_doodadList[i])
        {
            CDetailDoodad::s_doodadList[i]->m_record = reinterpret_cast<GroundEffectDoodadRec*>(rec);
            ++refreshed;
        }
        else
        {
            CDetailDoodadData* data = new CDetailDoodadData();
            data->m_record = reinterpret_cast<GroundEffectDoodadRec*>(rec);
            CDetailDoodad::s_doodadList[i] = data;
            ++added;
        }
    }
    LOG_DEBUG << "DBC detail doodad list refreshed=" << refreshed << " added=" << added
              << " size=" << (int)CDetailDoodad::s_doodadList.Count();
}

void DBCPatch::RefreshDerivedState(const char* dbcName)
{
    NativeClientDB* db = FindStorage(dbcName);
    if (db && db->m_loaded)
        RefreshDerivedState(dbcName, db);
}

static bool IsDenseOnlyStorage(const char* name)
{
    return iequals(name, "skillLineAbility");
}

static std::unordered_map<uint32_t, int>& CompactSlots(const char* dbcName)
{
    static std::unordered_map<std::string, std::unordered_map<uint32_t, int>> byDbc;
    std::string key(dbcName);
    for (char& c : key)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return byDbc[key];
}

static bool CompactRowIds(NativeClientDB* db, const char* dbcName, std::vector<int>& priorOrder,
                          const std::vector<uint32_t>& ids, std::vector<uint32_t>& outIds)
{
    auto& slots = CompactSlots(dbcName);
    if (slots.empty())
    {
        void** old = db->m_recordsById;
        std::vector<void*> rows;
        std::vector<int> reordered;
        rows.reserve(priorOrder.size() + ids.size());
        reordered.reserve(priorOrder.size());
        for (int id : priorOrder)
        {
            auto ins = slots.emplace(static_cast<uint32_t>(id), static_cast<int>(rows.size()));
            if (ins.second)
                rows.push_back(old ? old[id - db->m_minID] : nullptr);
            reordered.push_back(ins.first->second);
        }
        for (uint32_t id : ids)
            if (slots.emplace(id, static_cast<int>(rows.size())).second)
                rows.push_back(nullptr);

        void** fresh = static_cast<void**>(SMemAlloc(rows.size() * sizeof(void*), "DBCCompactRows", __LINE__, 0));
        if (!fresh)
        {
            slots.clear();
            return false;
        }
        std::memcpy(fresh, rows.data(), rows.size() * sizeof(void*));
        priorOrder.swap(reordered);
        db->m_recordsById = fresh;
        db->m_minID       = 0;
        db->m_maxID       = static_cast<int>(rows.size()) - 1;
        LOG_DEBUG << "DBC compact '" << dbcName << "' " << (int)rows.size() << " slots";
    }
    else
    {
        for (uint32_t id : ids)
            slots.emplace(id, static_cast<int>(slots.size()));
    }

    outIds.resize(ids.size());
    for (size_t i = 0; i < ids.size(); ++i)
        outIds[i] = static_cast<uint32_t>(slots[ids[i]]);
    return true;
}

bool DBCPatch::EnsureCapacity(NativeClientDB* db, int wantMin, int wantMax)
{
    int newMin = db->m_recordsById ? (wantMin < db->m_minID ? wantMin : db->m_minID) : wantMin;
    int newMax = db->m_recordsById ? (wantMax > db->m_maxID ? wantMax : db->m_maxID) : wantMax;
    if (db->m_recordsById && newMin == db->m_minID && newMax == db->m_maxID)
        return true;

    size_t newCount = static_cast<size_t>(newMax - newMin + 1);
    void** grown    = static_cast<void**>(SMemAlloc(newCount * sizeof(void*), "DBCExpand", __LINE__, 0));
    if (!grown)
        return false;
    std::memset(grown, 0, newCount * sizeof(void*));

    if (db->m_recordsById && db->m_maxID >= db->m_minID)
    {
        void** old   = db->m_recordsById;
        int oldCount = db->m_maxID - db->m_minID + 1;
        for (int i = 0; i < oldCount; ++i)
            grown[(db->m_minID + i) - newMin] = old[i];
    }

    db->m_recordsById = grown;
    db->m_minID       = newMin;
    db->m_maxID       = newMax;
    LOG_DEBUG << "DBC grow rows to [" << newMin << ".." << newMax << "] " << (int)newCount << " slots ("
              << (int)(newCount * sizeof(void*)) << "B)";
    return true;
}

void** DBCPatch::SlotFor(NativeClientDB* db, uint32_t id)
{
    int iid = static_cast<int>(id);
    if (db->m_recordsById && iid >= db->m_minID && iid <= db->m_maxID)
        return &db->m_recordsById[iid - db->m_minID];
    return nullptr;
}

void* DBCPatch::FindRow(const char* dbcName, uint32_t id)
{
    NativeClientDB* db = FindStorage(dbcName);
    if (!db || !db->m_loaded)
        return nullptr;

    void** slot = SlotFor(db, id);
    return slot ? *slot : nullptr;
}

static bool RowsAreInlineStorage(void** byId, int idCount, const void* denseBase, int denseCount, uint32_t recordSize)
{
    if (!byId || !denseBase || idCount <= 0 || denseCount <= 0 || recordSize < sizeof(void*))
        return true;

    const uint8_t* inlineBase = static_cast<const uint8_t*>(denseBase);
    const size_t inlineBytes  = static_cast<size_t>(denseCount) * recordSize;
    int populated             = 0;
    int inlineHits            = 0;
    for (int i = 0; i < idCount; ++i)
    {
        const uint8_t* row = static_cast<const uint8_t*>(byId[i]);
        if (!row)
            continue;
        ++populated;
        if (row >= inlineBase && row < inlineBase + inlineBytes &&
            static_cast<size_t>(row - inlineBase) % recordSize == 0)
            ++inlineHits;
    }
    if (populated == 0)
        return true;

    LOG_DEBUG << "DBC dense probe rows=" << denseCount << " recSize=" << recordSize << " populated=" << populated
              << " inlineHits=" << inlineHits;
    return static_cast<int64_t>(inlineHits) * 4 >= static_cast<int64_t>(populated) * 3;
}

std::vector<int> DBCPatch::SnapshotDenseOrder(NativeClientDB* db, uint32_t recordSize, bool inlineStorage)
{
    std::vector<int> order;
    if (!db->m_recordsById || !db->m_records || db->m_numRecords <= 0 || db->m_maxID < db->m_minID)
        return order;

    void** byId = db->m_recordsById;
    int idCount = db->m_maxID - db->m_minID + 1;
    if (inlineStorage)
    {
        std::vector<int> idByDenseIndex(static_cast<size_t>(db->m_numRecords), -1);
        const uint8_t* base = static_cast<const uint8_t*>(db->m_records);
        const size_t bytes  = static_cast<size_t>(db->m_numRecords) * recordSize;
        for (int i = 0; i < idCount; ++i)
        {
            const uint8_t* row = static_cast<const uint8_t*>(byId[i]);
            if (!row || row < base || row >= base + bytes)
                continue;
            const size_t offset = static_cast<size_t>(row - base);
            if (offset % recordSize == 0)
                idByDenseIndex[offset / recordSize] = db->m_minID + i;
        }

        order.reserve(static_cast<size_t>(db->m_numRecords));
        for (int id : idByDenseIndex)
            if (id >= 0)
                order.push_back(id);
        return order;
    }

    std::unordered_map<const void*, int> idOfRow;
    idOfRow.reserve(static_cast<size_t>(idCount));
    for (int i = 0; i < idCount; ++i)
        if (byId[i])
            idOfRow.emplace(byId[i], db->m_minID + i);

    order.reserve(static_cast<size_t>(db->m_numRecords));
    for (int k = 0; k < db->m_numRecords; ++k)
    {
        const void* row = reinterpret_cast<void**>(db->m_records)[k];
        auto it         = idOfRow.find(row);
        if (it != idOfRow.end())
            order.push_back(it->second);
    }
    return order;
}

static int CompareDenseKey(const void* a, const void* b, const uint16_t* key)
{
    for (int i = 0; key[i] != 0xFFFFu; ++i)
    {
        int32_t va, vb;
        std::memcpy(&va, static_cast<const uint8_t*>(a) + key[i], sizeof(va));
        std::memcpy(&vb, static_cast<const uint8_t*>(b) + key[i], sizeof(vb));
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

void DBCPatch::RebuildInlineDense(NativeClientDB* db, void** byId, const std::vector<int>& order, uint32_t recordSize)
{
    size_t count           = order.size();
    uint8_t* existing      = count ? static_cast<uint8_t*>(byId[order[0] - db->m_minID]) : nullptr;
    bool alreadyContiguous = existing != nullptr;
    for (size_t w = 0; alreadyContiguous && w < count; ++w)
        alreadyContiguous = byId[order[w] - db->m_minID] == existing + w * recordSize;
    if (alreadyContiguous)
    {
        db->m_records    = existing;
        db->m_numRecords = static_cast<int>(count);
        LOG_DEBUG << "DBC dense reuse (inline) rows=" << (int)count << " recSize=" << recordSize;
        return;
    }

    uint8_t* dense = static_cast<uint8_t*>(SMemAlloc(count * recordSize, "DBCDenseInline", __LINE__, 0));
    if (!dense)
        return;
    AddPatchedRange(dense, dense + count * recordSize);

    for (size_t w = 0; w < count; ++w)
    {
        void** slot  = &byId[order[w] - db->m_minID];
        uint8_t* dst = dense + w * recordSize;
        std::memcpy(dst, *slot, recordSize);
        *slot = dst;
    }
    db->m_records    = dense;
    db->m_numRecords = static_cast<int>(count);
    LOG_DEBUG << "DBC dense rebuild (inline) rows=" << (int)count << " recSize=" << recordSize;
}

void DBCPatch::RebuildPointerDense(NativeClientDB* db, void** byId, const std::vector<int>& order)
{
    size_t count = order.size();
    void** dense = static_cast<void**>(SMemAlloc(count * sizeof(void*), "DBCDensePtrs", __LINE__, 0));
    if (!dense)
        return;

    for (size_t w = 0; w < count; ++w)
        dense[w] = byId[order[w] - db->m_minID];
    db->m_records    = dense;
    db->m_numRecords = static_cast<int>(count);
    LOG_DEBUG << "DBC dense rebuild (pointer) rows=" << (int)count;
}

void DBCPatch::RebuildDenseArray(NativeClientDB* db, uint32_t recordSize, bool inlineStorage,
                                 const std::vector<int>& order, const uint16_t* sortKey)
{
    if (!db->m_recordsById || db->m_maxID < db->m_minID || recordSize == 0 || order.empty())
        return;

    void** byId = db->m_recordsById;
    if (sortKey)
    {
        std::vector<int> sorted = order;
        std::stable_sort(sorted.begin(), sorted.end(), [&](int a, int b)
                         { return CompareDenseKey(byId[a - db->m_minID], byId[b - db->m_minID], sortKey) < 0; });
        if (inlineStorage)
            RebuildInlineDense(db, byId, sorted, recordSize);
        else
            RebuildPointerDense(db, byId, sorted);
        return;
    }

    if (inlineStorage)
        RebuildInlineDense(db, byId, order, recordSize);
    else
        RebuildPointerDense(db, byId, order);
}

void DBCPatch::FixupStrings(void* record, const std::vector<uint16_t>& strOffsets, const char* internedBlob,
                            uint32_t blobSize)
{
    uint8_t* rec = static_cast<uint8_t*>(record);
    for (uint16_t fieldOff : strOffsets)
    {
        uint32_t blobOff;
        std::memcpy(&blobOff, rec + fieldOff, sizeof(uint32_t));
        const char* str;
        if (blobOff == kStrSentinelNull || !internedBlob)
            str = "";
        else if (blobOff < blobSize)
            str = internedBlob + blobOff;
        else
            str = "";
        std::memcpy(rec + fieldOff, &str, sizeof(const char*));
    }
}

void DBCPatch::WriteRecord(void* dst, const uint8_t* image, uint32_t recordSize,
                           const std::vector<uint16_t>& strOffsets, const char* blob, uint32_t blobSize)
{
    std::memcpy(dst, image, recordSize);
    FixupStrings(dst, strOffsets, blob, blobSize);
}

bool DBCPatch::ApplyRecords(const char* dbcName, uint32_t recordSize, const std::vector<uint16_t>& strOffsets,
                            const std::vector<uint32_t>& ids, const uint8_t* images, const char* strBlock,
                            uint32_t strBlockSize, bool idsAreComplete)
{
    NativeClientDB* db = FindStorage(dbcName);
    if (!db || !db->m_loaded || recordSize == 0)
    {
        LOG_WARN << "ContentStreaming: native DBC unavailable name=" << dbcName << " size=" << recordSize
                 << " storage=" << (db ? 1 : 0) << " loaded=" << (db && db->m_loaded ? 1 : 0);
        return false;
    }

    int idSpan = db->m_maxID - db->m_minID + 1;
    bool inlineStorage =
        RowsAreInlineStorage(db->m_recordsById, idSpan, db->m_records, db->m_numRecords, recordSize);

    std::vector<int> priorOrder = SnapshotDenseOrder(db, recordSize, inlineStorage);

    char* blob = nullptr;
    if (strBlockSize && strBlock)
    {
        blob = static_cast<char*>(SMemAlloc(strBlockSize, "DBCRecordStrings", __LINE__, 0));
        if (!blob)
            return false;
        std::memcpy(blob, strBlock, strBlockSize);
        blob[strBlockSize - 1] = '\0';
    }

    size_t n = ids.size();
    if (n == 0)
        return true;

    std::vector<uint32_t> compact;
    const std::vector<uint32_t>* keys = &ids;
    if (IsDenseOnlyStorage(dbcName) && CompactRowIds(db, dbcName, priorOrder, ids, compact))
        keys = &compact;

    int batchMin = static_cast<int>((*keys)[0]), batchMax = batchMin;
    for (size_t i = 1; i < n; ++i)
    {
        int v = static_cast<int>((*keys)[i]);
        if (v < batchMin)
            batchMin = v;
        if (v > batchMax)
            batchMax = v;
    }
    if (!EnsureCapacity(db, batchMin, batchMax))
        return false;

    uint8_t* block = static_cast<uint8_t*>(SMemAlloc(n * recordSize, "DBCRecordBlock", __LINE__, 0));
    if (!block)
        return false;
    AddPatchedRange(block, block + n * recordSize);

    LOG_DEBUG << "DBC apply '" << dbcName << "' records=" << (int)n << " recSize=" << recordSize << " ids=[" << batchMin
              << ".." << batchMax << "] block=" << (int)(n * recordSize) << "B strBlob=" << (int)strBlockSize << "B";

    std::vector<bool> written;
    if (idsAreComplete)
        written.assign(static_cast<size_t>(db->m_maxID - db->m_minID + 1), false);

    for (size_t i = 0; i < n; ++i)
    {
        void** slot = SlotFor(db, (*keys)[i]);
        if (!slot)
            continue;
        uint8_t* rec = block + i * recordSize;
        WriteRecord(rec, images + i * recordSize, recordSize, strOffsets, blob, strBlockSize);
        FixupRowForClient(dbcName, rec);
        *slot = rec;
        if (idsAreComplete)
            written[static_cast<size_t>(static_cast<int>((*keys)[i]) - db->m_minID)] = true;
    }

    if (idsAreComplete)
    {
        void** byId = db->m_recordsById;
        int dropped = 0;
        for (size_t i = 0; i < written.size(); ++i)
            if (!written[i] && byId[i])
            {
                byId[i] = nullptr;
                ++dropped;
            }
        if (dropped)
            LOG_DEBUG << "DBC apply '" << dbcName << "' dropped " << dropped
                      << " rows not present in the replacement table";
    }

    void** byId = db->m_recordsById;
    int span    = db->m_maxID - db->m_minID + 1;
    std::vector<bool> placed(static_cast<size_t>(span), false);
    std::vector<int> order;
    order.reserve(static_cast<size_t>(span));

    auto take = [&](int id)
    {
        if (id < db->m_minID || id > db->m_maxID)
            return;
        size_t k = static_cast<size_t>(id - db->m_minID);
        if (placed[k] || !byId[k])
            return;
        placed[k] = true;
        order.push_back(id);
    };

    if (!idsAreComplete)
        for (int id : priorOrder)
            take(id);
    for (size_t i = 0; i < n; ++i)
        take(static_cast<int>((*keys)[i]));
    for (int i = 0; i < span; ++i)
        take(db->m_minID + i);

    RebuildDenseArray(db, recordSize, inlineStorage, order, FindDenseSortKey(dbcName));
    return true;
}
