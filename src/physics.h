#pragma once

#include <btBulletDynamicsCommon.h>

class Physics
{
public:
	Physics();
	~Physics();
    btBroadphaseInterface* broadphase;
    btDefaultCollisionConfiguration* collision_config;
    btCollisionDispatcher* dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* world;
};
