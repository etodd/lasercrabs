#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "data/import_common.h"
#include "data/animator.h"
#include "render.h"

namespace VI
{

struct SkinnedModel : public ComponentType<SkinnedModel>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	StaticArray<Mat4, MAX_BONES> skin_transforms;
	Mat4 offset;
	Vec4 color;

	SkinnedModel();
	void awake();
	void draw(const RenderParams&);
};

}
