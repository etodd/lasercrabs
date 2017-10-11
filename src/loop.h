#include "types.h"
#include "vi_assert.h"

#include "render/views.h"
#include "render/render.h"
#include "data/entity.h"
#include "data/components.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/texture.h"
#include "physics.h"
#include "render/ui.h"
#include "input.h"
#include "mersenne/mersenne-twister.h"
#include <time.h>
#include "platform/util.h"
#include "noise.h"
#include "settings.h"
#include "game/team.h"
#include "game/entities.h"
#include "net.h"

#if DEBUG
	#define DEBUG_RENDER 0
#endif

#include "game/game.h"

namespace VI
{

namespace Loop
{

#define SHADOW_MAP_CASCADES 3
#define SHADOW_MAP_CASCADE_TRI_THRESHOLD 110.0f // if the far plane is farther than this, then we need three shadow map cascades

const s32 shadow_map_size[s32(Settings::ShadowQuality::count)][SHADOW_MAP_CASCADES] =
{
	{ // Off
		32, // detail
		32, // detail level 2
		32, // global
	},
	{ // Medium
		1024, // detail
		1024, // detail level 2
		1024, // global
	},
	{ // High
		2048, // detail
		2048, // detail level 2
		2048, // global
	},
};

Settings::ShadowQuality shadow_quality_current = Settings::ShadowQuality::count;
DisplayMode resolution_current;
AssetID g_albedo_buffer;
AssetID g_normal_buffer;
AssetID g_depth_buffer;
AssetID g_fbo;
AssetID g_albedo_fbo;
AssetID color1_buffer;
AssetID color1_fbo;
AssetID lighting_buffer;
AssetID lighting_fbo;
AssetID color2_buffer;
AssetID color2_depth_buffer;
AssetID color2_fbo;
AssetID shadow_buffer[SHADOW_MAP_CASCADES];
AssetID shadow_fbo[SHADOW_MAP_CASCADES];
AssetID half_depth_buffer;
AssetID half_buffer1;
AssetID half_fbo1;
AssetID half_buffer2;
AssetID half_fbo2;
AssetID half_fbo3;
AssetID ui_buffer;
AssetID ui_depth_buffer;
AssetID ui_fbo;

b8 draw_far_shadow_cascade = true;
Camera far_shadow_cascade_camera;

Mat4 relative_shadow_vp(const Camera& main_camera, const Camera& shadow_camera)
{
	Camera view_offset_camera = shadow_camera;

	view_offset_camera.pos = shadow_camera.pos - main_camera.pos;
	
	return view_offset_camera.view() * shadow_camera.projection;
}

void render_shadows(LoopSync* sync, s32 fbo, const Camera& main_camera, const Camera& shadow_camera)
{
	// render shadows
	sync->write(RenderOp::BindFramebuffer);
	sync->write<AssetID>(fbo);

	RenderParams shadow_render_params;
	shadow_render_params.sync = sync;

	sync->write(RenderOp::Viewport);
	sync->write<Rect2>(shadow_camera.viewport);

	sync->write(RenderOp::Clear);
	sync->write(false); // don't clear color
	sync->write(true); // clear depth

	shadow_render_params.camera = &shadow_camera;
	shadow_render_params.view = shadow_camera.view();

	shadow_render_params.view_projection = shadow_render_params.view * shadow_camera.projection;
	shadow_render_params.technique = RenderTechnique::Shadow;

	Game::draw_opaque(shadow_render_params);
}

void render_point_light(const RenderParams& render_params, const Vec3& pos, r32 radius, PointLight::Type type, const Vec3& color, s8 team)
{
	if (!render_params.camera->visible_sphere(pos, radius))
		return;
	
	RenderSync* sync = render_params.sync;

	Mat4 light_transform = Mat4::make_translation(pos);
	light_transform.scale(Vec3(radius));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::light_pos);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(1);
	sync->write<Vec3>((render_params.view * Vec4(pos, 1)).xyz());

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(light_transform * render_params.view_projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::type);
	sync->write(RenderDataType::S32);
	sync->write<s32>(1);
	sync->write<s32>(s32(type));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::light_color);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(1);
	if (team == s8(AI::TeamNone))
	{
		if (render_params.camera->flag(CameraFlagColors))
			sync->write<Vec3>(color);
		else
			sync->write<Vec3>(LMath::desaturate(color));
	}
	else
		sync->write<Vec3>(Team::color(AI::Team(render_params.camera->team), AI::Team(team)).xyz());

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::light_radius);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(radius);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(Asset::Mesh::sphere);
}

void render_point_lights(const RenderParams& render_params, s32 type_mask, const Vec2& inv_buffer_size, s16 team_mask)
{
	LoopSync* sync = render_params.sync;

	Loader::shader_permanent(Asset::Shader::point_light);

	sync->write(RenderOp::Shader);
	sync->write<AssetID>(Asset::Shader::point_light);
	sync->write(RenderTechnique::Default);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::p);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(render_params.camera->projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::normal_buffer);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write(RenderTextureType::Texture2D);
	sync->write<AssetID>(g_normal_buffer);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::depth_buffer);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write(RenderTextureType::Texture2D);
	sync->write<AssetID>(g_depth_buffer);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_offset);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(render_params.camera->viewport.pos * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_scale);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(render_params.camera->viewport.size * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::frustum);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(4);
	sync->write<Vec3>(render_params.camera->frustum_rays, 4);

	Loader::mesh_permanent(Asset::Mesh::sphere);
	for (auto i = PointLight::list.iterator(); !i.is_last(); i.next())
	{
		PointLight* light = i.item();
		if (!(s32(light->type) & type_mask) || !(light->mask & render_params.camera->mask))
			continue;

		if (light->team != (s8)AI::TeamNone && !((1 << light->team) & team_mask))
			continue;

		render_point_light(render_params, light->get<Transform>()->to_world(light->offset), light->radius, light->type, light->color, light->team);
	}

	if (render_params.camera->mask & RENDER_MASK_DEFAULT)
	{
		for (auto i = EffectLight::list.iterator(); !i.is_last(); i.next())
		{
			if ((i.item()->type == EffectLight::Type::Spark || i.item()->type == EffectLight::Type::BoltDroneBolter || i.item()->type == EffectLight::Type::Grenade) && s32(PointLight::Type::Normal) & type_mask)
				render_point_light(render_params, i.item()->absolute_pos(), i.item()->radius(), PointLight::Type::Normal, Vec3(i.item()->opacity()), AI::TeamNone);
			else if (i.item()->type == EffectLight::Type::Shockwave && s32(PointLight::Type::Shockwave) & type_mask)
				render_point_light(render_params, i.item()->absolute_pos(), i.item()->radius(), PointLight::Type::Shockwave, Vec3(i.item()->opacity()), AI::TeamNone);
		}
	}
}

void render_spot_lights(const RenderParams& render_params, s32 fbo, RenderBlendMode blend_mode, const Vec2& inv_buffer_size, const Mat4& inverse_view_rotation_only, s16 team_mask)
{
	LoopSync* sync = render_params.sync;

	sync->write(RenderOp::BlendMode);
	sync->write(RenderBlendMode::Opaque);
	sync->write(RenderOp::CullMode);
	sync->write(RenderCullMode::Back);

	for (auto i = SpotLight::list.iterator(); !i.is_last(); i.next())
	{
		SpotLight* light = i.item();
		if (!(light->mask & render_params.camera->mask))
			continue;

		if (light->team != s8(AI::TeamNone) && !((1 << light->team) & team_mask))
			continue;

		if (light->color.length_squared() == 0.0f || light->fov == 0.0f || light->radius == 0.0f)
			continue;

		Vec3 abs_pos;
		Quat abs_rot;
		light->get<Transform>()->absolute(&abs_pos, &abs_rot);

		{
			Vec3 center = abs_pos + (abs_rot * Vec3(0, 0, light->radius * 0.5f));
			r32 fov_size = light->radius * tanf(light->fov * 0.5f);
			Vec3 corner(fov_size, fov_size, light->radius);
			r32 radius = (corner - center).length();
			if (!render_params.camera->visible_sphere(center, radius))
				continue;
		}

		Mat4 light_vp;

		{
			sync->write(RenderOp::DepthMask);
			sync->write(true);
			sync->write(RenderOp::DepthTest);
			sync->write(true);

			Camera shadow_camera;
			shadow_camera.viewport =
			{
				Vec2(0, 0),
				Vec2(shadow_map_size[s32(Settings::shadow_quality)][0], shadow_map_size[s32(Settings::shadow_quality)][0]),
			};
			shadow_camera.perspective(light->fov, 0.1f, light->radius);
			shadow_camera.pos = abs_pos;
			shadow_camera.rot = abs_rot;
			render_shadows(sync, shadow_fbo[0], *render_params.camera, shadow_camera);
			light_vp = relative_shadow_vp(*render_params.camera, shadow_camera);

			sync->write(RenderOp::DepthMask);
			sync->write(false);
			sync->write(RenderOp::DepthTest);
			sync->write(false);
		}

		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(fbo);

		sync->write(RenderOp::BlendMode);
		sync->write(blend_mode);
		sync->write(RenderOp::CullMode);
		sync->write(RenderCullMode::Front);

		sync->write(RenderOp::Viewport);
		sync->write<Rect2>(render_params.camera->viewport);

		Loader::shader_permanent(Asset::Shader::spot_light);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::spot_light);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::uv_offset);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(render_params.camera->viewport.pos * inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::uv_scale);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(render_params.camera->viewport.size * inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(render_params.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::normal_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_normal_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_depth_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_pos);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		sync->write<Vec3>((render_params.view * Vec4(abs_pos, 1)).xyz());

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_vp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(inverse_view_rotation_only * light_vp);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_color);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		if (light->team == (s8)AI::TeamNone)
		{
			if (render_params.camera->flag(CameraFlagColors))
				sync->write<Vec3>(light->color);
			else
				sync->write<Vec3>(LMath::desaturate(light->color));
		}
		else
			sync->write<Vec3>(Team::color((AI::Team)light->team, (AI::Team)render_params.camera->team).xyz());

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_radius);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(light->radius);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_direction);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		sync->write<Vec3>((render_params.view * Vec4(abs_rot * Vec3(0, 0, -1), 0)).xyz());

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_fov_dot);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(cosf(light->fov));

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::shadow_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(shadow_buffer[0]);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		Mat4 light_transform;
		r32 width_scale = sinf(light->fov) * light->radius * 2.0f;
		Vec3 light_model_scale
		(
			width_scale,
			width_scale,
			cosf(light->fov) * light->radius * 2.0f
		);
		light_transform.make_transform(abs_pos, light_model_scale, abs_rot);
		sync->write<Mat4>(light_transform * render_params.view_projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::frustum);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(4);
		sync->write<Vec3>(render_params.camera->frustum_rays, 4);

		Loader::mesh_permanent(Asset::Mesh::cone);
		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::cone);

		sync->write(RenderOp::BlendMode);
		sync->write(RenderBlendMode::Opaque);
		sync->write(RenderOp::CullMode);
		sync->write(RenderCullMode::Back);
	}
}

void draw_edges(const RenderParams& render_params)
{
	RenderSync* sync = render_params.sync;
	sync->write(RenderOp::FillMode);
	sync->write(RenderFillMode::Line);

	sync->write(RenderOp::LineWidth);
	sync->write(vi_max(1.0f, 2.5f * UI::scale));

	{
		RenderParams p = render_params;
		p.technique = RenderTechnique::Shadow;
		p.flags |= RenderFlagEdges;
		Game::draw_opaque(p);
		Game::draw_hollow(p);
	}

	sync->write(RenderOp::FillMode);
	sync->write(RenderFillMode::Fill);

	Game::draw_particles(render_params);

	sync->write(RenderOp::DepthTest);
	sync->write(false);
}

void draw_alpha(const RenderParams& render_params)
{
	RenderSync* sync = render_params.sync;
	sync->write(RenderOp::BlendMode);
	sync->write(RenderBlendMode::Alpha);

	sync->write(RenderOp::CullMode);
	sync->write(RenderCullMode::None);

	sync->write(RenderOp::DepthMask);
	sync->write(false);

	sync->write(RenderOp::DepthTest);
	sync->write(true);

	Game::draw_alpha(render_params);

	sync->write(RenderOp::BlendMode);
	sync->write(RenderBlendMode::Additive);

	Game::draw_additive(render_params);

	sync->write(RenderOp::CullMode);
	sync->write(RenderCullMode::Back);

	sync->write(RenderOp::BlendMode);
	sync->write(RenderBlendMode::Alpha);
}

void shadow_quality_apply()
{
	if (shadow_quality_current != Settings::shadow_quality)
	{
		for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
			Loader::dynamic_texture_redefine(shadow_buffer[i], shadow_map_size[s32(Settings::shadow_quality)][i], shadow_map_size[s32(Settings::shadow_quality)][i], RenderDynamicTextureType::Depth, RenderTextureWrap::Clamp, RenderTextureFilter::Linear, RenderTextureCompare::RefToTexture);

		shadow_quality_current = Settings::shadow_quality;
	}
}

void draw(LoopSync* sync, const Camera* camera)
{
	RenderParams render_params;
	render_params.sync = sync;

	render_params.camera = camera;
	render_params.view = camera->view();
	render_params.view_projection = render_params.view * camera->projection;
	render_params.technique = RenderTechnique::Default;

	Rect2 half_viewport =
	{
		Vec2(s32(camera->viewport.pos.x * 0.5f), s32(camera->viewport.pos.y * 0.5f)),
		Vec2(s32(camera->viewport.size.x * 0.5f), s32(camera->viewport.size.y * 0.5f)),
	};

	Mat4 inverse_view = render_params.view.inverse();
	Mat4 inverse_view_rotation_only = inverse_view;
	inverse_view_rotation_only.translation(Vec3::zero);

	const Vec3* frustum = render_params.camera->frustum_rays;

	Vec2 buffer_size(Settings::display().width, Settings::display().height);
	Vec2 inv_buffer_size = 1.0f / buffer_size;
	Vec2 inv_half_buffer_size = inv_buffer_size * 2.0f;

	Rect2 screen_quad_uv =
	{
		camera->viewport.pos / Vec2(Settings::display().width, Settings::display().height),
		camera->viewport.size / Vec2(Settings::display().width, Settings::display().height),
	};
	Game::screen_quad.set
	(
		sync,
		{ Vec2(-1, -1), Vec2(2, 2) },
		camera,
		screen_quad_uv
	);

	UI::update(render_params);

	sync->write(RenderOp::Viewport);
	sync->write<Rect2>({ camera->viewport.pos, camera->viewport.size });

	// fill G buffer
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(g_fbo);

		sync->write(RenderOp::Clear);
		sync->write(true); // clear color
		sync->write(true); // clear depth

		Game::draw_opaque(render_params);
	}

	// render override lights
	if (!render_params.camera->flag(CameraFlagColors))
	{
		sync->write(RenderOp::DepthTest);
		sync->write(false);
		sync->write(RenderOp::DepthMask);
		sync->write(false);

		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(g_albedo_fbo);

		sync->write(RenderOp::CullMode);
		sync->write(RenderCullMode::Front);
		sync->write(RenderOp::BlendMode);
		sync->write(RenderBlendMode::AlphaDestination);

		if (camera->team == s8(-1))
		{
			// render all override lights
			render_point_lights(render_params, s32(PointLight::Type::Override), inv_buffer_size, -1);
		}
		else
		{
			// render our team lights first
			render_point_lights(render_params, s32(PointLight::Type::Override), inv_buffer_size, 1 << camera->team);

			// render other team lights
			render_point_lights(render_params, s32(PointLight::Type::Override), inv_buffer_size, ~(1 << camera->team));
		}

		sync->write(RenderOp::CullMode);
		sync->write(RenderCullMode::Back);

		sync->write(RenderOp::DepthMask);
		sync->write(true);
		sync->write(RenderOp::DepthTest);
		sync->write(true);
	}

	// regular lighting
	{
		{
			// global light (directional and player lights)
			Vec3 colors[MAX_DIRECTIONAL_LIGHTS] = {};
			Vec3 directions[MAX_DIRECTIONAL_LIGHTS] = {};
			Vec3 abs_directions[MAX_DIRECTIONAL_LIGHTS] = {};
			b8 shadowed = false;
			for (s32 i = 0; i < Game::level.directional_lights.length; i++)
			{
				const DirectionalLight& light = Game::level.directional_lights[i];

				colors[i] = render_params.camera->flag(CameraFlagColors) ? light.color : LMath::desaturate(light.color);
				abs_directions[i] = light.rot * Vec3(0, 1, 0);
				directions[i] = (render_params.view * Vec4(abs_directions[i], 0)).xyz();
				if (Settings::shadow_quality != Settings::ShadowQuality::Off && light.shadowed)
				{
					if (i > 0 && !shadowed)
					{
						Vec3 tmp;
						tmp = colors[0];
						colors[0] = colors[i];
						colors[i] = tmp;
						tmp = directions[0];
						directions[0] = directions[i];
						directions[i] = tmp;
						tmp = abs_directions[0];
						abs_directions[0] = abs_directions[i];
						abs_directions[i] = tmp;
					}
					shadowed = true;
				}
			}

			Mat4 detail_light_vp;
			Mat4 detail2_light_vp;

			if (shadowed)
			{
				// global shadow map
				Camera shadow_camera;
				r32 size = vi_min(800.0f, render_params.camera->far_plane * 1.5f);
				Vec3 pos = render_params.camera->pos;
				const r32 interval = 2.0f;
				pos = Vec3(s32(pos.x / interval), s32(pos.y / interval), s32(pos.z / interval)) * interval;
				r32 depth = vi_min(400.0f, size * 2.0f);
				shadow_camera.pos = pos + (abs_directions[0] * depth * -0.25f);
				shadow_camera.rot = Quat::look(abs_directions[0]);
				if (render_params.camera->mask == 0)
					shadow_camera.mask = 0;
				else
					shadow_camera.mask = RENDER_MASK_SHADOW;

				if (draw_far_shadow_cascade || Camera::list.count() > 1 || SpotLight::list.count() > 0) // only draw far shadow cascade every other frame, if we can
				{
					shadow_camera.viewport =
					{
						Vec2(0, 0),
						Vec2(shadow_map_size[s32(Settings::shadow_quality)][2], shadow_map_size[s32(Settings::shadow_quality)][2]),
					};
					shadow_camera.orthographic(size, size, 1.0f, depth);
					far_shadow_cascade_camera = shadow_camera;
					render_shadows(sync, shadow_fbo[2], *render_params.camera, shadow_camera);
				}
				draw_far_shadow_cascade = !draw_far_shadow_cascade;

				render_params.shadow_vp = relative_shadow_vp(*render_params.camera, far_shadow_cascade_camera);
				if (Settings::volumetric_lighting)
					render_params.shadow_buffer = shadow_buffer[2]; // skybox needs this for volumetric lighting

				// detail level 2 shadow map
				if (render_params.camera->far_plane > SHADOW_MAP_CASCADE_TRI_THRESHOLD)
				{
					shadow_camera.viewport =
					{
						Vec2(0, 0),
						Vec2(shadow_map_size[s32(Settings::shadow_quality)][1], shadow_map_size[s32(Settings::shadow_quality)][1]),
					};
					shadow_camera.orthographic(100.0f, 100.0f, 1.0f, depth);

					render_shadows(sync, shadow_fbo[1], *render_params.camera, shadow_camera);
					detail2_light_vp = relative_shadow_vp(*render_params.camera, shadow_camera);
				}

				// detail shadow map
				{
					shadow_camera.viewport =
					{
						Vec2(0, 0),
						Vec2(shadow_map_size[s32(Settings::shadow_quality)][0], shadow_map_size[s32(Settings::shadow_quality)][0]),
					};
					shadow_camera.orthographic(20.0f, 20.0f, 1.0f, depth);

					render_shadows(sync, shadow_fbo[0], *render_params.camera, shadow_camera);
					detail_light_vp = relative_shadow_vp(*render_params.camera, shadow_camera);
				}

				sync->write(RenderOp::Viewport);
				sync->write<Rect2>(camera->viewport);
			}

			sync->write(RenderOp::BlendMode);
			sync->write(RenderBlendMode::Opaque);

			sync->write(RenderOp::BindFramebuffer);
			sync->write<AssetID>(lighting_fbo);

			sync->write(RenderOp::Clear);
			sync->write(true); // clear color
			sync->write(true); // clear depth

			Loader::shader_permanent(Asset::Shader::global_light);

			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::global_light);
			sync->write(shadowed ? RenderTechnique::Shadow : RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::p);
			sync->write(RenderDataType::Mat4);
			sync->write<s32>(1);
			sync->write<Mat4>(render_params.camera->projection);

			// player light settings
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::camera_light_strength);
			sync->write(RenderDataType::R32);
			sync->write<s32>(1);
			sync->write(render_params.camera->flag(CameraFlagColors) ? 0.25f : 0.7f);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::camera_light_radius);
			sync->write(RenderDataType::R32);
			sync->write<s32>(1);
			sync->write(render_params.camera->flag(CameraFlagColors) ? 6.0f : render_params.camera->range);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::range);
			sync->write(RenderDataType::R32);
			sync->write<s32>(1);
			sync->write<r32>(render_params.camera->range);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::range_center);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(1);
			sync->write<Vec3>(render_params.camera->range_center);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::far_plane);
			sync->write(RenderDataType::R32);
			sync->write<s32>(1);
			sync->write<r32>(Game::level.skybox.far_plane);

			if (shadowed)
			{
				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::light_vp);
				sync->write(RenderDataType::Mat4);
				sync->write<s32>(1);
				sync->write<Mat4>(inverse_view_rotation_only * render_params.shadow_vp);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::shadow_map);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write(RenderTextureType::Texture2D);
				sync->write<AssetID>(shadow_buffer[2]);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::detail_light_vp);
				sync->write(RenderDataType::Mat4);
				sync->write<s32>(1);
				sync->write<Mat4>(inverse_view_rotation_only * detail_light_vp);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::detail_shadow_map);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write(RenderTextureType::Texture2D);
				sync->write<AssetID>(shadow_buffer[0]);

				if (render_params.camera->far_plane > SHADOW_MAP_CASCADE_TRI_THRESHOLD)
				{
					// three cascades
					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::tri_shadow_cascade);
					sync->write(RenderDataType::S32);
					sync->write<s32>(1);
					sync->write<s32>(1);

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::detail2_light_vp);
					sync->write(RenderDataType::Mat4);
					sync->write<s32>(1);
					sync->write<Mat4>(inverse_view_rotation_only * detail2_light_vp);

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::detail2_shadow_map);
					sync->write(RenderDataType::Texture);
					sync->write<s32>(1);
					sync->write(RenderTextureType::Texture2D);
					sync->write<AssetID>(shadow_buffer[1]);
				}
				else
				{
					// two cascades
					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::tri_shadow_cascade);
					sync->write(RenderDataType::S32);
					sync->write<s32>(1);
					sync->write<s32>(0);

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::detail2_shadow_map);
					sync->write(RenderDataType::Texture);
					sync->write<s32>(1);
					sync->write(RenderTextureType::Texture2D);
					sync->write<AssetID>(AssetNull);
				}

				Loader::texture_permanent(Asset::Texture::clouds);

				const Clouds::Config* cloud_shadow = nullptr;
				for (s32 i = 0; i < Game::level.clouds.length; i++)
				{
					const Clouds::Config& cloud = Game::level.clouds[i];
					if (cloud.shadow > 0.0f)
					{
						cloud_shadow = &cloud;
						break;
					}
				}

				Loader::texture_permanent(Asset::Texture::clouds);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::cloud_map);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write(RenderTextureType::Texture2D);
				sync->write<AssetID>(Asset::Texture::clouds);

				if (cloud_shadow)
				{
					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::cloud_uv_offset);
					sync->write(RenderDataType::Vec2);
					sync->write<s32>(1);
					Vec2 uv;
					{
						// HACK
						uv = cloud_shadow->uv_offset(render_params);
						Vec3 uv3 = Vec3(uv.x, 0, uv.y);
						uv3 = Quat::euler(0, PI * -0.5f, 0) * uv3;
						uv.x = uv3.x;
						uv.y = uv3.z;
					}
					sync->write<Vec2>(uv);

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::v);
					sync->write(RenderDataType::Mat4);
					sync->write<s32>(1);
					sync->write<Mat4>(render_params.camera->view().inverse());

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::cloud_inv_uv_scale);
					sync->write(RenderDataType::R32);
					sync->write<s32>(1);
					sync->write<r32>(1.0f / (render_params.camera->far_plane * cloud_shadow->scale));

					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::cloud_alpha);
					sync->write(RenderDataType::R32);
					sync->write<s32>(1);
					sync->write<r32>(cloud_shadow->shadow);
				}
				else
				{
					sync->write(RenderOp::Uniform);
					sync->write(Asset::Uniform::cloud_alpha);
					sync->write(RenderDataType::R32);
					sync->write<s32>(1);
					sync->write<r32>(0.0f);
				}
			}

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(g_normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(g_depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_color);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(MAX_DIRECTIONAL_LIGHTS);
			sync->write<Vec3>(colors, MAX_DIRECTIONAL_LIGHTS);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_direction);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(MAX_DIRECTIONAL_LIGHTS);
			sync->write<Vec3>(directions, MAX_DIRECTIONAL_LIGHTS);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}

		{
			// point lights
			sync->write(RenderOp::CullMode);
			sync->write(RenderCullMode::Front);
			sync->write(RenderOp::BlendMode);
			sync->write(RenderBlendMode::Additive);

			render_point_lights(render_params, s32(PointLight::Type::Normal) | s32(PointLight::Type::Shockwave), inv_buffer_size, -1);
		}

		{
			// spot lights
			render_spot_lights(render_params, lighting_fbo, RenderBlendMode::Additive, inv_buffer_size, inverse_view_rotation_only, -1);
		}

	}

	// SSAO
	if (Settings::ssao)
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(half_fbo1);

		sync->write(RenderOp::Viewport);
		sync->write<Rect2>(half_viewport);

		// downsample
		{
			sync->write(RenderOp::Clear);
			sync->write(true); // clear color
			sync->write(true); // clear depth

			Loader::shader_permanent(Asset::Shader::ssao_downsample);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao_downsample);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(g_normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(g_depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(inv_buffer_size);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}

		// SSAO
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<AssetID>(half_fbo2);

			Loader::shader_permanent(Asset::Shader::ssao);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(half_buffer1);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<AssetID>(half_depth_buffer);

			Loader::texture_permanent(Asset::Texture::noise, RenderTextureWrap::Repeat, RenderTextureFilter::Nearest);
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::noise_sampler);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(Asset::Texture::noise);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::p);
			sync->write(RenderDataType::Mat4);
			sync->write<s32>(1);
			sync->write<Mat4>(render_params.camera->projection);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::uv_offset);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(camera->viewport.pos * inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_uv_scale);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(Vec2(1, 1) / (camera->viewport.size * inv_buffer_size));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::far_plane);
			sync->write(RenderDataType::R32);
			sync->write<s32>(1);
			sync->write<r32>(camera->far_plane);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::frustum);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(4);
			sync->write<Vec3>(frustum, 4);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}

		sync->write(RenderOp::DepthMask);
		sync->write<b8>(false);
		sync->write(RenderOp::DepthTest);
		sync->write<b8>(false);

		// horizontal blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<AssetID>(half_fbo1);

			Loader::shader_permanent(Asset::Shader::ssao_blur);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao_blur);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(Vec2(inv_half_buffer_size.x, 0));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::color_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(half_buffer2);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(half_depth_buffer);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}

		// vertical blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<AssetID>(half_fbo2);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(Vec2(0, inv_half_buffer_size.y));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::color_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<AssetID>(half_buffer1);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}
	}

	// post processing

	sync->write(RenderOp::Viewport);
	sync->write<Rect2>(camera->viewport);

	// composite
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(color2_fbo);

		sync->write(RenderOp::DepthMask);
		sync->write(true);
		sync->write(RenderOp::DepthTest);
		sync->write(true);

		sync->write(RenderOp::Clear);
		sync->write(true); // clear color
		sync->write(true); // clear depth

		Loader::shader_permanent(Asset::Shader::composite);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::composite);
		sync->write(Settings::ssao ? RenderTechnique::Shadow : RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::range);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(render_params.camera->range);

		if (camera->range > 0.0f)
		{
			// we need to mark unreachable and out-of-range areas
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::range_center);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(1);
			sync->write<Vec3>(render_params.camera->range_center);
		}

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(render_params.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::ambient_color);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		if (camera->flag(CameraFlagColors))
			sync->write<Vec3>(Game::level.ambient_color);
		else
			sync->write<Vec3>(LMath::desaturate(Game::level.ambient_color));

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::ssao_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::lighting_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(lighting_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_albedo_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_depth_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);
	}

	// scene is in color2 at this point

	// edges and UI
	{
		if (!Settings::antialiasing)
		{
			render_params.depth_buffer = color2_depth_buffer;
			draw_alpha(render_params);
			draw_edges(render_params); // draw edges directly on scene
		}

		sync->write(RenderOp::DepthTest);
		sync->write(false);

		// render into UI buffer, blit to color1, overlay on top of color2
		sync->write(RenderOp::BindFramebuffer);
		sync->write(ui_fbo);

		if (Settings::antialiasing)
		{
			sync->write(RenderOp::DepthTest);
			sync->write(true);
			sync->write(RenderOp::DepthMask);
			sync->write(true);
		}

		sync->write(RenderOp::Clear);
		sync->write(true);
		sync->write(Settings::antialiasing);

		if (Settings::antialiasing)
		{
			// restore depth buffer
			sync->write(RenderOp::ColorMask);
			sync->write<RenderColorMask>(0);

			{
				RenderParams p = render_params;
				p.flags |= RenderFlagPolygonOffset;
				Game::draw_opaque(p);
			}

			sync->write(RenderOp::ColorMask);
			sync->write<RenderColorMask>(RENDER_COLOR_MASK_DEFAULT);
			sync->write(RenderOp::DepthMask);
			sync->write(false);

			// copy color2 to ui_fbo
			{
				Loader::shader_permanent(Asset::Shader::blit);
				sync->write(RenderOp::Shader);
				sync->write<AssetID>(Asset::Shader::blit);
				sync->write(RenderTechnique::Default);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::color_buffer);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write(RenderTextureType::Texture2D);
				sync->write<AssetID>(color2_buffer);

				sync->write(RenderOp::Mesh);
				sync->write(RenderPrimitiveMode::Triangles);
				sync->write(Game::screen_quad.mesh);
			}

			sync->write(RenderOp::BlendMode);
			sync->write(RenderBlendMode::Alpha);

			render_params.depth_buffer = color2_depth_buffer;
			draw_alpha(render_params);
			draw_edges(render_params);

			// stencil out back faces
			if (render_params.camera->cull_range > 0.0f)
			{
				Loader::shader_permanent(Asset::Shader::stencil_back_faces);
				sync->write(RenderOp::Shader);
				sync->write<AssetID>(Asset::Shader::stencil_back_faces);
				sync->write(RenderTechnique::Default);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::normal_buffer);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write(RenderTextureType::Texture2D);
				sync->write<AssetID>(g_normal_buffer);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::inv_buffer_size);
				sync->write(RenderDataType::Vec2);
				sync->write<s32>(1);
				sync->write<Vec2>(inv_buffer_size);

				sync->write(RenderOp::Mesh);
				sync->write(RenderPrimitiveMode::Triangles);
				sync->write(Game::screen_quad.mesh);
			}

			Game::draw_alpha_late(render_params);
		}
		
		UI::draw(render_params);

		if (Settings::antialiasing)
		{
			// blit to color2
			sync->write(RenderOp::BindFramebuffer);
			sync->write(color2_fbo);

			sync->write(RenderOp::Clear);
			sync->write(true);
			sync->write(false);

			sync->write(RenderOp::BlitFramebuffer);
			sync->write(ui_fbo);
			sync->write(camera->viewport); // source
			sync->write(camera->viewport); // destination
		}
		else
		{
			// blit to color1
			sync->write(RenderOp::BindFramebuffer);
			sync->write(color1_fbo);

			sync->write(RenderOp::Clear);
			sync->write(true);
			sync->write(false);

			sync->write(RenderOp::BlitFramebuffer);
			sync->write(ui_fbo);
			sync->write(camera->viewport); // source
			sync->write(camera->viewport); // destination

			// overlay on to color2
			sync->write(RenderOp::BindFramebuffer);
			sync->write(color2_fbo);

			Loader::shader_permanent(Asset::Shader::blit);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::blit);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::color_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write(RenderTextureType::Texture2D);
			sync->write<AssetID>(color1_buffer);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(Game::screen_quad.mesh);
		}
	}

	// scene is in color2

	// bloom
	{
		// downsample
		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(half_fbo1);

		sync->write(RenderOp::BlendMode);
		sync->write(RenderBlendMode::Opaque);

		sync->write(RenderOp::Viewport);
		sync->write<Rect2>(half_viewport);

		Loader::shader_permanent(Asset::Shader::bloom_downsample);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::bloom_downsample);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(color2_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(inv_buffer_size);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);

		// blur x
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<AssetID>(half_fbo3);

		Loader::shader_permanent(Asset::Shader::blur);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::blur);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(Vec2(inv_half_buffer_size.x, 0));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);

		// blur y
		sync->write(RenderOp::BindFramebuffer);
		sync->write<AssetID>(half_fbo2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(Vec2(0, inv_half_buffer_size.y));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);
	}

	sync->write(RenderOp::BindFramebuffer);
	sync->write(AssetNull);

	sync->write(RenderOp::Viewport);
	sync->write<Rect2>(camera->viewport);

	// copy color2 to back buffer
	{
		Loader::shader_permanent(Asset::Shader::blit);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::blit);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(color2_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);
	}

	// composite bloom
	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Additive);
	{
		Loader::shader_permanent(Asset::Shader::blit);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::blit);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer2);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);
	}

	if (Settings::scan_lines)
	{
		// scan lines
		Loader::shader_permanent(Asset::Shader::scan_lines);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::scan_lines);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(render_params.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::scan_line_interval);
		sync->write(RenderDataType::S32);
		sync->write<s32>(1);
		if (UI::scale < 1.0f)
			sync->write<s32>(3);
		else if (UI::scale == 1.0f)
			sync->write<s32>(4);
		else
			sync->write<s32>(5);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_depth_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::normal_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write(RenderTextureType::Texture2D);
		sync->write<AssetID>(g_normal_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::range);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		if (render_params.camera->range == 0.0f)
			sync->write<r32>(render_params.camera->far_plane);
		else
			sync->write<r32>(render_params.camera->range * 2.0f);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::time);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(Game::real_time.total);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Game::screen_quad.mesh);
	}

	sync->write(RenderOp::BlendMode);
	sync->write(RenderBlendMode::Opaque);

#if DEBUG && DEBUG_RENDER
	// Debug render buffers
	Loader::shader_permanent(Asset::Shader::debug_depth);
	Vec2 debug_buffer_size = camera->viewport.size / 4;
	UI::texture(render_params, color1_buffer, { Vec2(debug_buffer_size.x * 0, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, g_normal_buffer, { Vec2(debug_buffer_size.x * 1, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, lighting_buffer, { Vec2(debug_buffer_size.x * 2, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, g_depth_buffer, { Vec2(debug_buffer_size.x * 3, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv, Asset::Shader::debug_depth);
	UI::texture(render_params, color2_buffer, { Vec2(debug_buffer_size.x * 4, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, half_buffer1, { Vec2(debug_buffer_size.x * 5, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, half_buffer2, { Vec2(debug_buffer_size.x * 6, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, half_depth_buffer, { Vec2(debug_buffer_size.x * 7, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv, Asset::Shader::debug_depth);

	Vec2 debug_buffer_shadow_map_size = Vec2(128.0f * UI::scale);
	for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
		UI::texture(render_params, shadow_buffer[i], { Vec2(debug_buffer_shadow_map_size.x * i, debug_buffer_size.y), debug_buffer_shadow_map_size }, Vec4(1, 1, 1, 1), { Vec2::zero, Vec2(1, 1) }, Asset::Shader::debug_depth);
#endif

	sync->write(RenderOp::DepthMask);
	sync->write(true);
	sync->write(RenderOp::DepthTest);
	sync->write(true);
}

void resolution_apply(const DisplayMode& mode)
{
	if (mode.width != resolution_current.width || mode.height != resolution_current.height)
	{
		Loader::dynamic_texture_redefine(g_albedo_buffer, mode.width, mode.height, RenderDynamicTextureType::Color);
		Loader::dynamic_texture_redefine(g_normal_buffer, mode.width, mode.height, RenderDynamicTextureType::Color);
		Loader::dynamic_texture_redefine(g_depth_buffer, mode.width, mode.height, RenderDynamicTextureType::Depth);

		Loader::dynamic_texture_redefine(ui_buffer, mode.width, mode.height, RenderDynamicTextureType::ColorMultisample);
		Loader::dynamic_texture_redefine(ui_depth_buffer, mode.width, mode.height, RenderDynamicTextureType::DepthMultisample);

		Loader::dynamic_texture_redefine(color1_buffer, mode.width, mode.height, RenderDynamicTextureType::Color);
		Loader::dynamic_texture_redefine(color2_buffer, mode.width, mode.height, RenderDynamicTextureType::Color);
		Loader::dynamic_texture_redefine(color2_depth_buffer, mode.width, mode.height, RenderDynamicTextureType::Depth);

		Loader::dynamic_texture_redefine(lighting_buffer, mode.width, mode.height, RenderDynamicTextureType::Color);

		Loader::dynamic_texture_redefine(half_buffer1, mode.width / 2, mode.height / 2, RenderDynamicTextureType::Color);
		Loader::dynamic_texture_redefine(half_depth_buffer, mode.width / 2, mode.height / 2, RenderDynamicTextureType::Depth);
		Loader::dynamic_texture_redefine(half_buffer2, mode.width / 2, mode.height / 2, RenderDynamicTextureType::Color, RenderTextureWrap::Clamp, RenderTextureFilter::Linear);
	}

	resolution_current = mode;
}

void loop(LoopSwapper* swapper_render, PhysicsSwapper* swapper_physics)
{
	mersenne::srand(platform::timestamp());
	noise::reseed();

	LoopSync* sync_render = swapper_render->swap<SwapType_Write>();

	Loader::init(swapper_render);

	Game::init(sync_render);

	g_albedo_buffer = Loader::dynamic_texture_permanent();
	g_normal_buffer = Loader::dynamic_texture_permanent();
	g_depth_buffer = Loader::dynamic_texture_permanent();

	ui_buffer = Loader::dynamic_texture_permanent();
	ui_depth_buffer = Loader::dynamic_texture_permanent();

	color1_buffer = Loader::dynamic_texture_permanent();
	color2_buffer = Loader::dynamic_texture_permanent();
	color2_depth_buffer = Loader::dynamic_texture_permanent();

	lighting_buffer = Loader::dynamic_texture_permanent();

	for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
		shadow_buffer[i] = Loader::dynamic_texture_permanent();

	half_buffer1 = Loader::dynamic_texture_permanent();
	half_depth_buffer = Loader::dynamic_texture_permanent();
	half_buffer2 = Loader::dynamic_texture_permanent();

	resolution_apply(Settings::display());
	shadow_quality_apply();

	g_fbo = Loader::framebuffer_permanent(3);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, g_albedo_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color1, g_normal_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, g_depth_buffer);

	g_albedo_fbo = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, g_albedo_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, g_depth_buffer);

	ui_fbo = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, ui_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, ui_depth_buffer);

	color1_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color1_buffer);

	color2_fbo = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color2_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, color2_depth_buffer);

	lighting_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, lighting_buffer);

	for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
	{
		shadow_fbo[i] = Loader::framebuffer_permanent(1);
		Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, shadow_buffer[i]);
	}

	half_fbo1 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, half_depth_buffer);

	half_fbo2 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer2);

	half_fbo3 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);

	Game::screen_quad.init(sync_render);

	InputState last_input;

	Update u;
	u.input = &sync_render->input;
	u.last_input = &last_input;

	PhysicsSync* sync_physics = nullptr;

	r32 time_update = 0.0f; // time required for update

	while (!Game::quit)
	{
		// update loop

		Game::quit |= sync_render->quit;

		{
			// limit framerate

			r32 dt_limit;
#if SERVER
			dt_limit = Net::tick_rate();
#else
			dt_limit = vi_max(1.0f / r32(Settings::framerate_limit), u.input->focus ? 0.0f : (1.0f / 30.0f));
#endif

			r32 delay = dt_limit - time_update;
			if (delay > 0)
				platform::sleep(delay);
		}

		r64 time_update_start = platform::time();

		u.input = &sync_render->input;
		u.time = sync_render->time;

#if DEBUG
		if (u.input->keys.get(s32(KeyCode::F5)))
			vi_assert(false);
#endif
		if (sync_physics)
			sync_physics = swapper_physics->next<SwapType_Write>();
		else
			sync_physics = swapper_physics->get();

		Game::update(u);

		sync_physics->time = Game::time;
		sync_physics->timestep = Game::physics_timestep;

		swapper_physics->done<SwapType_Write>();

#if !SERVER
		resolution_apply(Settings::display());
		shadow_quality_apply();

		sync_render->write(RenderOp::Clear);
		sync_render->write(true);
		sync_render->write(true);

		for (auto i = Camera::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->flag(CameraFlagActive))
				draw(sync_render, i.item());
		}
#endif

		if (sync_render->quit)
			break;

		sync_render->quit |= Game::quit;
		sync_render->display_mode = Settings::display();
		sync_render->fullscreen = Settings::fullscreen;
		sync_render->vsync = Settings::vsync;

		memcpy(&last_input, &sync_render->input, sizeof(last_input));

		time_update = r32(platform::time() - time_update_start);

		sync_render = swapper_render->swap<SwapType_Write>();
		sync_render->queue.length = 0;
	}

	{
		PhysicsSync* sync_physics = swapper_physics->next<SwapType_Write>();
		sync_physics->quit = true;
		swapper_physics->done<SwapType_Write>();
	}

	Game::term();
}

}

}
