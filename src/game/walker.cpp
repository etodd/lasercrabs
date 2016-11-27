#include "walker.h"
#include "data/components.h"
#include "physics.h"
#include "entities.h"
#include "game.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "console.h"

namespace VI
{

Walker::Walker(r32 rot)
	: dir(),
	height(1.2f),
	support_height(0.35f),
	radius(0.35f),
	mass(1.0f),
	speed(2.5f),
	max_speed(5.0f),
	rotation(rot),
	rotation_speed(8.0f),
	target_rotation(rot),
	auto_rotate(true),
	air_control_accel(5.0f),
	enabled(true),
	accel1(10.0f),
	accel2(2.0f),
	accel_threshold(2.0f),
	deceleration(30.0f),
	obstacle_id((u32)-1),
	land(),
	net_speed_timer()
{
}

void Walker::awake()
{
	// NOTE: RigidBody must come before Walker in component_ids.cpp
	// It needs to be initialized first

	RigidBody* body;
	if (has<RigidBody>())
		body = get<RigidBody>(); // RigidBody will already be awake because it comes first in the component list
	else
		body = entity()->add<RigidBody>(RigidBody::Type::CapsuleY, Vec3(radius, height, 0), mass, CollisionWalker, ~CollisionShield);

	body->btBody->setFriction(0);
	body->btBody->setRollingFriction(0);
	body->btBody->setInvInertiaDiagLocal(btVector3(0, 0, 0)); // Prevent rotation
	body->btBody->setAngularFactor(0.0f); // Prevent rotation even harder
	body->btBody->setActivationState(DISABLE_DEACTIVATION);
}

Walker::~Walker()
{
	if (obstacle_id != (u32)-1)
		AI::obstacle_remove(obstacle_id);
}

const s32 num_corners = 5;
const Vec3 corners[num_corners] =
{
	Vec3(0.0f, 0.0f, 0.0f),
	Vec3(-1.0f, 0.0f, 0.0f),
	Vec3(1.0f, 0.0f, 0.0f),
	Vec3(0.0f, 0.0f, -1.0f),
	Vec3(0.0f, 0.0f, 1.0f),
};

// Slide against walls
b8 Walker::slide(Vec2* movement, const Vec3& wall_ray)
{
	Vec3 ray_start = get<Transform>()->absolute_pos();
	Vec3 ray_end = ray_start + wall_ray * (radius + 0.25f);
	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~CollisionWalker & ~CollisionTarget & ~CollisionShield & ~CollisionAllTeamsContainmentField);
	if (ray_callback.hasHit()
		&& Vec3(ray_callback.m_hitNormalWorld).dot(Vec3(movement->x, 0, movement->y)) < 0.0f)
	{
		Vec3 orthogonal = Vec3::normalize(ray_callback.m_hitNormalWorld.cross(wall_ray));
		Vec3 new_movement3 = ray_callback.m_hitNormalWorld.cross(orthogonal);
		Vec2 new_movement(new_movement3.x, new_movement3.z);
		new_movement.normalize();
		if (new_movement.dot(*movement) < 0)
			new_movement *= -1.0f;

		if (new_movement.dot(*movement) > 0.25f) // The new direction is similar to what we want. Go ahead.
		{
			new_movement *= movement->length();
			*movement = new_movement;
			return true;
		}
		else
			return false; // New direction is too different. Continue in the old direction.
	}
	else
		return false; // No wall. Continue in the old direction.
}

btCollisionWorld::ClosestRayResultCallback Walker::check_support(r32 extra_distance)
{
	Vec3 pos = get<Transform>()->absolute_pos();

	for (s32 i = 0; i < num_corners; i++)
	{
		Vec3 ray_start = pos + (corners[i] * (radius * 0.75f));
		Vec3 ray_end = ray_start + Vec3(0, (capsule_height() * -0.5f) + (support_height * -1.5f) - extra_distance, 0);

		btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
		Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~CollisionWalker & ~CollisionTarget & ~CollisionShield & ~CollisionAllTeamsContainmentField);
		if (ray_callback.hasHit())
			return ray_callback;
	}

	// no hits
	return btCollisionWorld::ClosestRayResultCallback(Vec3::zero, Vec3::zero);
}

void update_net_speed(const Update& u, Walker* w, const Vec3& v, const Vec3& support_velocity, const Vec3& z)
{
	r32 new_net_speed = vi_min(w->max_speed, v.dot(z) - support_velocity.dot(z));
	if (new_net_speed > w->net_speed - 0.1f)
	{
		w->net_speed_timer = 0.0f;
		if (new_net_speed > w->net_speed)
			w->net_speed = new_net_speed;
	}
	else
	{
		w->net_speed_timer += u.time.delta;
		if (w->net_speed_timer > 0.5f)
			w->net_speed = new_net_speed;
	}
}

void Walker::update(const Update& u)
{
	support = nullptr;

	Vec3 pos = get<Transform>()->absolute_pos();
	if (Game::level.local && pos.y < Game::level.min_y)
	{
		get<Health>()->kill(nullptr);
		return;
	}

	if (enabled)
	{
		btRigidBody* body = get<RigidBody>()->btBody;

		Vec3 velocity = body->getLinearVelocity();
		Vec3 support_velocity = Vec3::zero;
		Vec3 adjustment = Vec3::zero;

		btCollisionWorld::ClosestRayResultCallback ray_callback = check_support();

		if (ray_callback.hasHit())
		{
			const btRigidBody* object = (const btRigidBody*)(ray_callback.m_collisionObject);

			support_velocity = Vec3(object->getLinearVelocity())
				+ Vec3(object->getAngularVelocity()).cross(pos - Vec3(object->getCenterOfMassPosition()));

			r32 velocity_diff = velocity.y - support_velocity.y;

			Vec3 support_normal = ray_callback.m_hitNormalWorld;

			// Calculate our expected vertical velocity (if we're running up a ramp for example)
			// If our actual velocity is higher than that, we must be jumping,
			// so don't glue the player to the ground
			Vec3 velocity_flattened = velocity - support_normal * velocity.dot(support_normal);

			r32 expected_vertical_speed = velocity_flattened.y;

			if (velocity_diff < expected_vertical_speed + 0.5f)
			{
				if (velocity_diff < expected_vertical_speed - 0.5f)
					land.fire(velocity_diff - expected_vertical_speed);

				{
					r32 target_y = ray_callback.m_hitPointWorld.y() + support_height + capsule_height() * 0.5f;
					r32 height_difference = target_y - pos.y;
					const r32 max_adjustment_speed = 5.0f;
					pos.y += vi_min(vi_max(height_difference, (support_velocity.y - max_adjustment_speed) * u.time.delta), (support_velocity.y + max_adjustment_speed) * u.time.delta);
					absolute_pos(pos);
				}

				r32 normal_velocity = velocity.dot(Vec3(ray_callback.m_hitNormalWorld));
				r32 support_normal_velocity = support_velocity.dot(support_normal);
				adjustment += (support_normal_velocity - normal_velocity) * Vec3(ray_callback.m_hitNormalWorld);
				adjustment.y = vi_max(adjustment.y, 0.0f);

				b8 has_traction = support_normal.y > 0.5f;

				Vec2 movement = dir;

				r32 movement_length = vi_min(1.0f, movement.length());

				if (has_traction && !btFuzzyZero(movement_length))
				{
					// Slide against walls
					if (!slide(&movement, Vec3::normalize(Vec3(dir.x, 0, dir.y))))
					{
						r32 angle = atan2f(movement.x, movement.y);
						if (!slide(&movement, Vec3(cosf(angle + PI * 0.25f), 0, sinf(angle + PI * 0.25f))))
							slide(&movement, Vec3(cosf(angle - PI * 0.25f), 0, sinf(angle - PI * 0.25f)));
					}

					Vec3 x = Vec3::normalize(Vec3(movement.x, 0, movement.y).cross(ray_callback.m_hitNormalWorld));
					Vec3 z = Vec3::normalize(Vec3(ray_callback.m_hitNormalWorld).cross(x));

					// Remove velocity perpendicular to our desired movement
					r32 x_speed = velocity.dot(x);
					r32 support_x_speed = support_velocity.dot(x);
					r32 x_speed_change = LMath::clampf(x_speed - support_x_speed, -u.time.delta * deceleration, u.time.delta * deceleration);
					adjustment -= x_speed_change * x;

					// Calculate speed along desired movement direction
					r32 z_speed = velocity.dot(z);
					r32 support_z_speed = support_velocity.dot(z);
					r32 net_z_speed = z_speed - support_z_speed;

					// Makeshift acceleration curve
					r32 acceleration = net_speed < accel_threshold ? accel1 : accel2;

					// Increase acceleration if we're turning
					acceleration += fabs(Vec2(x.x, x.z).dot(Vec2(velocity.x - support_velocity.x, velocity.z - support_velocity.z))) * accel2 * 4.0f;

					// Increase acceleration if we're climbing
					if (z.y > 0.0f)
						acceleration += z.y * accel2 * 2.0f;

					// Don't let net_speed get above max_speed
					if (net_z_speed > movement_length * max_speed)
					{
						// Decelerate
						r32 z_speed_change = vi_min(u.time.delta * deceleration, net_z_speed - movement_length * max_speed);
						adjustment -= z_speed_change * z;
					}
					else
					{
						// Accelerate net_z_speed up to speed
						r32 target_speed = vi_max(net_speed, speed * movement_length);
						if (net_z_speed < target_speed)
						{
							r32 z_speed_change = vi_min(u.time.delta * acceleration, target_speed - net_z_speed);
							adjustment += z_speed_change * z;
							if (z.y > 0.0f)
								adjustment.y += z.y * vi_min(u.time.delta * acceleration * 4.0f, target_speed - net_z_speed) * 2.0f;
						}
					}

					if (auto_rotate)
						target_rotation = atan2f(movement.x, movement.y);

					// calculate new net speed
					update_net_speed(u, this, velocity + adjustment, support_velocity, z);
				}
				else
				{
					// Not moving or no traction; decelerate

					r32 decel;
					if (has_traction)
						decel = u.time.delta * deceleration;
					else
						decel = u.time.delta * accel2;

					// Remove from the character a portion of velocity defined by the deceleration.
					Vec3 horizontal_velocity = velocity - velocity.dot(support_normal) * support_normal;
					Vec3 support_horizontal_velocity = support_velocity - support_normal_velocity * support_normal;
					Vec3 relative_velocity = horizontal_velocity - support_horizontal_velocity;
					r32 speed = relative_velocity.length();
					if (speed > 0)
					{
						Vec3 relative_velocity_normalized = relative_velocity / speed;
						r32 velocity_change = vi_min(speed, decel);
						adjustment -= velocity_change * relative_velocity_normalized;
					}
					update_net_speed(u, this, velocity + adjustment, support_velocity, Vec3::zero);
				}

				support = Entity::list[object->getUserIndex()].get<RigidBody>();
			}
		}

		if (support.ref())
		{
			// Save last supported speed for jump calculation purposes
			Vec3 v = velocity - support_velocity;
			v.y = 0.0f;
			last_supported_speed = v.length();
		}
		else
		{
			// Air control
			Vec2 accel = dir * air_control_accel * u.time.delta;
			Vec3 accel3 = Vec3(accel.x, 0, accel.y);

			// Don't allow the walker to go faster than the speed we were going when we last hit the ground
			if (velocity.dot(accel3 / accel3.length()) < vi_max(speed * 0.25f, last_supported_speed))
				adjustment += accel3;
		}

		body->setLinearVelocity(velocity + adjustment);
	}

	if (net_speed > 0.05f && obstacle_id != (u32)-1)
	{
		AI::obstacle_remove(obstacle_id);
		obstacle_id = (u32)-1;
	}
	else if (net_speed < 0.05f && obstacle_id == (u32)-1)
		obstacle_id = AI::obstacle_add(base_pos(), radius * 2.0f, capsule_height() + support_height);

	// Handle rotation

	target_rotation = LMath::closest_angle(target_rotation, rotation);

	rotation = target_rotation > rotation ? vi_min(target_rotation, rotation + rotation_speed * u.time.delta) : vi_max(target_rotation, rotation - rotation_speed * u.time.delta);

	rotation = LMath::angle_range(rotation);
}

void Walker::absolute_pos(const Vec3& p)
{
	get<Transform>()->absolute_pos(p);
	btRigidBody* btBody = get<RigidBody>()->btBody;
	btBody->setWorldTransform(btTransform(Quat::identity, p));
	btBody->setInterpolationWorldTransform(btTransform(Quat::identity, p));
}

Vec3 Walker::base_pos() const
{
	return get<Transform>()->to_world(Vec3(0, (capsule_height() * -0.5f) - support_height, 0));
}

Vec3 Walker::forward() const
{
	return Quat::euler(0, rotation, 0) * Vec3(0, 0, 1);
}

r32 Walker::capsule_height() const
{
	return height + radius * 2.0f;
}

}
