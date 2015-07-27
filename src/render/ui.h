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
	Array<Vec4> colors;
	Font* font;
	Array<char> string;
	Mat4 transform;
	void text(const char*);
	void color(const Vec4&);
	void draw();
	UIText();
};

struct UI
{
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void draw(const RenderParams&);
};

}
