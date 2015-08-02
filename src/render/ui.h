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
	Vec2 pos;
	float rot;
	float size;
	void text(const char*);
	void draw(const RenderParams&);
	UIText();
};

struct UI
{
	static int mesh;
	static Array<Vec3> vertices;
	static Array<Vec4> colors;
	static Array<int> indices;
	static void draw(const RenderParams&);
};

}
