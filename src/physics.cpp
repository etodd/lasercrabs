#include "physics.h"

btDbvtBroadphase* Physics::broadphase = new btDbvtBroadphase();
btDefaultCollisionConfiguration* Physics::collision_config = new btDefaultCollisionConfiguration();
btCollisionDispatcher* Physics::dispatcher = new btCollisionDispatcher(Physics::collision_config);
btSequentialImpulseConstraintSolver* Physics::solver = new btSequentialImpulseConstraintSolver;
btDiscreteDynamicsWorld* Physics::btWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_config);

void Physics::update(Update u)
{
	btWorld->stepSimulation(u.time.delta, 10);
}

RigidBody::RigidBody(float mass, btMotionState* motion_state, btCollisionShape* shape)
	: btShape(shape), btBody(0.0f, motion_state, shape, btVector3(0, 0, 0))
{
}

void RigidBody::awake()
{
	Physics::btWorld->addRigidBody(&btBody);
	btBody.setUserIndex(entity_id);
}

RigidBody::~RigidBody()
{
	Physics::btWorld->removeRigidBody(&btBody);
	delete btShape;
	btBody.~btRigidBody();
}

void RigidBody::set_kinematic()
{
	btBody.setCollisionFlags(btBody.getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	btBody.setActivationState(DISABLE_DEACTIVATION);
}
