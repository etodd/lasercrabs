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

struct RectifierState
{
	Vec3 pos;
	AI::Team team;
};

typedef RectifierState ForceFieldState;

struct NavGameState
{
	Array<RectifierState> rectifiers;
	Array<ForceFieldState> force_fields;
	void clear();
};

typedef StaticArray<Vec3, AI_MAX_PATH_LENGTH> Path;

struct DronePathNode
{
	enum Flags : s8
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
extern Bitmask<nav_max_obstacles> obstacles;
extern SyncRingBuffer<SYNC_IN_SIZE> sync_in;
extern SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
extern u32 callback_in_id;
extern u32 callback_out_id;
extern u32 record_id_current;

b8 match(AI::Team, AI::TeamMask);
u32 obstacle_add(const Vec3&, r32, r32);
void obstacle_remove(u32);
u32 pathfind(AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
u32 drone_pathfind(DronePathfind, DroneAllow, AI::Team, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
void drone_mark_adjacency_bad(DroneNavMeshNode, DroneNavMeshNode);
u32 drone_closest_point(const Vec3&, AI::Team, const LinkEntryArg<const DronePathNode&>&);
u32 random_path(const Vec3&, const Vec3&, AI::Team, r32, const LinkEntryArg<const Result&>&);
u32 closest_walk_point(const Vec3&, const LinkEntryArg<const Vec3&>&);
u32 drone_random_path(DroneAllow, AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const DroneResult&>&);
void load(AssetID, const char*);
void init();
void loop();
void quit();
void update(const Update&);
void debug_draw_nav_mesh(const RenderParams&);
void draw_hollow(const RenderParams&);
r32 audio_pathfind(const Vec3&, const Vec3&);
u32 audio_pathfind(const Vec3&, const Vec3&, AudioEntry*, s8, r32);
void audio_reverb_calc(const Vec3&, ReverbCell*);

b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

namespace Worker
{
	struct NavMeshProcess : public dtTileCacheMeshProcess
	{
		void process(struct dtNavMeshCreateParams*, u8*, u16*);
	};

	struct DroneNavMeshNodeData
	{
		enum Flags : s8
		{
			FlagInQueue = (1 << 0),
			FlagVisited = (1 << 1),
			FlagCrawledFromParent = (1 << 2),
		};

		r32 travel_score;
		r32 estimate_score;
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

	enum DroneNavFlags : s8
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
	void audio_reverb_calc(const DroneNavContext&, const Vec3&, ReverbCell*);
}

extern ComponentMask entity_mask;
AI::Team entity_team(const Entity*);


}

struct AIAgent : public ComponentType<AIAgent>
{
	AI::Team team;
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
