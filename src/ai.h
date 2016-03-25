#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"
#include "data/import_common.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "recast/DetourTileCache/Include/DetourTileCache.h"

namespace VI
{

struct AIAgent;
struct RenderParams;

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

struct NavMeshProcess : public dtTileCacheMeshProcess
{
	void process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags);
};

struct AI
{
	struct Goal
	{
		b8 has_entity;
		Ref<Entity> entity;
		Vec3 pos;

		Goal();

		b8 valid() const;
		Vec3 get_pos() const;
		void set(const Vec3&);
		void set(Entity*);
	};

	enum class Team
	{
		A,
		B,
		None,
		count = None,
	};

	static Team other(Team);

	static const r32 default_search_extents[];

	static AssetID render_mesh;
	static AssetID awk_render_mesh;
	static dtNavMesh* nav_mesh;
	static AwkNavMesh* awk_nav_mesh;
	static AwkNavMeshKey awk_nav_mesh_key;
	static dtNavMeshQuery* nav_mesh_query;
	static dtTileCache* nav_tile_cache;
	static dtQueryFilter default_query_filter;
	static b8 render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void refresh_nav_render_meshes(const RenderParams&);
	static void debug_draw_nav_mesh(const RenderParams&);
	static void debug_draw_awk_nav_mesh(const RenderParams&);
	static void update(const Update&);

	static u32 obstacle_add(const Vec3&, r32, r32);
	static void obstacle_remove(u32);

	static Entity* vision_query(const AIAgent*, const Vec3&, const Vec3&, r32, r32, r32 = -1.0f, ComponentMask = -1);
	static Entity* sound_query(AI::Team, const Vec3&, ComponentMask = -1);
	static b8 vision_check(const Vec3&, const Vec3&, const Entity* = nullptr, const Entity* = nullptr);

	static dtPolyRef get_poly(const Vec3&, const r32*);

	static AwkNavMeshNode awk_closest_point(const Vec3&);
	static void awk_pathfind(const Vec3&, const Vec3&, Array<Vec3>*);
};

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
