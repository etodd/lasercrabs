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
	CollisionDefault = btBroadphaseProxy::DefaultFilter,
	CollisionWalker = 1 << 6,
	CollisionInaccessible = 1 << 7,
	CollisionTarget = 1 << 8,
	CollisionShield = 1 << 9,
	CollisionAwkIgnore = 1 << 10,
	CollisionAwk = 1 << 11,
	CollisionContainmentField = 1 << 12,
	CollisionTeamAContainmentField = 1 << 13,
	CollisionTeamBContainmentField = 1 << 14,
	CollisionInaccessibleMask = ~CollisionInaccessible,
};

#define AWK_PERMEABLE_MASK (CollisionTarget | CollisionShield | CollisionAwkIgnore)
#define AWK_INACCESSIBLE_MASK (CollisionInaccessible | CollisionWalker | AWK_PERMEABLE_MASK | CollisionTeamAContainmentField | CollisionTeamBContainmentField)

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
	};

	struct Constraint
	{
		enum class Type
		{
			ConeTwist,
			PointToPoint,
		};
		Type type;
		btTransform frame_a;
		btTransform frame_b;
		Vec3 limits;
		Ref<RigidBody> a;
		Ref<RigidBody> b;
		btTypedConstraint* btPointer;
	};

	static PinArray<Constraint, MAX_ENTITIES> global_constraints;
	static void instantiate_constraint(Constraint*);
	static ID add_constraint(Constraint&);
	static void remove_constraint(ID);

	short collision_group;
	short collision_filter;
	Type type;
	r32 mass;
	Vec3 size;
	s32 mesh_id;
	ID linked_entity; // set the rigid body's user index to this. if IDNull, it's "this" entity's ID.
	Vec2 damping; // use set_damping to ensure the btBody will be updated
	
	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;

	void rebuild(); // rebuild bullet objects from our settings

	void set_damping(r32, r32);

	RigidBody(Type, const Vec3&, r32, short, short, AssetID = AssetNull, ID = IDNull);
	~RigidBody();
	void awake();
};

}
