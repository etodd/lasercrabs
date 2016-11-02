#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "bullet/src/BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "data/import_common.h"
#include "ai.h"
#include "constants.h"

namespace VI
{

struct Transform;
struct Rope;
struct View;
struct HealthEvent;
struct RigidBody;
struct TargetEvent;
struct Target;

namespace Net
{
	struct StreamRead;
	struct StateFrame;
}

// If we raycast through a Minion's head, keep going.
struct AwkRaycastCallback : btCollisionWorld::ClosestRayResultCallback
{
	r32 closest_target_hit_fraction;
	s16 closest_target_hit_group;
	b8 hit_target() const;
	ID entity_id;

	AwkRaycastCallback(const Vec3&, const Vec3&, const Entity*);

	btScalar addSingleResult(btCollisionWorld::LocalRayResult&, b8);
};

struct Awk : public ComponentType<Awk>
{
	enum class State
	{
		Crawl,
		Dash,
		Fly,
		count,
	};

	struct Footing
	{
		Vec3 pos;
		Vec3 last_abs_pos;
		r32 blend;
		Ref<const Transform> parent;
	};

	struct Hit
	{
		enum class Type
		{
			Environment,
			Inaccessible,
			Awk,
			Target,
			count,
		};

		Vec3 pos;
		Vec3 normal;
		r32 fraction;
		Type type;
		Ref<Entity> entity;
	};

	struct Hits
	{
		StaticArray<Hit, 32> hits;
		r32 fraction_end;
		s32 index_end;
	};

	static Awk* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	Quat lerped_rotation;
	Vec3 velocity;
	Vec3 lerped_pos;
	Vec3 last_pos;
	r32 attach_time;
	r32 overshield_timer;
	r32 shield_time;
	r32 cooldown; // remaining cooldown time
	r32 last_footstep;
	r32 particle_accumulator;
	r32 dash_timer;
	Ability current_ability;
	Footing footing[AWK_LEGS];
	Ref<Entity> shield;
	Ref<Entity> overshield;
	StaticArray<Ref<Entity>, 4> hit_targets;
	LinkArg<const Vec3&> bounce;
	LinkArg<Entity*> hit;
	LinkArg<Ability> ability_spawned;
	Link done_flying;
	Link done_dashing;
	Link detached;
	Link dashed;
	s8 charges;

	Awk();
	void awake();
	~Awk();
	
	static b8 net_msg(Net::StreamRead*, Net::MessageSource);

	r32 range() const;

	void cooldown_setup();
	State state() const;
	b8 dash_start(const Vec3&);
	b8 cooldown_can_shoot() const; // can we go?
	void hit_by(const TargetEvent&); // called when we get hit
	b8 hit_target(Entity*); // called when we hit a target
	void health_changed(const HealthEvent&);
	void killed(Entity*);
	Entity* incoming_attacker() const;

	s16 ally_containment_field_mask() const;

	b8 predict_intersection(const Target*, const Net::StateFrame*, Vec3*, r32 = AWK_FLY_SPEED) const;

	void stealth(b8);

	void reflect(Entity*, const Vec3&, const Vec3&, const Net::StateFrame*);
	void crawl_wall_edge(const Vec3&, const Vec3&, const Update&, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3&, const Update&);
	void update_offset();

	void set_footing(s32, const Transform*, const Vec3&);

	Vec3 center_lerped() const;
	Vec3 attach_point(r32 = 0.0f) const;

	void detach_teleport();
	void finish_flying_dashing_common();
	b8 go(const Vec3&);

	b8 direction_is_toward_attached_wall(const Vec3&) const;
	b8 can_shoot(const Vec3&, Vec3* = nullptr, b8* = nullptr, const Net::StateFrame* = nullptr) const;
	b8 can_shoot(const Target*, Vec3* = nullptr, r32 = AWK_FLY_SPEED, const Net::StateFrame* = nullptr) const;
	b8 can_spawn(Ability, const Vec3&, Vec3* = nullptr, Vec3* = nullptr, RigidBody** = nullptr, b8* = nullptr) const;
	b8 can_dash(const Target*, Vec3* = nullptr) const;
	b8 can_hit(const Target*, Vec3* = nullptr) const; // shoot or dash

	void raycast(const Vec3&, const Vec3&, const Net::StateFrame*, Hits*) const;
	r32 movement_raycast(const Vec3&, const Vec3&);

	void update_server(const Update&);
	void update_client(const Update&);
};

}
