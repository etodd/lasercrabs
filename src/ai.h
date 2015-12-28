#pragma once
#include "types.h"
#include "data/entity.h"
#include "lmath.h"

#include "recast/Detour/Include/DetourNavMesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"

namespace VI
{

struct AIAgent;
struct RenderParams;

struct AI
{
	enum class Team
	{
		A,
		B,
		C,
		D,
		None,
		count,
	};

	static const Vec4 colors[];

	static const float default_search_extents[];

	static AssetID render_mesh;
	static dtNavMesh* nav_mesh;
	static dtNavMeshQuery* nav_mesh_query;
	static dtQueryFilter default_query_filter;
	static bool render_mesh_dirty;
	static void init();
	static void load_nav_mesh(AssetID);
	static void debug_draw(const RenderParams&);

	static Entity* vision_query(const AIAgent*, const Vec3&, const Vec3&, float, float, float = -1.0f, ComponentMask = -1);
	static Entity* sound_query(AI::Team, const Vec3&, ComponentMask = -1);
	static bool vision_check(const Vec3&, const Vec3&, const AIAgent*, const AIAgent*);

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

	FSM()
		: current()
	{
	}
	FSM(const T state)
		: current(state)
	{
	}
	bool transition(const T t)
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
