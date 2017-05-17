#include "physics.h"
#include "data/components.h"
#include "load.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "game/game.h"

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
		btWorld->stepSimulation(vi_min(data->time.delta, 0.1f), 3, data->timestep);
		data = swapper->swap<SwapType_Read>();
	}
}

void Physics::sync_static()
{
	for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
	{
#if SERVER
		if (!(i.item()->flags & RigidBody::FlagGhost))
#endif
		{
			btRigidBody* body = i.item()->btBody;
			if (body->isStaticOrKinematicObject())
			{
				btTransform transform;
				i.item()->get<Transform>()->get_bullet(transform);
				body->setWorldTransform(transform);
				body->setInterpolationWorldTransform(transform);
			}
		}
	}
}

void Physics::sync_dynamic()
{
	for (auto i = RigidBody::list.iterator(); !i.is_last(); i.next())
	{
#if SERVER
		if (!(i.item()->flags & RigidBody::FlagGhost))
#endif
		{
			btRigidBody* body = i.item()->btBody;
			if (body->isActive() && !body->isStaticOrKinematicObject())
				i.item()->get<Transform>()->set_bullet(body->getInterpolationWorldTransform());
		}
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
	ray_callback->m_collisionFilterMask = mask;
	ray_callback->m_collisionFilterGroup = -1;
	Physics::btWorld->rayTest(ray_callback->m_rayFromWorld, ray_callback->m_rayToWorld, *ray_callback);
}

PinArray<RigidBody::Constraint, MAX_ENTITIES> RigidBody::global_constraints;

RigidBody::RigidBody(Type type, const Vec3& size, r32 mass, s16 group, s16 mask, AssetID mesh_id, s8 flags)
	: type(type),
	size(size),
	mass(mass),
	collision_group(group),
	collision_filter(mask),
	btBody(),
	btMesh(),
	btShape(),
	mesh_id(mesh_id),
	restitution(),
	flags(flags)
{
}

RigidBody::RigidBody()
	: type(),
	size(),
	mass(),
	collision_group(),
	collision_filter(),
	btBody(),
	btMesh(),
	btShape(),
	mesh_id(IDNull),
	restitution(),
	flags()
{
}

void RigidBody::awake()
{
	if (btBody) // already initialized
		return;

	// rigid bodies controlled by the server appear as kinematic bodies to the client
	short actual_collision_filter;
	r32 m;
#if SERVER
	if (flags & FlagGhost)
	{
		m = 0.0f;
		actual_collision_filter = 0;
	}
	else
#endif
	if (!Game::level.local && Game::net_transform_filter(entity(), Game::level.mode))
	{
		m = 0.0f;
		actual_collision_filter = collision_filter // prevent static-static collisions
		& ~(
			CollisionStatic
			| CollisionInaccessible
			| CollisionAllTeamsForceField
			| CollisionParkour
			| CollisionElectric
		);
	}
	else
	{
		m = mass;
		actual_collision_filter = collision_filter;
	}

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
	if (m > 0.0f)
		btShape->calculateLocalInertia(m, localInertia);

	btRigidBody::btRigidBodyConstructionInfo info(m, 0, btShape, localInertia);

	Quat quat;
	Vec3 pos;
	get<Transform>()->absolute(&pos, &quat);

	info.m_startWorldTransform = btTransform(quat, pos);
	btBody = new btRigidBody(info);
	btBody->setWorldTransform(btTransform(quat, pos));

	if (m == 0.0f)
		btBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);

	btBody->setUserIndex(entity_id);
	btBody->setRestitution(restitution);
	btBody->setDamping(damping.x, damping.y);
	set_ccd(flags & FlagContinuousCollisionDetection);

	Physics::btWorld->addRigidBody(btBody, collision_group, actual_collision_filter);

	// rebuild constraints

#if SERVER
	if (!(flags & FlagGhost))
#endif
	{
		for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
		{
			Constraint* constraint = i.item();
			if (!constraint->btPointer
				&& (constraint->a.ref() == this || constraint->b.ref() == this)
				&& (constraint->a.ref() && constraint->b.ref())
				&& (constraint->a.ref()->btBody && constraint->b.ref()->btBody))
			{
				instantiate_constraint(constraint, i.index);
			}
		}
	}
}

RigidBody::~RigidBody()
{
	remove_all_constraints();
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
	if (btMesh)
		delete btMesh;
}

void RigidBody::set_restitution(r32 r)
{
	restitution = r;
	if (btBody)
		btBody->setRestitution(restitution);
}

void RigidBody::set_ccd(b8 c)
{
	if (c)
		flags |= FlagContinuousCollisionDetection;
	else
		flags &= ~FlagContinuousCollisionDetection;
	if (btBody)
	{
		if (c)
		{
			r32 min_radius = FLT_MAX;
			if (size.x > 0.0f)
				min_radius = vi_min(min_radius, size.x);
			if (size.y > 0.0f)
				min_radius = vi_min(min_radius, size.y);
			if (size.z > 0.0f)
				min_radius = vi_min(min_radius, size.z);
			btBody->setCcdMotionThreshold(min_radius);
			btBody->setCcdSweptSphereRadius(min_radius * 0.5f);
		}
		else
		{
			btBody->setCcdMotionThreshold(0.0f);
			btBody->setCcdSweptSphereRadius(0.0f);
		}
	}
}

void RigidBody::set_ghost(b8 g)
{
	if (g != b8(flags & FlagGhost))
	{
		if (g)
			flags |= FlagGhost;
		else
			flags &= ~FlagGhost;

		rebuild();
	}
}

void RigidBody::set_collision_masks(s16 group, s16 filter)
{
	collision_group = group;
	collision_filter = filter;
	if (btBody)
	{
		Physics::btWorld->removeRigidBody(btBody);
		Physics::btWorld->addRigidBody(btBody, group, filter);
	}
}

void RigidBody::rebuild()
{
	Vec3 velocity_angular = Vec3::zero;
	Vec3 velocity_linear = Vec3::zero;
	if (btBody)
	{
		velocity_angular = btBody->getAngularVelocity();
		velocity_linear = btBody->getLinearVelocity();
	}
	// delete constraints but leave them in the global array so we can rebuild them
	for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
	{
		Constraint* constraint = i.item();
		if ((constraint->a.ref() == this || constraint->b.ref() == this) && constraint->btPointer)
		{
			Physics::btWorld->removeConstraint(constraint->btPointer);
			delete constraint->btPointer;
			constraint->btPointer = nullptr;
		}
	}

	// delete body
	if (btBody)
	{
		Physics::btWorld->removeRigidBody(btBody);
		delete btBody;
		delete btShape;
		if (btMesh)
			delete btMesh;
		btMesh = nullptr;
		btBody = nullptr;
		btShape = nullptr;
	}

	awake(); // rebuild body
	btBody->setLinearVelocity(velocity_linear);
	btBody->setInterpolationLinearVelocity(velocity_linear);
	btBody->setAngularVelocity(velocity_angular);
	btBody->setInterpolationAngularVelocity(velocity_angular);
}

void RigidBody::set_damping(r32 linear, r32 angular)
{
	damping = Vec2(linear, angular);
	if (btBody)
		btBody->setDamping(linear, angular);
}

void RigidBody::instantiate_constraint(Constraint* constraint, ID id)
{
	vi_assert(!constraint->btPointer);

	constraint->a.ref()->flags |= FlagHasConstraints;
	constraint->b.ref()->flags |= FlagHasConstraints;

#if SERVER
	if (!(constraint->a.ref()->flags & FlagGhost)
		&& !(constraint->b.ref()->flags & FlagGhost))
#endif
	{
		switch (constraint->type)
		{
			case Constraint::Type::ConeTwist:
			{
				constraint->btPointer = new btConeTwistConstraint
				(
					*constraint->a.ref()->btBody,
					*constraint->b.ref()->btBody,
					constraint->frame_a,
					constraint->frame_b
				);
				((btConeTwistConstraint*)constraint->btPointer)->setLimit(constraint->limits.x, constraint->limits.y, constraint->limits.z);
				break;
			}
			case Constraint::Type::PointToPoint:
			{
				constraint->btPointer = new btPoint2PointConstraint
				(
					*constraint->a.ref()->btBody,
					*constraint->b.ref()->btBody,
					constraint->frame_a.getOrigin(),
					constraint->frame_b.getOrigin()
				);
				break;
			}
			case Constraint::Type::Fixed:
			{
				constraint->btPointer = new btFixedConstraint
				(
					*constraint->a.ref()->btBody,
					*constraint->b.ref()->btBody,
					constraint->frame_a,
					constraint->frame_b
				);
				break;
			}
			default:
				vi_assert(false);
				break;
		}

		constraint->btPointer->setUserConstraintId(id);

		Physics::btWorld->addConstraint(constraint->btPointer);
	}
}

void RigidBody::rebuild_constraint(ID id)
{
	Constraint* constraint = &global_constraints[id];

	constraint->a.ref()->btBody->activate(true);
	constraint->b.ref()->btBody->activate(true);

	if (constraint->btPointer)
	{
		Physics::btWorld->removeConstraint(constraint->btPointer);

		delete constraint->btPointer;
		constraint->btPointer = nullptr;
	}

	instantiate_constraint(constraint, id);
}

ID RigidBody::add_constraint(const Constraint& constraint)
{
	s32 constraint_id = global_constraints.add(constraint);

	instantiate_constraint(&global_constraints[constraint_id], constraint_id);

	return constraint_id;
}

RigidBody::Constraint* RigidBody::net_add_constraint()
{
	return global_constraints.add();
}

void RigidBody::remove_constraint(ID id)
{
	Constraint* constraint = &global_constraints[id];

	if (constraint->btPointer)
	{
		constraint->a.ref()->btBody->activate(true);
		constraint->b.ref()->btBody->activate(true);

		Physics::btWorld->removeConstraint(constraint->btPointer);

		delete constraint->btPointer;
		constraint->btPointer = nullptr;
	}

	global_constraints.remove(id);
}

void RigidBody::activate_linked()
{
	btBody->activate(true);
	if (flags & FlagHasConstraints)
	{
		for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
		{
			Constraint* constraint = i.item();
			if (constraint->a.ref() == this)
				constraint->b.ref()->btBody->activate(true);
			else if (constraint->b.ref() == this)
				constraint->a.ref()->btBody->activate(true);
		}
	}
}

void RigidBody::remove_all_constraints()
{
	if (flags & FlagHasConstraints)
	{
		for (auto i = global_constraints.iterator(); !i.is_last(); i.next())
		{
			Constraint* constraint = i.item();
			if (constraint->a.ref() == this || constraint->b.ref() == this)
				remove_constraint(i.index);
		}
		flags &= ~FlagHasConstraints;
	}
}


}
