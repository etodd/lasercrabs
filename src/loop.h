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

#if DEBUG
	#define DEBUG_RENDER 0
#endif

#include "game/game.h"

namespace VI
{

namespace Loop
{

ScreenQuad screen_quad = ScreenQuad();

#define SHADOW_MAP_CASCADES 2

const s32 shadow_map_size[(s32)Settings::ShadowQuality::count][SHADOW_MAP_CASCADES] =
{
	{
		512, // detail
		512, // global
	},
	{
		1024, // detail
		1024, // global
	},
	{
		2048, // detail
		2048, // global
	},
};

s32 color_buffer;
s32 normal_buffer;
s32 depth_buffer;
s32 color_fbo1;
s32 g_fbo;
s32 lighting_buffer;
s32 lighting_fbo;
s32 color_buffer2;
s32 color_fbo2;
s32 shadow_buffer[SHADOW_MAP_CASCADES];
s32 shadow_fbo[SHADOW_MAP_CASCADES];
s32 half_depth_buffer;
s32 half_buffer1;
s32 half_fbo1;
s32 half_buffer2;
s32 half_fbo2;
s32 half_fbo3;
s32 ui_buffer;
s32 ui_fbo;

b8 draw_far_shadow_cascade = true;
Mat4 far_shadow_cascade_vp;

Mat4 relative_shadow_vp(const Camera& main_camera, const Camera& shadow_camera)
{
	Camera view_offset_camera = shadow_camera;

	view_offset_camera.pos = shadow_camera.pos - main_camera.pos;
	
	return view_offset_camera.view() * shadow_camera.projection;
}

void render_shadows(LoopSync* sync, s32 fbo, const Camera& main_camera, const Camera& shadow_camera)
{
	// Render shadows
	sync->write<RenderOp>(RenderOp::BindFramebuffer);
	sync->write<s32>(fbo);

	RenderParams shadow_render_params;
	shadow_render_params.sync = sync;

	sync->write<RenderOp>(RenderOp::Viewport);
	sync->write<Rect2>(shadow_camera.viewport);

	sync->write<RenderOp>(RenderOp::Clear);
	sync->write<b8>(false); // Don't clear color
	sync->write<b8>(true); // Clear depth

	shadow_render_params.camera = &shadow_camera;
	shadow_render_params.view = shadow_camera.view();

	shadow_render_params.view_projection = shadow_render_params.view * shadow_camera.projection;
	shadow_render_params.technique = RenderTechnique::Shadow;

	Game::draw_opaque(shadow_render_params);
}

void render_point_lights(const RenderParams& render_params, s32 type_mask, const Vec2& inv_buffer_size, u8 team)
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
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(normal_buffer);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::depth_buffer);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(depth_buffer);

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
		if (!((s32)light->type & type_mask) || !(light->mask & render_params.camera->mask))
			continue;

		if (team != (u8)AI::Team::None && light->team != (u8)AI::Team::None && light->team != team)
			continue;

		Vec3 light_pos = light->get<Transform>()->to_world(light->offset);

		if (!render_params.camera->visible_sphere(light_pos, light->radius))
			continue;

		Mat4 light_transform = Mat4::make_translation(light_pos);
		light_transform.scale(Vec3(light->radius));

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_pos);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		sync->write<Vec3>((render_params.view * Vec4(light_pos, 1)).xyz());

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(light_transform * render_params.view_projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::type);
		sync->write(RenderDataType::S32);
		sync->write<s32>(1);
		sync->write<s32>((s32)light->type);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_color);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		if (light->team == (u8)AI::Team::None)
			sync->write<Vec3>(light->color);
		else
			sync->write<Vec3>(Team::color((AI::Team)render_params.camera->team, (AI::Team)light->team).xyz());

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_radius);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(light->radius);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::sphere);
	}
}

void render_spot_lights(const RenderParams& render_params, s32 fbo, RenderBlendMode blend_mode, const Vec2& inv_buffer_size, const Mat4& inverse_view_rotation_only, u8 team)
{
	LoopSync* sync = render_params.sync;

	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
	sync->write<RenderOp>(RenderOp::CullMode);
	sync->write<RenderCullMode>(RenderCullMode::Back);

	for (auto i = SpotLight::list.iterator(); !i.is_last(); i.next())
	{
		SpotLight* light = i.item();
		if (!(light->mask & render_params.camera->mask))
			continue;

		if (team != (u8)AI::Team::None && light->team != (u8)AI::Team::None && light->team != team)
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
			sync->write<b8>(true);
			sync->write(RenderOp::DepthTest);
			sync->write<b8>(true);

			Camera shadow_camera;
			shadow_camera.viewport =
			{
				Vec2(0, 0),
				Vec2(shadow_map_size[(s32)Settings::shadow_quality][0], shadow_map_size[(s32)Settings::shadow_quality][0]),
			};
			shadow_camera.perspective(light->fov, 1.0f, 0.1f, light->radius);
			shadow_camera.pos = abs_pos;
			shadow_camera.rot = abs_rot;
			render_shadows(sync, shadow_fbo[0], *render_params.camera, shadow_camera);
			light_vp = relative_shadow_vp(*render_params.camera, shadow_camera);

			sync->write(RenderOp::DepthMask);
			sync->write<b8>(false);
			sync->write(RenderOp::DepthTest);
			sync->write<b8>(false);
		}

		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(fbo);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(blend_mode);
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);

		sync->write<RenderOp>(RenderOp::Viewport);
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
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(normal_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(depth_buffer);

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
		if (light->team == (u8)AI::Team::None)
			sync->write<Vec3>(light->color);
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
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<s32>(shadow_buffer[0]);

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

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);
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
		Vec2((s32)(camera->viewport.pos.x * 0.5f), (s32)(camera->viewport.pos.y * 0.5f)),
		Vec2((s32)(camera->viewport.size.x * 0.5f), (s32)(camera->viewport.size.y * 0.5f)),
	};

	Mat4 inverse_view = render_params.view.inverse();
	Mat4 inverse_view_rotation_only = inverse_view;
	inverse_view_rotation_only.translation(Vec3::zero);

	const Vec3* frustum = render_params.camera->frustum_rays;

	Vec2 buffer_size(sync->input.width, sync->input.height);
	Vec2 inv_buffer_size = 1.0f / buffer_size;
	Vec2 inv_half_buffer_size = inv_buffer_size * 2.0f;

	Rect2 screen_quad_uv =
	{
		camera->viewport.pos / Vec2(sync->input.width, sync->input.height),
		camera->viewport.size / Vec2(sync->input.width, sync->input.height),
	};
	screen_quad.set
	(
		sync,
		{ Vec2(-1, -1), Vec2(2, 2) },
		camera,
		screen_quad_uv
	);

	UI::update(render_params);

	sync->write<RenderOp>(RenderOp::Viewport);
	sync->write<Rect2>(camera->viewport);

	// Fill G buffer
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(g_fbo);

		sync->write(RenderOp::Clear);
		sync->write<b8>(true); // Clear color
		sync->write<b8>(true); // Clear depth

		Game::draw_opaque(render_params);
	}

	sync->write(RenderOp::DepthMask);
	sync->write<b8>(false);
	sync->write(RenderOp::DepthTest);
	sync->write<b8>(false);

	// Render override lights
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(color_fbo1);

		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);
		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::AlphaDestination);

		if (camera->team == (u8)-1)
		{
			// render all override lights
			render_point_lights(render_params, (s32)PointLight::Type::Override, inv_buffer_size, (u8)AI::Team::None);
		}
		else
		{
			// render our team lights first
			render_point_lights(render_params, (s32)PointLight::Type::Override, inv_buffer_size, camera->team);

			// render other team lights
			render_point_lights(render_params, (s32)PointLight::Type::Override, inv_buffer_size, (u8)AI::other((AI::Team)camera->team));
		}

		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);

		sync->write(RenderOp::DepthMask);
		sync->write<b8>(true);
		sync->write(RenderOp::DepthTest);
		sync->write<b8>(true);
	}

	// Regular lighting
	{
		{
			// Global light (directional and player lights)
			const s32 max_lights = 3;
			Vec3 colors[max_lights] = {};
			Vec3 directions[max_lights] = {};
			Vec3 abs_directions[max_lights] = {};
			b8 shadowed = false;
			s32 j = 0;
			for (auto i = DirectionalLight::list.iterator(); !i.is_last(); i.next())
			{
				DirectionalLight* light = i.item();

				if (!(light->mask & render_params.camera->mask))
					continue;

				colors[j] = light->color;
				abs_directions[j] = light->get<Transform>()->absolute_rot() * Vec3(0, 1, 0);
				directions[j] = (render_params.view * Vec4(abs_directions[j], 0)).xyz();
				if (Settings::shadow_quality != Settings::ShadowQuality::Off && light->shadowed)
				{
					if (j > 0 && !shadowed)
					{
						Vec3 tmp;
						tmp = colors[0];
						colors[0] = colors[j];
						colors[j] = tmp;
						tmp = directions[0];
						directions[0] = directions[j];
						directions[j] = tmp;
						tmp = abs_directions[0];
						abs_directions[0] = abs_directions[j];
						abs_directions[j] = tmp;
					}
					shadowed = true;
				}

				j++;
				if (j >= max_lights)
					break;
			}

			Mat4 detail_light_vp;

			if (shadowed)
			{
				// Global shadow map
				Camera shadow_camera;
				r32 size = vi_min(800.0f, render_params.camera->far_plane * 1.5f);
				Vec3 pos = render_params.camera->pos;
				const r32 interval = size * 0.025f;
				pos = Vec3((s32)(pos.x / interval), (s32)(pos.y / interval), (s32)(pos.z / interval)) * interval;
				shadow_camera.pos = pos + (abs_directions[0] * size * -0.5f);
				shadow_camera.rot = Quat::look(abs_directions[0]);
				shadow_camera.mask = RENDER_MASK_SHADOW;

				if (draw_far_shadow_cascade) // only draw far shadow cascade every other frame
				{
					shadow_camera.viewport =
					{
						Vec2(0, 0),
						Vec2(shadow_map_size[(s32)Settings::shadow_quality][1], shadow_map_size[(s32)Settings::shadow_quality][1]),
					};
					shadow_camera.orthographic(size, size, 1.0f, size * 2.0f);
					far_shadow_cascade_vp = relative_shadow_vp(*render_params.camera, shadow_camera);
					render_shadows(sync, shadow_fbo[1], *render_params.camera, shadow_camera);
				}
				draw_far_shadow_cascade = !draw_far_shadow_cascade;

				render_params.shadow_vp = far_shadow_cascade_vp;
				render_params.shadow_buffer = shadow_buffer[1]; // skybox needs this for volumetric lighting

				// Detail shadow map
				shadow_camera.viewport =
				{
					Vec2(0, 0),
					Vec2(shadow_map_size[(s32)Settings::shadow_quality][0], shadow_map_size[(s32)Settings::shadow_quality][0]),
				};
				shadow_camera.orthographic(size * 0.15f, size * 0.15f, 1.0f, size * 2.0f);

				render_shadows(sync, shadow_fbo[0], *render_params.camera, shadow_camera);
				detail_light_vp = relative_shadow_vp(*render_params.camera, shadow_camera);

				sync->write<RenderOp>(RenderOp::Viewport);
				sync->write<Rect2>(camera->viewport);
			}

			sync->write<RenderOp>(RenderOp::BlendMode);
			sync->write<RenderBlendMode>(RenderBlendMode::Opaque);

			sync->write<RenderOp>(RenderOp::BindFramebuffer);
			sync->write<s32>(lighting_fbo);

			sync->write(RenderOp::Clear);
			sync->write<b8>(true); // Clear color
			sync->write<b8>(true); // Clear depth

			Loader::shader_permanent(Asset::Shader::global_light);

			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::global_light);
			sync->write(shadowed ? RenderTechnique::Shadow : RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::p);
			sync->write(RenderDataType::Mat4);
			sync->write<s32>(1);
			sync->write<Mat4>(render_params.camera->projection);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::player_light);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(1);
			sync->write<Vec3>(Game::level.skybox.player_light);

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
				sync->write<Mat4>(inverse_view_rotation_only * far_shadow_cascade_vp);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::shadow_map);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write<RenderTextureType>(RenderTextureType::Texture2D);
				sync->write<s32>(shadow_buffer[1]);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::detail_light_vp);
				sync->write(RenderDataType::Mat4);
				sync->write<s32>(1);
				sync->write<Mat4>(inverse_view_rotation_only * detail_light_vp);

				sync->write(RenderOp::Uniform);
				sync->write(Asset::Uniform::detail_shadow_map);
				sync->write(RenderDataType::Texture);
				sync->write<s32>(1);
				sync->write<RenderTextureType>(RenderTextureType::Texture2D);
				sync->write<s32>(shadow_buffer[0]);
			}

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<AssetID>(normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<AssetID>(depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_color);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(max_lights);
			sync->write<Vec3>(colors, max_lights);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_direction);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(max_lights);
			sync->write<Vec3>(directions, max_lights);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(screen_quad.mesh);
		}

		{
			// Point lights
			sync->write<RenderOp>(RenderOp::CullMode);
			sync->write<RenderCullMode>(RenderCullMode::Front);
			sync->write<RenderOp>(RenderOp::BlendMode);
			sync->write<RenderBlendMode>(RenderBlendMode::Additive);

			render_point_lights(render_params, (s32)PointLight::Type::Normal | (s32)PointLight::Type::Shockwave, inv_buffer_size, (u8)AI::Team::None);
		}

		{
			// Spot lights
			render_spot_lights(render_params, lighting_fbo, RenderBlendMode::Additive, inv_buffer_size, inverse_view_rotation_only, (u8)AI::Team::None);
		}

	}

	// SSAO
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write<s32>(half_fbo1);

		sync->write(RenderOp::Viewport);
		sync->write<Rect2>(half_viewport);

		// Downsample
		{
			sync->write(RenderOp::Clear);
			sync->write<b8>(true); // Clear color
			sync->write<b8>(true); // Clear depth

			Loader::shader_permanent(Asset::Shader::ssao_downsample);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao_downsample);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<s32>(1);
			sync->write<Vec2>(inv_buffer_size);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(screen_quad.mesh);
		}

		// SSAO
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<s32>(half_fbo2);

			Loader::shader_permanent(Asset::Shader::ssao);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(half_buffer1);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(half_depth_buffer);

			Loader::texture_permanent(Asset::Texture::noise, RenderTextureWrap::Repeat, RenderTextureFilter::Nearest);
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::noise_sampler);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(Asset::Texture::noise);

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
			sync->write(screen_quad.mesh);
		}

		sync->write(RenderOp::DepthMask);
		sync->write<b8>(false);
		sync->write(RenderOp::DepthTest);
		sync->write<b8>(false);

		// Horizontal blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<s32>(half_fbo1);

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
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(half_buffer2);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<s32>(1);
			sync->write<RenderTextureType>(RenderTextureType::Texture2D);
			sync->write<s32>(half_depth_buffer);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(screen_quad.mesh);
		}

		// Vertical blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<s32>(half_fbo2);

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
			sync->write<s32>(half_buffer1);

			sync->write(RenderOp::Mesh);
			sync->write(RenderPrimitiveMode::Triangles);
			sync->write(screen_quad.mesh);
		}
	}

	// Post processing

	sync->write<RenderOp>(RenderOp::Viewport);
	sync->write<Rect2>(camera->viewport);

	// Composite
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(color_fbo2);

		Loader::shader_permanent(Asset::Shader::composite);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::composite);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::range);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(render_params.camera->range);

		if (camera->range > 0.0f)
		{
			// we need to mark unreachable and out-of-range areas
			// set the wall normal so everything behind the wall is marked off
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::wall_normal);
			sync->write(RenderDataType::Vec3);
			sync->write<s32>(1);
			sync->write<Vec3>(render_params.camera->wall_normal);

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
		sync->write<Vec3>(Game::level.skybox.ambient_color);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::ssao_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::lighting_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(lighting_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(color_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(depth_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(screen_quad.mesh);
	}

	// Alpha components
	{
		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Alpha);

		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::None);

		sync->write<RenderOp>(RenderOp::DepthTest);
		sync->write<b8>(true);
		sync->write<RenderOp>(RenderOp::DepthMask);
		sync->write<b8>(true);

		render_params.depth_buffer = depth_buffer;

		Game::draw_alpha(render_params);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Additive);

		Game::draw_additive(render_params);

		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Opaque);

		sync->write<RenderOp>(RenderOp::DepthTest);
		sync->write<b8>(false);
		sync->write<RenderOp>(RenderOp::DepthMask);
		sync->write<b8>(false);
	}

	sync->write<RenderOp>(RenderOp::BindFramebuffer);
	sync->write<s32>(color_fbo1);

	// Edge detect
	{
		Loader::shader_permanent(Asset::Shader::edge_detect);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::edge_detect);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(color_buffer2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(depth_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::normal_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(normal_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(screen_quad.mesh);
	}

	// UI
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write(ui_fbo);

		sync->write(RenderOp::Clear);
		sync->write(true);
		sync->write(false);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Alpha);

		UI::draw(render_params);

		sync->write(RenderOp::BindFramebuffer);
		sync->write(color_fbo2);

		sync->write(RenderOp::Clear);
		sync->write(true);
		sync->write(false);

		sync->write(RenderOp::BlitFramebuffer);
		sync->write(ui_fbo);
		sync->write(camera->viewport); // Source
		sync->write(camera->viewport); // Destination

		sync->write(RenderOp::BindFramebuffer);
		sync->write(color_fbo1);

		// Overlay UI on to the color buffer
		UI::texture(render_params, color_buffer2, { Vec2::zero, camera->viewport.size }, Vec4(1, 1, 1, 1), screen_quad_uv);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
	}

	// Bloom
	{
		// Downsample
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(half_fbo1);

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
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(color_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(inv_buffer_size);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(screen_quad.mesh);

		// Blur x
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(half_fbo3);

		Loader::shader_permanent(Asset::Shader::blur);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::blur);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(Vec2(inv_half_buffer_size.x, 0));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(screen_quad.mesh);

		// Blur y
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<s32>(half_fbo2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<s32>(1);
		sync->write<Vec2>(Vec2(0, inv_half_buffer_size.y));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(screen_quad.mesh);
	}

	sync->write(RenderOp::BindFramebuffer);
	sync->write(AssetNull);

	sync->write(RenderOp::Viewport);
	sync->write<Rect2>(camera->viewport);

	UI::texture(render_params, color_buffer, { Vec2::zero, camera->viewport.size }, Vec4(1, 1, 1, 1), screen_quad_uv);

	// Composite bloom
	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Additive);
	UI::texture(render_params, half_buffer2, { Vec2::zero, camera->viewport.size }, Vec4(1, 1, 1, 0.5f), screen_quad_uv);
	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Opaque);

#if DEBUG && DEBUG_RENDER
	// Debug render buffers
	Loader::shader_permanent(Asset::Shader::debug_depth);
	const s32 buffer_count = 8;
	Vec2 debug_buffer_size = camera->viewport.size / buffer_count;
	UI::texture(render_params, color_buffer, { Vec2(debug_buffer_size.x * 0, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, normal_buffer, { Vec2(debug_buffer_size.x * 1, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, lighting_buffer, { Vec2(debug_buffer_size.x * 2, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
	UI::texture(render_params, depth_buffer, { Vec2(debug_buffer_size.x * 3, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv, Asset::Shader::debug_depth);
	UI::texture(render_params, color_buffer2, { Vec2(debug_buffer_size.x * 4, 0), debug_buffer_size }, Vec4(1, 1, 1, 1), screen_quad_uv);
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

void loop(LoopSwapper* swapper, PhysicsSwapper* physics_swapper)
{
	mersenne::srand(platform::timestamp());
	noise::reseed();

	LoopSync* sync = swapper->swap<SwapType_Write>();

	Loader::init(swapper);

	if (!Game::init(sync))
	{
		fprintf(stderr, "Failed to initialize game.\n");
		exit(-1);
	}

	lighting_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Color);
	color_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Color);
	normal_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Color);
	depth_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Depth);
	for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
		shadow_buffer[i] = Loader::dynamic_texture_permanent(shadow_map_size[(s32)Settings::shadow_quality][i], shadow_map_size[(s32)Settings::shadow_quality][i], RenderDynamicTextureType::Depth, RenderTextureWrap::Clamp, RenderTextureFilter::Linear, RenderTextureCompare::RefToTexture);

	g_fbo = Loader::framebuffer_permanent(3);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color1, normal_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, depth_buffer);

	color_fbo1 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, depth_buffer);

	color_buffer2 = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Color);
	color_fbo2 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, depth_buffer);

	ui_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::ColorMultisample);
	ui_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, ui_buffer);

	lighting_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, lighting_buffer);

	for (s32 i = 0; i < SHADOW_MAP_CASCADES; i++)
	{
		shadow_fbo[i] = Loader::framebuffer_permanent(1);
		Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, shadow_buffer[i]);
	}

	half_buffer1 = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Color);
	half_depth_buffer = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Depth);
	half_fbo1 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, half_depth_buffer);

	half_buffer2 = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Color, RenderTextureWrap::Clamp, RenderTextureFilter::Linear);
	half_fbo2 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer2);

	half_fbo3 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);

	screen_quad.init(sync);

	InputState last_input;

	Update u;
	u.render = sync;
	u.input = &sync->input;
	u.last_input = &last_input;

	PhysicsSync* physics_sync = nullptr;

	r32 time_update = 0.0f; // time required for update

	while (!sync->quit && !Game::quit)
	{
		// Update

		{
			// limit framerate

			r32 framerate_limit = u.input->focus ? Settings::framerate_limit : 30;

			r32 delay = (1.0f / framerate_limit) - time_update;
			if (delay > 0)
				platform::sleep(delay);
		}

		r64 time_update_start = platform::time();

		u.input = &sync->input;
		u.time = sync->time;

#if DEBUG
		if (u.input->keys[(s32)KeyCode::F5])
			vi_assert(false);
#endif
		if (physics_sync)
			physics_sync = physics_swapper->next<SwapType_Write>();
		else
			physics_sync = physics_swapper->get();

		Game::update(u);

		physics_sync->time = Game::time;
		physics_sync->timestep = Game::physics_timestep;

		physics_swapper->done<SwapType_Write>();

		sync->write(RenderOp::Clear);
		sync->write(true);
		sync->write(true);

		for (s32 i = 0; i < Camera::max_cameras; i++)
		{
			if (Camera::all[i].active)
				draw(sync, &Camera::all[i]);
		}

		sync->quit |= Game::quit;

		memcpy(&last_input, &sync->input, sizeof(last_input));

		time_update = (r32)(platform::time() - time_update_start);

		sync = swapper->swap<SwapType_Write>();
		sync->queue.length = 0;
	}

	{
		PhysicsSync* physics_sync = physics_swapper->next<SwapType_Write>();
		physics_sync->quit = true;
		physics_swapper->done<SwapType_Write>();
	}

	Game::term();
}

}

}
