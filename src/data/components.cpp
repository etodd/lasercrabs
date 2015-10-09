#include "components.h"
#include "physics.h"

namespace VI
{

Transform::Transform()
	: parent(), pos(Vec3::zero), rot(Quat::identity)
{

}

void Transform::awake()
{
}

void Transform::mat(Mat4* m) const
{
	*m = Mat4::identity;
	const Transform* t = this;
	while (t)
	{ 
		Mat4 local = Mat4(t->rot);
		local.translation(t->pos);
		*m = *m * local;
		t = t->parent;
	}
}

void Transform::get_bullet(btTransform& world) const
{
	Quat abs_rot;
	Vec3 abs_pos;
	absolute(&abs_pos, &abs_rot);
	world.setOrigin(abs_pos);
	world.setRotation(abs_rot);
}

void Transform::set_bullet(const btTransform& world)
{
	pos = world.getOrigin();
	rot = world.getRotation();
}

void Transform::absolute(Vec3* abs_pos, Quat* abs_rot) const
{
	*abs_rot = Quat::identity;
	*abs_pos = Vec3::zero;
	const Transform* t = this;
	while (t)
	{ 
		*abs_rot = t->rot * *abs_rot;
		*abs_pos = (t->rot * *abs_pos) + t->pos;
		t = t->parent;
	}
}

void Transform::absolute(const Vec3& abs_pos, const Quat& abs_rot)
{
	if (parent)
	{
		Quat parent_rot;
		Vec3 parent_pos;
		parent->absolute(&parent_pos, &parent_rot);
		Quat parent_rot_inverse = parent_rot.inverse();
		pos = parent_rot_inverse * (abs_pos - parent_pos);
		rot = parent_rot_inverse * abs_rot;
	}
	else
	{
		rot = abs_rot;
		pos = abs_pos;
	}
}

Quat Transform::absolute_rot() const
{
	Quat q = Quat::identity;
	const Transform* t = this;
	while (t)
	{ 
		q = t->rot * q;
		t = t->parent;
	}
	return q;
}

void Transform::absolute_rot(const Quat& q)
{
	if (parent)
		rot = parent->absolute_rot().inverse() * q;
	else
		rot = q;
}

Vec3 Transform::absolute_pos() const
{
	Vec3 abs_pos = Vec3::zero;
	const Transform* t = this;
	while (t)
	{ 
		abs_pos = (t->rot * abs_pos) + t->pos;
		t = t->parent;
	}
	return abs_pos;
}

void Transform::absolute_pos(const Vec3& p)
{
	if (parent)
		pos = parent->to_local(p);
	else
		pos = p;
}

Vec3 Transform::to_world(const Vec3& p) const
{
	Quat abs_rot = Quat::identity;
	Vec3 abs_pos = p;
	const Transform* t = this;
	while (t)
	{ 
		abs_rot = t->rot * abs_rot;
		abs_pos = (t->rot * abs_pos) + t->pos;
		t = t->parent;
	}
	return abs_pos;
}

Vec3 Transform::to_local(const Vec3& p) const
{
	Quat abs_rot;
	Vec3 abs_pos;
	absolute(&abs_pos, &abs_rot);

	return abs_rot.inverse() * (p - abs_pos);
}

void Transform::to_world(Vec3* p, Quat* q) const
{
	const Transform* t = this;
	while (t)
	{ 
		*q = t->rot * *q;
		*p = (t->rot * *p) + t->pos;
		t = t->parent;
	}
}

void Transform::to_local(Vec3* p, Quat* q) const
{
	Quat abs_rot;
	Vec3 abs_pos;
	absolute(&abs_pos, &abs_rot);

	Quat abs_rot_inverse = abs_rot.inverse();

	*q = abs_rot_inverse * *q;
	*p = abs_rot_inverse * (*p - abs_pos);
}

void Transform::reparent(Transform* p)
{
	Quat abs_rot;
	Vec3 abs_pos;
	absolute(&abs_pos, &abs_rot);

	if (p)
	{
		Quat parent_rot;
		Vec3 parent_pos;
		p->absolute(&parent_pos, &parent_rot);

		Quat parent_rot_inverse = parent_rot.inverse();

		rot = parent_rot_inverse * abs_rot;
		pos = parent_rot_inverse * (abs_pos - parent_pos);
	}
	else
	{
		rot = abs_rot;
		pos = abs_pos;
	}
	parent = p;
}

}
