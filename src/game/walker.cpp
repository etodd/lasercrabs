#include "walker.h"
#include "data/components.h"
#include "physics.h"

Walker::Walker(float height, float support_height, float radius, float mass)
	: velocity(), height(height), support_height(support_height), radius(radius), mass(mass)
{
}

void Walker::awake()
{
	RigidBody* body = entity()->add<RigidBody>(get<Transform>()->pos, get<Transform>()->rot, mass, new btCapsuleShape(radius, height), CollisionWalker, short(btBroadphaseProxy::AllFilter));
	body->btBody->setMassProps(mass, btVector3(0, 0, 0)); // Prevent rotation
}

void Walker::update(Update u)
{
	Vec3 ray_start = get<Transform>()->pos + Vec3(0, height * 0.25f, 0);
	Vec3 ray_end = get<Transform>()->pos + Vec3(0, (height * -0.5f) + (support_height * -2.0f), 0);

	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	ray_callback.m_collisionFilterMask = ~CollisionWalker;

	Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

	if (ray_callback.hasHit())
	{
		float target_y = ray_callback.m_hitPointWorld.y() + support_height + height * 0.5f;
		get<RigidBody>()->btBody->translate(btVector3(0, target_y - get<Transform>()->pos.y, 0));
		get<RigidBody>()->btBody->setLinearVelocity(btVector3(0, 0, 0));
	}
}