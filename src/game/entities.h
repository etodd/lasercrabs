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

#define CONE_NORMAL Vec3(1, 1, 0.6f)
#define CONE_NORMAL_ALPHA 0.3f
#define CONE_ATTACKING Vec3(1, 0.0f, 0.0f)
#define CONE_ATTACKING_ALPHA 0.7f

struct VisionCone
{
	static Entity* create(Transform* parent, r32, r32, const Vec3&);
};

struct AwkEntity : public Entity
{
	AwkEntity(AI::Team);
};

struct Health : public ComponentType<Health>
{
	s32 hp;
	LinkArg<s32> damaged;
	LinkArg<Entity*> killed;

	Health(const s32);

	void awake() {}
	void damage(Entity*, const s32);
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

struct SentinelSpawnControl : public ComponentType<SentinelSpawnControl>
{
	StaticArray<Ref<Transform>, 8> idle_path;
	Ref<Entity> spawned;
	Ref<Transform> spawn;
	void awake() {}
	void reset(Entity* = nullptr);
	void go(Entity*);
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
	void segment_hit(Entity*);
};

struct SentinelSpawn : public Entity
{
	SentinelSpawn(const Vec3&, const Quat&);
};

struct CreditsPickupEntity : public Entity
{
	CreditsPickupEntity(const Vec3&, const Quat&);
};

struct CreditsPickup : public ComponentType<CreditsPickup>
{
	void awake() {}
	void hit_by(Entity*);
};

struct PlayerSpawn : public Entity
{
	PlayerSpawn(AI::Team);
};

struct Portal : public Entity
{
	Portal();
};

struct PortalControl : public ComponentType<PortalControl>
{
	AssetID next;
	UIText text;

	PortalControl();
	void awake() {}
	void player_enter(Entity*);
	void draw_alpha(const RenderParams&);
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