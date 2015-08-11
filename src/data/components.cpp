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

void Transform::mat(Mat4* m)
{
	*m = Mat4::identity;
	Transform* t = this;
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
	Transform* t = (Transform*)this;
	world.setIdentity();
	while (t)
	{ 
		btTransform local;
		local.setRotation(t->rot);
		local.setOrigin(t->pos);
		world = world * local;
		t = t->parent;
	}
}

void Transform::set_bullet(const btTransform& world)
{
	pos = world.getOrigin();
	rot = world.getRotation();
}

void Transform::absolute(Quat* abs_rot, Vec3* abs_pos)
{
	*abs_rot = Quat::identity;
	*abs_pos = Vec3::zero;
	Transform* t = this;
	while (t)
	{ 
		*abs_rot = t->rot * *abs_rot;
		*abs_pos = (t->rot * *abs_pos) + t->pos;
		t = t->parent;
	}
}

void Transform::absolute(const Quat& abs_rot, const Vec3& abs_pos)
{
	if (parent)
	{
		Quat parent_rot;
		Vec3 parent_pos;
		parent->absolute(&parent_rot, &parent_pos);
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

Quat Transform::absolute_rot()
{
	Quat q = Quat::identity;
	Transform* t = this;
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

Vec3 Transform::absolute_pos()
{
	Vec3 abs_pos = Vec3::zero;
	Transform* t = this;
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
	{
		Quat abs_rot;
		Vec3 abs_pos;
		parent->absolute(&abs_rot, &abs_pos);
	}
	else
		pos = p;
}

void Transform::reparent(Transform* p)
{
	Quat abs_rot;
	Vec3 abs_pos;
	absolute(&abs_rot, &abs_pos);

	if (p)
	{
		Quat parent_rot;
		Vec3 parent_pos;
		p->absolute(&parent_rot, &parent_pos);

		rot = parent_rot.inverse() * abs_rot;
		pos = parent_rot.inverse() * (abs_pos - parent_pos);
	}
	else
	{
		rot = abs_rot;
		pos = abs_pos;
	}
	parent = p;
}

}