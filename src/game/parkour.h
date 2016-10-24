#pragma once

#include "data/entity.h"
#include "physics.h"
#include "ai.h"

namespace VI
{

struct Traceur : public Entity
{
	Traceur(const Vec3&, const Quat&, AI::Team);
};

#define LANDING_VELOCITY_LIGHT 5.0f * -1.25f
#define LANDING_VELOCITY_HARD 5.0f * -3.0f

#define MAX_TILE_HISTORY 8
struct Parkour : public ComponentType<Parkour>
{
	enum class State
	{
		Normal,
		Mantle,
		HardLanding,
		WallRun,
		Slide,
		Roll,
	};

	enum class WallRunState
	{
		Left,
		Right,
		Forward,
		None,
	};

	Vec3 relative_wall_run_normal;
	Vec3 relative_support_pos;
	r32 last_support_time;
	r32 lean;
	r32 last_angular_velocity;
	r32 last_angle_horizontal;
	WallRunState wall_run_state;
	FSM<State> fsm;
	Ref<RigidBody> last_support;
	//b8 can_double_jump;
	b8 slide_continue;

	b8 wallrun(const Update&, RigidBody*, const Vec3&, const Vec3&);

	void lessen_gravity();
	void land(r32);
	b8 try_slide();
	b8 try_jump(r32);
	void do_normal_jump();
	b8 try_parkour(b8 = false);
	void awake();
	Vec3 head_pos() const;
	void head_to_object_space(Vec3*, Quat*) const;
	void footstep();
	b8 try_wall_run(WallRunState, const Vec3&);
	void wall_run_up_add_velocity(const Vec3&, const Vec3&);
	void wall_jump(r32, const Vec3&, const btRigidBody*);
	Vec3 get_support_velocity(const Vec3&, const btCollisionObject*) const;

	void update(const Update&);
};

}
