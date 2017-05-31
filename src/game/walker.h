#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"

namespace VI
{

struct RigidBody;

#define WALKER_SUPPORT_HEIGHT 0.35f
#define WALKER_HEIGHT 0.95f
#define WALKER_RADIUS 0.35f
#define WALKER_DEFAULT_CAPSULE_HEIGHT (WALKER_HEIGHT + WALKER_RADIUS * 2.0f)

struct Walker : public ComponentType<Walker>
{
	static Vec3 get_support_velocity(const Vec3&, const btCollisionObject*);

	Vec2 dir;
	r32 speed,
		max_speed,
		rotation,
		target_rotation,
		rotation_speed,
		net_speed,
		net_speed_timer;
	u32 obstacle_id;
	Ref<RigidBody> support;
	LinkArg<r32> land;
	b8 auto_rotate;
	b8 enabled;

	Walker(r32 = 0.0f);
	~Walker();
	void awake();
	b8 slide(Vec2*, const Vec3&);
	btCollisionWorld::ClosestRayResultCallback check_support(r32 = 0.0f) const;
	RigidBody* get_support(r32 = 0.0f) const;

	Vec3 base_pos() const;
	void absolute_pos(const Vec3&);
	Vec3 forward() const;
	r32 capsule_height() const;
	void crouch(b8);

	void update(const Update&);
};

}
