#pragma once

#include "entity.h"
#include "LinearMath/btMotionState.h"

struct Transform : public ComponentType<Transform>, public btMotionState
{
	Vec3 pos;
	Quat rot;

	Transform();

	void awake();
	virtual void getWorldTransform(btTransform&) const;
	virtual void setWorldTransform(const btTransform&);

	void mat(Mat4*);
};
