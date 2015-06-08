#include "physics.h"

Physics Physics::world = Physics();

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
    btWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_config);
    btWorld->setGravity(btVector3(0, -9.8, 0));
}

void Physics::exec(Update u)
{
	btWorld->stepSimulation(u.time.delta, 10);
}

Physics::~Physics()
{
    delete broadphase;
    delete collision_config;
    delete dispatcher;
    delete solver;
    delete btWorld;
}
