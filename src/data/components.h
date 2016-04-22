#pragma once

#include "entity.h"
#include "lmath.h"
#include "bullet/src/LinearMath/btTransform.h"

namespace VI
{

struct Transform : public ComponentType<Transform>
{
	Ref<Transform> parent;
	Vec3 pos;
	Quat rot;

	Transform();

	void awake() {}
	void get_bullet(btTransform&) const;
	void set_bullet(const btTransform&);

	void set(const Vec3&, const Quat&);

	void mat(Mat4*) const;

	Vec3 to_world(const Vec3&) const;
	Vec3 to_local(const Vec3&) const;
	Vec3 to_world_normal(const Vec3&) const;
	Vec3 to_local_normal(const Vec3&) const;

	void to_local(Vec3*, Quat*) const;
	void to_world(Vec3*, Quat*) const;

	void absolute(Vec3*, Quat*) const;
	void absolute(const Vec3&, const Quat&);
	Vec3 absolute_pos() const;
	void absolute_pos(const Vec3&);
	Quat absolute_rot() const;
	void absolute_rot(const Quat&);
	void reparent(Transform*);
};

struct PointLight : public ComponentType<PointLight>
{
	enum class Type
	{
		Normal = 1,
		Override = 1 << 1,
		Shockwave = 1 << 2,
	};

	r32 radius;
	Vec3 color;
	Vec3 offset;
	Type type;
	RenderMask mask;
	u8 team;

	PointLight();
	void awake() {}
};

struct SpotLight : public ComponentType<SpotLight>
{
	r32 radius;
	Vec3 color;
	r32 fov;
	RenderMask mask;
	u8 team;

	SpotLight();

	void awake() {}
};

struct DirectionalLight : public ComponentType<DirectionalLight>
{
	Vec3 color;
	b8 shadowed;
	RenderMask mask;

	DirectionalLight();

	void awake() {}
};

}
