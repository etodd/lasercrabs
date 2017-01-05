#pragma once

#include "data/entity.h"
#include "render.h"

namespace VI
{


struct View : public ComponentType<View>
{
	static Bitmask<MAX_ENTITIES> list_alpha;
	static Bitmask<MAX_ENTITIES> list_additive;
	static Bitmask<MAX_ENTITIES> list_hollow;

	Mat4 offset;
	Vec4 color;
	RenderMask mask;
	AssetID mesh;
	AssetID mesh_shadow;
	AssetID shader;
	AssetID texture;
	s8 team;

	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_hollow(const RenderParams&);
	static void draw_additive(const RenderParams&);

	static void draw_mesh(const RenderParams&, AssetID, AssetID, AssetID, const Mat4&, const Vec4&);

	View();
	View(AssetID);
	void awake();
	~View();

	AlphaMode alpha_mode() const;
	void alpha_mode(AlphaMode);
	void alpha();
	void hollow();
	void additive();
	void alpha_disable();
	void draw(const RenderParams&) const;
};

struct Skybox
{
	struct Config
	{
		Vec3 color;
		Vec3 ambient_color;
		Vec3 player_light;
		r32 far_plane;
		r32 fog_start;
		AssetID texture;
		AssetID mesh;
		AssetID shader;
		b8 valid() const;
	};

	static void draw_alpha(const RenderParams&);
};

struct SkyDecal : ComponentType<SkyDecal>
{
	Vec4 color;
	r32 scale;
	AssetID texture;

	static void draw_alpha(const RenderParams&);

	void awake() {}
};

struct Water : public ComponentType<Water>
{
	struct Config
	{
		Vec4 color;
		r32 displacement_horizontal;
		r32 displacement_vertical;
		AssetID texture;
		AssetID mesh;

		Config(AssetID = AssetNull);
	};

	static Water* underwater(const Vec3&);
	static void draw_opaque(const RenderParams&, const Config&, const Vec3&, const Quat&);
	static void draw_hollow(const RenderParams&, const Config&, const Vec3&, const Quat&);
	static void draw_opaque(const RenderParams&);
	static void draw_alpha_late(const RenderParams&);

	Config config;
	RenderMask mask;

	Water(AssetID = AssetNull);
	void awake();
	void draw_hollow(const RenderParams&);
	b8 contains(const Vec3&) const;
};

struct SkyPattern
{
	static void draw_opaque(const RenderParams&);
	static void draw_hollow(const RenderParams&);
};

struct Cube
{
	static void draw(const RenderParams&, const Vec3&, const b8 = false, const Vec3& = Vec3(1), const Quat& = Quat::identity, const Vec4& = Vec4(1));
};

struct ScreenQuad
{
	ScreenQuad();
	AssetID mesh;
	void init(RenderSync*);
	void set(RenderSync*, const Rect2&, const Camera*, const Rect2& = { Vec2::zero, Vec2(1, 1) });
};

}
