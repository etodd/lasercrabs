#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "exec.h"
#include "render.h"

struct View : public ComponentType<View>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	void draw(RenderParams*);
	void awake();
	View()
		: mesh(), shader(), texture()
	{
	}
};