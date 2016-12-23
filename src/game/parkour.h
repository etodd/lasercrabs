#pragma once

#include "data/entity.h"
#include "ai.h"

class btRigidBody;

namespace VI
{

struct Traceur : public Entity
{
	Traceur(const Vec3&, const Quat&, AI::Team);
};

#define LANDING_VELOCITY_LIGHT 5.0f * -1.25f
#define LANDING_VELOCITY_HARD 5.0f * -3.0f

namespace Net
{
	struct StreamRead;
};

struct MinionCommon;
struct Transform;
struct RigidBody;

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
		Climb,
		count,
	};

	enum class WallRunState
	{
		Left,
		Right,
		Forward,
		None,
	};

	static b8 net_msg(Net::StreamRead*, Net::MessageSource);

	Vec3 relative_wall_run_normal;
	Vec3 relative_support_pos;
	Vec3 relative_animation_start_pos;
	r32 last_support_time;
	r32 lean;
	r32 last_angular_velocity;
	r32 last_angle_horizontal;
	r32 climb_velocity;
	FSM<State> fsm;
	WallRunState wall_run_state;
	WallRunState last_support_wall_run_state;
	State last_frame_state;
	Ref<RigidBody> last_support;
	Ref<Transform> rope;
	ID rope_constraint = IDNull;
	StaticArray<Ref<MinionCommon>, 4> damage_minions; // HACK; minions we're currently damaging
	b8 slide_continue;

	b8 wallrun(const Update&, RigidBody*, const Vec3&, const Vec3&);

	void awake();
	void killed(Entity*);
	void land(r32);
	void lessen_gravity();
	b8 try_slide();
	b8 try_jump(r32);
	void do_normal_jump();
	b8 try_parkour(b8 = false);
	Vec3 head_pos() const;
	void head_to_object_space(Vec3*, Quat*) const;
	void footstep();
	b8 try_wall_run(WallRunState, const Vec3&);
	void wall_run_up_add_velocity(const Vec3&, const Vec3&);
	void wall_jump(r32, const Vec3&, const btRigidBody*);
	void pickup_animation_complete();

	void update(const Update&);
};

}
