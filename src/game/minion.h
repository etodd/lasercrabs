#pragma once

#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"

namespace VI
{


struct TargetEvent;
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
	void hit_by(const TargetEvent& e);
	void killed(Entity*);
	void footstep();
	void update(const Update&);
};

struct MinionAI : public ComponentType<MinionAI>
{
	struct Goal
	{
		enum class Type
		{
			Random,
			Sensor,
		};

		Type type;
		Ref<Entity> entity;
		Vec3 pos;
	};

	enum class PathRequest
	{
		None,
		Random,
		Sensor,
		Repath,
	};

	PathRequest path_request;
	Goal goal;
	AI::Path path;
	u8 path_index;
	r32 path_timer;
	r32 attack_timer;

	void awake();

	b8 can_see(Entity*) const;

	void new_goal();
	void set_path(const AI::Path&);
	void update(const Update&);
	void turn_to(const Vec3&);
};


}
