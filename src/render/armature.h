#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"

struct Armature : public ComponentType<Armature>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	void draw(RenderParams*);
	void awake();
	Armature();
};
