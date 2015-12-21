#pragma once

#include "data/entity.h"
#include "render.h"
#define MAX_BONES 100

namespace VI
{

struct SkinnedModel : public ComponentType<SkinnedModel>
{
	AssetID mesh;
	AssetID shader;
	AssetID texture;
	RenderMask mask;
	StaticArray<Mat4, MAX_BONES> skin_transforms;
	Mat4 offset;
	Vec4 color;

	SkinnedModel();
	void awake();
	void draw(const RenderParams&);
};

}
