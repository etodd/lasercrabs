#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"

namespace VI
{

struct View : public ComponentType<View>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Vec4 color;
	Mat4 offset;
	void draw(const RenderParams&);
	void awake();
	View();
};

}
