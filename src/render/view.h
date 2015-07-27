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
	Mat4 offset;
	void draw(const RenderParams&);
	void awake();
	View();
};

}
