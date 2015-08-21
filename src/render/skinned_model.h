#pragma once

#include "data/entity.h"
#include "data/components.h"
#include "data/import_common.h"
#include "render.h"

namespace VI
{

struct SkinnedModel : public ComponentType<SkinnedModel>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	Array<Mat4> skin_transforms;
	Mat4 offset;
	Vec4 color;

	void draw(const RenderParams&);
	void awake();
	SkinnedModel();
};

}
