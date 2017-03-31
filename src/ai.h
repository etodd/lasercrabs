#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"
#include "data/import_common.h"
#include "sync.h"
#include "game/constants.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"

namespace VI
{

struct AIAgent;
struct RenderParams;

namespace AI
{
	enum class Op
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
	};

	enum class Callback
	{
		Path,
		DronePath,
		Point,
		DronePoint,
		Load,
	};

	enum class DronePathfind
	{
		LongRange,
		Target,
		Away,
		Random,
		Spawn,
	};

	enum class DroneAllow
	{
		Crawl = 1,
		Shoot = 1 << 1,
		All = Crawl | Shoot,
	};

	typedef StaticArray<Vec3, AI_MAX_PATH_LENGTH> Path;

	struct DronePathNode
	{
		Vec3 pos;
		Vec3 normal;
		DroneNavMeshNode ref;
		b8 crawl;
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
	
	struct SensorState
	{
		Vec3 pos;
		AI::Team team;
	};

	typedef SensorState ForceFieldState;

	static const s32 SYNC_IN_SIZE = 512 * 1024;
	static const s32 SYNC_OUT_SIZE = 512 * 1024;
	extern Array<b8> obstacles;
	extern SyncRingBuffer<SYNC_IN_SIZE> sync_in;
	extern SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
	extern u32 callback_in_id;
	extern u32 callback_out_id;

	b8 match(AI::Team, AI::TeamMask);
	u32 obstacle_add(const Vec3&, r32, r32);
	void obstacle_remove(u32);
	u32 pathfind(const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
	u32 drone_pathfind(DronePathfind, DroneAllow, AI::Team, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
	void drone_mark_adjacency_bad(DroneNavMeshNode, DroneNavMeshNode);
	u32 drone_closest_point(const Vec3&, AI::Team, const LinkEntryArg<const DronePathNode&>&);
	u32 random_path(const Vec3&, const Vec3&, r32, const LinkEntryArg<const Result&>&);
	u32 closest_walk_point(const Vec3&, const LinkEntryArg<const Vec3&>&);
	u32 drone_random_path(DroneAllow, AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
	void load(AssetID, const char*, const char*);
	void loop();
	void quit();
	void update(const Update&);
	void debug_draw_nav_mesh(const RenderParams&);
	void debug_draw_drone_nav_mesh(const RenderParams&);

	b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

	namespace Worker
	{
		struct NavMeshProcess : public dtTileCacheMeshProcess
		{
			void process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags);
		};

		struct DroneNavMeshNodeData
		{
			r32 travel_score;
			r32 estimate_score;
			r32 sensor_score;
			DroneNavMeshNode parent;
			b8 visited;
			b8 in_queue;
			b8 crawled_from_parent;
		};

		struct DroneNavMeshKey
		{
			Chunks<Array<DroneNavMeshNodeData>> data;
			r32 priority(const DroneNavMeshNode&);
			void resize(const DroneNavMesh&);
			void reset();
			DroneNavMeshNodeData& get(const DroneNavMeshNode&);
		};

		const extern r32 default_search_extents[];

		extern dtNavMesh* nav_mesh;
		extern DroneNavMesh drone_nav_mesh;
		extern DroneNavMeshKey drone_nav_mesh_key;
		extern dtNavMeshQuery* nav_mesh_query;
		extern dtTileCache* nav_tile_cache;
		extern dtQueryFilter default_query_filter;
		extern dtTileCache* nav_tile_cache;
		extern dtTileCacheAlloc nav_tile_allocator;
		extern FastLZCompressor nav_tile_compressor;
		extern NavMeshProcess nav_tile_mesh_process;

		void loop();
	}

	extern ComponentMask entity_mask;
	void entity_info(Entity*, AI::Team, AI::Team*, s8*);

	struct RecordedLife
	{
		struct ControlPointState
		{
			static const s8 StateNormal = 0;
			static const s8 StateLosingFirstHalf = 1;
			static const s8 StateLosingSecondHalf = 2;
			static const s8 StateRecapturingFirstHalf = 3;
			static const s8 StateLost = 4;

			s8 a;
			s8 b;
		};

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
			s16 energy;
			ControlPointState control_point_state;
			s8 shield;
			u8 time_remaining;
			b8 stealth;

			s32 battery_count(BatteryState) const;
			BatteryState battery(s32) const;

			void init(Entity*);
		};

		struct Action
		{
			static const s8 TypeNone = 0;
			static const s8 TypeMove = 1;
			static const s8 TypeAttack = 2;
			static const s8 TypeUpgrade = 3;
			static const s8 TypeAbility = 4;
			static const s8 TypeCapture = 5;
			static const s8 TypeWait = 6;

			Vec3 pos; // for move and build ability actions
			Vec3 normal; // for move and build ability actions
			s8 type;
			union
			{
				s8 ability; // for build and shoot ability actions
				s8 upgrade; // for upgrade actions
				s8 entity_type; // for attack and shoot ability actions
			};
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
		static const s8 EntityRocketEnemyAttached = 9;
		static const s8 EntityRocketEnemyDetached = 10;
		static const s8 EntityRocketFriendAttached = 11;
		static const s8 EntityRocketFriendDetached = 12;
		static const s8 EntityDroneEnemyShield2 = 13;
		static const s8 EntityDroneEnemyShield1 = 14;
		static const s8 EntityDroneFriendShield2 = 15;
		static const s8 EntityDroneFriendShield1 = 16;
		static const s8 EntityDecoyEnemyShield2 = 17;
		static const s8 EntityDecoyEnemyShield1 = 18;
		static const s8 EntityProjectileEnemy = 19;
		static const s8 EntityProjectileFriend = 20;
		static const s8 EntityGrenadeEnemyAttached = 21;
		static const s8 EntityGrenadeEnemyDetached = 22;
		static const s8 EntityGrenadeFriendAttached = 23;
		static const s8 EntityGrenadeFriendDetached = 24;

		Array<Vec3> pos;
		Array<Vec3> normal;
		Array<s32> upgrades;
		Array<s32> enemy_upgrades;
		Array<s32> nearby_entities;
		Array<s32> battery_state;
		Array<s16> energy;
		Array<ControlPointState> control_point_state;
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