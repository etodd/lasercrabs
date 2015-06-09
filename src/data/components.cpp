#include "components.h"
#include "physics.h"

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
	Transform* p = this;
	while (p)
	{ 
		Mat4 local = Mat4(p->rot);
		local.translation(p->pos);
		*m = *m * local;

		p = p->parent;
	}
}

void Transform::getWorldTransform(btTransform& world) const
{
	const Transform* p = this;
	world.setIdentity();
	while (p)
	{ 
		btTransform local;
		local.setRotation(p->rot);
		local.setOrigin(p->pos);
		world = world * local;

		p = p->parent;
	}
}

void Transform::setWorldTransform(const btTransform& world)
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
