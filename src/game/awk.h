#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "bullet/src/BulletCollision/CollisionDispatch/btCollisionWorld.h"

namespace VI
{

struct Transform;
struct Rope;

#define AWK_HEALTH 100
#define AWK_MAX_DISTANCE 25.0f
#define AWK_FLY_SPEED 100.0f
#define AWK_CRAWL_SPEED 2.0f
#define AWK_RADIUS 0.2f
#define AWK_MIN_COOLDOWN 0.3f
#define AWK_MAX_DISTANCE_COOLDOWN 1.0f
#define AWK_COOLDOWN_DISTANCE_RATIO (AWK_MAX_DISTANCE_COOLDOWN / AWK_MAX_DISTANCE)
#define AWK_SHOCKWAVE_RADIUS 8
#define AWK_LEGS 3

// If we raycast through a Minion's head, keep going.
struct AwkRaycastCallback : btCollisionWorld::ClosestRayResultCallback
{
	b8 hit_target;
	ID entity_id;

	AwkRaycastCallback(const Vec3& a, const Vec3& b, const Entity*);

	virtual	btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, b8 normalInWorldSpace);
};

enum class SecondaryFire
{
	None,
	Melee,
	Command,
	Wall,
	Count,
};

struct SecondaryFireColors
{
	static Vec4 all[];
};

struct Awk : public ComponentType<Awk>
{
	struct Footing
	{
		Ref<const Transform> parent;
		Vec3 pos;
		Vec3 last_abs_pos;
		r32 blend;
	};

	Vec3 velocity;
	Link attached;
	LinkArg<const Vec3&> bounce;
	LinkArg<Entity*> hit;
	r32 attach_time;
	Ref<Rope> rope;
	Footing footing[AWK_LEGS];
	r32 last_speed;
	r32 last_footstep;

	Awk();
	void awake();

	void hit_by(Entity*); // Called when we get hit
	void hit_target(Entity*); // Called when we hit a target
	void killed(Entity*);

	void set_footing(const s32, const Transform*, const Vec3&);

	Vec3 center();

	b8 detach(const Update&, const Vec3&);

	b8 can_go(const Vec3&, Vec3* = nullptr);

	void update(const Update&);
};

}
