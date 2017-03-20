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
	Hollow,
	count,
};

struct Frustum
{
	Plane planes[6];
};

#define RENDER_MASK_SHADOW ((RenderMask)(1 << 15))
#define RENDER_MASK_DEFAULT ((RenderMask)-1)
#define RENDER_CLIP_PLANE_MAX 4

// material indices
// these are alpha values stored in the g-buffer

// material not affected by override lights
#define MATERIAL_NO_OVERRIDE 0.0f
#define MATERIAL_INACCESSIBLE (1.0f / 255.0f)

enum CameraFlags
{
	CameraFlagActive = 1 << 0,
	CameraFlagColors = 1 << 1,
	CameraFlagFog = 1 << 2,
	CameraFlagCullBehindWall = 1 << 3,
};

struct Camera
{
	static const s32 max_cameras = 8;
	static PinArray<Camera, max_cameras> list;

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

	static s32 active_count();
	static Camera* add(s8);

	Mat4 projection;
	Mat4 projection_inverse;
	Quat rot;
	Rect2 viewport;
	Plane frustum[4];
	Vec3 frustum_rays[4];
	Plane clip_planes[RENDER_CLIP_PLANE_MAX];
	Vec3 pos;
	Vec3 range_center;
	r32 cull_range;
	r32 near_plane;
	r32 far_plane;
	r32 range;
	s32 flags;
	Revision revision;
	s8 team;
	s8 gamepad;

	Camera(s8 = 0);
	~Camera();

	void remove();

	inline b8 flag(s32 flag) const
	{
		return b8(flags & flag);
	}

	inline void flag(s32 flag, b8 value)
	{
		if (value)
			flags |= flag;
		else
			flags &= ~flag;
	}

	void perspective(r32, r32, r32, r32);
	void orthographic(r32, r32, r32, r32);
	b8 visible_sphere(const Vec3&, r32) const;
	void update_frustum();
	Mat4 view() const;

	inline ID id() const
	{
		return ID(this - &list[0]);
	}
};

struct LoopSync : RenderSync
{
	b8 quit;
	GameTime time;
	InputState input;
};

typedef Sync<LoopSync>::Swapper LoopSwapper;

enum RenderFlags
{
	RenderFlagEdges = 1 << 0,
	RenderFlagBackFace = 1 << 1,
};

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
	s32 flags;

	RenderParams()
		: camera(),
		view(),
		view_projection(),
		technique(),
		sync(),
		depth_buffer(AssetNull),
		shadow_buffer(AssetNull),
		shadow_vp(),
		flags()
	{
	}
};

}
