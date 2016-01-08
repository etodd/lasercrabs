#include "physics.h"
#include "data/components.h"
#include "load.h"

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
	for (auto i = RigidBody::list().iterator(); !i.is_last(); i.next())
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
	for (auto i = RigidBody::list().iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isActive() && !body->isStaticOrKinematicObject())
			i.item()->get<Transform>()->set_bullet(body->getWorldTransform());
	}
}

PinArray<RigidBody::Constraint, MAX_ENTITIES> RigidBody::global_constras32s;

void RigidBody::init()
{
	pool.global(&global_constras32s);
}

RigidBody::RigidBody(Type type, const Vec3& size, r32 mass, short group, short mask, AssetID mesh_id, ID linked_entity)
	: type(type), size(size), mass(mass), collision_group(group), collision_filter(mask), linked_entity(linked_entity), btBody(), btMesh(), btShape(), mesh_id(mesh_id)
{
}

void RigidBody::awake()
{
	switch (type)
	{
		case Type::Box:
			btShape = new btBoxShape(size);
			break;
		case Type::CapsuleX:
			btShape = new btCapsuleShapeX(size.x, size.y);
			break;
		case Type::CapsuleY:
			btShape = new btCapsuleShape(size.x, size.y);
			break;
		case Type::CapsuleZ:
			btShape = new btCapsuleShapeZ(size.x, size.y);
			break;
		case Type::Sphere:
			btShape = new btSphereShape(size.x);
			break;
		case Type::Mesh:
		{
			Mesh* mesh = Loader::mesh(mesh_id);
			btMesh = new btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(s32), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));
			btShape = new btBvhTriangleMeshShape(btMesh, true, mesh->bounds_min, mesh->bounds_max);
			break;
		}
		default:
			vi_assert(false);
			break;
	}

	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0f)
		btShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo info(mass, 0, btShape, localInertia);

	Quat quat;
	Vec3 pos;
	get<Transform>()->absolute(&pos, &quat);

	info.m_startWorldTransform = btTransform(quat, pos);
	btBody = new btRigidBody(info);
	btBody->setWorldTransform(btTransform(quat, pos));

	if (mass == 0.0f)
		btBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);

	btBody->setUserIndex(linked_entity == IDNull ? entity()->id() : linked_entity);
	btBody->setDamping(damping.x, damping.y);

	Physics::btWorld->addRigidBody(btBody, collision_group, collision_filter);
}

void RigidBody::set_damping(r32 linear, r32 angular)
{
	damping = Vec2(linear, angular);
	if (btBody)
		btBody->setDamping(linear, angular);
}

ID RigidBody::add_constras32(Constraint& constras32)
{
	switch (constras32.type)
	{
		case Constraint::Type::ConeTwist:
			constras32.btPointer = new btConeTwistConstraint
			(
				*constras32.a.ref()->btBody,
				*constras32.b.ref()->btBody,
				constras32.frame_a,
				constras32.frame_b
			);
			((btConeTwistConstraint*)constras32.btPointer)->setLimit(constras32.limits.x, constras32.limits.y, constras32.limits.z);
			break;
		case Constraint::Type::PointToPoint:
			constras32.btPointer = new btPoint2PointConstraint
			(
				*constras32.a.ref()->btBody,
				*constras32.b.ref()->btBody,
				constras32.frame_a.getOrigin(),
				constras32.frame_b.getOrigin()
			);
			break;
		default:
			vi_assert(false);
			break;
	}

	s32 constras32_id = global_constras32s.add(constras32);

	constras32.btPointer->setUserConstraintId(constras32_id);

	Physics::btWorld->addConstraint(constras32.btPointer);

	return constras32_id;
}

void RigidBody::remove_constras32(ID id)
{
	Constraint* constras32 = &global_constras32s[id];

	constras32->a.ref()->btBody->activate(true);
	constras32->b.ref()->btBody->activate(true);

	Physics::btWorld->removeConstraint(constras32->btPointer);

	delete constras32->btPointer;

	global_constras32s.remove(id);
}

RigidBody::~RigidBody()
{
	ID me = id();
	for (auto i = global_constras32s.iterator(); !i.is_last(); i.next())
	{
		Constraint* constras32 = i.item();
		if (constras32->a.id == me || constras32->b.id == me)
			remove_constras32(i.index);
	}
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
	if (btMesh)
		delete btMesh;
}

}
