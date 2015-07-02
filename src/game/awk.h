#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"

struct RigidBody;
struct Transform;

struct Awk : public ComponentType<Awk>
{
	Vec3 velocity;
	Link attached;
	Awk();
	void awake();

	void detach(Vec3);

	void update(Update);
};
