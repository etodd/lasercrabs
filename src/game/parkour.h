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

	struct TilePos
	{
		s32 x;
		s32 y;
		b8 operator==(const TilePos&) const;
		b8 operator!=(const TilePos&) const;
	};

	FSM<State> fsm;
	Vec3 relative_wall_run_normal;
	WallRunState wall_run_state;
	Vec3 relative_support_pos;
	Ref<RigidBody> last_support;
	r32 last_support_time;
	StaticArray<TilePos, MAX_TILE_HISTORY> tile_history;
	b8 can_double_jump;
	b8 slide_continue;
	r32 lean;
	r32 last_angular_velocity;

	b8 wallrun(const Update&, RigidBody*, const Vec3&, const Vec3&);

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
	void spawn_tiles(const Vec3&, const Vec3&, const Vec3&, const Vec3&);

	void update(const Update&);
};

}
