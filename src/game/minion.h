#pragma once

#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"

namespace VI
{


struct PlayerManager;

#define MINION_HEAD_RADIUS 0.35f

struct Minion : public Entity
{
	Minion(const Vec3&, const Quat&, AI::Team, PlayerManager* = nullptr);
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
	b8 path_valid;
	AI::Path path;
	u8 path_index;
	r32 last_path_recalc;
	Behavior* behavior;

	MinionAI();
	void awake();
	~MinionAI();

	b8 can_see(Entity*) const;

	void set_path(const AI::Path&);
	void update(const Update&);
	void go(const Vec3&);
	void recalc_path(const Update&);
	void turn_to(const Vec3&);
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

struct MinionPath : public MinionBehavior<MinionPath>
{
	Vec3 goal;
	b8 random_path;
	r32 path_timer;
	void run();
	void update(const Update&);
	void abort();
};

struct MinionAttack : public MinionBehavior<MinionAttack>
{
	Ref<Entity> target;
	void run();
	void update(const Update&);
};


}
