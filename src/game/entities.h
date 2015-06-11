#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"

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