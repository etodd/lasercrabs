#pragma once

#include "entity.h"
#include "LinearMath/btMotionState.h"

struct Transform : public ComponentType<Transform>, public btMotionState
{
	ID parent_id;
	bool has_parent;
	Vec3 pos;
	Quat rot;

	Transform* parent();

	Transform();

	void awake();
	virtual void getWorldTransform(btTransform&) const;
	virtual void setWorldTransform(const btTransform&);

	void mat(Mat4*);

	void absolute(Quat*, Vec3*);
	Vec3 absolute_pos();
	Quat absolute_rot();
	void reparent(Transform*);
};
