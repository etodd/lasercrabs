#pragma once

#include "types.h"

#include <bullet/src/btBulletDynamicsCommon.h>
#include "data/entity.h"
#include "lmath.h"
#include "sync.h"

namespace VI
{

enum CollisionGroup
{
	CollisionDefault = btBroadphaseProxy::DefaultFilter, // 1 << 0
	CollisionStatic = btBroadphaseProxy::StaticFilter, // 1 << 1
	CollisionWalker = 1 << 2,
	CollisionInaccessible = 1 << 3,
	CollisionTarget = 1 << 4,
	CollisionShield = 1 << 5,
	CollisionAwkIgnore = 1 << 6,
	CollisionTeamAContainmentField = 1 << 7,
	CollisionTeamBContainmentField = 1 << 8,
	CollisionTeamCContainmentField = 1 << 9,
	CollisionTeamDContainmentField = 1 << 10,
	CollisionAllTeamsContainmentField =
	(
		CollisionTeamAContainmentField
		| CollisionTeamBContainmentField
		| CollisionTeamCContainmentField
		| CollisionTeamDContainmentField
	),
	CollisionParkour = 1 << 12,
};

#define AWK_PERMEABLE_MASK (CollisionTarget | CollisionShield | CollisionAwkIgnore)
#define AWK_INACCESSIBLE_MASK (CollisionInaccessible | CollisionWalker | AWK_PERMEABLE_MASK | CollisionAllTeamsContainmentField)

struct RaycastCallbackExcept : btCollisionWorld::ClosestRayResultCallback
{
	ID entity_id;
	RaycastCallbackExcept(const Vec3& a, const Vec3& b, const Entity*);
	virtual	btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, b8 normalInWorldSpace);
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
};

struct RigidBody : public ComponentType<RigidBody>
{
	enum class Type
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
		enum class Type
		{
			ConeTwist,
			PointToPoint,
			count,
		};
		btTypedConstraint* btPointer;
		btTransform frame_a;
		btTransform frame_b;
		Vec3 limits;
		Type type;
		Ref<RigidBody> a;
		Ref<RigidBody> b;
	};

	static PinArray<Constraint, MAX_ENTITIES> global_constraints;
	static void instantiate_constraint(Constraint*, ID);
	static ID add_constraint(const Constraint&);
	static Constraint* net_add_constraint();
	static void remove_constraint(ID);

	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;
	Vec3 size;
	Vec2 damping; // use set_damping to ensure the btBody will be updated
	Type type;
	r32 mass;
	r32 restitution;
	AssetID mesh_id;
	s16 collision_group;
	s16 collision_filter;
	b8 ccd; // continuous collision detection

	RigidBody(Type, const Vec3&, r32, s16, s16, AssetID = AssetNull);
	RigidBody();
	~RigidBody();
	void awake();

	void rebuild(); // rebuild bullet objects from our settings

	void set_damping(r32, r32);
	void set_restitution(r32);
	void set_ccd(b8);
	void set_collision_masks(s16, s16);

	void activate_linked();
};

}
