#include "physics.h"
#include "data/components.h"

namespace VI
{

btDbvtBroadphase* Physics::broadphase = new btDbvtBroadphase();
btDefaultCollisionConfiguration* Physics::collision_config = new btDefaultCollisionConfiguration();
btCollisionDispatcher* Physics::dispatcher = new btCollisionDispatcher(Physics::collision_config);
btSequentialImpulseConstraintSolver* Physics::solver = new btSequentialImpulseConstraintSolver;
btDiscreteDynamicsWorld* Physics::btWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_config);

void Physics::loop(PhysicsSwapper* swapper)
{
	PhysicsSync* data = swapper->swap<SwapType_Read>();
	while (!data->quit)
	{
		btWorld->stepSimulation(data->time.delta > 0.1f ? 0.1f : data->time.delta, 0);
		data = swapper->swap<SwapType_Read>();
	}
}

void Physics::sync_static()
{
	for (auto i = World::components<RigidBody>().iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isStaticOrKinematicObject())
		{
			btTransform transform;
			i.item()->get<Transform>()->get_bullet(transform);
			btTransform existing_transform = body->getWorldTransform();
			body->setWorldTransform(transform);
		}
	}
}

void Physics::sync_dynamic()
{
	for (auto i = World::components<RigidBody>().iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isActive() && !body->isStaticOrKinematicObject())
			i.item()->get<Transform>()->set_bullet(body->getWorldTransform());
	}
}

void RigidBody::init(Vec3 pos, Quat quat, float mass)
{
	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0f)
		btShape->calculateLocalInertia(mass, localInertia);
	btRigidBody::btRigidBodyConstructionInfo info(mass, 0, btShape, localInertia);
	info.m_startWorldTransform = btTransform(quat, pos);
	btBody = new btRigidBody(info);
	btBody->setWorldTransform(btTransform(quat, pos));

	if (mass == 0.0f)
		btBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
}

RigidBody::RigidBody(Vec3 pos, Quat quat, float mass, btCollisionShape* shape)
	: btShape(shape), btMesh()
{
	init(pos, quat, mass);
	Physics::btWorld->addRigidBody(btBody);
}

RigidBody::RigidBody(Vec3 pos, Quat quat, float mass, btCollisionShape* shape, short group, short mask, ID linked_entity)
	: btShape(shape), btMesh()
{
	init(pos, quat, mass);
	Physics::btWorld->addRigidBody(btBody, group, mask);
	btBody->setUserIndex(linked_entity);
}

void RigidBody::awake()
{
	if (btBody->getUserIndex() < 0)
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
