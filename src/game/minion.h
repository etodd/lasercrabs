#pragma once

#include "data/entity.h"
#include "ai.h"

namespace VI
{


struct TargetEvent;
struct PlayerManager;

struct MinionEntity : public Entity
{
	MinionEntity(const Vec3&, const Quat&, AI::Team, PlayerManager* = nullptr);
};

struct MinionTarget : public ComponentType<MinionTarget>
{
	Array<Vec3> ingress_points; // points for AI minions to attack from

	void awake() {}
};

struct Minion : public ComponentType<Minion>
{
	struct Goal
	{
		enum class Type : s8
		{
			Position,
			Target,
			count,
		};

		Vec3 pos;
		Ref<Entity> entity;
		Type type;
	};

	enum class PathRequest : s8
	{
		None,
		Random,
		Position,
		Target,
		Repath,
		PointQuery,
		count,
	};

	static r32 particle_accumulator;

	static Minion* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(AI::TeamMask);
	static void update_all(const Update&);
	static Vec3 goal_path_position(const Goal&, const Vec3&);

	Goal goal;
	AI::Path path;
	Vec3 obstacle_pos;
	r32 attack_timer;
	r32 path_timer;
	r32 target_timer;
	r32 target_scan_timer;
	u32 obstacle_id = u32(-1);
	Ref<PlayerManager> owner;
	Ref<Entity> carrying;
	s8 path_index;
	b8 charging;
	PathRequest path_request;

	void awake();
	~Minion();

	// animation callbacks
	void footstep();
	void melee_started();
	void melee_thrust();
	void melee_hand_closed();
	void melee_hand_open();
	void melee_damage();
	void fired();

	Vec3 head_pos() const;
	Vec3 hand_pos() const;
	Vec3 aim_pos(r32) const;
	b8 headshot_test(const Vec3&, const Vec3&);
	void hit_by(const TargetEvent& e);
	void fire(const Vec3&);
	void killed(Entity*);
	void update_server(const Update&);
	b8 has_grenade() const;

	b8 can_see(Entity*, b8 = false, b8 = true) const;

	void new_goal();
	void set_path(const AI::Result&);
	void turn_to(const Vec3&);
};


}