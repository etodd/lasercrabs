#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"

struct RigidBody;
struct Transform;

struct Player : public EntityType<Player>, public ExecDynamic<EntityUpdate>
{
	float angle_horizontal;
	float angle_vertical;

	float attach_timer;
	float attach_time;
	Quat attach_quat_start;
	Quat attach_quat;

	Vec3 velocity;
	Mat4 view;
	Mat4 projection;

	Player();
	~Player();
	void awake();
	virtual void exec(EntityUpdate);
};
