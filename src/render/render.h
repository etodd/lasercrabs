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

struct Camera
{
	static const int max_cameras = 8;
	static Camera all[max_cameras];

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
	Vec3 pos;
	Quat rot;
	ScreenRect viewport;
	Plane frustum[4];
	Vec3 frustum_rays[4];

	Camera()
		: active(), projection(), projection_inverse(), pos(), rot(), viewport(), near_plane(), far_plane()
	{

	}
	void perspective(const float, const float, const float, const float);
	void orthographic(const float, const float, const float, const float);
	bool visible_sphere(const Vec3&, const float) const;
	void update_frustum();
	Mat4 view() const;
	void remove();
};

struct LoopSync : RenderSync
{
	bool quit;
	bool focus;
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
};

}
