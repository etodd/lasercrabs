#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "bullet/src/BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "data/import_common.h"

namespace VI
{

struct Transform;
struct Rope;
struct View;
struct DamageEvent;

#define AWK_HEALTH 3
#define AWK_FLY_SPEED 45.0f
#define AWK_CRAWL_SPEED 1.4f
#define AWK_MIN_COOLDOWN 0.5f
#define AWK_MAX_DISTANCE_COOLDOWN 1.5f
#define AWK_COOLDOWN_DISTANCE_RATIO (AWK_MAX_DISTANCE_COOLDOWN / AWK_MAX_DISTANCE)
#define AWK_SHOCKWAVE_RADIUS 8
#define AWK_LEGS 3

// If we raycast through a Minion's head, keep going.
struct AwkRaycastCallback : btCollisionWorld::ClosestRayResultCallback
{
	r32 closest_target_hit;
	b8 hit_target() const;
	ID entity_id;

	AwkRaycastCallback(const Vec3& a, const Vec3& b, const Entity*);

	virtual	btScalar addSingleResult(btCollisionWorld::LocalRayResult&, b8);
};

struct TargetEvent;
struct Target;

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
	StaticArray<Ref<Entity>, 4> hit_targets;
	Link detached;
	r32 attach_time;
	Footing footing[AWK_LEGS];
	r32 last_speed;
	r32 last_footstep;
	Vec3 lerped_pos;
	Quat lerped_rotation;
	Ref<View> shield;

	Awk();
	void awake();
	~Awk();

	void hit_by(const TargetEvent&); // Called when we get hit
	void hit_target(Entity*); // Called when we hit a target
	void damaged(const DamageEvent&);
	void killed(Entity*);
	void update_shield_visibility();

	b8 predict_intersection(const Target*, Vec3*) const;

	void stealth(b8);

	void reflect(const Vec3&, const Vec3&, const Update&);
	void crawl_wall_edge(const Vec3&, const Vec3&, const Update&, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3& dir, const Update& u);
	void update_offset();

	void set_footing(s32, const Transform*, const Vec3&);

	Vec3 center() const;

	b8 detach(const Vec3&);

	b8 can_go(const Vec3&, Vec3* = nullptr) const;
	b8 can_hit(const Target*, Vec3* = nullptr) const;

	void update(const Update&);
};

}
