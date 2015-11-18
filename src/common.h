#pragma once

#include "data/entity.h"
#include "render/render.h"
#include <btBulletDynamicsCommon.h>

class btTriangleIndexVertexArray;
class btBvhTriangleMeshShape;
class btCollisionShape;

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
	StaticGeom(const AssetID, const Vec3&, const Quat&, const short = btBroadphaseProxy::StaticFilter, const short = ~btBroadphaseProxy::StaticFilter);
};

struct PhysicsEntity : public Entity
{
	PhysicsEntity(const Vec3&, const Quat&, const AssetID, const float, btCollisionShape*, const Vec3&);
	PhysicsEntity(const Vec3&, const Quat&, const AssetID, const float, btCollisionShape*, const Vec3&, const short, const short);
};

struct Noclip : public Entity
{
	Noclip();
};

struct NoclipControl : public ComponentType<NoclipControl>
{
	float angle_horizontal;
	float angle_vertical;

	Camera* camera;

	NoclipControl();
	~NoclipControl();

	void update(const Update&);
	void awake();
};

}
