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
#define LANDING_VELOCITY_HARD 5.0f * -2.65f
#define GRAPPLE_COOLDOWN_THRESHOLD 3.0f
#define GRAPPLE_COOLDOWN 4.5f

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
		Grapple,
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

	enum class MantleAttempt : s8
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

	enum Flags
	{
		FlagCanDoubleJump = 1 << 0,
		FlagTryGrapple = 1 << 1,
	};

	Vec3 grapple_start_pos;
	Vec3 grapple_normal;
	Vec3 grapple_pos;
	Vec3 relative_wall_run_normal;
	Vec3 relative_support_pos;
	Vec3 relative_animation_start_pos;
	r32 grapple_cooldown;
	r32 last_support_time;
	r32 last_jump_time;
	r32 last_footstep_time;
	r32 last_climb_time;
	r32 lean;
	r32 last_angular_velocity;
	r32 last_angle_horizontal;
	r32 climb_velocity;
	r32 breathing;
	StaticArray<TilePos, 8> tile_history;
	StaticArray<Vec3, 4> jump_history;
	FSM<State> fsm;
	Ref<RigidBody> last_support;
	Ref<Transform> rope;
	Ref<Transform> animation_start_support;
	ID rope_constraint = IDNull;
	Link jumped;
	WallRunState wall_run_state;
	WallRunState last_support_wall_run_state;
	s8 flags;

	inline b8 flag(s32 f) const
	{
		return flags & f;
	}

	inline void flag(s32 f, b8 value)
	{
		if (value)
			flags |= f;
		else
			flags &= ~f;
	}

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
	void grapple_start(const Vec3&, const Quat&);
	void grapple_cancel();
	b8 grapple_valid(const Vec3&, const Quat&, Vec3* = nullptr, Vec3* = nullptr) const;
	b8 grapple_try(const Vec3&, const Quat&);
	Vec3 head_pos() const;
	Vec3 hand_pos() const;
	void head_to_object_space(Vec3*, Quat*) const;
	void spawn_tiles(const Vec3&, const Vec3&, const Vec3&, const Vec3&);
	b8 try_wall_run(WallRunState, const Vec3&);
	void wall_jump(r32, const Vec3&, const btRigidBody*);
	void pickup_animation_complete();

	void update(const Update&);
};

}
