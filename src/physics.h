#pragma once

#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

namespace VI
{

enum CollisionGroup
{
	CollisionNothing = btBroadphaseProxy::DefaultFilter,
	CollisionWalker = 1 << 6,
	CollisionReflective = 1 << 7,
	CollisionReflectiveMask = ~btBroadphaseProxy::StaticFilter && ~CollisionReflective,
};

struct Physics
{
    static btDbvtBroadphase* broadphase;
    static btDefaultCollisionConfiguration* collision_config;
    static btCollisionDispatcher* dispatcher;
    static btSequentialImpulseConstraintSolver* solver;
    static btDiscreteDynamicsWorld* btWorld;
    static void update(Update);
};

struct RigidBody : public ComponentType<RigidBody>
{
	RigidBody(Vec3, Quat, float, btCollisionShape*);
	RigidBody(Vec3, Quat, float, btCollisionShape*, short, short);
	void init(Vec3, Quat, float);
	btCollisionShape* btShape;
	btStridingMeshInterface* btMesh;
	btRigidBody* btBody;
	void awake();
	~RigidBody();
};

}
