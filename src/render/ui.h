#pragma once

#include "types.h"
#include "lmath.h"
#include "data/array.h"
#include "data/mesh.h"

namespace VI
{

struct UIText
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec4> colors;
	Font* font;
	Array<char> string;
	bool dirty;
	void text(char*);
	void color(Vec4);
};

struct UI
{
	
};

}