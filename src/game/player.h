#pragma once

#include "data/entity.h"
#include "lmath.h"

struct Player : public EntityType<Player>, public ExecDynamic<EntityUpdate>
{
	Vec3 position; 
	float angle_horizontal;
	float angle_vertical;
	Player();
	~Player();
	void awake();
	virtual void exec(EntityUpdate);
	Mat4 view;
	Mat4 projection;
};
