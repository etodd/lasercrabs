#include "render.h"

namespace VI
{


Camera Camera::all[Camera::max_cameras];

Camera* Camera::add()
{
	for (int i = 0; i < max_cameras; i++)
	{
		if (!all[i].active)
		{
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

void Camera::perspective(const float fov, const float aspect, const float near, const float far)
{
	near_plane = near;
	far_plane = far;
	projection = Mat4::perspective(fov, aspect, near, far);
	projection_inverse = projection.inverse();
}

void Camera::orthographic(const float width, const float height, const float near, const float far)
{
	near_plane = near;
	far_plane = far;
	projection = Mat4::orthographic(width, height, near, far);
	projection_inverse = projection.inverse();
}

void Camera::remove()
{
	active = false;
}

void Camera::projection_frustum(Vec3* out) const
{
	Vec4 rays[] =
	{
		projection_inverse * Vec4(-1, -1, 0, 1),
		projection_inverse * Vec4(1, -1, 0, 1),
		projection_inverse * Vec4(-1, 1, 0, 1),
		projection_inverse * Vec4(1, 1, 0, 1),
	};
	rays[0] /= rays[0].w;
	rays[0] /= rays[0].z;
	rays[1] /= rays[1].w;
	rays[1] /= rays[1].z;
	rays[2] /= rays[2].w;
	rays[2] /= rays[2].z;
	rays[3] /= rays[3].w;
	rays[3] /= rays[3].z;

	out[0] = rays[0].xyz();
	out[1] = rays[1].xyz();
	out[2] = rays[2].xyz();
	out[3] = rays[3].xyz();
}


}