#include "physics.h"

Physics::Physics()
{
	// Build the broadphase
    broadphase = new btDbvtBroadphase();

    // Set up the collision configuration and dispatcher
    collision_config = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collision_config);

    // The actual physics solver
    solver = new btSequentialImpulseConstraintSolver;

    // The world.
    world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_config);
    world->setGravity(btVector3(0, -9.8, 0));
}

Physics::~Physics()
{
    delete broadphase;
    delete collision_config;
    delete dispatcher;
    delete solver;
    delete world;
}
