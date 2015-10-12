#pragma once

#include "data/entity.h"
#include "render/render.h"

class btTriangleIndexVertexArray;
class btBvhTriangleMeshShape;

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
	StaticGeom(const ID, const AssetID, const Vec3&, const Quat&);
	StaticGeom(const ID, const AssetID, const Vec3&, const Quat&, const short, const short);
	void init(const AssetID, btTriangleIndexVertexArray**, btBvhTriangleMeshShape**);
	void awake() {}
};

struct Box : public Entity
{
	Box(ID, Vec3, Quat, float, Vec3);
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
