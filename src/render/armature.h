#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "render.h"
#include "data/mesh.h"

struct Armature : public ComponentType<Armature>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Animation* animation;
	void draw(RenderParams*);
	void awake();
	Armature();
};
