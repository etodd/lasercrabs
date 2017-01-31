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
struct EffectLight;

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

struct AwkReflectEvent
{
	Entity* entity;
	Vec3 new_velocity;
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
			ContainmentField,
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

	enum class RaycastMode
	{
		Default,
		IgnoreContainmentFields,
	};

	static Awk* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static void update_shield_view(const Update&, Entity*, View*, View*, r32);
	static b8 net_msg(Net::StreamRead*, Net::MessageSource);
	static void stealth(Entity*, b8);

	Quat lerped_rotation;
	Vec3 velocity;
	Vec3 lerped_pos;
	Vec3 last_pos;
	Vec3 remote_reflection_pos;
	Vec3 remote_reflection_dir;
	r32 attach_time;
	r32 invincible_timer;
	r32 shield_time;
	r32 cooldown; // remaining cooldown time
	r32 stun_timer;
	r32 last_footstep;
	r32 particle_accumulator;
	r32 dash_timer;
	r32 remote_reflection_timer;
	Ability current_ability;
	Footing footing[AWK_LEGS];
	Ref<Entity> shield;
	Ref<Entity> overshield;
	Ref<Entity> remote_reflection_entity;
	StaticArray<Ref<Entity>, 8> hit_targets;
	StaticArray<Ref<EffectLight>, 4> fake_projectiles;
	LinkArg<const AwkReflectEvent&> reflecting;
	LinkArg<Entity*> hit;
	LinkArg<Ability> ability_spawned;
	Link done_flying;
	Link done_dashing;
	Link detached;
	Link dashed;
	s8 charges;
	b8 reflection_source_remote;

	Awk();
	void awake();
	~Awk();

	r32 range() const;

	void cooldown_setup();
	State state() const;
	b8 dash_start(const Vec3&);
	b8 cooldown_can_shoot() const; // can we go?
	b8 hit_target(Entity*); // called when we hit a target
	void health_changed(const HealthEvent&);
	void killed(Entity*);

	s16 ally_containment_field_mask() const;

	b8 predict_intersection(const Target*, const Net::StateFrame*, Vec3*, r32) const;

	void reflect(Entity*, const Vec3&, const Vec3&, const Net::StateFrame*);
	void crawl_wall_edge(const Vec3&, const Vec3&, const Update&, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3&, const Update&);
	void update_offset();

	void handle_remote_reflection(Entity*, const Vec3&, const Vec3&);

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
	b8 can_hit(const Target*, Vec3* = nullptr, r32 = AWK_FLY_SPEED) const; // shoot or dash

	void raycast(RaycastMode, const Vec3&, const Vec3&, const Net::StateFrame*, Hits*) const;
	r32 movement_raycast(const Vec3&, const Vec3&);

	void update_server(const Update&);
	void update_client(const Update&);
};

}