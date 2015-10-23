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
	Empty(ID);
	void awake() {}
};

struct Prop : public Entity
{
	Prop(ID, AssetID);
	void awake() {}
};

struct StaticGeom : public Entity
{
	StaticGeom(const ID, const AssetID, const Vec3&, const Quat&, const short = btBroadphaseProxy::StaticFilter, const short = ~btBroadphaseProxy::StaticFilter);
	void awake() {}
};

struct PhysicsEntity : public Entity
{
	PhysicsEntity(const ID, const Vec3&, const Quat&, const AssetID, const float, btCollisionShape*, const Vec3&);
	PhysicsEntity(const ID, const Vec3&, const Quat&, const AssetID, const float, btCollisionShape*, const Vec3&, const short, const short);
	void awake() {}
};

struct Noclip : public Entity
{
	Noclip(ID id);
	void awake() {}
};

struct NoclipControl : public ComponentType<NoclipControl>
{
	float angle_horizontal;
	float angle_vertical;

	Camera* camera;

	NoclipControl();
	~NoclipControl();
	void awake() {}

	void update(const Update&);
};

struct Debug : public ComponentType<Debug>
{
	void awake() {}
	void draw(const RenderParams&);
};

}
