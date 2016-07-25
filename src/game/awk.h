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

#define AWK_HEALTH 4
#define AWK_FLY_SPEED 45.0f
#define AWK_DASH_SPEED 20.0f
#define AWK_CRAWL_SPEED 1.5f
#define AWK_MIN_COOLDOWN 0.75f
#define AWK_MAX_DISTANCE_COOLDOWN 2.0f
#define AWK_COOLDOWN_DISTANCE_RATIO (AWK_MAX_DISTANCE_COOLDOWN / AWK_MAX_DISTANCE)
#define AWK_LEGS 3
#define AWK_INVINCIBLE_TIME 5.0f

// If we raycast through a Minion's head, keep going.
struct AwkRaycastCallback : btCollisionWorld::ClosestRayResultCallback
{
	r32 closest_target_hit;
	b8 hit_target() const;
	ID entity_id;

	AwkRaycastCallback(const Vec3&, const Vec3&, const Entity*);

	btScalar addSingleResult(btCollisionWorld::LocalRayResult&, b8);
};

struct TargetEvent;
struct Target;

struct Awk : public ComponentType<Awk>
{
	enum class State
	{
		Crawl,
		Dash,
		Fly,
	};

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
	Link dashed;
	r32 attach_time;
	r32 stun_timer;
	r32 invincible_timer;
	r32 cooldown_total; // total cooldown time
	r32 cooldown; // remaining cooldown time
	Footing footing[AWK_LEGS];
	r32 last_speed;
	r32 last_footstep;
	Vec3 lerped_pos;
	Quat lerped_rotation;
	Vec3 last_pos;
	Ref<Entity> shield;
	b8 disable_cooldown_skip;
	r32 particle_accumulator;
	r32 dash_timer;

	Awk();
	void awake();
	~Awk();

	State state() const;
	Vec3 calculated_velocity() const;
	b8 dash_start(const Vec3&);
	b8 cooldown_can_go() const; // can we go?
	void hit_by(const TargetEvent&); // called when we get hit
	void hit_target(Entity*); // called when we hit a target
	void damaged(const DamageEvent&);
	void killed(Entity*);
	Entity* incoming_attacker() const;

	s32 ally_containment_field_mask() const;

	b8 predict_intersection(const Target*, Vec3*) const;

	void stealth(b8);

	void reflect(const Vec3&, const Vec3&, const Update&);
	void crawl_wall_edge(const Vec3&, const Vec3&, const Update&, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3&, const Update&);
	void update_offset();
	void update_lerped_pos(r32, const Update&);

	void set_footing(s32, const Transform*, const Vec3&);

	Vec3 center() const;
	Vec3 attach_point() const;

	void detach_teleport();
	b8 detach(const Vec3&);

	void finish_flying_or_dashing();
	b8 direction_is_toward_attached_wall(const Vec3&) const;
	b8 can_go(const Vec3&, Vec3* = nullptr, b8* = nullptr) const;
	b8 can_hit(const Target*, Vec3* = nullptr) const;

	void update(const Update&);
};

}
