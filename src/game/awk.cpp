#include "awk.h"
#include "data/components.h"

#define speed 25.0f
#define crawl_speed 1.5f
#define awk_radius 0.05f

Awk::Awk()
	: velocity(), attached(), reattached()
{
}

void Awk::awake()
{
}

void Awk::detach(Vec3 dir)
{
	velocity = dir * speed;
	get<Transform>()->reparent(NULL);
	get<Transform>()->pos += dir * awk_radius * 0.5f;
}

void Awk::crawl(Vec3 dir, Update u)
{
	if (get<Transform>()->has_parent)
	{
		dir.normalize();

		Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);

		Vec3 dir_flattened = dir - wall_normal * wall_normal.dot(dir);
		float dir_flattened_length = dir_flattened.length();
		if (dir_flattened_length > 0.1f)
		{
			dir_flattened /= dir_flattened_length;

			Vec3 pos = get<Transform>()->pos;
			Vec3 next_pos = pos + dir_flattened * u.time.delta * crawl_speed;

			// Check for obstacles
			btCollisionWorld::ClosestRayResultCallback rayCallback(pos, next_pos);

			Physics::btWorld->rayTest(pos, next_pos, rayCallback);

			if (rayCallback.hasHit())
			{
				// Reparent to obstacle
				Quat old_quat = get<Transform>()->absolute_rot();
				Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
				get<Transform>()->parent(entity->get<Transform>());
				get<Transform>()->absolute(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius);
				(&reattached)->fire(old_quat);
			}
			else
			{
				// No obstacle. Check if we still have wall to walk on.

				Vec3 wall_ray_start = next_pos + wall_normal * awk_radius;
				Vec3 wall_ray_end = next_pos + wall_normal * awk_radius * -2.0f;

				btCollisionWorld::ClosestRayResultCallback rayCallback(wall_ray_start, wall_ray_end);

				Physics::btWorld->rayTest(wall_ray_start, wall_ray_end, rayCallback);

				if (rayCallback.hasHit())
				{
					// All good, go ahead
					get<Transform>()->absolute(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius);
					Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
					if (entity->get<Transform>() != get<Transform>()->parent())
						get<Transform>()->reparent(entity->get<Transform>());
				}
				else
				{
					// No wall left
					// See if we can walk around the corner
					Vec3 wall_ray2_end = wall_ray_end + dir_flattened * awk_radius * -2.0f;

					btCollisionWorld::ClosestRayResultCallback rayCallback(wall_ray_end, wall_ray2_end);

					Physics::btWorld->rayTest(wall_ray_end, wall_ray2_end, rayCallback);

					if (rayCallback.hasHit())
					{
						// Walk around the corner

						Vec3 other_wall_normal = Vec3(rayCallback.m_hitNormalWorld);
						Vec3 dir_flattened_other_wall = dir - other_wall_normal * other_wall_normal.dot(dir);
						// Check to make sure that our movement direction won't get flipped if we switch walls.
						// This prevents jittering back and forth between walls all the time.
						if (dir.dot(wall_normal) < 0.05f)
						{
							// Transition to the other wall
							get<Transform>()->absolute(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius);
							Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
							if (entity->get<Transform>() != get<Transform>()->parent())
								get<Transform>()->reparent(entity->get<Transform>());
						}
						else
						{
							// Stay on our current wall
							Vec3 dir_flattened_both_walls = dir_flattened - other_wall_normal * other_wall_normal.dot(dir_flattened);
							float dir_flattened_both_length = dir_flattened_both_walls.length();
							if (dir_flattened_both_length > 0.0f)
							{
								dir_flattened_both_walls /= dir_flattened_both_length;
								Vec3 next_pos = pos + dir_flattened_both_walls * u.time.delta * crawl_speed;
								Vec3 wall_ray_start = next_pos + wall_normal * awk_radius;
								Vec3 wall_ray_end = next_pos + wall_normal * awk_radius * -2.0f;

								btCollisionWorld::ClosestRayResultCallback rayCallback(wall_ray_start, wall_ray_end);

								Physics::btWorld->rayTest(wall_ray_start, wall_ray_end, rayCallback);

								if (rayCallback.hasHit())
								{
									// All good, go ahead
									get<Transform>()->absolute(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius);
									Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
									if (entity->get<Transform>() != get<Transform>()->parent())
										get<Transform>()->reparent(entity->get<Transform>());
								}
							}
						}
					}
				}
			}
		}
	}
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

			Vec3 ray_start = position - direction * awk_radius;
			btCollisionWorld::ClosestRayResultCallback rayCallback(ray_start, next_position);

			Physics::btWorld->rayTest(ray_start, next_position, rayCallback);

			if (rayCallback.hasHit())
			{
				Entity* entity = &World::list[rayCallback.m_collisionObject->getUserIndex()];
				get<Transform>()->parent(entity->get<Transform>());
				get<Transform>()->absolute(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * awk_radius);

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