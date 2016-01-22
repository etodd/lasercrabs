#pragma once
#include "data/entity.h"
#include "ai.h"

namespace VI
{

struct AIPlayerControl;
struct Transform;

#define MAX_AI_PLAYERS 3

struct AIPlayer
{
	static PinArray<AIPlayer, MAX_AI_PLAYERS>& list();

	char username[255];
	Ref<AIPlayerControl> control;
	Ref<Transform> spawn;
	AI::Team team;
	r32 spawn_timer;

	AIPlayer(AI::Team);
	void update(const Update&);
};

struct AIPlayerControl : public ComponentType<AIPlayerControl>
{
	struct Goal
	{
		Ref<Entity> entity;
		Vec3 pos;
		r32 vision_timer;

		Goal();

		Vec3 get_pos() const;
		s32 priority() const;
		r32 inaccuracy() const;
	};
	Goal goal;
	r32 move_timer;
	r32 goal_timer;
	Vec3 last_pos;
	r32 stick_timer;

	AIPlayerControl();
	void awake();

	Goal find_goal(const Entity* = nullptr) const;
	void reset_move_timer();
	void awk_attached();
	void awk_hit(Entity*);
	b8 goal_reachable(const Entity*) const;
	void update(const Update&);
};

}
