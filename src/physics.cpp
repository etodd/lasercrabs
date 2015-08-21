#include "physics.h"
#include "data/components.h"

namespace VI
{

btDbvtBroadphase* Physics::broadphase = new btDbvtBroadphase();
btDefaultCollisionConfiguration* Physics::collision_config = new btDefaultCollisionConfiguration();
btCollisionDispatcher* Physics::dispatcher = new btCollisionDispatcher(Physics::collision_config);
btSequentialImpulseConstraintSolver* Physics::solver = new btSequentialImpulseConstraintSolver;
btDiscreteDynamicsWorld* Physics::btWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_config);

void Physics::update(Update u)
{
	btWorld->stepSimulation(u.time.delta > 0.1f ? 0.1f : u.time.delta, 0);
	sync();
}

void Physics::sync()
{
	for (auto i = World::components<RigidBody>().iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isActive())
		{
			if (body->isStaticOrKinematicObject())
			{
				btTransform transform;
				i.item()->get<Transform>()->get_bullet(transform);
				body->setWorldTransform(transform);
			}
			else
				i.item()->get<Transform>()->set_bullet(body->getWorldTransform());
		}
	}
}

void RigidBody::init(Vec3 pos, Quat quat, float mass)
{
	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0f)
		btShape->calculateLocalInertia(mass, localInertia);
	btBody = new btRigidBody(mass, 0, btShape, localInertia);
	btBody->setWorldTransform(btTransform(quat, pos));

	if (mass == 0.0f)
	{
		btBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
		btBody->setActivationState(DISABLE_DEACTIVATION);
	}
}

RigidBody::RigidBody(Vec3 pos, Quat quat, float mass, btCollisionShape* shape)
	: btShape(shape)
{
	init(pos, quat, mass);
	Physics::btWorld->addRigidBody(btBody);
}

RigidBody::RigidBody(Vec3 pos, Quat quat, float mass, btCollisionShape* shape, short group, short mask)
	: btShape(shape)
{
	init(pos, quat, mass);
	Physics::btWorld->addRigidBody(btBody, group, mask);
}

void RigidBody::awake()
{
	btBody->setUserIndex(entity_id);
}

RigidBody::~RigidBody()
{
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
	if (btMesh)
		delete btMesh;
}

}
