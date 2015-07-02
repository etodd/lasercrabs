#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

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
	RigidBody(float, btMotionState*, btCollisionShape*);
	btCollisionShape* btShape;
	btRigidBody btBody;
	void awake();
	~RigidBody();
	void set_kinematic();
};
