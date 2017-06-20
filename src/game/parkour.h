#pragma once

#include "data/entity.h"
#include "ai.h"

class btRigidBody;

namespace VI
{

struct Traceur : public Entity
{
	Traceur(const Vec3&, r32, AI::Team);
};

#define LANDING_VELOCITY_LIGHT 5.0f * -1.25f
#define LANDING_VELOCITY_HARD 5.0f * -3.0f

namespace Net
{
	struct StreamRead;
};

struct Minion;
struct Transform;
struct RigidBody;

struct Parkour : public ComponentType<Parkour>
{
	enum class State : s8
	{
		Normal,
		Mantle,
		HardLanding,
		WallRun,
		Climb,
		count,
	};

	enum class WallRunState : s8
	{
		Left,
		Right,
		Forward,
		None,
		count,
	};

	enum class MantleAttempt
	{
		Normal,
		Extra,
		Force,
		count,
	};

	struct TilePos
	{
		s32 x;
		s32 y;
		b8 operator==(const TilePos&) const;
		b8 operator!=(const TilePos&) const;
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
	StaticArray<TilePos, 8> tile_history;
	FSM<State> fsm;
	WallRunState wall_run_state;
	WallRunState last_support_wall_run_state;
	State last_frame_state;
	Ref<RigidBody> last_support;
	Ref<Transform> rope;
	Ref<Transform> animation_start_support;
	ID rope_constraint = IDNull;
	StaticArray<Ref<Minion>, 4> damage_minions; // HACK; minions we're currently damaging
	b8 can_double_jump;

	b8 wallrun(const Update&, RigidBody*, const Vec3&, const Vec3&);

	void awake();
	~Parkour();

	void footstep();
	void climb_sound();

	void killed(Entity*);
	void land(r32);
	void lessen_gravity();
	b8 try_jump(r32);
	void do_normal_jump();
	b8 try_parkour(MantleAttempt = MantleAttempt::Normal);
	Vec3 head_pos() const;
	void head_to_object_space(Vec3*, Quat*) const;
	void spawn_tiles(const Vec3&, const Vec3&, const Vec3&, const Vec3&);
	b8 try_wall_run(WallRunState, const Vec3&);
	void wall_run_up_add_velocity(const Vec3&, const Vec3&);
	void wall_jump(r32, const Vec3&, const btRigidBody*);
	void pickup_animation_complete();

	void update(const Update&);
};

}
