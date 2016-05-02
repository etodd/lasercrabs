#pragma once

#include "data/entity.h"
#include "physics.h"
#include <bullet/src/btBulletDynamicsCommon.h>
#include "ai.h"

namespace VI
{

struct Empty : public Entity
{
	Empty();
};

struct Prop : public Entity
{
	Prop(const AssetID, const AssetID = AssetNull, const AssetID = AssetNull);
};

struct StaticGeom : public Entity
{
	StaticGeom(AssetID, const Vec3&, const Quat&, short = btBroadphaseProxy::StaticFilter, short = ~btBroadphaseProxy::StaticFilter);
};

struct PhysicsEntity : public Entity
{
	PhysicsEntity(AssetID, const Vec3&, const Quat&, RigidBody::Type, const Vec3&, r32, short, short);
};

struct Noclip : public Entity
{
	Noclip();
};

struct Camera;

struct NoclipControl : public ComponentType<NoclipControl>
{
	r32 angle_horizontal;
	r32 angle_vertical;

	Camera* camera;

	NoclipControl();
	~NoclipControl();

	void update(const Update&);
	void awake();
};


}
