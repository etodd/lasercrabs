#pragma once
#include "types.h"
#include "render/render.h"
#include "data/entity.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

namespace VI
{

struct AI
{
	enum Team
	{
		A,
		B,
		C,
		D
	};

	static const float default_search_extents[];

	static AssetID render_mesh;
	static dtNavMesh* nav_mesh;
	static dtNavMeshQuery* nav_mesh_query;
	static dtQueryFilter default_query_filter;
	static bool render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void debug_draw(const RenderParams&);

	static Entity* get_enemy(const AI::Team&, const Vec3&, const Vec3&, const float, const float);

	static dtPolyRef get_poly(const Vec3&, const float*);
};

struct AIAgent : public ComponentType<AIAgent>
{
	AI::Team team;
	void awake();
};

template<typename T>
struct FSM
{
	T current;
	float time;
	bool transition(T t)
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
