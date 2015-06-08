#pragma once

#include "data/entity.h"
#include "lmath.h"

class RigidBody;
class Transform;

struct Player : public EntityType<Player>, public ExecDynamic<EntityUpdate>
{
	float angle_horizontal;
	float angle_vertical;
	Transform* transform;
	RigidBody* body;
	Player();
	~Player();
	void awake();
	virtual void exec(EntityUpdate);
	Mat4 view;
	Mat4 projection;
};
