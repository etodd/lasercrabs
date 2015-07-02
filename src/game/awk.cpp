#include "awk.h"
#include "data/components.h"

#define speed 15.0f
#define awk_radius 0.05f

Awk::Awk()
	: velocity()
{
}

void Awk::awake()
{
}

void Awk::detach(Vec3 direction)
{
	velocity = direction * speed;
	get<Transform>()->reparent(0);
	get<Transform>()->pos += direction * awk_radius * 0.5f;
}

void Awk::update(Update u)
{
	if (!get<Transform>()->has_parent)
	{
		velocity.y -= u.time.delta * 9.8f;

		Vec3 position = get<Transform>()->pos;
		Vec3 next_position = position + velocity * u.time.delta;

		if (!btVector3(velocity).fuzzyZero())
		{
			Vec3 direction = Vec3::normalize(velocity);

			btCollisionWorld::ClosestRayResultCallback rayCallback(position, next_position + direction * awk_radius);

			Physics::btWorld->rayTest(position, next_position, rayCallback);

			if (rayCallback.hasHit())
			{
				Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
				get<Transform>()->pos = rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius;
				get<Transform>()->rot = Quat::look(rayCallback.m_hitNormalWorld);
				get<Transform>()->reparent(entity->get<Transform>());

				(&attached)->fire();

				velocity = Vec3::zero;
			}
			else
				get<Transform>()->pos = next_position;
		}
		else
			get<Transform>()->pos = next_position;
	}
}