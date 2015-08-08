#pragma once

#include "entity.h"
#include "components.h"
#include "mesh.h"

namespace VI
{

struct Animator : public ComponentType<Animator>
{
	AssetID armature;
	AssetID animation;
	Array<Mat4> bones;
	float time;

	void update(const Update&);
	void update_world_transforms();
	void awake();
	Animator();
};

}