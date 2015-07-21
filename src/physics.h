#pragma once

#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

enum CollisionGroup
{
	CollisionNothing = 0,
	CollisionWalker = 1 << 0,
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
	btRigidBody* btBody;
	void awake();
	~RigidBody();
};
