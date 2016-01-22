#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"
#include "recast/Detour/Include/DetourNavMesh.h"
#include "ai.h"
#include <bullet/src/BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include "render/ui.h"
#include "common.h"
#include "ease.h"

namespace VI
{

struct Sentinel : public Entity
{
	Sentinel(const Vec3&, const Quat&, AI::Team);
};

struct SentinelCommon : public ComponentType<SentinelCommon>
{
	enum class State
	{
		Normal,
		Run,
		Mantle,
		WallRun,
	};

	enum class WallRunState
	{
		Left,
		Right,
		Forward,
		None,
	};

	FSM<State> fsm;
	Vec3 relative_wall_run_normal;
	WallRunState wall_run_state;
	Vec3 relative_support_pos;
	Ref<RigidBody> last_support;
	r32 last_support_time;

	b8 try_jump(r32);
	b8 try_parkour(b8 = false);
	void set_run(b8);
	void awake();
	Vec3 head_pos();
	void head_to_object_space(Vec3*, Quat*);
	b8 headshot_test(const Vec3&, const Vec3&);
	void killed(Entity*);
	void footstep();
	b8 try_wall_run(WallRunState, const Vec3&);
	void wall_jump(r32, const Vec3&, const btRigidBody*);

	void update(const Update&);
};

struct SentinelControl : public ComponentType<SentinelControl>
{
	enum class State
	{
		Scanning,
		Idle,
		Walking,
		Attacking,
	};
	static const s32 MAX_POLYS = 256;

	StaticArray<Ref<Transform>, 8> idle_path;
	u8 idle_path_index;
	Vec3 path_points[MAX_POLYS];
	Vec3 target_last_seen;
	dtPolyRef path_polys[MAX_POLYS];
	FSM<State> fsm;
	u8 path_point_count;
	u8 path_index;
	r32 vision_timer;
	r32 last_path_recalc;
	Ref<Entity> vision_cone;
	Ref<Entity> target;

	void awake();
	~SentinelControl();
	void update(const Update&);
	void go(const Vec3&);
	void recalc_path(const Update&);
};

}
