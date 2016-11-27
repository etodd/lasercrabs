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
		AwkPathfind,
		AwkMarkAdjacencyBad,
		RandomPath,
		ClosestWalkPoint,
		UpdateState,
		Quit,
	};

	enum class Callback
	{
		Path,
		AwkPath,
		Point,
	};

	enum class AwkPathfind
	{
		LongRange,
		Target,
		Away,
		Random,
	};

	enum class AwkAllow
	{
		Crawl = 1,
		Shoot = 1 << 1,
		All = Crawl | Shoot,
	};

	const s32 MAX_PATH_LENGTH = 32;
	typedef StaticArray<Vec3, MAX_PATH_LENGTH> Path;

	struct AwkPathNode
	{
		Vec3 pos;
		Vec3 normal;
		AwkNavMeshNode ref;
		b8 crawl;
	};
	typedef StaticArray<AwkPathNode, MAX_PATH_LENGTH> AwkPath;

	struct Result
	{
		Path path;
		u32 id;
	};

	struct AwkResult
	{
		AwkPath path;
		u32 id;
	};
	
	struct SensorState
	{
		Vec3 pos;
		AI::Team team;
	};

	typedef SensorState ContainmentFieldState;

	static const s32 SYNC_IN_SIZE = 6 * 1024 * 1024;
	static const s32 SYNC_OUT_SIZE = 1 * 1024 * 1024;
	extern Array<b8> obstacles;
	extern SyncRingBuffer<SYNC_IN_SIZE> sync_in;
	extern SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
	extern u32 callback_in_id;
	extern u32 callback_out_id;

	b8 match(AI::Team, AI::TeamMask);
	u32 obstacle_add(const Vec3&, r32, r32);
	void obstacle_remove(u32);
	u32 pathfind(const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
	u32 awk_pathfind(AwkPathfind, AwkAllow, AI::Team, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const LinkEntryArg<const AwkResult&>&);
	void awk_mark_adjacency_bad(AwkNavMeshNode, AwkNavMeshNode);
	u32 random_path(const Vec3&, const Vec3&, r32, const LinkEntryArg<const Result&>&);
	u32 closest_walk_point(const Vec3&, const LinkEntryArg<const Vec3&>&);
	u32 awk_random_path(AwkAllow, AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const AwkResult&>&);
	void load(AssetID, const u8*, s32);
	void loop();
	void quit();
	void update(const Update&);
	void debug_draw_nav_mesh(const RenderParams&);
	void debug_draw_awk_nav_mesh(const RenderParams&);

	b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

	namespace Worker
	{
		struct NavMeshProcess : public dtTileCacheMeshProcess
		{
			void process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags);
		};

		struct AwkNavMeshNodeData
		{
			r32 travel_score;
			r32 estimate_score;
			r32 sensor_score;
			AwkNavMeshNode parent;
			b8 visited;
			b8 in_queue;
			b8 crawled_from_parent;
		};

		struct AwkNavMeshKey
		{
			Chunks<Array<AwkNavMeshNodeData>> data;
			r32 priority(const AwkNavMeshNode&);
			void resize(const AwkNavMesh&);
			void reset();
			AwkNavMeshNodeData& get(const AwkNavMeshNode&);
		};

		const extern r32 default_search_extents[];

		extern dtNavMesh* nav_mesh;
		extern AwkNavMesh awk_nav_mesh;
		extern AwkNavMeshKey awk_nav_mesh_key;
		extern dtNavMeshQuery* nav_mesh_query;
		extern dtTileCache* nav_tile_cache;
		extern dtQueryFilter default_query_filter;
		extern dtTileCache* nav_tile_cache;
		extern dtTileCacheAlloc nav_tile_allocator;
		extern FastLZCompressor nav_tile_compressor;
		extern NavMeshProcess nav_tile_mesh_process;

		void loop();
	}
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