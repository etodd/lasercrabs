#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"
#include "data/import_common.h"
#include "sync.h"
#include "game/constants.h"
#include "game/audio.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"
#include "data/priority_queue.h"

namespace VI
{

struct AIAgent;
struct PlayerManager;
struct RenderParams;

namespace AI
{


enum class Op : s8
{
	Load,
	ObstacleAdd,
	ObstacleRemove,
	Pathfind,
	DronePathfind,
	DroneMarkAdjacencyBad,
	DroneClosestPoint,
	RandomPath,
	ClosestWalkPoint,
	UpdateState,
	Quit,
	RecordInit,
	RecordAdd,
	RecordClose,
	AudioPathfind,
	count,
};

enum class Callback : s8
{
	Path,
	DronePath,
	Point,
	DronePoint,
	Load,
	AudioPath,
	count,
};

enum class DronePathfind : s8
{
	LongRange,
	Target,
	Away,
	Random,
	Spawn,
	count,
};

enum class DroneAllow : s8
{
	Crawl = 1,
	Shoot = 1 << 1,
	All = Crawl | Shoot,
};

struct SensorState
{
	Vec3 pos;
	AI::Team team;
};

typedef SensorState ForceFieldState;

struct NavGameState
{
	Array<SensorState> sensors;
	Array<ForceFieldState> force_fields;
	void clear();
};

typedef StaticArray<Vec3, AI_MAX_PATH_LENGTH> Path;

struct DronePathNode
{
	enum Flags
	{
		FlagCrawledFromParent = 1 << 0,
	};

	Vec3 pos;
	Vec3 normal;
	DroneNavMeshNode ref;
	s8 flags;

	inline void flag(Flags f, b8 v)
	{
		if (v)
			flags |= f;
		else
			flags &= ~f;
	}

	inline b8 flag(Flags f) const
	{
		return flags & f;
	}
};
typedef StaticArray<DronePathNode, AI_MAX_PATH_LENGTH> DronePath;

struct Result
{
	Path path;
	u32 id;
};

struct DroneResult
{
	DronePath path;
	u32 id;
};

// if an AI is inside a PathZone and wants to reach one of the targets associated with that zone,
// it must pass through the choke point first.
// this is used for path distance estimation.
struct PathZone
{
	static const PathZone* get(const Vec3&, const Entity*);

	StaticArray<Ref<Entity>, 16> targets;
	Vec3 pos;
	Vec3 radius;
	Vec3 choke_point;
};

static const s32 SYNC_IN_SIZE = 256 * 1024;
static const s32 SYNC_OUT_SIZE = 256 * 1024;
extern Array<b8> obstacles;
extern SyncRingBuffer<SYNC_IN_SIZE> sync_in;
extern SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
extern u32 callback_in_id;
extern u32 callback_out_id;
extern u32 record_id_current;

b8 match(AI::Team, AI::TeamMask);
u32 obstacle_add(const Vec3&, r32, r32);
void obstacle_remove(u32);
u32 pathfind(const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
u32 drone_pathfind(DronePathfind, DroneAllow, AI::Team, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
void drone_mark_adjacency_bad(DroneNavMeshNode, DroneNavMeshNode);
u32 drone_closest_point(const Vec3&, AI::Team, const LinkEntryArg<const DronePathNode&>&);
u32 random_path(const Vec3&, const Vec3&, AI::Team, r32, const LinkEntryArg<const Result&>&);
u32 closest_walk_point(const Vec3&, const LinkEntryArg<const Vec3&>&);
u32 drone_random_path(DroneAllow, AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
void load(AssetID, const char*, const char*);
void init();
void loop();
void quit();
void update(const Update&);
void debug_draw_nav_mesh(const RenderParams&);
void draw_hollow(const RenderParams&);
r32 audio_pathfind(const Vec3&, const Vec3&);
u32 audio_pathfind(const Vec3&, const Vec3&, AudioEntry*, s8, r32);

b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

namespace Worker
{
	struct NavMeshProcess : public dtTileCacheMeshProcess
	{
		void process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags);
	};

	struct DroneNavMeshNodeData
	{
		enum Flags
		{
			FlagInQueue = (1 << 0),
			FlagVisited = (1 << 1),
			FlagCrawledFromParent = (1 << 2),
		};

		r32 travel_score;
		r32 estimate_score;
		r32 sensor_score;
		DroneNavMeshNode parent;
		s8 flags;

		inline void flag(Flags f, b8 v)
		{
			if (v)
				flags |= f;
			else
				flags &= ~f;
		}

		inline b8 flag(Flags f) const
		{
			return flags & f;
		}
	};

	struct DroneNavMeshKey
	{
		Chunks<Array<DroneNavMeshNodeData>> data;
		r32 priority(const DroneNavMeshNode&);
		void resize(const DroneNavMesh&);
		void reset();
		DroneNavMeshNodeData& get(const DroneNavMeshNode&);
	};

	typedef PriorityQueue<DroneNavMeshNode, DroneNavMeshKey> AstarQueue;

	struct DroneNavContext
	{
		const DroneNavMesh& mesh;
		DroneNavMeshKey* key;
		const NavGameState& game_state;
		AstarQueue* astar_queue;
		s8 flags;
	};

	enum DroneNavFlags
	{
		DroneNavFlagBias = (1 << 0),
	};

	const extern r32 default_search_extents[];

	extern dtNavMesh* nav_mesh;
	extern dtNavMeshQuery* nav_mesh_query;
	extern dtTileCache* nav_tile_cache;
	extern dtQueryFilter default_query_filter;
	extern dtTileCache* nav_tile_cache;
	extern dtTileCacheAlloc nav_tile_allocator;
	extern FastLZCompressor nav_tile_compressor;
	extern NavMeshProcess nav_tile_mesh_process;

	void loop();

	r32 audio_pathfind(const DroneNavContext&, const Vec3&, const Vec3&);
}

extern ComponentMask entity_mask;
void entity_info(const Entity*, AI::Team, AI::Team*, s8* = nullptr);

struct RecordedLife
{
	struct Tag
	{
		enum BatteryState
		{
			BatteryStateNone = 0,
			BatteryStateNeutral = 1 << 0,
			BatteryStateFriendly = 1 << 1,
			BatteryStateEnemy = 1 << 2,
		};

		Vec3 pos;
		Vec3 normal;
		s32 upgrades;
		s32 enemy_upgrades;
		s32 nearby_entities;
		s32 battery_state;
		s32 turret_state;
		s16 energy;
		s8 shield;
		u8 time_remaining;
		b8 stealth;

		s32 battery_count(BatteryState) const;
		BatteryState battery(s32) const;
		s32 turret_count() const;
		b8 turret(s32) const;

		void init(const PlayerManager*);
	};

	struct Action
	{
		static const s8 TypeNone = 0;
		static const s8 TypeMove = 1;
		static const s8 TypeAttack = 2;
		static const s8 TypeUpgrade = 3;
		static const s8 TypeAbility = 4;
		static const s8 TypeWait = 5;
		static const s8 TypeRunAway = 6;
		static const s8 TypeSpawn = 7;

		Vec3 pos; // for move, spawn, and build ability actions
		Vec3 normal; // for move, spawn, and build ability actions
		s8 type;
		union
		{
			Ability ability; // for build and shoot ability actions
			Upgrade upgrade; // for upgrade actions
		};
		s8 entity_type; // for attack and shoot ability actions
		Action();
		Action& operator=(const Action&);

		b8 fuzzy_equal(const Action&) const;
	};

	static const s8 EntityNone = -1;
	static const s8 EntitySensorEnemy = 0;
	static const s8 EntitySensorFriend = 1;
	static const s8 EntityBatteryEnemy = 2;
	static const s8 EntityBatteryFriend = 3;
	static const s8 EntityBatteryNeutral = 4;
	static const s8 EntityMinionEnemy = 5;
	static const s8 EntityMinionFriend = 6;
	static const s8 EntityForceFieldEnemy = 7;
	static const s8 EntityForceFieldFriend = 8;
	static const s8 EntityDroneEnemyShield2 = 9;
	static const s8 EntityDroneEnemyShield1 = 10;
	static const s8 EntityDroneFriendShield2 = 11;
	static const s8 EntityDroneFriendShield1 = 12;
	static const s8 EntityBoltEnemy = 13;
	static const s8 EntityBoltFriend = 14;
	static const s8 EntityGrenadeEnemyAttached = 15;
	static const s8 EntityGrenadeEnemyDetached = 16;
	static const s8 EntityGrenadeFriendAttached = 17;
	static const s8 EntityGrenadeFriendDetached = 18;
	static const s8 EntitySpawnPointFriend = 19;
	static const s8 EntitySpawnPointEnemy = 20;
	static const s8 EntityTurretFriend = 21;
	static const s8 EntityTurretEnemy = 22;
	static const s8 EntityCoreModuleInvincible = 23;
	static const s8 EntityCoreModuleVulnerable = 24;

	Array<Vec3> pos;
	Array<Vec3> normal;
	Array<s32> upgrades;
	Array<s32> enemy_upgrades;
	Array<s32> nearby_entities;
	Array<s32> battery_state;
	Array<s32> turret_state;
	Array<s16> energy;
	Array<s8> shield;
	Array<s8> time_remaining;
	Array<b8> stealth;
	Array<Action> action;
	AI::Team team;
	s8 drones_remaining;

	void reset();
	void reset(AI::Team, s8);
	void add(const Tag&, const Action&);

	static size_t custom_fwrite(void*, size_t, size_t, FILE*);
	static size_t custom_fread(void*, size_t, size_t, FILE*);
	void serialize(FILE*, size_t(*)(void*, size_t, size_t, FILE*));
};

u32 record_init(Team, s8);
void record_add(u32, const RecordedLife::Tag&, const RecordedLife::Action&);
void record_close(u32);


}

struct AIAgent : public ComponentType<AIAgent>
{
	AI::Team team;
	b8 stealth;
	void awake() {}
};

template<typename T>
struct FSM
{
	T current;
	T last;
	r32 time;

	FSM()
		: current(), last(), time()
	{
	}

	FSM(T state)
		: current(state), last(state), time()
	{
	}

	inline b8 transition(T t)
	{
		if (t != current)
		{
			last = current;
			current = t;
			time = 0.0f;
			return true;
		}
		return false;
	}
};


}