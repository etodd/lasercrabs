#include "render.h"

namespace VI
{


b8 RenderParams::update_instances() const
{
	return technique == RenderTechnique::Default && !(flags & RenderFlagEdges);
}

Camera::ViewportBlueprint Camera::viewports_one_player[] =
{
	{ 0, 0, 1, 1, },
};

Camera::ViewportBlueprint Camera::viewports_two_player[] =
{
	{ 0, 0, 0.5f, 1, },
	{ 0.5f, 0, 0.5f, 1, },
};

Camera::ViewportBlueprint Camera::viewports_three_player[] =
{
	{ 0, 0, 0.5f, 1, },
	{ 0.5f, 0.5f, 0.5f, 0.5f, },
	{ 0.5f, 0, 0.5f, 0.5f, },
};

Camera::ViewportBlueprint Camera::viewports_four_player[] =
{
	{ 0, 0.5f, 0.5f, 0.5f, },
	{ 0.5f, 0.5f, 0.5f, 0.5f, },
	{ 0, 0, 0.5f, 0.5f, },
	{ 0.5f, 0, 0.5f, 0.5f, },
};

Camera::ViewportBlueprint* Camera::viewport_blueprints[] =
{
	Camera::viewports_one_player,
	Camera::viewports_two_player,
	Camera::viewports_three_player,
	Camera::viewports_four_player,
};

PinArray<Camera, Camera::max_cameras> Camera::list;

Camera::Camera(s8 gamepad)
	: projection(),
	projection_inverse(),
	pos(),
	rot(),
	viewport(),
	near_plane(),
	far_plane(),
	mask(~RENDER_MASK_SHADOW),
	clip_planes(),
	range(),
	range_center(),
	cull_range(),
	flags(CameraFlagActive | CameraFlagColors | CameraFlagFog),
	gamepad(gamepad),
	team(-1)
{
}

Camera* Camera::add(s8 gamepad)
{
	Camera* c = list.add();
	new (c) Camera(gamepad);
	c->revision++;
	return c;
}

Camera* Camera::for_gamepad(s8 gamepad)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->gamepad == gamepad)
			return i.item();
	}
	return nullptr;
}

void Camera::remove()
{
	this->~Camera();
	list.remove(id());
	revision++;
}

Mat4 Camera::view() const
{
	return Mat4::look(pos, rot * Vec3(0, 0, 1), rot * Vec3(0, 1, 0));
}

void Camera::perspective(r32 fov, r32 near, r32 far)
{
	near_plane = near;
	far_plane = far;
	projection = Mat4::perspective(fov, viewport.size.y > 0.0f ? viewport.size.x / viewport.size.y : 1.0f, near, far);
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
