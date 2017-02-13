#pragma once

#include "data/entity.h"
#include "ai.h"

namespace VI
{


struct TargetEvent;
struct PlayerManager;

#define MINION_HEAD_RADIUS 0.4f
#define MINION_ATTACK_TIME 2.0f

struct Minion : public Entity
{
	Minion(const Vec3&, const Quat&, AI::Team, PlayerManager* = nullptr);
};

struct MinionCommon : public ComponentType<MinionCommon>
{
	static r32 particle_accumulator;

	static MinionCommon* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(AI::TeamMask);
	static void update_client_all(const Update&);

	r32 attack_timer;
	Ref<PlayerManager> owner;

	void awake();
	Vec3 head_pos() const;
	Vec3 hand_pos() const;
	b8 headshot_test(const Vec3&, const Vec3&);
	void hit_by(const TargetEvent& e);
	void fire(const Vec3&);
	void melee_damage();
	void killed(Entity*);
	void footstep();
	void update_server(const Update&);
	void team(AI::Team);
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

		Vec3 pos;
		Type type;
		Ref<Entity> entity;
	};

	enum class PathRequest
	{
		None,
		Random,
		Position,
		Target,
		Repath,
		PointQuery,
	};

	PathRequest path_request;
	Goal goal;
	AI::Path path;
	Vec3 patrol_point;
	r32 path_timer;
	r32 target_timer;
	r32 target_scan_timer;
	s8 path_index;

	void awake();

	b8 can_see(Entity*, b8 = false) const;

	void new_goal(const Vec3& = Vec3::zero, b8 = true);
	void set_path(const AI::Result&);
	void set_patrol_point(const Vec3&);
	void update(const Update&);
	void turn_to(const Vec3&);
};


}
