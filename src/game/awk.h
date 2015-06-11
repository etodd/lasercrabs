#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"

struct RigidBody;
struct Transform;

struct Awk : public ComponentType<Awk>, public ExecDynamic<EntityUpdate>
{
	Vec3 velocity;
	Link attached;
	Awk();
	~Awk();
	void awake();

	void detach(Vec3);

	virtual void exec(EntityUpdate);
};
