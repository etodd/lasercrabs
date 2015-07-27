#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "data/mesh.h"
#include "render.h"

namespace VI
{

struct UIText
{
	Array<int> indices;
	Array<Vec3> vertices;
	Vec4 color;
	Font* font;
	Array<char> string;
	Mat4 transform;
	void text(const char*);
	void draw(const Vec3&);
	UIText();
};

struct UI
{
	static size_t mesh;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void test(const Vec3&);
	static void draw(const RenderParams&);
};

}
