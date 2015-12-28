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
	CollisionNothing = btBroadphaseProxy::DefaultFilter,
	CollisionWalker = 1 << 6,
	CollisionInaccessible = 1 << 7,
	CollisionTarget = 1 << 8,
	CollisionInaccessibleMask = btBroadphaseProxy::AllFilter,
};

struct PhysicsSync
{
	bool quit;
	GameTime time;
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
};

struct RigidBody : public ComponentType<RigidBody>
{
	static void init();

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

	short collision_group;
	short collision_filter;
	Type type;
	float mass;
	Vec3 size;
	int mesh_id;
	ID linked_entity; // set the rigid body's user index to this. if IDNull, it's "this" entity's ID.
	Vec2 damping; // use set_damping to ensure the btBody will be updated
	
	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;

	void set_damping(float, float);
	static ID add_constraint(Constraint&);
	static void remove_constraint(ID);

	RigidBody(Type, const Vec3&, float, short, short, AssetID = AssetNull, ID = IDNull);
	~RigidBody();
	void awake();
};

}
