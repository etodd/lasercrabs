#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

struct Physics : public ExecDynamic<Update>
{
	static Physics main;
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
	RigidBody(float, btMotionState*, btCollisionShape*);
	btCollisionShape* btShape;
	btRigidBody btBody;
	void awake();
	~RigidBody();
	void set_kinematic();
};
