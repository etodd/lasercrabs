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
#include "data/behavior.h"

namespace VI
{

struct Minion : public Entity
{
	Minion(const Vec3&, const Quat&, AI::Team);
};

struct MinionCommon : public ComponentType<MinionCommon>
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

struct MinionAI : public ComponentType<MinionAI>
{
	static const s32 MAX_POLYS = 256;

	Vec3 path_points[MAX_POLYS];
	dtPolyRef path_polys[MAX_POLYS];
	u8 path_point_count;
	u8 path_index;
	Ref<Entity> vision_cone;
	r32 last_path_recalc;
	Behavior* behavior;
	Ref<Entity> target;

	MinionAI();
	void awake();
	~MinionAI();

	void update(const Update&);
	void go(const Vec3&);
	void recalc_path(const Update&);
};

// behaviors

template<typename Derived> struct MinionBehavior : public BehaviorBase<Derived>
{
	MinionAI* minion;
	virtual void set_context(void* ctx)
	{
		minion = (MinionAI*)ctx;
	}
};

namespace MinionBehaviors
{
	void update_active(const Update& u);
}

struct MinionCheckTarget : public MinionBehavior<MinionCheckTarget>
{
	void run();
};

struct MinionGoToTarget : public MinionBehavior<MinionGoToTarget>
{
	void run();
	b8 target_in_range() const;
	void done(b8 = true);
	void update(const Update&);
};

struct MinionAttack : public MinionBehavior<MinionAttack>
{
	void run();
};


}
