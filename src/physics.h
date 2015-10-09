#pragma once

#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"
#include "lmath.h"
#include "sync.h"

namespace VI
{

enum CollisionGroup
{
	CollisionNothing = btBroadphaseProxy::DefaultFilter,
	CollisionWalker = 1 << 6,
	CollisionReflective = 1 << 7,
	CollisionTarget = 1 << 8,
	CollisionReflectiveMask = ~btBroadphaseProxy::StaticFilter & ~CollisionReflective,
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
	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;

	RigidBody(Vec3, Quat, float, btCollisionShape*);
	RigidBody(Vec3, Quat, float, btCollisionShape*, short, short, ID = -1);
	~RigidBody();
	void init(Vec3, Quat, float);
	void awake();
};

}
