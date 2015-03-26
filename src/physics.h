#pragma once

#include "exec.h"
#include "types.h"

#include <btBulletDynamicsCommon.h>

struct Physics : public ExecDynamic<GameTime>
{
	Physics();
	~Physics();
    btBroadphaseInterface* broadphase;
    btDefaultCollisionConfiguration* collision_config;
    btCollisionDispatcher* dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* world;
    void exec(GameTime);
};
