#pragma once

#include "data/entity.h"
#include "physics.h"

namespace VI
{

// has only a transform
struct Empty : public Entity
{
	Empty();
};

// has no transform
struct ContainerEntity : public Entity
{
	ContainerEntity();
};

struct Prop : public Entity
{
	Prop(AssetID, AssetID = AssetNull, AssetID = AssetNull);
};

struct StaticGeom : public Entity
{
	StaticGeom(AssetID, const Vec3&, const Quat&, short = CollisionStatic, short = ~CollisionStatic & ~CollisionAudio, s8 = 0);
};

struct PhysicsEntity : public Entity
{
	PhysicsEntity(AssetID, const Vec3&, const Quat&, RigidBody::Type, const Vec3&, r32, short, short, s8 = 0);
};

}
