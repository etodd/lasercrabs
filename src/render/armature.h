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
	Array<Mat4> bones;
	Array<Mat4> skin_transforms;
	Mat4 offset;
	float time;

	void draw(RenderParams*);
	void update(Update);
	void update_world_transforms();
	void awake();
	Armature();
};