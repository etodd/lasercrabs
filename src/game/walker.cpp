#include "walker.h"
#include "data/components.h"
#include "physics.h"
#include "entities.h"
#include "game.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "console.h"
#include "team.h"

namespace VI
{

#define ACCEL1 15.0f
#define ACCEL_THRESHOLD 3.0f
#define ACCEL2 2.0f
#define DECELERATION 30.0f

void walker_set_rigid_body_props(btRigidBody* btBody)
{
	btBody->setFriction(0);
	btBody->setRollingFriction(0);
	btBody->setInvInertiaDiagLocal(btVector3(0, 0, 0)); // Prevent rotation
	btBody->setAngularFactor(0.0f); // Prevent rotation even harder
	btBody->setActivationState(DISABLE_DEACTIVATION);
}

Walker::Walker(r32 rot)
	: dir(),
	speed(2.5f),
	max_speed(5.0f),
	rotation(rot),
	target_rotation(rot),
	rotation_speed(8.0f),
	auto_rotate(true),
	enabled(true),
	land(),
	net_speed_timer(),
	net_speed()
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
		body = entity()->add<RigidBody>(RigidBody::Type::CapsuleY, Vec3(WALKER_RADIUS, WALKER_HEIGHT, 0), 1.5f, CollisionWalker, ~Team::force_field_mask(get<AIAgent>()->team));
	walker_set_rigid_body_props(body->btBody);
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

// slide against walls
b8 Walker::slide(Vec2* movement, const Vec3& wall_ray)
{
	Vec3 ray_start = get<Transform>()->absolute_pos();
	Vec3 ray_end = ray_start + wall_ray * (WALKER_RADIUS + 0.25f);
	btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
	Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionWalker & ~CollisionTarget & ~Team::force_field_mask(get<AIAgent>()->team));
	if (ray_callback.hasHit()
		&& Vec3(ray_callback.m_hitNormalWorld).dot(Vec3(movement->x, 0, movement->y)) < 0.0f)
	{
		Vec3 orthogonal = Vec3::normalize(ray_callback.m_hitNormalWorld.cross(wall_ray));
		Vec3 new_movement3 = ray_callback.m_hitNormalWorld.cross(orthogonal);
		Vec2 new_movement(new_movement3.x, new_movement3.z);
		new_movement.normalize();
		r32 dot = new_movement.dot(*movement);
		if (dot < 0)
		{
			new_movement *= -1.0f;
			dot *= -1.0f;
		}

		if (dot > 0.25f) // the new direction is similar to what we want. go ahead.
		{
			new_movement *= movement->length();
			*movement = new_movement;
			return true;
		}
		else
			return false; // new direction is too different. continue in the old direction.
	}
	else
		return false; // no wall. continue in the old direction.
}

const btRigidBody* get_actual_support_body(const btRigidBody* object)
{
	Entity* e = &Entity::list[object->getUserIndex()];
	Transform* parent = e->get<Transform>()->parent.ref();
	if (parent && parent->has<RigidBody>())
		return parent->get<RigidBody>()->btBody;
	else
		return object;
}

btCollisionWorld::ClosestRayResultCallback Walker::check_support(r32 extra_distance) const
{
	Vec3 pos = get<Transform>()->absolute_pos();

	for (s32 i = 0; i < num_corners; i++)
	{
		Vec3 ray_start = pos + (corners[i] * (WALKER_RADIUS * 0.75f));
		Vec3 ray_end = ray_start + Vec3(0, (capsule_height() * -0.5f) + (WALKER_SUPPORT_HEIGHT * -1.5f) - extra_distance, 0);

		btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
		Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionWalker & ~CollisionTarget & ~Team::force_field_mask(get<AIAgent>()->team));
		if (ray_callback.hasHit())
		{
			ray_callback.m_collisionObject = get_actual_support_body((const btRigidBody*)(ray_callback.m_collisionObject));
			return ray_callback;
		}
	}

	// no hits
	return btCollisionWorld::ClosestRayResultCallback(Vec3::zero, Vec3::zero);
}

RigidBody* Walker::get_support(r32 extra_distance) const
{
	btCollisionWorld::ClosestRayResultCallback ray_callback = check_support(extra_distance);
	if (ray_callback.hasHit())
		return Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
	else
		return nullptr;
}

Vec3 Walker::get_support_velocity(const Vec3& absolute_pos, const btCollisionObject* support)
{
	Vec3 support_velocity = Vec3::zero;
	if (support)
	{
		const btRigidBody* support_body = (const btRigidBody*)(support);
		support_velocity = support_body->getLinearVelocity()
			+ Vec3(support_body->getAngularVelocity()).cross(absolute_pos - Vec3(support_body->getCenterOfMassPosition()));
	}
	return support_velocity;
}


void update_net_speed(const Update& u, Walker* w, const Vec3& v, const Vec3& support_velocity, const Vec3& z)
{
	r32 new_net_speed = vi_min(w->max_speed, (v - support_velocity).dot(z));
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
	if (Game::level.local && (pos.y < Game::level.min_y || Water::underwater(pos)))
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

			support_velocity = get_support_velocity(pos, object);

			r32 velocity_diff = velocity.y - support_velocity.y;

			Vec3 support_normal = ray_callback.m_hitNormalWorld;

			// calculate our expected vertical velocity (if we're running up a ramp for example)
			// if our actual velocity is higher than that, we must be jumping,
			// so don't glue the player to the ground
			Vec3 velocity_flattened = velocity - support_normal * velocity.dot(support_normal);

			r32 expected_vertical_speed = vi_min(0.0f, velocity_flattened.y);

			if (velocity_diff < expected_vertical_speed + 3.5f)
			{
#if !SERVER
				if (velocity_diff < expected_vertical_speed - 0.5f)
				{
					land.fire(velocity_diff - expected_vertical_speed);
					velocity = body->getLinearVelocity(); // event handlers may modify velocity
				}
#endif

				{
					r32 target_y = ray_callback.m_hitPointWorld.y() + WALKER_SUPPORT_HEIGHT + capsule_height() * 0.5f;
					r32 height_difference = target_y - pos.y;
					const r32 max_adjustment_speed = 5.0f;
					pos.y += vi_min(vi_max(height_difference, (support_velocity.y - max_adjustment_speed) * u.time.delta), (support_velocity.y + max_adjustment_speed) * u.time.delta);
					absolute_pos(pos);
				}

				r32 normal_velocity = velocity.dot(support_normal);
				r32 support_normal_velocity = support_velocity.dot(support_normal);
				adjustment += (support_normal_velocity - normal_velocity) * support_normal;

				b8 has_traction = support_normal.y > 0.7f;

				Vec2 movement = dir;

				r32 movement_length = vi_min(1.0f, movement.length());

				if (has_traction && !btFuzzyZero(movement_length))
				{
					// slide against walls
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
					r32 x_speed_change = LMath::clampf(x_speed - support_x_speed, -u.time.delta * DECELERATION, u.time.delta * DECELERATION);
					adjustment -= x_speed_change * x;

					// Calculate speed along desired movement direction
					r32 net_z_speed = (velocity - support_velocity).dot(z);

					// Makeshift acceleration curve
					r32 acceleration = net_speed < ACCEL_THRESHOLD ? ACCEL1 : ACCEL2;

					// Increase acceleration if we're turning
					acceleration += fabsf(Vec2(x.x, x.z).dot(Vec2(velocity.x - support_velocity.x, velocity.z - support_velocity.z))) * ACCEL2 * 4.0f;

					// Increase acceleration if we're climbing
					if (z.y > 0.0f)
						acceleration += z.y * ACCEL2 * 2.0f;

					r32 target_speed = vi_max(net_speed, speed) * movement_length;
					if (net_z_speed > target_speed)
					{
						// Decelerate
						r32 z_speed_change = vi_min(u.time.delta * DECELERATION, target_speed - net_z_speed);
						adjustment += z_speed_change * z;
					}
					else
					{
						// Accelerate net_z_speed up to speed
						r32 z_speed_change = vi_min(u.time.delta * acceleration, target_speed - net_z_speed);
						adjustment += z_speed_change * z;
						if (z.y > 0.0f)
							adjustment.y += z.y * vi_min(u.time.delta * acceleration * 4.0f, target_speed - net_z_speed) * 2.0f;
					}

					if (auto_rotate)
						target_rotation = atan2f(movement.x, movement.y);

					// calculate new net speed
					update_net_speed(u, this, velocity + adjustment, support_velocity, z);
				}
				else if (has_traction)
				{
					// Not moving; decelerate
					// Remove from the character a portion of velocity defined by the DECELERATION.
					Vec3 horizontal_velocity = velocity - velocity.dot(support_normal) * support_normal;
					Vec3 support_horizontal_velocity = support_velocity - support_normal_velocity * support_normal;
					Vec3 relative_velocity = horizontal_velocity - support_horizontal_velocity;
					r32 speed = relative_velocity.length();
					if (speed > 0)
					{
						Vec3 relative_velocity_normalized = relative_velocity / speed;
						r32 velocity_change = vi_min(speed, u.time.delta * DECELERATION);
						adjustment -= velocity_change * relative_velocity_normalized;
					}
					update_net_speed(u, this, velocity + adjustment, support_velocity, Vec3::zero);
				}

				support = Entity::list[object->getUserIndex()].get<RigidBody>();
			}
		}

		if (!support.ref())
		{
			// air control
			Vec2 accel = dir * AIR_CONTROL_ACCEL * u.time.delta;
			Vec3 accel3 = Vec3(accel.x, 0, accel.y);

			// don't allow the walker to go faster than the speed we were going when we last hit the ground
			if (velocity.dot(accel3 / accel3.length()) < vi_max(speed * 0.25f, net_speed))
				adjustment += accel3;
		}

		body->setLinearVelocity(velocity + adjustment);
	}

	// handle rotation

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
	return get<Transform>()->to_world(Vec3(0, (capsule_height() * -0.5f) - WALKER_SUPPORT_HEIGHT, 0));
}

Vec3 Walker::forward() const
{
	return Quat::euler(0, rotation, 0) * Vec3(0, 0, 1);
}

Vec3 Walker::right() const
{
	return Quat::euler(0, rotation, 0) * Vec3(-1, 0, 0);
}

r32 Walker::capsule_height() const
{
	const Vec3& size = get<RigidBody>()->size;
	return size.y + size.x * 2.0f;
}

void Walker::crouch(b8 c)
{
	r32 desired_height = c ? WALKER_HEIGHT * 0.25f : WALKER_HEIGHT;
	RigidBody* body = get<RigidBody>();
	if (body->size.y != desired_height)
	{
		body->get<Transform>()->pos.y += (desired_height - body->size.y) * 0.5f;
		body->size.y = desired_height;
		body->rebuild();
		walker_set_rigid_body_props(body->btBody);
	}
}

}
