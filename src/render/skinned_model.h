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
	static Bitmask<MAX_ENTITIES> list_alpha_depth;

	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_alpha_depth(const RenderParams&);
	static void draw_additive(const RenderParams&);

	Mat4 offset;
	Vec4 color;
	AssetID mesh;
	AssetID mesh_shadow;
	AssetID shader;
	AssetID texture;
	RenderMask mask;
	s8 team;

	SkinnedModel();
	void awake();
	~SkinnedModel();

	void draw(const RenderParams&);
	void alpha();
	void additive();
	void alpha_depth();
	void alpha_disable();
	AlphaMode alpha_mode() const;
	void alpha_mode(AlphaMode);
};

}
