#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "load.h"
#include "physics.h"

struct Empty : public EntityType<Empty>
{
	Empty();
	void awake();
};

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

struct StaticGeom : public EntityType<StaticGeom>, public ExecDynamic<EntityUpdate>
{
	StaticGeom(Loader*, AssetID);
	~StaticGeom();
	void awake();
	virtual void exec(EntityUpdate);
};
