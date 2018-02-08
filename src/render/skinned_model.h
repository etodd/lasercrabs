#pragma once

#include "data/entity.h"
#include "render.h"

namespace VI
{

struct SkinnedModel : public ComponentType<SkinnedModel>
{
	enum class ObstructingBehavior : s8
	{
		Normal,
		Hide,
		Alpha,
		count,
	};

	static Bitmask<MAX_ENTITIES> list_alpha;
	static Bitmask<MAX_ENTITIES> list_alpha_if_obstructing;
	static Bitmask<MAX_ENTITIES> list_additive;

	static void draw_opaque(const RenderParams&);
	static void draw_alpha(const RenderParams&);
	static void draw_additive(const RenderParams&);

	Mat4 offset;
	Vec4 color;
	r32 radius;
	Ref<Camera> first_person_camera;
	AssetID mesh;
	AssetID mesh_first_person;
	AssetID shader;
	AssetID texture;
	RenderMask mask;
	s8 team;

	SkinnedModel();
	void awake();
	~SkinnedModel();

	void alpha();
	void alpha_if_obstructing();
	void additive();
	void alpha_disable();
	AlphaMode alpha_mode() const;
	void alpha_mode(AlphaMode);
	void draw(const RenderParams&, ObstructingBehavior = ObstructingBehavior::Normal);
};

}
