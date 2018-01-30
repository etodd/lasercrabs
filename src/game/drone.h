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
struct Flag;

namespace Net
{
	struct StreamRead;
	struct StateFrame;
}

struct DroneReflectEvent
{
	Entity* entity;
	Vec3 new_velocity;
};

struct Drone : public ComponentType<Drone>
{
	enum class State : s8
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
		struct Comparator
		{
			s32 compare(const Hit&, const Hit&);
		};

		enum class Type : s8
		{
			Environment,
			Inaccessible,
			ForceField,
			Shield,
			Target,
			Glass,
			None, // empty space; used as the final hit entry if no hit was registered
			count,
		};

		Vec3 pos;
		Vec3 normal;
		r32 fraction;
		Ref<Entity> entity;
		Type type;
	};

	struct Hits
	{
		StaticArray<Hit, 32> hits;
		s8 index_end;
	};

	enum class RaycastMode : s8
	{
		Default,
		IgnoreForceFields,
	};

	struct Reflection
	{
		Vec3 pos;
		Vec3 dir;
		r32 timer;

		// when the server waits for a client to confirm a local reflection, but that confirmation never comes...
		// the server fast-forwards as if nothing ever happened.
		// this parameter represents the time within the actual frame where the drone reflection happened, which the drone would have travelled if it had not reflected
		r32 additional_fast_forward_time;

		Ref<Entity> entity;
		Net::MessageSource src;
	};

	static r32 particle_accumulator;

	static Drone* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static b8 net_msg(Net::StreamRead*, Net::MessageSource);
	static void stealth(Entity*, b8);
	static void update_all(const Update&);
	static void init();

	Array<Reflection> reflections;
	Array<Ref<Entity>> hit_targets;
	Array<Ref<EffectLight>> fake_projectiles;
	Quat lerped_rotation;
	Vec3 velocity;
	Vec3 lerped_pos;
	Vec3 last_pos;
	Vec3 dash_target;
	r32 attach_time;
	r32 cooldown; // remaining cooldown time
	r32 cooldown_last_local_change;
	r32 cooldown_ability_switch;
	r32 cooldown_ability_switch_last_local_change;
	r32 last_footstep;
	r32 dash_timer;
	r32 last_ability_fired;
	Footing footing[DRONE_LEGS];
	LinkArg<const DroneReflectEvent&> reflecting;
	LinkArg<Entity*> hit;
	LinkArg<Ability> ability_spawned;
	Ref<Flag> flag;
	Link detaching;
	Link dashing;
	Link done_flying;
	Link done_dashing;
	Ability current_ability;
	b8 dash_combo;

	DroneCollisionState collision_state() const;

	Drone();
	~Drone();
	void awake();

	b8 bolter_can_fire() const;

	r32 target_prediction_speed() const;
	r32 range() const;

	b8 net_state_frame(Net::StateFrame*) const;

	Vec3 rotation_clamp() const;
	Vec3 camera_center() const;
	void ability(Ability);
	void cooldown_recoil_setup(Ability);
	b8 cooldown_remote_controlled(r32* = nullptr) const;
	b8 cooldown_ability_switch_remote_controlled(r32* = nullptr) const;
	State state() const;
	b8 dash_start(const Vec3&, const Vec3&, r32 = DRONE_DASH_TIME);
	b8 cooldown_can_shoot() const; // can we go?
	b8 hit_target(Entity*);
	void killed(Entity*);

	s16 ally_force_field_mask() const;

	b8 predict_intersection(const Target*, const Net::StateFrame*, Vec3*, r32) const;

	void reflect(Entity*, const Vec3&, const Vec3&, const Net::StateFrame*);
	void crawl_wall_edge(const Vec3&, const Vec3&, r32, r32);
	b8 transfer_wall(const Vec3&, const btCollisionWorld::ClosestRayResultCallback&);
	void move(const Vec3&, const Quat&, const ID);
	void crawl(const Vec3&, r32);

	enum class OffsetMode : s8
	{
		WithUpgradeStation,
		WithoutUpgradeStation,
		count,
	};

	void get_offset(Mat4*, OffsetMode = OffsetMode::WithUpgradeStation) const;
	void update_offset();

	void handle_remote_reflection(Entity*, const Vec3&, const Vec3&);

	void set_footing(s32, const Transform*, const Vec3&);

	Vec3 center_lerped() const;
	Vec3 attach_point(r32 = 0.0f) const;

	void ensure_detached();
	void finish_flying_dashing_common();
	b8 go(const Vec3&);

	b8 direction_is_toward_attached_wall(const Vec3&) const;
	b8 should_collide(const Target*, const Net::StateFrame* = nullptr) const;
	b8 can_shoot(const Vec3&, Vec3* = nullptr, b8* = nullptr, const Net::StateFrame* = nullptr) const;
	b8 can_shoot(const Target*, Vec3* = nullptr, r32 = DRONE_FLY_SPEED, const Net::StateFrame* = nullptr) const;
	b8 could_shoot(const Vec3&, const Vec3&, Vec3* = nullptr, Vec3* = nullptr, b8* = nullptr, const Net::StateFrame* = nullptr) const;
	b8 can_spawn(Ability, const Vec3&, const Net::StateFrame* = nullptr, Vec3* = nullptr, Vec3* = nullptr, RigidBody** = nullptr, b8* = nullptr) const;
	b8 can_dash(const Target*, Vec3* = nullptr) const;
	b8 can_hit(const Target*, Vec3* = nullptr, r32 = DRONE_FLY_SPEED) const; // shoot or dash

	void raycast(RaycastMode, const Vec3&, const Vec3&, const Net::StateFrame*, Hits*, s32 = 0, Entity* = nullptr) const;
	void movement_raycast(const Vec3&, const Vec3&, Hits* = nullptr, const Net::StateFrame* = nullptr);

	void update_server(const Update&);
	void update_client(const Update&);
};

}
