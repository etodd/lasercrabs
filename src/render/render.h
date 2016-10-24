#pragma once

#include "types.h"
#include "lmath.h"
#include "sync.h"
#include "input.h"
#include "glvm.h"

namespace VI
{


enum class AlphaMode
{
	Opaque,
	Alpha,
	Additive,
	AlphaDepth,
	count,
};

struct Frustum
{
	Plane planes[6];
};

#define RENDER_MASK_SHADOW ((RenderMask)(1 << 15))
#define RENDER_MASK_DEFAULT ((RenderMask)-1)

// material indices
// these are alpha values stored in the g-buffer

// material not affected by override lights
#define MATERIAL_NO_OVERRIDE 0.0f
// special alpha depth signal for edge detection shader
#define MATERIAL_ALPHA_DEPTH 254.0f

struct Camera
{
	static const s32 max_cameras = 8;
	static Camera list[max_cameras];

	RenderMask mask;

	struct ViewportBlueprint
	{
		r32 x, y, w, h;
	};

	static ViewportBlueprint viewports_one_player[1];
	static ViewportBlueprint viewports_two_player[2];
	static ViewportBlueprint viewports_three_player[3];
	static ViewportBlueprint viewports_four_player[4];

	static ViewportBlueprint* viewport_blueprints[4];

	static Camera* add();

	static s32 active_count();

	Mat4 projection;
	Mat4 projection_inverse;
	Quat rot;
	Rect2 viewport;
	Plane frustum[4];
	Vec3 frustum_rays[4];
	Vec3 wall_normal;
	Vec3 pos;
	Vec3 range_center;
	r32 cull_range;
	r32 near_plane;
	r32 far_plane;
	r32 range;
	u8 team;
	b8 active;
	b8 cull_behind_wall;
	b8 colors;

	Camera();

	void perspective(r32, r32, r32, r32);
	void orthographic(r32, r32, r32, r32);
	b8 visible_sphere(const Vec3&, r32) const;
	void update_frustum();
	Mat4 view() const;
	void remove();
};

struct LoopSync : RenderSync
{
	b8 quit;
	GameTime time;
	InputState input;
};

typedef Sync<LoopSync>::Swapper LoopSwapper;

struct RenderParams
{
	const Camera* camera;
	Mat4 view;
	Mat4 view_projection;
	RenderTechnique technique;
	LoopSync* sync;
	s32 depth_buffer;
	s32 shadow_buffer;
	Mat4 shadow_vp;

	RenderParams()
		: camera(),
		view(),
		view_projection(),
		technique(),
		sync(),
		depth_buffer(AssetNull),
		shadow_buffer(AssetNull),
		shadow_vp()
	{
	}
};

}
