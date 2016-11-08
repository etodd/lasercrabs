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

namespace Net
{
	struct StreamRead;
	struct StateFrame;
}

void explosion(const Vec3&, const Quat&);

struct AwkEntity : public Entity
{
	AwkEntity(AI::Team);
};

struct HealthEvent
{
	Ref<Entity> source;
	s8 hp;
	s8 shield;
};

struct Health : public ComponentType<Health>
{
	static b8 net_msg(Net::StreamRead*);

	r32 regen_timer;
	LinkArg<const HealthEvent&> changed;
	LinkArg<Entity*> killed;
	s8 shield;
	s8 shield_max;
	s8 hp;
	s8 hp_max;

	Health(s8 = 0, s8 = 0, s8 = 0, s8 = 0);

	void update(const Update&);
	void awake() {}
	void damage(Entity*, s8);
	void take_shield();
	void kill(Entity*);
	void add(s8);
	s8 total() const;
};

struct EnergyPickupEntity : public Entity
{
	EnergyPickupEntity(const Vec3&);
};

struct EnergyPickup : public ComponentType<EnergyPickup>
{
	struct Key
	{
		Vec3 me;
		b8 closest_first;
		r32 priority(EnergyPickup*);
	};

	static r32 power_particle_timer;
	static r32 particle_accumulator;

	static void update_all(const Update&);
	static void sort_all(const Vec3&, Array<Ref<EnergyPickup>>*, b8, AI::TeamMask);
	static EnergyPickup* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(AI::TeamMask);
	static b8 net_msg(Net::StreamRead*);

	Ref<Entity> light;
	AI::Team team = AI::TeamNone;

	void awake();
	~EnergyPickup();
	void hit(const TargetEvent&);
	b8 set_team(AI::Team, Entity* = nullptr);
	void reset();
};

struct PlayerSpawnEntity : public Entity
{
	PlayerSpawnEntity(AI::Team);
};

struct ControlPointEntity : public Entity
{
	ControlPointEntity(AI::Team, const Vec3&);
};

struct PlayerSpawn : public ComponentType<PlayerSpawn>
{
	AI::Team team;
	void awake() {}
};

struct ControlPoint : public ComponentType<ControlPoint>
{
	static ControlPoint* closest(AI::TeamMask, const Vec3&, r32* = nullptr);
	static s32 count(AI::TeamMask);
	static s32 count_capturing();

	r32 capture_timer;
	AI::Team team;
	AI::Team team_next = AI::TeamNone;
	u32 obstacle_id;

	ControlPoint(AI::Team = AI::TeamNone);
	~ControlPoint();
	b8 owned_by(AI::Team) const;
	void awake();
	void set_team(AI::Team);
	void capture_start(AI::Team);
	void capture_cancel();
	void update(const Update&);
};

struct SensorEntity : public Entity
{
	SensorEntity(AI::Team, const Vec3&, const Quat&);
};

struct Sensor : public ComponentType<Sensor>
{
	AI::Team team;

	Sensor(AI::Team = AI::TeamNone);

	void killed_by(Entity*);
	void awake();

	void hit_by(const TargetEvent&);

	static b8 can_see(AI::Team, const Vec3&, const Vec3&);
	static Sensor* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	static void update_all_client(const Update&);
};

struct Rocket : public ComponentType<Rocket>
{
	r32 particle_accumulator;
	r32 remaining_lifetime;
	Ref<Entity> target;
	Ref<PlayerManager> owner;

	static Rocket* inbound(Entity*);
	static Rocket* closest(AI::TeamMask, const Vec3&, r32* = nullptr);

	Rocket();
	void awake();

	void explode();
	AI::Team team() const;
	void killed(Entity*);
	void update(const Update&);
	void launch(Entity*);
};

struct RocketEntity : public Entity
{
	RocketEntity(PlayerManager*, Transform*, const Vec3&, const Quat&, AI::Team);
};

struct Decoy : public ComponentType<Decoy>
{
	Ref<PlayerManager> owner;
	void awake();
	void hit_by(const TargetEvent&);
	void killed(Entity*);
	void destroy();
	AI::Team team() const;
};

struct DecoyEntity : public Entity
{
	DecoyEntity(PlayerManager*, Transform*, const Vec3&, const Quat&);
};

struct ContainmentField : public ComponentType<ContainmentField>
{
	static r32 particle_accumulator;

	static void update_all(const Update&);
	static ContainmentField* inside(AI::TeamMask, const Vec3&);
	static ContainmentField* closest(AI::TeamMask, const Vec3&, r32*);
	static u32 hash(AI::Team, const Vec3&);

	r32 remaining_lifetime;
	Ref<Entity> field;
	Ref<PlayerManager> owner;
	AI::Team team;
	b8 powered;

	ContainmentField();
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
	TeleporterEntity(Transform*, const Vec3&, const Quat&, AI::Team);
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
	AICue();
	void awake() {}
};

struct Shockwave
{
	static PinArray<Shockwave, MAX_ENTITIES> list;

	static void add(const Vec3&, r32, r32);

	Vec3 pos;
	r32 max_radius;
	r32 timer;
	r32 duration;

	r32 radius() const;
	Vec3 color() const;
	void update(const Update&);

	inline ID id() const
	{
		return this - &list[0];
	}
};

struct WaterEntity : public Entity
{
	WaterEntity(AssetID);
};

struct Rope : public ComponentType<Rope>
{
	static Array<Mat4> instances;

	static void draw_opaque(const RenderParams&);
	static void spawn(const Vec3&, const Vec3&, r32, r32 = 0.0f);

	void awake() {}
	static Rope* start(RigidBody*, const Vec3&, const Vec3&, const Quat&, r32 = 0.0f);
	void end(const Vec3&, const Vec3&, RigidBody*, r32 = 0.0f);
};

struct ProjectileEntity : public Entity
{
	ProjectileEntity(PlayerManager*, const Vec3&, const Vec3&);
};

struct Projectile : public ComponentType<Projectile>
{
	Vec3 velocity;
	r32 lifetime;
	Ref<PlayerManager> owner;

	void awake();

	AI::Team team() const;
	void update(const Update&);
};

struct GrenadeEntity : public Entity
{
	GrenadeEntity(PlayerManager*, const Vec3&, const Vec3&);
};

struct Grenade : public ComponentType<Grenade>
{
	static r32 particle_timer;
	static r32 particle_accumulator;

	Vec3 velocity;
	r32 timer;
	Ref<PlayerManager> owner;
	b8 active;

	void awake();

	void hit_by(const TargetEvent&);
	void killed_by(Entity*);
	AI::Team team() const;
	void explode();

	void update_server(const Update&);
	static void update_client_all(const Update&);
};

struct TargetEvent
{
	Entity* hit_by;
	Entity* target;
};

struct Target : public ComponentType<Target>
{
	Vec3 local_offset;
	Vec3 net_velocity;
	LinkArg<const TargetEvent&> target_hit;

	void awake() {}
	Vec3 velocity() const;
	Vec3 absolute_pos() const;
	void hit(Entity*);
	b8 predict_intersection(const Vec3&, r32, const Net::StateFrame*, Vec3*) const;
	r32 radius() const;
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