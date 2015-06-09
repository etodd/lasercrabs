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

	Vec3 velocity;
	Mat4 view;
	Mat4 projection;

	Player();
	~Player();
	void awake();
	virtual void exec(EntityUpdate);
};
