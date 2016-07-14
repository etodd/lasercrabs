#pragma once

#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"

namespace VI
{


struct TargetEvent;
struct PlayerManager;

#define MINION_HEAD_RADIUS 0.4f
#define MINION_ATTACK_TIME 4.0f

struct Minion : public Entity
{
	Minion(const Vec3&, const Quat&, AI::Team, PlayerManager* = nullptr);
};

struct MinionCommon : public ComponentType<MinionCommon>
{
	static MinionCommon* closest(AI::Team, const Vec3&, r32* = nullptr);

	Ref<PlayerManager> owner;
	r32 attack_timer;

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
			Position,
			Target,
		};

		Type type;
		Ref<Entity> entity;
		Vec3 pos;
	};

	enum class PathRequest
	{
		None,
		Random,
		Position,
		Target,
		Repath,
	};

	PathRequest path_request;
	Goal goal;
	AI::Path path;
	u8 path_index;
	r32 path_timer;
	r32 target_timer;

	void awake();

	b8 can_see(Entity*, b8 = false) const;

	void new_goal();
	void set_path(const AI::Result&);
	void update(const Update&);
	void turn_to(const Vec3&);
};


}
