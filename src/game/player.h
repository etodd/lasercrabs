#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"

struct RigidBody;
struct Transform;

struct Player : public Entity
{
	Player(ID id);
	void awake();
};

struct PlayerControl : public ComponentType<PlayerControl>
{
	float angle_horizontal;
	float angle_vertical;

	float attach_timer;
	float attach_time;
	Quat attach_quat;
	Quat attach_quat_start;

	PlayerControl();
	void awake();

	void awk_attached();

	void update(Update);
};