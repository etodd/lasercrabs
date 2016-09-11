#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"
#include "recast/Detour/Include/DetourNavMesh.h"
#include "ai.h"
#include <bullet/src/BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include "render/ui.h"
#include "common.h"
#include "ease.h"
#include "player.h"

namespace VI
{

struct AwkEntity : public Entity
{
	AwkEntity(AI::Team);
};

struct DamageEvent
{
	Entity* damager;
	u16 amount;
};

struct Health : public ComponentType<Health>
{
	r32 regen_timer;
	LinkArg<const DamageEvent&> damaged;
	LinkArg<Entity*> killed;
	Link added;
	u8 shield;
	u8 shield_max;
	u8 hp;
	u8 hp_max;

	Health(u8, u8, u8 = 0, u8 = 0);

	void update(const Update&);
	void awake() {}
	void damage(Entity*, u8);
	void take_shield();
	void take_health(Entity*, u8);
	void kill(Entity*);
	void add(u8);
	u8 total() const;
};

#define HEALTH_PICKUP_RADIUS 0.55f
struct HealthPickupEntity : public Entity
{
	HealthPickupEntity(const Vec3&);
};

#define CONTROL_POINT_INTERVAL 15.0f
struct HealthPickup : public ComponentType<HealthPickup>
{
	struct Key
	{
		Vec3 me;
		b8 closest_first;
		r32 priority(HealthPickup*);
	};

	static r32 power_particle_timer;
	static r32 particle_accumulator;

	static void update_all(const Update&);
	static void sort_all(const Vec3&, Array<Ref<HealthPickup>>*, b8, Health* = nullptr);
	static HealthPickup* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(Health*);

	Ref<Health> owner;

	void awake();
	void hit(const TargetEvent&);
	b8 set_owner(Health*);
	void reset();
};

struct ControlPointEntity : public Entity
{
	ControlPointEntity(AI::Team);
};

#define CONTROL_POINT_RADIUS 3.0f
struct ControlPoint : public ComponentType<ControlPoint>
{
	static ControlPoint* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(AI::TeamMask);

	AI::Team team;

	ControlPoint(AI::Team);
	void awake();
	void set_team(AI::Team);
};

struct SensorEntity : public Entity
{
	SensorEntity(PlayerManager*, const Vec3&, const Quat&);
};

#define SENSOR_TIME 1.0f
#define SENSOR_TIMEOUT 5.0f
#define SENSOR_RADIUS 0.2f
#define SENSOR_HEALTH 2
struct Sensor : public ComponentType<Sensor>
{
	AI::Team team;
	Ref<PlayerManager> owner;

	Sensor(AI::Team, PlayerManager* = nullptr);

	void killed_by(Entity*);
	void awake();

	void hit_by(const TargetEvent&);

	static b8 can_see(AI::Team, const Vec3&, const Vec3&);
	static Sensor* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	static void update_all(const Update&);
};

#define ROCKET_RANGE (AWK_MAX_DISTANCE * 1.5f)
struct Rocket : public ComponentType<Rocket>
{
	AI::Team team;
	Ref<Entity> target;
	Ref<Entity> owner;
	r32 particle_accumulator;
	r32 remaining_lifetime;

	static Rocket* inbound(Entity*);
	static Rocket* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	Rocket();

	void explode();

	void awake();
	void killed(Entity*);
	void update(const Update&);
	void launch(Entity*);
};

struct RocketEntity : public Entity
{
	RocketEntity(Entity*, Transform*, const Vec3&, const Quat&, AI::Team);
};

#define CONTAINMENT_FIELD_BASE_OFFSET 0.95f
#define CONTAINMENT_FIELD_LIFETIME 15.0f
struct ContainmentField : public ComponentType<ContainmentField>
{
	static r32 particle_accumulator;

	static void update_all(const Update&);
	static ContainmentField* inside(AI::TeamMask, const Vec3&);
	static ContainmentField* closest(AI::TeamMask, const Vec3&, r32*);
	static u32 hash(AI::Team, const Vec3&);

	AI::Team team;
	Ref<Entity> field;
	Ref<PlayerManager> owner;
	r32 remaining_lifetime;
	b8 powered;

	ContainmentField(const Vec3&, PlayerManager*);
	void awake();
	~ContainmentField();
	void hit_by(const TargetEvent&);
	void killed(Entity*);
	void destroy();
	b8 contains(const Vec3&) const;
};

struct ContainmentFieldEntity : public Entity
{
	ContainmentFieldEntity(Transform*, const Vec3&, const Quat&, PlayerManager*);
};

struct Teleporter : public ComponentType<Teleporter>
{
	static Teleporter* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	AI::Team team;

	void awake();
	void killed(Entity*);
	void destroy();
};

void teleport(Entity*, Teleporter*);

struct TeleporterEntity : public Entity
{
	TeleporterEntity(const Vec3&, const Quat&, AI::Team);
};

struct AICue : public ComponentType<AICue>
{
	enum Type
	{
		Sensor = 1,
		Rocket = 1 << 1,
		Snipe = 1 << 2,
	};

	typedef s32 TypeMask;
	static const TypeMask TypeAll = (TypeMask)-1;

	static AICue* in_range(TypeMask, const Vec3&, r32, s32* = nullptr);

	TypeMask type;
	AICue(TypeMask);
	void awake() {}
};

struct ShockwaveEntity : public Entity
{
	ShockwaveEntity(r32, r32);
};

struct Shockwave : public ComponentType<Shockwave>
{
	r32 max_radius;
	r32 timer;
	r32 duration;

	Shockwave(r32, r32);
	void awake() {}

	r32 radius() const;
	void update(const Update&);
};

struct WaterEntity : public Entity
{
	WaterEntity(AssetID);
};

#define rope_segment_length 0.5f
#define rope_radius 0.075f

struct Rope : public ComponentType<Rope>
{
	static Array<Mat4> instances;

	static void draw_opaque(const RenderParams&);
	static void spawn(const Vec3&, const Vec3&, r32, r32 = 0.0f);

	Ref<RigidBody> prev;
	Ref<RigidBody> next;

	void awake() {}
	static Rope* start(RigidBody*, const Vec3&, const Vec3&, const Quat&, r32 = 0.0f);
	void end(const Vec3&, const Vec3&, RigidBody*, r32 = 0.0f);
};

struct ProjectileEntity : public Entity
{
	ProjectileEntity(Entity*, const Vec3&, const Vec3&);
};

#define PROJECTILE_SPEED 20.0f
struct Projectile : public ComponentType<Projectile>
{
	Ref<Entity> owner;
	Vec3 velocity;
	r32 lifetime;

	Projectile(Entity*, const Vec3&);
	void awake();

	void update(const Update&);
};

struct TargetEvent
{
	Entity* hit_by;
	Entity* target;
};

struct Target : public ComponentType<Target>
{
	Vec3 local_offset;
	Vec3 absolute_pos() const;
	LinkArg<const TargetEvent&> target_hit;

	void hit(Entity*);
	void awake() {}
	b8 predict_intersection(const Vec3&, r32, Vec3*) const;
};

struct PlayerTrigger : public ComponentType<PlayerTrigger>
{
	const static s32 max_trigger = MAX_PLAYERS;
	r32 radius;
	LinkArg<Entity*> entered;
	LinkArg<Entity*> exited;
	Ref<Entity> triggered[max_trigger];

	PlayerTrigger();

	void awake() {}

	void update(const Update&);

	b8 is_triggered(const Entity*) const;

	s32 count() const;
};


}