#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"
#include "data/intrusive_list.h"

namespace VI
{

struct Uniform
{
	AssetID uniform;
	Mat4 data_float;
	int data_integer;
};

struct View : public ComponentType<View>, public IntrusiveLinkedList<View>
{
	static View* first_alpha;

	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Vec4 color;
	Mat4 offset;

	int alpha_order;
	void alpha(const int = 0);
	void alpha_disable();
	
	View();
	~View();
	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	void draw(const RenderParams&) const;
	void awake();
};

struct Skybox
{
	static Vec4 color;
	static AssetID texture;
	static AssetID mesh;
	static AssetID shader;
	static float fog_start;
	static bool valid();
	static void set(const Vec4&, const AssetID&, const AssetID&, const AssetID&);
	static void draw(const RenderParams&, const int);
};

struct ScreenQuad
{
	ScreenQuad();
	int mesh;
	void init(RenderSync*);
	void set(RenderSync*, const Vec2&, const Vec2&, const Camera*, const Vec2& = Vec2::zero, const Vec2& = Vec2(1, 1));
};

}
