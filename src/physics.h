#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>
#include "data/entity.h"

struct Physics
{
	static Physics main;
	Physics();
	~Physics();
    btBroadphaseInterface* broadphase;
    btDefaultCollisionConfiguration* collision_config;
    btCollisionDispatcher* dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* btWorld;
    void update(Update);
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
