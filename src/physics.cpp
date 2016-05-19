#include "physics.h"
#include "data/components.h"
#include "load.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

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
		btWorld->stepSimulation(vi_min(data->time.delta, 0.1f), 2, data->timestep);
		data = swapper->swap<SwapType_Read>();
	}
}

void Physics::sync_static()
{
	for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isStaticOrKinematicObject())
		{
			btTransform transform;
			i.item()->get<Transform>()->get_bullet(transform);
			body->setWorldTransform(transform);
		}
	}
}

void Physics::sync_dynamic()
{
	for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
	{
		btRigidBody* body = i.item()->btBody;
		if (body->isActive() && !body->isStaticOrKinematicObject())
			i.item()->get<Transform>()->set_bullet(body->getInterpolationWorldTransform());
	}
}

RaycastCallbackExcept::RaycastCallbackExcept(const Vec3& a, const Vec3& b, const Entity* entity)
	: btCollisionWorld::ClosestRayResultCallback(a, b)
{
	entity_id = entity->id();
}

btScalar RaycastCallbackExcept::addSingleResult(btCollisionWorld::LocalRayResult& rayResult, b8 normalInWorldSpace)
{
	if (rayResult.m_collisionObject->getUserIndex() == entity_id)
		return m_closestHitFraction; // ignore

	m_closestHitFraction = rayResult.m_hitFraction;
	m_collisionObject = rayResult.m_collisionObject;
	if (normalInWorldSpace)
		m_hitNormalWorld = rayResult.m_hitNormalLocal;
	else
	{
		///need to transform normal into worldspace
		m_hitNormalWorld = m_collisionObject->getWorldTransform().getBasis() * rayResult.m_hitNormalLocal;
	}
	m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, rayResult.m_hitFraction);
	return rayResult.m_hitFraction;
}

void Physics::raycast(btCollisionWorld::ClosestRayResultCallback* ray_callback, s16 mask)
{
	ray_callback->m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
		| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	ray_callback->m_collisionFilterMask = ray_callback->m_collisionFilterGroup = mask;
	Physics::btWorld->rayTest(ray_callback->m_rayFromWorld, ray_callback->m_rayToWorld, *ray_callback);
}

PinArray<RigidBody::Constraint, MAX_ENTITIES> RigidBody::global_constraints;

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
			const Mesh* mesh = Loader::mesh(mesh_id);
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

void RigidBody::rebuild()
{
	ID me = id();

	// delete constraints but leave them in the global array so we can rebuild them
	for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
	{
		Constraint* constraint = i.item();
		if (constraint->a.id == me || constraint->b.id == me)
		{
			Physics::btWorld->removeConstraint(constraint->btPointer);
			delete constraint->btPointer;
		}
	}

	// delete body
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
	if (btMesh)
		delete btMesh;
	btMesh = nullptr;

	awake(); // rebuild body

	// rebuild constraints
	for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
	{
		Constraint* constraint = i.item();
		if (constraint->a.id == me || constraint->b.id == me)
			instantiate_constraint(constraint);
	}
}

void RigidBody::set_damping(r32 linear, r32 angular)
{
	damping = Vec2(linear, angular);
	if (btBody)
		btBody->setDamping(linear, angular);
}

void RigidBody::instantiate_constraint(Constraint* constraint)
{
	switch (constraint->type)
	{
		case Constraint::Type::ConeTwist:
			constraint->btPointer = new btConeTwistConstraint
			(
				*constraint->a.ref()->btBody,
				*constraint->b.ref()->btBody,
				constraint->frame_a,
				constraint->frame_b
			);
			((btConeTwistConstraint*)constraint->btPointer)->setLimit(constraint->limits.x, constraint->limits.y, constraint->limits.z);
			break;
		case Constraint::Type::PointToPoint:
			constraint->btPointer = new btPoint2PointConstraint
			(
				*constraint->a.ref()->btBody,
				*constraint->b.ref()->btBody,
				constraint->frame_a.getOrigin(),
				constraint->frame_b.getOrigin()
			);
			break;
		default:
			vi_assert(false);
			break;
	}
}

ID RigidBody::add_constraint(Constraint& constraint)
{
	instantiate_constraint(&constraint);

	s32 constraint_id = global_constraints.add(constraint);

	constraint.btPointer->setUserConstraintId(constraint_id);

	Physics::btWorld->addConstraint(constraint.btPointer);

	return constraint_id;
}

void RigidBody::remove_constraint(ID id)
{
	Constraint* constraint = &global_constraints[id];

	constraint->a.ref()->btBody->activate(true);
	constraint->b.ref()->btBody->activate(true);

	Physics::btWorld->removeConstraint(constraint->btPointer);

	delete constraint->btPointer;

	global_constraints.remove(id);
}

RigidBody::~RigidBody()
{
	ID me = id();
	for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
	{
		Constraint* constraint = i.item();
		if (constraint->a.id == me || constraint->b.id == me)
			remove_constraint(i.index);
	}
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
	if (btMesh)
		delete btMesh;
}

}
