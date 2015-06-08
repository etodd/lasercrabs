#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

struct Physics : public ExecDynamic<Update>
{
	static Physics world;
	Physics();
	~Physics();
    btBroadphaseInterface* broadphase;
    btDefaultCollisionConfiguration* collision_config;
    btCollisionDispatcher* dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* btWorld;
    void exec(Update);
};

struct RigidBody : public ComponentType<RigidBody>
{
	RigidBody(btCollisionShape* shape, btRigidBody::btRigidBodyConstructionInfo);
	btCollisionShape* btShape;
	btRigidBody btBody;
	void awake();
	~RigidBody();
	void set_kinematic();
};
