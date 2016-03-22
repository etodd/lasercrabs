#pragma once

#include "data/entity.h"
#include "render.h"

namespace VI
{

struct View : public ComponentType<View>
{
	static Bitmask<MAX_ENTITIES> list_alpha;
	static Bitmask<MAX_ENTITIES> list_additive;

	RenderMask mask;

	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Vec4 color;
	Mat4 offset;

	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);

	View();
	void awake();
	~View();

	void alpha();
	void additive();
	void alpha_disable();
	void draw(const RenderParams&) const;
};

struct SkyDecal : ComponentType<SkyDecal>
{
	Vec4 color;
	r32 scale;
	AssetID texture;

	static void draw_alpha(const RenderParams&);

	void awake() {}
};

struct Skybox
{
	static r32 far_plane;
	static Vec3 color;
	static Vec3 ambient_color;
	static Vec3 zenith_color;
	static AssetID texture;
	static AssetID mesh;
	static AssetID shader;
	static r32 fog_start;
	static b8 valid();
	static void set(const r32, const Vec3&, const Vec3&, const Vec3&, const AssetID&, const AssetID&, const AssetID&);
	static void draw_alpha(const RenderParams&);
};

struct SkyPattern
{
	static void draw_alpha(const RenderParams&);
};

struct Cube
{
	static void draw(const RenderParams&, const Vec3&, const b8 = false, const Vec3& = Vec3(1), const Quat& = Quat::identity, const Vec4& = Vec4(1));
};

struct ScreenQuad
{
	ScreenQuad();
	s32 mesh;
	void init(RenderSync*);
	void set(RenderSync*, const Rect2&, const Camera*, const Rect2& = { Vec2::zero, Vec2(1, 1) });
};

}
