#pragma once

#include "data/entity.h"
#include "render.h"
#define MAX_BONES 100

namespace VI
{

struct SkinnedModel : public ComponentType<SkinnedModel>
{
	static Bitmask<MAX_ENTITIES> list_alpha;
	static Bitmask<MAX_ENTITIES> list_additive;

	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);

	AssetID mesh;
	AssetID shader;
	AssetID texture;
	RenderMask mask;
	StaticArray<Mat4, MAX_BONES> skin_transforms;
	Mat4 offset;
	Vec4 color;

	SkinnedModel();
	void awake();
	~SkinnedModel();

	void draw(const RenderParams&);
	void alpha();
	void additive();
	void alpha_disable();
};

}
