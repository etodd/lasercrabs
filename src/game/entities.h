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
	HealthPickupEntity();
};

struct HealthPickup : public ComponentType<HealthPickup>
{
	Ref<Health> owner;
	void awake();
	void hit(const TargetEvent&);
	void reset();
};

struct SensorEntity : public Entity
{
	SensorEntity(PlayerManager*, const Vec3&, const Quat&);
};

#define SENSOR_TIME 2.0f
#define SENSOR_TIMEOUT 5.0f
#define SENSOR_RADIUS 0.15f
struct Sensor : public ComponentType<Sensor>
{
	AI::Team team;
	Ref<PlayerManager> player_manager;

	Sensor(AI::Team, PlayerManager* = nullptr);

	void killed_by(Entity*);
	void awake();

	static b8 can_see(AI::Team, const Vec3&, const Vec3&);
	static Sensor* closest(AI::Team, const Vec3&, r32* = nullptr);

	static void update_all(const Update&);
};

struct Teleporter : public ComponentType<Teleporter>
{
	static Teleporter* closest(const Vec3&, AI::Team);

	AI::Team team;
	u32 obstacle_id;

	Teleporter();
	void awake() {}
	~Teleporter();
};

#define TELEPORT_TIME 1.0f
#define TELEPORT_INVINCIBLE_PERIOD 1.0f
// yes really
struct Teleportee : public ComponentType<Teleportee>
{
	Ref<Teleporter> target;
	r32 accumulator;
	r32 timer;

	void awake() {}
	void update(const Update&);
	b8 in_progress() const;
	b8 invincible() const;
	void go(Teleporter*);
};

struct TeleporterEntity : public Entity
{
	TeleporterEntity(const Vec3&, const Quat&, AI::Team);
};

// for AI
struct SensorInterestPoint : public ComponentType<SensorInterestPoint>
{
	void awake() {}
	static SensorInterestPoint* in_range(const Vec3&);
};

struct ShockwaveEntity : public Entity
{
	ShockwaveEntity(r32);
};

struct Shockwave : public ComponentType<Shockwave>
{
	r32 max_radius;
	r32 timer;

	Shockwave(r32);
	void awake() {}

	r32 duration() const;
	r32 radius() const;
	void update(const Update&);
};

struct DataFragment : public ComponentType<DataFragment>
{
	b8 collected;
	void awake() {}
	void collect();
	static DataFragment* in_range(const Vec3&);
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

	Mover(const b8 = false, const b8 = true, const b8 = true);
	void awake() {}
	void update(const Update&);
	void setup(Transform*, Transform*, const r32);
	void refresh();
};

#define rope_segment_length 0.5f
#define rope_radius 0.05f

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

struct ControlPoint : public ComponentType<ControlPoint>
{
	static r32 timer;
	static void update_all(const Update&);
	AI::Team team;
	ControlPoint();
	void awake() {}
};

struct ControlPointEntity : public Entity
{
	ControlPointEntity();
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

#define PLAYER_SPAWN_RADIUS 2.0f
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
	ProjectileEntity(Entity*, const Vec3&, u16, const Vec3&);
};

struct Projectile : public ComponentType<Projectile>
{
	Ref<Entity> owner;
	Vec3 velocity;
	u16 damage;
	r32 lifetime;

	Projectile(Entity*, u16, const Vec3&);
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