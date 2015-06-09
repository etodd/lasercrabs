#include "player.h"
#include <GLFW/glfw3.h>
#include "data/components.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"

#define fov_initial PI * 0.25f
#define speed 15.0f
#define speed_mouse 0.0025f
#define player_radius 0.1f
#define attach_speed 2.0f
#define max_attach_time 0.25f

Player::Player()
	: velocity(),
	angle_horizontal(),
	angle_vertical(),
	view(),
	projection(),
	attach_timer(1.0f),
	attach_time(),
	attach_quat(),
	attach_quat_start()
{
	Transform* transform = Entities::all.component<Transform>(this);
	transform->pos = Vec3(0, 10, 0);
}

void Player::awake()
{
	Entities::all.update.add(this);
}

Player::~Player()
{
	Entities::all.update.remove(this);
}

void Player::exec(EntityUpdate u)
{
	Quat look_quat = Quat::euler(0, angle_horizontal, angle_vertical);
	if (get<Transform>()->parent)
	{
		if (attach_timer < attach_time)
		{
			Quat attach_quat_absolute = get<Transform>()->parent->absolute_rot() * attach_quat;

			attach_timer += u.time.delta;
			if (attach_timer >= attach_time)
			{
				look_quat = attach_quat_absolute;

				Vec3 forward = look_quat * Vec3(0, 0, 1);
				angle_horizontal = atan2f(forward.x, forward.z);
				angle_vertical = -asinf(forward.y);
			}
			else
				look_quat = Quat::slerp(Ease::quad_out<float>(attach_timer / attach_time), attach_quat_start, attach_quat_absolute);
		}
		else
		{
			angle_horizontal += speed_mouse * ((u.input->width / 2) - (float)u.input->cursor_x);
			angle_vertical -= speed_mouse * ((u.input->height / 2) - (float)u.input->cursor_y);

			if (angle_vertical < PI * -0.495f)
				angle_vertical = PI * -0.495f;
			if (angle_vertical > PI * 0.495f)
				angle_vertical = PI * 0.495f;

			look_quat = Quat::euler(0, angle_horizontal, angle_vertical);

			Vec3 forward = look_quat * Vec3(0, 0, 1);
			Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
			float dot = forward.dot(wall_normal);
			if (dot < 0.0f)
			{
				forward = Vec3::normalize(forward - dot * wall_normal);
				angle_horizontal = atan2f(forward.x, forward.z);
				angle_vertical = -asinf(forward.y);
				look_quat = Quat::euler(0, angle_horizontal, angle_vertical);
			}
		}

		if (u.input->mouse)
		{
			Vec3 direction = look_quat * Vec3(0, 0, 1);
			velocity = direction * speed;
			get<Transform>()->reparent(0);
			get<Transform>()->pos += direction * player_radius;
		}
	}
	else
	{
		velocity.y -= u.time.delta * 9.8f;

		Vec3 position = get<Transform>()->pos;
		Vec3 next_position = position + velocity * u.time.delta;

		if (!btVector3(velocity).fuzzyZero())
		{
			Vec3 direction = Vec3::normalize(velocity);
			look_quat = Quat::look(direction);

			btCollisionWorld::ClosestRayResultCallback rayCallback(position, next_position + Vec3::normalize(velocity) * player_radius);

			// Perform raycast
			Physics::world.btWorld->rayTest(position, next_position, rayCallback);

			if (rayCallback.hasHit())
			{
				Entity* entity = (Entity*)rayCallback.m_collisionObject->getUserPointer();
				get<Transform>()->pos = rayCallback.m_hitPointWorld + rayCallback.m_hitNormalWorld * player_radius;
				get<Transform>()->rot = Quat::look(rayCallback.m_hitNormalWorld);
				get<Transform>()->reparent(entity->get<Transform>());

				attach_quat_start = look_quat;

				Quat attach_quat_absolute = Quat::look(direction.reflect(rayCallback.m_hitNormalWorld));
				attach_quat = entity->get<Transform>()->absolute_rot().inverse() * attach_quat_absolute;
				attach_time = fmin(max_attach_time, Quat::angle(look_quat, attach_quat_absolute) / attach_speed);
				attach_timer = 0.0f;
				velocity = Vec3::zero;
			}
			else
				get<Transform>()->pos = next_position;
		}
		else
			get<Transform>()->pos = next_position;
	}
	
	// Right vector
	Vec3 right = Vec3(
		(float)(sin(angle_horizontal - 3.14f/2.0f)),
		0,
		(float)(cos(angle_horizontal - 3.14f/2.0f))
	);
	
	Vec3 direction = look_quat * Vec3(0, 0, 1);
	Vec3 up = right.cross(direction);

	/*
	// Move forward
	if (u.input->keys[GLFW_KEY_W] == GLFW_PRESS)
		force += direction * u.time.delta * speed;
	// Move backward
	if (u.input->keys[GLFW_KEY_S] == GLFW_PRESS)
		force -= direction * u.time.delta * speed;
	// Strafe right
	if (u.input->keys[GLFW_KEY_D] == GLFW_PRESS)
		force += right * u.time.delta * speed;
	// Strafe left
	if (u.input->keys[GLFW_KEY_A] == GLFW_PRESS)
		force -= right * u.time.delta * speed;
	*/
	
	float FoV = fov_initial;

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	float aspect = u.input->height == 0 ? 1 : (float)u.input->width / (float)u.input->height;
	projection = Mat4::perspective(FoV, aspect, 0.01f, 1000.0f);
	// Camera matrix
	view       = Mat4::look_at(
								get<Transform>()->absolute_pos(),           // Camera is here
								get<Transform>()->absolute_pos() + direction, // and looks here : at the same position, plus "direction"
								up                  // Head is up (set to 0,-1,0 to look upside-down)
						   );
}