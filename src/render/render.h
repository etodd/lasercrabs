#pragma once

#include "types.h"
#include "data/array.h"
#include "lmath.h"
#include "sync.h"
#include "data/import_common.h"
#include "input.h"
#include "glvm.h"

namespace VI
{

struct Frustum
{
	Plane planes[6];
};

typedef unsigned short RenderMask;

#define RENDER_MASK_SHADOW (1 << 15)

struct Camera
{
	static const int max_cameras = 8;
	static Camera all[max_cameras];

	RenderMask mask;

	struct ViewportBlueprint
	{
		float x, y, w, h;
	};

	static ViewportBlueprint one_player_viewports[1];
	static ViewportBlueprint two_player_viewports[2];
	static ViewportBlueprint three_player_viewports[3];
	static ViewportBlueprint four_player_viewports[4];

	static ViewportBlueprint* viewport_blueprints[4];

	static Camera* add();

	bool active;
	Mat4 projection;
	Mat4 projection_inverse;
	float near_plane;
	float far_plane;
	bool fog;
	Vec3 pos;
	Quat rot;
	ScreenRect viewport;
	Plane frustum[4];
	Vec3 frustum_rays[4];

	Camera()
		: active(),
		projection(),
		projection_inverse(),
		pos(),
		rot(),
		viewport(),
		near_plane(),
		far_plane(),
		mask((RenderMask)-1),
		fog(true)
	{
	}

	void perspective(float, float, float, float);
	void orthographic(float, float, float, float);
	bool visible_sphere(const Vec3&, float) const;
	void update_frustum();
	Mat4 view() const;
	void remove();
};

struct LoopSync : RenderSync
{
	bool quit;
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
	int depth_buffer;
	int shadow_buffer;
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
