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
	u16 hp;
	u16 hp_max;
	LinkArg<const DamageEvent&> damaged;
	LinkArg<Entity*> killed;
	Link added;

	Health(u16, u16);

	void set(u16);
	void awake() {}
	void damage(Entity*, u16);
	void add(u16);
	b8 is_full() const;
};

#define HEALTH_PICKUP_RADIUS 0.55f
struct HealthPickupEntity : public Entity
{
	HealthPickupEntity(const Vec3&);
};

struct HealthPickup : public ComponentType<HealthPickup>
{
	static r32 timer;
	static r32 power_particle_timer;
	static r32 particle_accumulator;
	static void update_all(const Update&);
	static u16 increment(PlayerCommon*);

	struct Key
	{
		Vec3 me;
		b8 closest_first;
		r32 priority(HealthPickup*);
	};

	static void sort_all(const Vec3&, Array<Ref<HealthPickup>>*, b8, Health* = nullptr);
	static s32 available_count();

	Ref<Health> owner;

	void awake();
	void hit(const TargetEvent&);
	b8 set_owner(Health*);
	void reset();
};

struct SensorEntity : public Entity
{
	SensorEntity(PlayerManager*, const Vec3&, const Quat&);
};

#define SENSOR_TIME 1.5f
#define SENSOR_TIMEOUT 5.0f
#define SENSOR_RADIUS 0.15f
#define SENSOR_HEALTH 3
struct Sensor : public ComponentType<Sensor>
{
	AI::Team team;
	Ref<PlayerManager> owner;

	Sensor(AI::Team, PlayerManager* = nullptr);

	void killed_by(Entity*);
	void awake();

	void hit_by(const TargetEvent&);

	static b8 can_see(AI::Team, const Vec3&, const Vec3&);
	static Sensor* closest(AI::Team, const Vec3&, r32* = nullptr);

	static void update_all(const Update&);
};

#define ROCKET_RANGE (AWK_MAX_DISTANCE * 2.0f)
struct Rocket : public ComponentType<Rocket>
{
	AI::Team team;
	Ref<Entity> target;
	Ref<Entity> owner;
	r32 particle_accumulator;

	static Rocket* inbound(Entity*);
	static Rocket* closest(AI::Team, const Vec3&, r32* = nullptr);

	void awake();
	void killed(Entity*);
	void update(const Update&);
	void launch(Entity*);
};

struct RocketEntity : public Entity
{
	RocketEntity(Entity*, Transform*, const Vec3&, const Quat&, AI::Team);
};

#define CONTAINMENT_FIELD_RADIUS 16.0f
#define CONTAINMENT_FIELD_BASE_OFFSET 0.95f
struct ContainmentField : public ComponentType<ContainmentField>
{
	static r32 particle_accumulator;

	static void update_all(const Update&);
	static ContainmentField* inside(AI::Team, const Vec3&);
	static ContainmentField* closest(AI::Team, const Vec3&, r32*);

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
};

struct ContainmentFieldEntity : public Entity
{
	ContainmentFieldEntity(Transform*, const Vec3&, const Quat&, PlayerManager*);
};

// for AI
struct SensorInterestPoint : public ComponentType<SensorInterestPoint>
{
	void awake() {}
	static SensorInterestPoint* in_range(const Vec3&);
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

struct DataFragment : public ComponentType<DataFragment>
{
	b8 collected() const;
	s32 hash() const;
	AssetID text() const;
	void awake();
	void collect();
	static DataFragment* in_range(const Vec3&);
	static s32 count_collected();
};

struct DataFragmentEntity : public Entity
{
	DataFragmentEntity(const Vec3&, const Quat&);
};

struct MoverEntity : public Entity
{
	MoverEntity(const b8, const b8, const b8);
};

struct Mover : public ComponentType<Mover>
{
	Vec3 start_pos;
	Quat start_rot;
	Vec3 end_pos;
	Quat end_rot;
	Ref<Transform> object;
	Ease::Type ease;
	b8 reversed;
	b8 translation;
	b8 rotation;
	r32 x;
	r32 target;
	r32 speed;
	b8 last_moving;

	Mover(b8 = false, b8 = true, b8 = true);
	void awake() {}
	void update(const Update&);
	void setup(Transform*, Transform*, r32);
	void go();
	void refresh();
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

struct TileEntity : public Entity
{
	TileEntity(const Vec3&, const Quat&, Transform*, const Vec3&, r32 = 0.3f);
};

#define TILE_SIZE 0.5f
struct Tile : public ComponentType<Tile>
{
	static Array<Mat4> instances;

	Vec3 relative_start_pos;
	Quat relative_start_rot;
	Vec3 relative_target_pos;
	Quat relative_target_rot;
	r32 timer;
	r32 anim_time;

	Tile(const Vec3&, const Quat&, r32 = 0.3f);
	void awake();
	r32 scale() const;
	void update(const Update&);
	static void draw_alpha(const RenderParams&);
};

#define PLAYER_SPAWN_RADIUS 3.0f
struct PlayerSpawn : public Entity
{
	PlayerSpawn(AI::Team);
};

#define TERMINAL_HEIGHT 2.0f
#define TERMINAL_TRIGGER_RADIUS 3.0f
#define TERMINAL_LIGHT_RADIUS 8.0f
struct Terminal : public Entity
{
	Terminal();
};

struct ProjectileEntity : public Entity
{
	ProjectileEntity(Entity*, const Vec3&, const Vec3&);
};

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