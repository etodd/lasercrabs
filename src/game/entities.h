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

struct Health : public ComponentType<Health>
{
	u16 hp;
	u16 total;
	LinkArg<Entity*> damaged;
	LinkArg<Entity*> killed;

	Health(u16);

	void awake() {}
	void damage(Entity*, u16);
};

struct ShockwaveEntity : public Entity
{
	ShockwaveEntity(Entity*, r32);
};

struct Shockwave : public ComponentType<Shockwave>
{
	r32 max_radius;
	r32 timer;
	Ref<Entity> owner;

	Shockwave(Entity*, r32);
	void awake() {}

	r32 duration() const;
	r32 radius() const;
	void update(const Update&);
};

struct SocketEntity : public StaticGeom
{
	SocketEntity(const Vec3&, const Quat&, const b8);
};

struct Socket : public ComponentType<Socket>
{
	b8 permanent_powered;
	b8 powered;
	StaticArray<Ref<Socket>, 8> links;
	Ref<Entity> target;

	Socket(const b8);
	void awake();
	static void refresh_all();
	void refresh();
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

struct RopeEntity : public Entity
{
	RopeEntity(const Vec3&, const Vec3&, RigidBody*, const r32 = 0.0f);
};

struct Rope : public ComponentType<Rope>
{
	static Array<Mat4> instances;

	static void draw_opaque(const RenderParams&);
	static void spawn(const Vec3&, const Vec3&, const r32, const r32 = 0.0f);

	StaticArray<Ref<RigidBody>, 50> segments;
	Ref<RigidBody> last_segment;
	Vec3 last_segment_relative_pos;
	r32 slack;
	Ref<Socket> socket1;
	Ref<Socket> socket2;

	Rope(const Vec3&, const Vec3&, RigidBody*, const r32 = 0.0f);
	void awake() {}
	void add(const Vec3&, const Quat&);
	void end(const Vec3&, const Vec3&, RigidBody*);
	void remove(Entity*);
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

struct TargetEntity : public Entity
{
	TargetEntity(const Vec3&, const Quat&, AI::Team);
};

#define PLAYER_SPAWN_RADIUS 1
struct PlayerSpawn : public Entity
{
	PlayerSpawn(AI::Team);
};

#define MINION_SPAWN_RADIUS 1
struct MinionSpawn : public Entity
{
	MinionSpawn(AI::Team);
};

#define TURRET_VIEW_RANGE 20.0f
struct Turret : public Entity
{
	Turret(AI::Team);
};

struct TurretControl : public ComponentType<TurretControl>
{
	Ref<Entity> target;
	r32 yaw;
	r32 pitch;
	r32 cooldown;
	r32 target_check_time;

	void awake();

	void killed(Entity*);
	void update(const Update&);
	void check_target();
	b8 can_see(Entity*) const;
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

struct Target : public ComponentType<Target>
{
	LinkArg<Entity*> hit_by; // Passes the entity we were hit by
	LinkArg<Entity*> hit_this; // Passes the entity getting hit
	Target() : hit_by(), hit_this() {}
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
