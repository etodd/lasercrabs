#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.h"

struct Walker : public ComponentType<Walker>
{
	Vec3 velocity;
	float height, support_height, radius, mass;
	Walker(float, float, float, float);
	void awake();

	void update(Update);
};
