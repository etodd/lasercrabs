#include "render.h"

namespace VI
{


Camera::ViewportBlueprint Camera::one_player_viewports[] =
{
	{ 0, 0, 1, 1, },
};

Camera::ViewportBlueprint Camera::two_player_viewports[] =
{
	{ 0, 0, 0.5f, 1, },
	{ 0.5f, 0, 0.5f, 1, },
};

Camera::ViewportBlueprint Camera::three_player_viewports[] =
{
	{ 0, 0.5f, 1, 0.5f, },
	{ 0, 0, 0.5f, 0.5f, },
	{ 0.5f, 0, 0.5f, 0.5f, },
};

Camera::ViewportBlueprint Camera::four_player_viewports[] =
{
	{ 0, 0, 0.25f, 0.25f, },
	{ 0.25f, 0, 0.25f, 0.25f, },
	{ 0, 0.25f, 0.25f, 0.25f, },
	{ 0.25f, 0.25f, 0.25f, 0.25f, },
};

Camera::ViewportBlueprint* Camera::viewport_blueprints[] =
{
	Camera::one_player_viewports,
	Camera::two_player_viewports,
	Camera::three_player_viewports,
	Camera::four_player_viewports,
};

Camera Camera::all[Camera::max_cameras];

Camera::Camera()
	: active(),
	projection(),
	projection_inverse(),
	pos(),
	rot(),
	viewport(),
	near_plane(),
	far_plane(),
	mask(~RENDER_MASK_SHADOW),
	fog(true),
	wall_normal(0, 0, 0),
	range(),
	range_center(),
	cull_range(),
	team(-1)
{
}

Camera* Camera::add()
{
	for (s32 i = 0; i < max_cameras; i++)
	{
		if (!all[i].active)
		{
			new (&all[i]) Camera();
			all[i].active = true;
			return &all[i];
		}
	}
	vi_assert(false);
	return 0;
}

Mat4 Camera::view() const
{
	return Mat4::look(pos, rot * Vec3(0, 0, 1), rot * Vec3(0, 1, 0));
}

void Camera::perspective(r32 fov, r32 aspect, r32 near, r32 far)
{
	near_plane = near;
	far_plane = far;
	projection = Mat4::perspective(fov, aspect, near, far);
	projection_inverse = projection.inverse();
	update_frustum();
}

void Camera::orthographic(r32 width, r32 height, r32 near, r32 far)
{
	near_plane = near;
	far_plane = far;
	projection = Mat4::orthographic(width, height, near, far);
	projection_inverse = projection.inverse();
	update_frustum();
}

void Camera::remove()
{
	active = false;
}

b8 Camera::visible_sphere(const Vec3& sphere_pos, r32 sphere_radius) const
{
	Vec3 view_space = rot.inverse() * (sphere_pos - pos);
	if (view_space.z + sphere_radius > near_plane && view_space.z - sphere_radius < far_plane)
	{
		if (view_space.length_squared() < sphere_radius * sphere_radius)
			return true;

		if (frustum[0].distance(view_space) < -sphere_radius) // left
			return false;
		if (frustum[1].distance(view_space) < -sphere_radius) // bottom
			return false;
		if (frustum[2].distance(view_space) < -sphere_radius) // top
			return false;
		if (frustum[3].distance(view_space) < -sphere_radius) // right
			return false;
		return true;
	}
	return false;
}

void Camera::update_frustum()
{
	Vec4 rays[] =
	{
		projection_inverse * Vec4(-1, -1, -1, 1),
		projection_inverse * Vec4(1, -1, -1, 1),
		projection_inverse * Vec4(-1, 1, -1, 1),
		projection_inverse * Vec4(1, 1, -1, 1),
		projection_inverse * Vec4(-1, -1, 1, 1),
		projection_inverse * Vec4(1, -1, 1, 1),
		projection_inverse * Vec4(-1, 1, 1, 1),
		projection_inverse * Vec4(1, 1, 1, 1),
	};

	for (s32 i = 0; i < 8; i++)
		rays[i] /= rays[i].w;

	frustum[0] = Plane(rays[0].xyz(), rays[4].xyz(), rays[2].xyz()); // left
	frustum[1] = Plane(rays[1].xyz(), rays[5].xyz(), rays[0].xyz()); // bottom
	frustum[2] = Plane(rays[2].xyz(), rays[6].xyz(), rays[3].xyz()); // top
	frustum[3] = Plane(rays[3].xyz(), rays[7].xyz(), rays[1].xyz()); // right

	frustum_rays[0] = rays[4].xyz() / rays[4].z;
	frustum_rays[1] = rays[5].xyz() / rays[5].z;
	frustum_rays[2] = rays[6].xyz() / rays[6].z;
	frustum_rays[3] = rays[7].xyz() / rays[7].z;
}

}
