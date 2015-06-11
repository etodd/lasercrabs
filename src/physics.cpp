#include "physics.h"

Physics Physics::main = Physics();

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
    btWorld->setGravity(btVector3(0, -9.8f, 0));
}

void Physics::exec(Update u)
{
	btWorld->stepSimulation(u.time.delta, 10);
}

Physics::~Physics()
{
	/*
    delete broadphase;
    delete collision_config;
    delete dispatcher;
    delete solver;
    delete btWorld;
	*/
}

RigidBody::RigidBody(float mass, btMotionState* motion_state, btCollisionShape* shape)
	: btShape(shape), btBody(0.0f, motion_state, shape, btVector3(0, 0, 0))
{
}

void RigidBody::awake()
{
	Physics::main.btWorld->addRigidBody(&btBody);
	btBody.setUserIndex(entity_id);
}

RigidBody::~RigidBody()
{
	Physics::main.btWorld->removeRigidBody(&btBody);
	delete btShape;
	btBody.~btRigidBody();
}

void RigidBody::set_kinematic()
{
	btBody.setCollisionFlags(btBody.getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	btBody.setActivationState(DISABLE_DEACTIVATION);
}
