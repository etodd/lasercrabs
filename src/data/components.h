#pragma once

#include "entity.h"
#include "LinearMath/btTransform.h"

namespace VI
{

struct Transform : public ComponentType<Transform>
{
	ID parent_id;
	bool has_parent;
	Vec3 pos;
	Quat rot;

	void parent(const Transform*);
	Transform* parent();

	Transform();

	void awake();
	void get_bullet(btTransform&) const;
	void set_bullet(const btTransform&);

	void mat(Mat4*);

	void absolute(Quat*, Vec3*);
	void absolute(const Quat&, const Vec3&);
	Vec3 absolute_pos();
	void absolute_pos(const Vec3&);
	Quat absolute_rot();
	void absolute_rot(const Quat&);
	void reparent(Transform*);
};

}
