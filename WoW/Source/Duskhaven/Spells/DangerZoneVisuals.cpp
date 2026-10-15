#include "DangerZoneVisuals.h"
#include "WoW/Source/Duskhaven/DBC/Defs/DangerZoneVisualProfile.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CAaSphere.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellRadiusRec.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Duskhaven
{
namespace DangerZoneVisuals
{
namespace
{
    constexpr uint32_t SMSG_SPELL_START   = 0x131;
    constexpr uint32_t SMSG_SPELL_GO      = 0x132;
    constexpr uint32_t MSG_CHANNEL_START  = 0x139;
    constexpr uint32_t MSG_CHANNEL_UPDATE = 0x13A;

    constexpr uint32_t CAST_FLAG_AMMO            = 0x00000020;
    constexpr uint32_t CAST_FLAG_POWER_LEFT_SELF = 0x00000800;
    constexpr uint32_t CAST_FLAG_ADJUST_MISSILE  = 0x00020000;
    constexpr uint32_t CAST_FLAG_VISUAL_CHAIN    = 0x00080000;
    constexpr uint32_t CAST_FLAG_RUNE_LIST       = 0x00200000;
    constexpr uint32_t CAST_FLAG_IMMUNITY        = 0x04000000;

    constexpr uint32_t TARGET_FLAG_UNIT            = 0x00000002;
    constexpr uint32_t TARGET_FLAG_UNIT_RAID       = 0x00000004;
    constexpr uint32_t TARGET_FLAG_UNIT_PARTY      = 0x00000008;
    constexpr uint32_t TARGET_FLAG_ITEM            = 0x00000010;
    constexpr uint32_t TARGET_FLAG_SOURCE_LOCATION = 0x00000020;
    constexpr uint32_t TARGET_FLAG_DEST_LOCATION   = 0x00000040;
    constexpr uint32_t TARGET_FLAG_UNIT_ENEMY      = 0x00000080;
    constexpr uint32_t TARGET_FLAG_UNIT_ALLY       = 0x00000100;
    constexpr uint32_t TARGET_FLAG_CORPSE_ENEMY    = 0x00000200;
    constexpr uint32_t TARGET_FLAG_UNIT_DEAD       = 0x00000400;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT      = 0x00000800;
    constexpr uint32_t TARGET_FLAG_TRADE_ITEM      = 0x00001000;
    constexpr uint32_t TARGET_FLAG_STRING          = 0x00002000;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_ITEM = 0x00004000;
    constexpr uint32_t TARGET_FLAG_CORPSE_ALLY     = 0x00008000;
    constexpr uint32_t TARGET_FLAG_UNIT_MINIPET    = 0x00010000;
    constexpr uint32_t TARGET_FLAG_UNIT_PASSENGER  = 0x00100000;
    constexpr uint32_t TARGET_FLAG_UNIT_MASK = TARGET_FLAG_UNIT | TARGET_FLAG_UNIT_RAID | TARGET_FLAG_UNIT_PARTY |
                                               TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT_ALLY | TARGET_FLAG_UNIT_DEAD |
                                               TARGET_FLAG_UNIT_MINIPET | TARGET_FLAG_UNIT_PASSENGER;
    constexpr uint32_t TARGET_FLAG_GAMEOBJECT_MASK = TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_GAMEOBJECT_ITEM;
    constexpr uint32_t TARGET_FLAG_CORPSE_MASK     = TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_CORPSE_ENEMY;
    constexpr uint32_t TARGET_FLAG_ITEM_MASK = TARGET_FLAG_TRADE_ITEM | TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT_ITEM;

    constexpr uint32_t PROFILE_FLAG_CREATURE_CASTERS     = 0x01;
    constexpr uint32_t PROFILE_FLAG_DEBUG_PLAYER_CASTERS = 0x02;
    constexpr uint32_t PROFILE_FLAG_REQUIRE_HARMFUL      = 0x04;
    constexpr uint32_t PROFILE_FLAG_SHOW_CAST            = 0x08;
    constexpr uint32_t PROFILE_FLAG_SHOW_TRAVEL          = 0x10;
    constexpr uint32_t PROFILE_FLAG_SHOW_CHANNEL         = 0x20;
    constexpr uint32_t SHAPE_CIRCLE                      = 0x01;

    struct TargetData
    {
        uint32_t flags    = 0;
        uint64_t unitGuid = 0;
        uint64_t itemGuid = 0;
        C3Vector src{};
        C3Vector dst{};
        bool hasUnit = false;
        bool hasSrc  = false;
        bool hasDst  = false;
    };

    struct CastPacket
    {
        uint32_t opcode         = 0;
        uint64_t casterGuid     = 0;
        uint64_t casterUnitGuid = 0;
        uint8_t castId          = 0;
        uint32_t spellId        = 0;
        uint32_t castFlags      = 0;
        uint32_t castTime       = 0;
        TargetData target;
        bool hasMissileTrajectory = false;
        float missilePitch        = 0.0f;
        uint32_t missileTravelMs  = 0;
    };

    struct RenderModel
    {
        CM2Model* model          = nullptr;
        CMapEntity* worldObject  = nullptr;
    };

    struct DangerZoneInstance
    {
        uint32_t instanceId = 0;
        uint64_t casterGuid = 0;
        uint32_t spellId    = 0;
        C3Vector position{};
        float radius     = 5.0f;
        uint64_t startMs = 0;
        uint64_t endMs   = 0;
        std::string modelPath;
        float modelRadius = 5.0f;
        float radiusScale = 1.0f;
        float zOffset     = 0.05f;
        float red         = 1.0f;
        float green       = 0.0f;
        float blue        = 0.0f;
        float alpha       = 0.75f;
        RenderModel renderModel;
    };

    void DestroyRenderModel(RenderModel& renderModel)
    {
        if (renderModel.worldObject)
        {
            World::ObjectDestroy(renderModel.worldObject);
            renderModel.worldObject = nullptr;
        }

        if (renderModel.model)
        {
            renderModel.model->Release();
            renderModel.model = nullptr;
        }
    }

    std::unordered_map<uint32_t, DangerZoneInstance> s_instances;
    std::unordered_map<uint64_t, uint32_t> s_instanceByCasterSpell;
    uint32_t s_nextInstanceId = 1;

    uint64_t MakeCasterSpellKey(uint64_t casterGuid, uint32_t spellId)
    {
        return (casterGuid ^ (uint64_t(spellId) << 32) ^ spellId);
    }

    void ReadGuid(CDataStore* packet, uint64_t& value)
    {
        SmartGUID guid;
        *packet >> guid;
        value = guid.m_guid;
    }

    void ReadU8(CDataStore* packet, uint8_t& value)
    {
        value = 0;
        packet->Get(value);
    }

    void ReadU32(CDataStore* packet, uint32_t& value)
    {
        value = 0;
        packet->Get(value);
    }

    void ReadU64(CDataStore* packet, uint64_t& value)
    {
        value = 0;
        packet->Get(value);
    }

    void ReadFloat(CDataStore* packet, float& value)
    {
        value = 0.0f;
        packet->Get(value);
    }

    void SkipCString(CDataStore* packet)
    {
        if (!packet)
            return;
        char dummy[256];
        packet->GetString(dummy, sizeof(dummy));
    }

    C3Vector UnitPosition(uint64_t guid, C3Vector fallback)
    {
        if (!guid)
            return fallback;

        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPEMASK_UNIT, __FILE__, __LINE__));
        if (!unit)
            return fallback;

        return unit->GetPosition();
    }

    bool IsCreatureGuid(uint64_t guid)
    {
        CGObject_C* object = ClntObjMgrObjectPtr(WOWGUID{ guid }, static_cast<TypeMask>(TYPEMASK_UNIT | TYPEMASK_PLAYER), __FILE__, __LINE__);
        if (!object || !object->m_obj)
            return false;

        uint32_t const type = object->m_obj->m_type;
        return (type & TYPEMASK_UNIT) != 0 && (type & TYPEMASK_PLAYER) == 0;
    }

    bool IsPlayerGuid(uint64_t guid)
    {
        CGObject_C* object = ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPEMASK_PLAYER, __FILE__, __LINE__);
        return object && object->m_obj && (object->m_obj->m_type & TYPEMASK_PLAYER) != 0;
    }

    float Distance2D(C3Vector const& a, C3Vector const& b)
    {
        float const dx = a.x - b.x;
        float const dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool GroundPosition(C3Vector const& position, float zOffset, C3Vector& out)
    {
        C3Vector start(position.x, position.y, position.z + 50.0f);
        C3Vector end(position.x, position.y, position.z - 200.0f);
        C3Vector hit;
        float distance = 1.0f;
        if (World::Intersect(start, end, &hit, &distance, 0x10111, nullptr))
        {
            out = C3Vector(hit.x, hit.y, hit.z + zOffset);
            return true;
        }

        out = C3Vector(position.x, position.y, position.z + zOffset);
        return false;
    }

    float RadiusForSpell(SpellRec const& spell)
    {
        float radius = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            int32_t radiusId = spell.m_effectRadiusIndex[i];
            if (!radiusId)
                continue;

            SpellRadiusRec* rec = g_spellRadiusDB.GetRecord(radiusId);
            if (rec)
                radius = (std::max)(radius, rec->m_radius);
        }

        return radius;
    }

    bool IsHarmfulEffect(uint32_t effect)
    {
        switch (effect)
        {
            case 2:   // SCHOOL_DAMAGE
            case 7:   // ENVIRONMENTAL_DAMAGE
            case 9:   // HEALTH_LEECH
            case 27:  // PERSISTENT_AREA_AURA
            case 32:  // TRIGGER_MISSILE
            case 129: // APPLY_AREA_AURA_ENEMY
            case 148: // TRIGGER_MISSILE_SPELL_WITH_VALUE
                return true;
            default:
                return false;
        }
    }

    bool HasHarmfulEffect(SpellRec const& spell)
    {
        for (int i = 0; i < 3; ++i)
            if (IsHarmfulEffect(spell.m_effect[i]))
                return true;
        return false;
    }

    bool IsCasterAreaTarget(uint32_t target)
    {
        switch (target)
        {
            case 1:  // UNIT_CASTER
            case 15: // UNIT_SRC_AREA_ENEMY
            case 18: // DEST_CASTER
            case 20: // UNIT_CASTER_AREA_PARTY
            case 30: // UNIT_SRC_AREA_ALLY
            case 33: // UNIT_SRC_AREA_PARTY
            case 36: // DEST_CASTER_UNK36
            case 39: // DEST_CASTER_FISHING
            case 41:
            case 42:
            case 43:
            case 44:
            case 47:
            case 48:
            case 49:
            case 50:
            case 56:  // UNIT_CASTER_AREA_RAID
            case 62:  // DEST_DYNOBJ_CASTER
            case 72:  // DEST_CASTER_RANDOM
            case 73:  // DEST_CASTER_RADIUS
            case 106: // DEST_CHANNEL_CASTER
            case 111: // UNIT_CASTER_AREA_SUMMONS
                return true;
            default:
                return false;
        }
    }

    bool IsTargetAreaTarget(uint32_t target)
    {
        switch (target)
        {
            case 8:  // UNIT_DEST_AREA_ENTRY
            case 16: // UNIT_DEST_AREA_ENEMY
            case 28: // DEST_DYNOBJ_ENEMY
            case 29: // DEST_DYNOBJ_ALLY
            case 31: // UNIT_DEST_AREA_ALLY
            case 34: // UNIT_DEST_AREA_PARTY
            case 52: // GAMEOBJECT_DEST_AREA
            case 53: // DEST_TARGET_ENEMY
            case 63:
            case 64:
            case 65:
            case 66:
            case 67:
            case 68:
            case 69:
            case 70:
            case 71:
            case 74: // DEST_TARGET_RANDOM
            case 75: // DEST_TARGET_RADIUS
            case 76: // DEST_CHANNEL_TARGET
            case 77: // UNIT_CHANNEL_TARGET
            case 87: // DEST_DEST
            case 88: // DEST_DYNOBJ_NONE
            case 89: // DEST_TRAJ
            case 91: // DEST_DEST_RADIUS
                return true;
            default:
                return false;
        }
    }

    bool ResolveDangerPosition(CastPacket const& cast, SpellRec const& spell, C3Vector& out)
    {
        C3Vector casterPos = UnitPosition(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, C3Vector(0.0f, 0.0f, 0.0f));

        if (cast.target.hasDst)
        {
            out = cast.target.dst;
            return true;
        }

        for (int i = 0; i < 3; ++i)
        {
            if (IsCasterAreaTarget(spell.m_implicitTargetA[i]) || IsCasterAreaTarget(spell.m_implicitTargetB[i]))
            {
                out = casterPos;
                return true;
            }

            if (IsTargetAreaTarget(spell.m_implicitTargetA[i]) || IsTargetAreaTarget(spell.m_implicitTargetB[i]))
            {
                if (cast.target.hasUnit)
                {
                    out = UnitPosition(cast.target.unitGuid, casterPos);
                    return true;
                }
            }
        }

        if (cast.target.hasUnit)
        {
            out = UnitPosition(cast.target.unitGuid, casterPos);
            return true;
        }

        out = casterPos;
        return casterPos.x != 0.0f || casterPos.y != 0.0f || casterPos.z != 0.0f;
    }

    DangerZoneVisualProfileRow const* FindProfile(SpellRec const& spell, uint32_t spellId, bool playerCaster,
                                                  bool harmful)
    {
        DangerZoneVisualProfileRow const* fallback = nullptr;
        for (uint32_t i = 0; i < DangerZoneVisualProfileDB::GetNumRows(); ++i)
        {
            DangerZoneVisualProfileRow const* row = DangerZoneVisualProfileDB::GetRowByIndex(i);
            if (!row || !row->modelPath || !row->modelPath[0])
                continue;

            if ((row->shapeMask & SHAPE_CIRCLE) == 0)
                continue;

            if ((row->schoolMask & int(spell.m_schoolMask)) == 0)
                continue;

            if ((row->flags & PROFILE_FLAG_REQUIRE_HARMFUL) != 0 && !harmful)
                continue;

            if (playerCaster)
            {
                if (row->debugSpellId != int(spellId) || (row->flags & PROFILE_FLAG_DEBUG_PLAYER_CASTERS) == 0)
                    continue;
                return row;
            }

            if ((row->flags & PROFILE_FLAG_CREATURE_CASTERS) == 0)
                continue;

            if (!fallback || row->debugSpellId == 0)
                fallback = row;
        }

        return fallback;
    }

    bool ReadLocation(CDataStore* packet, C3Vector& out)
    {
        uint64_t transportGuid = 0;
        ReadGuid(packet, transportGuid);
        ReadFloat(packet, out.x);
        ReadFloat(packet, out.y);
        ReadFloat(packet, out.z);
        return true;
    }

    bool ReadTargetData(CDataStore* packet, TargetData& target)
    {
        ReadU32(packet, target.flags);

        if (target.flags & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_GAMEOBJECT_MASK | TARGET_FLAG_CORPSE_MASK))
        {
            ReadGuid(packet, target.unitGuid);
            target.hasUnit = target.unitGuid != 0;
        }

        if (target.flags & TARGET_FLAG_ITEM_MASK)
            ReadGuid(packet, target.itemGuid);

        if (target.flags & TARGET_FLAG_SOURCE_LOCATION)
            target.hasSrc = ReadLocation(packet, target.src);

        if (target.flags & TARGET_FLAG_DEST_LOCATION)
            target.hasDst = ReadLocation(packet, target.dst);

        if (target.flags & TARGET_FLAG_STRING)
            SkipCString(packet);

        return true;
    }

    bool ParseCastPacket(CDataStore* packet, uint32_t opcode, CastPacket& out)
    {
        if (!packet)
            return false;

        uint32_t const savedRead = packet->m_read;
        out.opcode               = opcode;

        ReadGuid(packet, out.casterGuid);
        ReadGuid(packet, out.casterUnitGuid);
        ReadU8(packet, out.castId);
        ReadU32(packet, out.spellId);
        ReadU32(packet, out.castFlags);
        ReadU32(packet, out.castTime);

        if (opcode == SMSG_SPELL_GO)
        {
            uint8_t hitCount = 0;
            ReadU8(packet, hitCount);
            for (uint8_t i = 0; i < hitCount; ++i)
            {
                uint64_t ignored = 0;
                ReadU64(packet, ignored);
            }

            uint8_t missCount = 0;
            ReadU8(packet, missCount);
            for (uint8_t i = 0; i < missCount; ++i)
            {
                uint64_t ignored = 0;
                uint8_t reason   = 0;
                ReadU64(packet, ignored);
                ReadU8(packet, reason);
                if (reason == 11)
                    ReadU8(packet, reason);
            }
        }

        ReadTargetData(packet, out.target);

        if (out.castFlags & CAST_FLAG_POWER_LEFT_SELF)
        {
            uint32_t ignored = 0;
            ReadU32(packet, ignored);
        }

        if (out.castFlags & CAST_FLAG_RUNE_LIST)
        {
            uint8_t start = 0;
            uint8_t count = 0;
            ReadU8(packet, start);
            ReadU8(packet, count);
            for (uint8_t i = 0; i < count; ++i)
                ReadU8(packet, start);
        }

        if (out.castFlags & CAST_FLAG_ADJUST_MISSILE)
        {
            ReadFloat(packet, out.missilePitch);
            ReadU32(packet, out.missileTravelMs);
            out.hasMissileTrajectory = out.missileTravelMs > 0;
        }

        packet->m_read = savedRead;
        return out.spellId != 0 && out.casterGuid != 0;
    }

    C44Matrix MakeMatrix(C3Vector const& position, float radius, DangerZoneInstance const& instance)
    {
        float scale = radius * instance.radiusScale / (std::max)(0.01f, instance.modelRadius);
        scale       = (std::max)(0.01f, scale);
        C44Matrix m;
        m.a0 = scale;      m.a1 = 0.0f;       m.a2 = 0.0f;       m.a3 = 0.0f;
        m.b0 = 0.0f;       m.b1 = scale;      m.b2 = 0.0f;       m.b3 = 0.0f;
        m.c0 = 0.0f;       m.c1 = 0.0f;       m.c2 = scale;      m.c3 = 0.0f;
        m.d0 = position.x; m.d1 = position.y; m.d2 = position.z; m.d3 = 1.0f;
        return m;
    }

    void UpdateRenderModel(DangerZoneInstance& instance)
    {
        if (!instance.renderModel.model && !instance.modelPath.empty())
        {
            if (!World::s_m2Scene)
                return;

            instance.renderModel.model = World::s_m2Scene->CreateModel(instance.modelPath.c_str(), 0);
            if (!instance.renderModel.model)
                return;
        }

        if (!instance.renderModel.model)
            return;

        C44Matrix matrix = MakeMatrix(instance.position, instance.radius, instance);
        instance.renderModel.model->m_fFlags |= 0x8000;
        instance.renderModel.model->m_worldTransform = matrix;

        if (!instance.renderModel.worldObject && instance.renderModel.model->IsLoaded(0, 0))
        {
            instance.renderModel.worldObject = CMap::ObjectCreate(
                instance.renderModel.model, CGWorldFrame::MissileEnumProc, nullptr, 0, 0, 6);
        }

        if (instance.renderModel.worldObject)
        {
            float const boundsRadius = (std::max)(1.0f, instance.radius + 2.0f);
            CAaBox bounds(C3Vector(-boundsRadius, -boundsRadius, -1.0f), C3Vector(boundsRadius, boundsRadius, 1.0f));
            CAaSphere sphere;
            sphere.c = C3Vector(0.0f, 0.0f, 0.0f);
            sphere.r = 0.0f;
            C3Vector localOffset(0.0f, 0.0f, 0.0f);
            CMap::ObjectUpdate(instance.renderModel.worldObject, matrix, bounds, sphere, localOffset, false, 0xFFFFFFFF);
        }
    }

    void RemoveInstance(uint32_t instanceId)
    {
        auto itr = s_instances.find(instanceId);
        if (itr == s_instances.end())
            return;

        s_instanceByCasterSpell.erase(MakeCasterSpellKey(itr->second.casterGuid, itr->second.spellId));
        DestroyRenderModel(itr->second.renderModel);
        s_instances.erase(itr);
    }

    void SpawnOrExtend(CastPacket const& cast, SpellRec const& spell, DangerZoneVisualProfileRow const& profile,
                       C3Vector const& position, float radius, uint32_t durationMs)
    {
        uint32_t const now = OsGetAsyncTimeMs();
        uint32_t const clampedDuration =
            std::clamp<uint32_t>(durationMs, uint32_t((std::max)(1, profile.minDurationMs)),
                                 uint32_t((std::max)(profile.minDurationMs, profile.maxDurationMs)));

        uint64_t const key =
            MakeCasterSpellKey(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, cast.spellId);
        auto existing = s_instanceByCasterSpell.find(key);
        if (existing != s_instanceByCasterSpell.end())
        {
            auto instanceItr = s_instances.find(existing->second);
            if (instanceItr != s_instances.end())
            {
                DangerZoneInstance& instance = instanceItr->second;
                GroundPosition(position, profile.zOffset, instance.position);
                instance.radius = radius;
                instance.endMs  = (std::max)(instance.endMs, static_cast<uint64_t>(now) + clampedDuration);
                return;
            }
        }

        DangerZoneInstance instance{};
        instance.instanceId = s_nextInstanceId++;
        if (!s_nextInstanceId)
            s_nextInstanceId = 1;
        instance.casterGuid = cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid;
        instance.spellId    = cast.spellId;
        GroundPosition(position, profile.zOffset, instance.position);
        instance.radius      = radius;
        instance.startMs     = now;
        instance.endMs       = static_cast<uint64_t>(now) + clampedDuration;
        instance.modelPath   = profile.modelPath ? profile.modelPath : "";
        instance.modelRadius = profile.modelRadius > 0.0f ? profile.modelRadius : 5.0f;
        instance.radiusScale = profile.radiusScale > 0.0f ? profile.radiusScale : 1.0f;
        instance.zOffset     = profile.zOffset;
        instance.red         = profile.red;
        instance.green       = profile.green;
        instance.blue        = profile.blue;
        instance.alpha       = profile.alpha;

        s_instanceByCasterSpell[key] = instance.instanceId;
        s_instances.emplace(instance.instanceId, std::move(instance));
    }

    uint32_t EstimateTravelMs(CastPacket const& cast, SpellRec const& spell, C3Vector const& position)
    {
        if (cast.hasMissileTrajectory)
            return cast.missileTravelMs;

        if (spell.m_speed <= 0.001f)
            return 0;

        C3Vector casterPos   = UnitPosition(cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid, position);
        float const distance = Distance2D(casterPos, position);
        if (distance <= 0.01f)
            return 0;

        return uint32_t(distance / spell.m_speed * 1000.0f);
    }

    void ConsiderCast(CastPacket const& cast, bool isStart)
    {
        SpellRec spell;
        if (!g_spellDB.GetRecord(cast.spellId, &spell))
            return;

        uint64_t caster = cast.casterUnitGuid ? cast.casterUnitGuid : cast.casterGuid;
        bool const creatureCaster = IsCreatureGuid(caster);
        bool const playerCaster   = IsPlayerGuid(caster);
        bool const harmful        = HasHarmfulEffect(spell);
        DangerZoneVisualProfileRow const* profile = FindProfile(spell, cast.spellId, playerCaster, harmful);
        if (!profile)
            return;

        if (!creatureCaster && !(playerCaster && profile->debugSpellId == int(cast.spellId)))
            return;

        if (isStart && (profile->flags & PROFILE_FLAG_SHOW_CAST) == 0)
            return;

        if (!isStart && (profile->flags & PROFILE_FLAG_SHOW_TRAVEL) == 0)
            return;

        float radius = RadiusForSpell(spell);
        if (radius <= 0.01f)
            return;

        C3Vector position{};
        if (!ResolveDangerPosition(cast, spell, position))
            return;

        uint32_t durationMs = isStart ? cast.castTime : EstimateTravelMs(cast, spell, position);
        if (durationMs == 0)
            return;

        SpawnOrExtend(cast, spell, *profile, position, radius, durationMs);
    }

    void ConsiderChannel(uint64_t casterGuid, uint32_t spellId, uint32_t durationMs)
    {
        if (!casterGuid || !spellId)
            return;

        if (durationMs == 0)
        {
            auto existing = s_instanceByCasterSpell.find(MakeCasterSpellKey(casterGuid, spellId));
            if (existing != s_instanceByCasterSpell.end())
                RemoveInstance(existing->second);
            return;
        }

        SpellRec spell;
        if (!g_spellDB.GetRecord(spellId, &spell))
            return;

        bool const creatureCaster                 = IsCreatureGuid(casterGuid);
        bool const playerCaster                   = IsPlayerGuid(casterGuid);
        bool const harmful                        = HasHarmfulEffect(spell);
        DangerZoneVisualProfileRow const* profile = FindProfile(spell, spellId, playerCaster, harmful);
        if (!profile || (profile->flags & PROFILE_FLAG_SHOW_CHANNEL) == 0)
            return;

        if (!creatureCaster && !(playerCaster && profile->debugSpellId == int(spellId)))
            return;

        float radius = RadiusForSpell(spell);
        if (radius <= 0.01f)
            return;

        CastPacket cast{};
        cast.casterGuid     = casterGuid;
        cast.casterUnitGuid = casterGuid;
        cast.spellId        = spellId;

        C3Vector position = UnitPosition(casterGuid, C3Vector(0.0f, 0.0f, 0.0f));
        SpawnOrExtend(cast, spell, *profile, position, radius, durationMs);
    }

    void HandleSpellPacket(uint32_t opcode, CDataStore* packet)
    {
        CastPacket cast{};
        if (!ParseCastPacket(packet, opcode, cast))
            return;

        ConsiderCast(cast, opcode == SMSG_SPELL_START);
    }
} // namespace

void HandleSpellPacketPreservingCursor(uint32_t opcode, CDataStore* packet)
{
    if (!packet)
        return;

    uint32_t const savedRead = packet->m_read;
    HandleSpellPacket(opcode, packet);
    packet->m_read = savedRead;
}

void HandleChannelStart(CDataStore* packet)
{
    if (!packet)
        return;

    uint32_t const savedRead = packet->m_read;
    uint64_t casterGuid      = 0;
    uint32_t spellId         = 0;
    uint32_t durationMs      = 0;
    ReadGuid(packet, casterGuid);
    ReadU32(packet, spellId);
    ReadU32(packet, durationMs);
    packet->m_read = savedRead;
    ConsiderChannel(casterGuid, spellId, durationMs);
}

void HandleChannelUpdate(CDataStore* packet)
{
    if (!packet)
        return;

    uint32_t const savedRead = packet->m_read;
    uint64_t casterGuid      = 0;
    uint32_t durationMs      = 0;
    ReadGuid(packet, casterGuid);
    ReadU32(packet, durationMs);
    packet->m_read = savedRead;

    CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ casterGuid }, TYPEMASK_UNIT, __FILE__, __LINE__));
    if (!unit)
        return;

    uint32_t spellId = unit->m_channelSpellId;
    ConsiderChannel(casterGuid, spellId, durationMs);
}

void Pump()
{
    uint32_t const now = OsGetAsyncTimeMs();
    static thread_local std::vector<uint32_t> expired;
    expired.clear();

    for (auto& pair : s_instances)
    {
        DangerZoneInstance& instance = pair.second;
        if (static_cast<uint64_t>(now) >= instance.endMs)
        {
            expired.push_back(pair.first);
            continue;
        }

        UpdateRenderModel(instance);
    }

    for (uint32_t instanceId : expired)
        RemoveInstance(instanceId);
}

void ClearAll()
{
    for (auto& pair : s_instances)
    {
        DestroyRenderModel(pair.second.renderModel);
    }
    s_instances.clear();
    s_instanceByCasterSpell.clear();
}

} // namespace DangerZoneVisuals
} // namespace Duskhaven
