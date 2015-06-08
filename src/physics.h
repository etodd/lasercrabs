#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>

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
