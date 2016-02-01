#pragma once

#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"

namespace VI
{

struct Minion : public Entity
{
	Minion(const Vec3&, const Quat&, AI::Team);
};

struct MinionCommon : public ComponentType<MinionCommon>
{
	void awake();
	Vec3 head_pos();
	b8 headshot_test(const Vec3&, const Vec3&);
	void killed(Entity*);
	void footstep();
	void update(const Update&);
};

struct MinionAI : public ComponentType<MinionAI>
{
	static const s32 MAX_POLYS = 256;

	Vec3 path_points[MAX_POLYS];
	dtPolyRef path_polys[MAX_POLYS];
	u8 path_point_count;
	u8 path_index;
	r32 last_path_recalc;
	Behavior* behavior;
	Ref<Entity> target;

	MinionAI();
	void awake();
	~MinionAI();

	b8 can_see(Entity*) const;

	void update(const Update&);
	void go(const Vec3&);
	void recalc_path(const Update&);
	void damaged(Entity*);
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
