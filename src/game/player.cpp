#include "player.h"
#include "awk.h"
#include "data/components.h"
#include "render/render.h"
#include <GLFW/glfw3.h>

#define fov_initial PI * 0.25f
#define speed_mouse 0.0025f
#define crawl_speed 1.0f
#define attach_speed 2.0f
#define max_attach_time 0.25f

Player::Player(ID id)
	: Entity(id)
{
	Transform* transform = create<Transform>();
	transform->pos = Vec3(0, 10, 0);
	create<Awk>();
	create<PlayerControl>();
}

void Player::awake()
{
}

void PlayerControl::awk_attached()
{
	Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
	attach_quat_start = Quat::look(direction);

	Quat attach_quat_absolute = Quat::look(direction.reflect(get<Transform>()->absolute_rot() * Vec3(0, 0, 1)));
	attach_quat = get<Transform>()->parent()->absolute_rot().inverse() * attach_quat_absolute;
	attach_time = fmin(max_attach_time, Quat::angle(attach_quat_start, attach_quat_absolute) / attach_speed);
	attach_timer = 0.0f;
}

PlayerControl::PlayerControl()
	: angle_horizontal(),
	angle_vertical(),
	attach_timer(1.0f),
	attach_time(),
	attach_quat_start(Quat::identity)
{
}

void PlayerControl::awake()
{
	link<&PlayerControl::awk_attached>(&get<Awk>()->attached);
}

void PlayerControl::update(Update u)
{
	Quat look_quat = Quat::euler(0, angle_horizontal, angle_vertical);
	if (get<Transform>()->has_parent)
	{
		if (attach_timer < attach_time)
		{
			Quat attach_quat_absolute = Quat::normalize(get<Transform>()->parent()->absolute_rot() * attach_quat);

			attach_timer += u.time.delta;
			if (attach_timer >= attach_time)
			{
				look_quat = attach_quat_absolute;

				Vec3 forward = look_quat * Vec3(0, 0, 1);
				if (fabs(forward.y) < 0.99f)
				{
					angle_horizontal = atan2f(forward.x, forward.z);
					angle_vertical = -asinf(forward.y);
				}
			}
			else
				look_quat = Quat::slerp(Ease::cubic_out<float>(attach_timer / attach_time), attach_quat_start, attach_quat_absolute);
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

			if (u.input->keys[GLFW_KEY_W] == GLFW_PRESS)
				get<Transform>()->pos += Vec3(0, 0, 1) * u.time.delta * crawl_speed;
			if (u.input->keys[GLFW_KEY_S] == GLFW_PRESS)
				get<Transform>()->pos += Vec3(0, 0, -1) * u.time.delta * crawl_speed;
			if (u.input->keys[GLFW_KEY_D] == GLFW_PRESS)
				get<Transform>()->pos += Vec3(1, 0, 0) * u.time.delta * crawl_speed;
			if (u.input->keys[GLFW_KEY_A] == GLFW_PRESS)
				get<Transform>()->pos += Vec3(-1, 0, 0) * u.time.delta * crawl_speed;
		}

		if (u.input->mouse)
			get<Awk>()->detach(look_quat * Vec3(0, 0, 1));
	}
	else
	{
		if (!btVector3(get<Awk>()->velocity).fuzzyZero())
		{
			Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
			look_quat = Quat::look(direction);
		}
	}
	
	float FoV = fov_initial;

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	float aspect = u.input->height == 0 ? 1 : (float)u.input->width / (float)u.input->height;
	Camera::main.projection = Mat4::perspective(FoV, aspect, 0.01f, 1000.0f);

	// Camera matrix
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 look = look_quat * Vec3(0, 0, 1);
	Camera::main.view = Mat4::look(pos, look);
}