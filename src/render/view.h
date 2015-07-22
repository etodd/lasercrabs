#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"

struct View : public ComponentType<View>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Mat4 offset;
	void draw(RenderParams*);
	void awake();
	View();
};
