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

void Camera::remove()
{
	active = false;
}


}