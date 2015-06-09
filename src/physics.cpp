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

RigidBody::RigidBody(btCollisionShape* shape, btRigidBody::btRigidBodyConstructionInfo cInfo)
	: btShape(shape), btBody(cInfo)
{

}

void RigidBody::awake()
{
	Physics::world.btWorld->addRigidBody(&btBody);
	btBody.setUserPointer(entity);
}

RigidBody::~RigidBody()
{
	delete btShape;
	btBody.~btRigidBody();
	Physics::world.btWorld->removeRigidBody(&btBody);
}

void RigidBody::set_kinematic()
{
	btBody.setCollisionFlags(btBody.getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	btBody.setActivationState(DISABLE_DEACTIVATION);
}
