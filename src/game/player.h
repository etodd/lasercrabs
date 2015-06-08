#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"

class RigidBody;
class Transform;

struct Player : public EntityType<Player>, public ExecDynamic<EntityUpdate>
{
	float angle_horizontal;
	float angle_vertical;
	Vec3 last_position;
	Mat4 view;
	Mat4 projection;
	btGeneric6DofConstraint* joint;

	Player();
	~Player();
	void awake();
	virtual void exec(EntityUpdate);
};
