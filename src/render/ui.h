#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "render.h"

namespace VI
{

struct UIText
{
	static Array<UIText*> instances;
	char* string;
	Array<int> indices;
	Array<Vec3> vertices;
	Vec4 color;
	AssetID font;
	Vec2 pos;
	float rot;
	float size;
	void text(const char*);
	void reeval();
	static void reeval_all();
	void draw(const RenderParams&);
	UIText();
	~UIText();
};

struct UI
{
	static float scale;
	static int mesh;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void init(const int, const int);
	static float get_scale(const int, const int);
	static void box(const RenderParams&, const Vec2&, const Vec2&, const Vec4&);
	static void centered_box(const RenderParams&, const Vec2&, const Vec2&, const Vec4&, const float);
	static void border(const RenderParams&, const Vec2&, const Vec2&, const Vec4&, float);
	static void centered_border(const RenderParams&, const Vec2&, const Vec2&, const Vec4&, const float, const float);
	static void triangle(const RenderParams&, const Vec2&, const Vec2&, const Vec4&, const float);
	static void update(const RenderParams&);
	static void draw(const RenderParams&);
	static bool project(const RenderParams&, const Vec3&, Vec2&);
};

}
