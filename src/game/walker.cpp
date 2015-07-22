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
	Vec3 pos = get<Transform>()->pos;
	Vec3 ray_start = pos + Vec3(0, height * 0.25f, 0);
	Vec3 ray_end = pos + Vec3(0, (height * -0.5f) + (support_height * -2.0f), 0);

	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	ray_callback.m_collisionFilterMask = ~CollisionWalker;

	Physics::btWorld->rayTest(ray_start, ray_end, ray_callback);

	if (ray_callback.hasHit())
	{
		const btRigidBody* object = dynamic_cast<const btRigidBody*>(ray_callback.m_collisionObject);
		Vec3 support_velocity;
		if (object)
		{
			support_velocity = Vec3(object->getLinearVelocity())
				+ Vec3(object->getAngularVelocity()).cross(pos - Vec3(object->getCenterOfMassPosition()));
		}
		else
			support_velocity = Vec3(0, 0, 0);
		float target_y = ray_callback.m_hitPointWorld.y() + support_height + height * 0.5f;
		float height_difference = target_y - pos.y;
		get<RigidBody>()->btBody->translate(btVector3(0, fmin(fmax(height_difference, (support_velocity.y - 5.0f) * u.time.delta), (support_velocity.y + 5.0f) * u.time.delta), 0));

		Vec3 velocity = get<RigidBody>()->btBody->getLinearVelocity();
		float normal_velocity = velocity.dot(Vec3(ray_callback.m_hitNormalWorld));
		float support_normal_velocity = support_velocity.dot(Vec3(ray_callback.m_hitNormalWorld));
		Vec3 diff = (support_normal_velocity - normal_velocity) * Vec3(ray_callback.m_hitNormalWorld);
		diff.y = fmax(diff.y, 0);
		get<RigidBody>()->btBody->setLinearVelocity(velocity + diff);
	}
}