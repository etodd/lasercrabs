#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"

namespace VI
{

struct AIAgent;
struct RenderParams;

struct NavMeshProcess : public dtTileCacheMeshProcess
{
	void process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags);
};

struct AI
{
	enum class Team
	{
		A,
		B,
		None,
		count = None,
	};

	static Team other(Team);

	static const Vec4 colors[];

	static const r32 default_search_extents[];

	static AssetID render_mesh;
	static dtNavMesh* nav_mesh;
	static dtNavMeshQuery* nav_mesh_query;
	static dtTileCache* nav_tile_cache;
	static dtQueryFilter default_query_filter;
	static b8 render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void debug_draw(const RenderParams&);
	static void update(const Update&);

	static u32 obstacle_add(const Vec3&, r32, r32);
	static void obstacle_remove(u32);

	static Entity* vision_query(const AIAgent*, const Vec3&, const Vec3&, r32, r32, r32 = -1.0f, ComponentMask = -1);
	static Entity* sound_query(AI::Team, const Vec3&, ComponentMask = -1);
	static b8 vision_check(const Vec3&, const Vec3&, const AIAgent*, const AIAgent*);

	static dtPolyRef get_poly(const Vec3&, const r32*);
};

struct AIAgent : public ComponentType<AIAgent>
{
	AI::Team team;
	void awake() {}
};

template<typename T>
struct FSM
{
	T current;
	r32 time;

	FSM()
		: current()
	{
	}
	FSM(const T state)
		: current(state)
	{
	}
	b8 transition(const T t)
	{
		if (t != current)
		{
			current = t;
			time = 0.0f;
			return true;
		}
		return false;
	}
};

}
