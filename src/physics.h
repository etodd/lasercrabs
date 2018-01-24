#pragma once

#include "types.h"

#include <bullet/src/btBulletDynamicsCommon.h>
#include "data/entity.h"
#include "lmath.h"
#include "sync.h"

namespace VI
{

enum CollisionGroup : s16
{
	CollisionDefault = btBroadphaseProxy::DefaultFilter, // 1 << 0
	CollisionStatic = btBroadphaseProxy::StaticFilter, // 1 << 1
	CollisionWalker = 1 << 2,
	CollisionMinionMoving = 1 << 3,
	CollisionInaccessible = 1 << 4,
	CollisionTarget = 1 << 5,
	CollisionDroneIgnore = 1 << 6,
	CollisionTeamAForceField = 1 << 7,
	CollisionTeamBForceField = 1 << 8,
	CollisionTeamCForceField = 1 << 9,
	CollisionTeamDForceField = 1 << 10,
	CollisionAllTeamsForceField =
	(
		CollisionTeamAForceField
		| CollisionTeamBForceField
		| CollisionTeamCForceField
		| CollisionTeamDForceField
	),
	CollisionParkour = 1 << 12,
	CollisionElectric = 1 << 13,
	CollisionAudio = 1 << 14,
	CollisionGlass = s16(1 << 15),
};

#define DRONE_PERMEABLE_MASK (CollisionTarget | CollisionDroneIgnore | CollisionDefault | CollisionWalker | CollisionMinionMoving)
#define DRONE_INACCESSIBLE_MASK (CollisionInaccessible | CollisionElectric | DRONE_PERMEABLE_MASK | CollisionAllTeamsForceField | CollisionGlass)

struct RaycastCallbackExcept : btCollisionWorld::ClosestRayResultCallback
{
	Array<ID> additional_ids;
	ID entity_id;
	RaycastCallbackExcept(const Vec3& a, const Vec3& b, const Entity*);
	virtual	btScalar addSingleResult(btCollisionWorld::LocalRayResult&, b8);
	void ignore(const Entity*);
};

struct PhysicsSync
{
	b8 quit;
	GameTime time;
	r32 timestep;
};

typedef Sync<PhysicsSync, 1>::Swapper PhysicsSwapper;

struct Physics
{
	static btDbvtBroadphase* broadphase;
	static btDefaultCollisionConfiguration* collision_config;
	static btCollisionDispatcher* dispatcher;
	static btSequentialImpulseConstraintSolver* solver;
	static btDiscreteDynamicsWorld* btWorld;

	static void loop(PhysicsSwapper*);
	static void sync_static();
	static void sync_dynamic();

	static void raycast(btCollisionWorld::ClosestRayResultCallback*, s16 = ~CollisionTarget & ~CollisionWalker);
	static void raycast(btCollisionWorld::AllHitsRayResultCallback*, s16 = ~CollisionTarget & ~CollisionWalker);
};

struct RigidBody : public ComponentType<RigidBody>
{
	enum class Type : s8
	{
		Box,
		CapsuleX,
		CapsuleY,
		CapsuleZ,
		Sphere,
		Mesh,
		count,
	};

	struct Constraint
	{
		enum class Type : s8
		{
			ConeTwist,
			PointToPoint,
			Fixed,
			count,
		};
		btTypedConstraint* btPointer;
		btTransform frame_a;
		btTransform frame_b;
		Vec3 limits;
		Ref<RigidBody> a;
		Ref<RigidBody> b;
		Type type;
	};

	static PinArray<Constraint, MAX_ENTITIES> global_constraints;
	static void instantiate_constraint(Constraint*, ID);
	static ID add_constraint(const Constraint&);
	static Constraint* net_add_constraint();
	static void remove_constraint(ID);
	static void rebuild_constraint(ID);

	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;
	Vec3 size;
	Vec2 damping; // use set_damping to ensure the btBody will be updated
	r32 mass;
	r32 restitution;
	AssetID mesh_id;
	s16 collision_group;
	s16 collision_filter;
	Type type;
	s8 flags;

	static const s8 FlagContinuousCollisionDetection = 1 << 0;
	static const s8 FlagHasConstraints = 1 << 1;
	static const s8 FlagGhost = 1 << 2; // ghost rigidbodies still exist, but don't collide or move or affect constraints. Only servers have ghost objects. Clients always simulate everything.
	static const s8 FlagAudioReflector = 1 << 3;

	RigidBody(Type, const Vec3&, r32, s16, s16, AssetID = AssetNull, s8 = 0);
	RigidBody();
	~RigidBody();
	void awake();

	void rebuild(); // rebuild bullet objects from our settings

	void set_damping(r32, r32);
	void set_restitution(r32);
	void set_ccd(b8); // continuous collision detection
	void set_ghost(b8);
	void set_collision_masks(s16, s16);

	void activate_linked();
	void remove_all_constraints();
};

}
