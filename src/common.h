#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"

namespace VI
{

struct Empty : public Entity
{
	Empty(ID);
	void awake();
};

struct StaticGeom : public Entity
{
	StaticGeom(ID, AssetID);
	void awake();
};

struct Box : public Entity
{
	Box(ID, Vec3, Quat, float, Vec3);
	void awake();
};

struct Noclip : public Entity
{
	Noclip(ID id);
	void awake();
};

struct NoclipControl : public ComponentType<NoclipControl>
{
	float angle_horizontal;
	float angle_vertical;

	NoclipControl();
	void awake();

	void update(Update);
};

}
