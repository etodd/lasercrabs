#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"

struct View : public ComponentType<View>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Vec3 scale;
	void draw(RenderParams*);
	void awake();
	View();
};
