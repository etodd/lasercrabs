#pragma once

#include "entity.h"
#include "LinearMath/btTransform.h"

struct Transform : public ComponentType<Transform>
{
	ID parent_id;
	bool has_parent;
	Vec3 pos;
	Quat rot;

	Transform* parent();

	Transform();

	void awake();
	void get_bullet(btTransform&) const;
	void set_bullet(const btTransform&);

	void mat(Mat4*);

	void absolute(Quat*, Vec3*);
	Vec3 absolute_pos();
	Quat absolute_rot();
	void reparent(Transform*);
};
