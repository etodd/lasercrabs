#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"
#include "data/import_common.h"
#include "sync.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"

namespace VI
{

#define SENSOR_RANGE 20.0f

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
		AwkPathfindHit,
		RandomPath,
		AwkRandomPath,
		UpdateSensors,
		Quit,
	};

	enum class Callback
	{
		Path,
		AwkPath,
	};

	const s32 MAX_PATH_LENGTH = 32;
	typedef StaticArray<Vec3, MAX_PATH_LENGTH> Path;

	struct Result
	{
		Path path;
		u32 id;
	};

	enum class Team
	{
		A,
		B,
		None,
		count = None,
	};

	Team other(Team);

	struct SensorState
	{
		Vec3 pos;
		AI::Team team;
	};

	static const s32 SYNC_IN_SIZE = 5 * 1024 * 1024;
	static const s32 SYNC_OUT_SIZE = 1 * 1024 * 1024;
	extern Array<b8> obstacles;
	extern SyncRingBuffer<SYNC_IN_SIZE> sync_in;
	extern SyncRingBuffer<SYNC_OUT_SIZE> sync_out;
	extern u32 callback_in_id;
	extern u32 callback_out_id;

	u32 obstacle_add(const Vec3&, r32, r32);
	void obstacle_remove(u32);
	u32 pathfind(const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
	u32 awk_pathfind(AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
	u32 awk_pathfind_hit(AI::Team, const Vec3&, const Vec3&, const LinkEntryArg<const Result&>&);
	u32 random_path(const Vec3&, const LinkEntryArg<const Result&>&);
	u32 awk_random_path(AI::Team, const Vec3&, const LinkEntryArg<const Result&>&);
	void load(const u8*, s32);
	void loop();
	void quit();
	void update(const Update&);
	void debug_draw_nav_mesh(const RenderParams&);
	void debug_draw_awk_nav_mesh(const RenderParams&);

	Entity* vision_query(const AIAgent*, const Vec3&, const Vec3&, r32, r32, r32 = -1.0f, ComponentMask = -1);
	b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

	namespace Internal
	{
		struct NavMeshProcess : public dtTileCacheMeshProcess
		{
			void process(struct dtNavMeshCreateParams* params, u8* polyAreas, u16* polyFlags);
		};

		struct AwkNavMeshNodeData
		{
			AwkNavMeshNode parent;
			b8 visited;
			r32 travel_score;
			r32 estimate_score;
		};

		struct AwkNavMeshKey
		{
			Chunks<Array<AwkNavMeshNodeData>> data;
			r32 priority(const AwkNavMeshNode&);
			void reset(const AwkNavMesh&);
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

		dtPolyRef get_poly(const Vec3&, const r32*);

		void awk_pathfind(AI::Team, const Vec3&, const Vec3&, Path*);
		void awk_pathfind_hit(AI::Team, const Vec3&, const Vec3&, Path*);

		AwkNavMeshNode awk_closest_point(const Vec3&);
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