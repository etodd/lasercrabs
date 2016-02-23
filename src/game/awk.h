#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "bullet/src/BulletCollision/CollisionDispatch/btCollisionWorld.h"

namespace VI
{

struct Transform;
struct Rope;
struct View;

#define AWK_HEALTH 100
#define AWK_MAX_DISTANCE 25.0f
#define AWK_FLY_SPEED 100.0f
#define AWK_CRAWL_SPEED 3.0f
#define AWK_RADIUS 0.2f
#define AWK_MIN_COOLDOWN 0.5f
#define AWK_MAX_DISTANCE_COOLDOWN 2.0f
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
	Vec3 lerped_pos;
	Quat lerped_rotation;
	Ref<View> shield;

	Awk();
	void awake();
	~Awk();

	void hit_by(Entity*); // Called when we get hit
	void hit_target(Entity*); // Called when we hit a target
	void killed(Entity*);

	void reflect(const Vec3&, const Vec3&, const Update&);
	void crawl_wall_edge(const Vec3&, const Vec3&, const Update&, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3& dir, const Update& u);
	void update_offset();

	void set_footing(s32, const Transform*, const Vec3&);

	Vec3 center();

	b8 detach(const Update&, const Vec3&);

	b8 can_go(const Vec3&, Vec3* = nullptr);

	void update(const Update&);
};

}
