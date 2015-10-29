#include "types.h"
#include "vi_assert.h"

#include "render/views.h"
#include "render/render.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "data/array.h"
#include "data/entity.h"
#include "data/components.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/texture.h"
#include "physics.h"
#include "render/ui.h"
#include "input.h"
#include "mersenne-twister.h"
#include <time.h>
#include "asset/lookup.h"
#include "cJSON.h"

#if DEBUG
	#define DEBUG_RENDER 0
#endif

#include "game/game.h"

namespace VI
{

namespace Loop
{

ScreenQuad screen_quad = ScreenQuad();

int color_buffer;
int normal_buffer;
int depth_buffer;
int color_fbo1;
int g_fbo;
int lighting_buffer;
int lighting_fbo;
int color_buffer2;
int color_fbo2;
int shadow_buffer;
int shadow_fbo;
int half_depth_buffer;
int half_buffer1;
int half_fbo1;
int half_buffer2;
int half_fbo2;
int half_fbo3;
int ui_buffer;
int ui_fbo;

#define SHADOW_MAP_SIZE 256

void draw(RenderSync* sync, const Camera* camera)
{
	RenderParams render_params;
	render_params.sync = sync;

	render_params.camera = camera;
	render_params.view = camera->view();
	render_params.view_projection = render_params.view * camera->projection;
	render_params.technique = RenderTechnique::Default;

	ScreenRect half_viewport = { (int)(camera->viewport.x * 0.5f), (int)(camera->viewport.y * 0.5f), (int)(camera->viewport.width * 0.5f), (int)(camera->viewport.height * 0.5f) };

	Mat4 inverse_view = render_params.view.inverse();

	Vec3 frustum[4];
	render_params.camera->projection_frustum(frustum);

	Vec2 buffer_size(sync->input.width, sync->input.height);
	Vec2 inv_buffer_size(1.0f / buffer_size.x, 1.0f / buffer_size.y);
	Vec2 inv_half_buffer_size = inv_buffer_size * 2.0f;

	Vec2 screen_quad_uva = Vec2((float)camera->viewport.x / (float)sync->input.width, (float)camera->viewport.y / (float)sync->input.height);
	Vec2 screen_quad_uvb = Vec2(((float)camera->viewport.x + (float)camera->viewport.width) / (float)sync->input.width, ((float)camera->viewport.y + (float)camera->viewport.height) / (float)sync->input.height);
	screen_quad.set
	(
		sync,
		Vec2(-1, -1),
		Vec2(1, 1),
		camera,
		screen_quad_uva,
		screen_quad_uvb
	);

	UI::update(render_params);

	sync->write(RenderOp::PointSize);
	sync->write<float>(UI::scale);

	sync->write<RenderOp>(RenderOp::Viewport);
	sync->write<ScreenRect>(camera->viewport);

	// Fill G buffer
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(g_fbo);

		sync->write(RenderOp::Clear);
		sync->write<bool>(true); // Clear color
		sync->write<bool>(true); // Clear depth

		Game::draw_opaque(render_params);
	}

	// Lighting
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(lighting_fbo);

		sync->write(RenderOp::Clear);
		sync->write<bool>(true); // Clear color
		sync->write<bool>(true); // Clear depth

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Additive);
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);

		Loader::shader_permanent(Asset::Shader::point_light);

		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::point_light);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<int>(1);
		sync->write<Mat4>(render_params.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::normal_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(normal_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(depth_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::uv_offset);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(Vec2(camera->viewport.x, camera->viewport.y) * inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::uv_scale);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(Vec2(camera->viewport.width, camera->viewport.height) * inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::frustum);
		sync->write(RenderDataType::Vec3);
		sync->write<int>(4);
		sync->write<Vec3>(frustum, 4);

		Loader::mesh_permanent(Asset::Mesh::sphere);
		for (auto i = World::components<PointLight>().iterator(); !i.is_last(); i.next())
		{
			PointLight* light = i.item();

			Vec3 light_pos = light->get<Transform>()->to_world(light->offset);
			Mat4 light_transform = Mat4::make_translation(light_pos);
			light_transform.scale(Vec3(light->radius));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_pos);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(1);
			sync->write<Vec3>((render_params.view * Vec4(light_pos, 1)).xyz());

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::mvp);
			sync->write(RenderDataType::Mat4);
			sync->write<int>(1);
			sync->write<Mat4>(light_transform * render_params.view_projection);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_color);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(1);
			sync->write<Vec3>(light->color);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_radius);
			sync->write(RenderDataType::Float);
			sync->write<int>(1);
			sync->write<float>(light->radius);

			sync->write(RenderOp::Mesh);
			sync->write(Asset::Mesh::sphere);
		}

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);

		for (auto i = World::components<SpotLight>().iterator(); !i.is_last(); i.next())
		{
			SpotLight* light = i.item();
			if (light->color.length_squared() == 0.0f || light->fov == 0.0f || light->radius == 0.0f)
				continue;

			Vec3 abs_pos;
			Quat abs_rot;
			light->get<Transform>()->absolute(&abs_pos, &abs_rot);

			Mat4 light_vp;

			{
				// Render shadows
				sync->write<RenderOp>(RenderOp::BindFramebuffer);
				sync->write<int>(shadow_fbo);

				RenderParams shadow_render_params;
				shadow_render_params.sync = sync;

				Camera shadow_camera;
				shadow_camera.viewport = { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
				shadow_camera.perspective(light->fov, 1.0f, 0.1f, light->radius);
				shadow_camera.pos = abs_pos;
				shadow_camera.rot = abs_rot;

				sync->write<RenderOp>(RenderOp::Viewport);
				sync->write<ScreenRect>(shadow_camera.viewport);

				sync->write<RenderOp>(RenderOp::Clear);
				sync->write<bool>(false); // Don't clear color
				sync->write<bool>(true); // Clear depth

				shadow_render_params.camera = &shadow_camera;
				shadow_render_params.view = shadow_camera.view();
				shadow_render_params.view_projection = light_vp = shadow_render_params.view * shadow_camera.projection;
				shadow_render_params.technique = RenderTechnique::Default;

				Game::draw_opaque(shadow_render_params);
			}

			sync->write<RenderOp>(RenderOp::BindFramebuffer);
			sync->write<int>(lighting_fbo);

			sync->write<RenderOp>(RenderOp::BlendMode);
			sync->write<RenderBlendMode>(RenderBlendMode::Additive);
			sync->write<RenderOp>(RenderOp::CullMode);
			sync->write<RenderCullMode>(RenderCullMode::Front);

			sync->write<RenderOp>(RenderOp::Viewport);
			sync->write<ScreenRect>(camera->viewport);

			Loader::shader_permanent(Asset::Shader::spot_light);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::spot_light);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::uv_offset);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(camera->viewport.x, camera->viewport.y) * inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::uv_scale);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(camera->viewport.width, camera->viewport.height) * inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::p);
			sync->write(RenderDataType::Mat4);
			sync->write<int>(1);
			sync->write<Mat4>(render_params.camera->projection);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<AssetID>(normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<AssetID>(depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_pos);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(1);
			sync->write<Vec3>((render_params.view * Vec4(abs_pos, 1)).xyz());

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_vp);
			sync->write(RenderDataType::Mat4);
			sync->write<int>(1);
			sync->write<Mat4>(inverse_view * light_vp);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_color);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(1);
			sync->write<Vec3>(light->color);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_radius);
			sync->write(RenderDataType::Float);
			sync->write<int>(1);
			sync->write<float>(light->radius);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_direction);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(1);
			sync->write<Vec3>((render_params.view * Vec4(abs_rot * Vec3(0, 0, -1), 0)).xyz());

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::light_fov_dot);
			sync->write(RenderDataType::Float);
			sync->write<int>(1);
			sync->write<float>(cosf(light->fov));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::shadow_map);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(shadow_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::mvp);
			sync->write(RenderDataType::Mat4);
			sync->write<int>(1);
			Mat4 light_transform;
			float width_scale = sinf(light->fov) * light->radius * 2.0f;
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
			sync->write<int>(4);
			sync->write<Vec3>(frustum, 4);

			Loader::mesh_permanent(Asset::Mesh::cone);
			sync->write(RenderOp::Mesh);
			sync->write(Asset::Mesh::cone);

			sync->write<RenderOp>(RenderOp::BlendMode);
			sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
			sync->write<RenderOp>(RenderOp::CullMode);
			sync->write<RenderCullMode>(RenderCullMode::Back);
		}
	}

	// SSAO
	{
		sync->write(RenderOp::BindFramebuffer);
		sync->write<int>(half_fbo1);

		sync->write(RenderOp::Viewport);
		sync->write<ScreenRect>(half_viewport);

		// Downsample
		{
			sync->write(RenderOp::Clear);
			sync->write<bool>(true); // Clear color
			sync->write<bool>(true); // Clear depth

			Loader::shader_permanent(Asset::Shader::ssao_downsample);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao_downsample);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(normal_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(depth_buffer);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(inv_buffer_size);

			sync->write(RenderOp::Mesh);
			sync->write(screen_quad.mesh);
		}

		// SSAO
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<int>(half_fbo2);

			Loader::shader_permanent(Asset::Shader::ssao);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::normal_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(half_buffer1);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(half_depth_buffer);

			Loader::texture_permanent(Asset::Texture::noise);
			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::noise_sampler);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(Asset::Texture::noise);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::p);
			sync->write(RenderDataType::Mat4);
			sync->write<int>(1);
			sync->write<Mat4>(render_params.camera->projection);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::uv_offset);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(camera->viewport.x, camera->viewport.y) * inv_buffer_size);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_uv_scale);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(1, 1) / (Vec2(camera->viewport.width, camera->viewport.height) * inv_buffer_size));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::far_plane);
			sync->write(RenderDataType::Float);
			sync->write<int>(1);
			sync->write<float>(camera->far_plane);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::frustum);
			sync->write(RenderDataType::Vec3);
			sync->write<int>(4);
			sync->write<Vec3>(frustum, 4);

			sync->write(RenderOp::Mesh);
			sync->write(screen_quad.mesh);
		}

		sync->write(RenderOp::DepthMask);
		sync->write<bool>(false);
		sync->write(RenderOp::DepthTest);
		sync->write<bool>(false);

		// Horizontal blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<int>(half_fbo1);

			Loader::shader_permanent(Asset::Shader::ssao_blur);
			sync->write(RenderOp::Shader);
			sync->write<AssetID>(Asset::Shader::ssao_blur);
			sync->write(RenderTechnique::Default);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(inv_half_buffer_size.x, 0));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::color_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(half_buffer2);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::depth_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(half_depth_buffer);

			sync->write(RenderOp::Mesh);
			sync->write(screen_quad.mesh);
		}

		// Vertical blur
		{
			sync->write(RenderOp::BindFramebuffer);
			sync->write<int>(half_fbo2);

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::inv_buffer_size);
			sync->write(RenderDataType::Vec2);
			sync->write<int>(1);
			sync->write<Vec2>(Vec2(0, inv_half_buffer_size.y));

			sync->write(RenderOp::Uniform);
			sync->write(Asset::Uniform::color_buffer);
			sync->write(RenderDataType::Texture);
			sync->write<int>(1);
			sync->write<RenderTextureType>(RenderTexture2D);
			sync->write<int>(half_buffer1);

			sync->write(RenderOp::Mesh);
			sync->write(screen_quad.mesh);
		}
	}

	// Post processing

	sync->write<RenderOp>(RenderOp::Viewport);
	sync->write<ScreenRect>(camera->viewport);

	// Composite
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(color_fbo2);

		Loader::shader_permanent(Asset::Shader::composite);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::composite);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<int>(1);
		sync->write<Mat4>(render_params.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::ambient_color);
		sync->write(RenderDataType::Vec3);
		sync->write<int>(1);
		sync->write<Vec3>(Skybox::ambient_color);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::zenith_color);
		sync->write(RenderDataType::Vec3);
		sync->write<int>(1);
		sync->write<Vec3>(Skybox::zenith_color);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::uv_offset);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(Vec2(mersenne::randf_oo(), mersenne::randf_oo()));

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::ssao_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(half_buffer2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::lighting_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(lighting_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(color_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(depth_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(screen_quad.mesh);
	}

	// Edge detect
	{
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(color_fbo1);

		Loader::shader_permanent(Asset::Shader::edge_detect);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::edge_detect);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(inv_buffer_size);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::p);
		sync->write(RenderDataType::Mat4);
		sync->write<int>(1);
		sync->write<Mat4>(camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(color_buffer2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::depth_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(depth_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::normal_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(normal_buffer);

		sync->write(RenderOp::Mesh);
		sync->write(screen_quad.mesh);
	}

	sync->write(RenderOp::Viewport);
	sync->write<ScreenRect>(camera->viewport);

	// Alpha components
	{
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::None);
		sync->write<RenderOp>(RenderOp::DepthMask);
		sync->write<bool>(false);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Alpha);

		sync->write<RenderOp>(RenderOp::DepthTest);
		sync->write<bool>(true);

		Game::draw_alpha(render_params);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Additive);

		Game::draw_additive(render_params);

		sync->write<RenderOp>(RenderOp::DepthTest);
		sync->write<bool>(false);

		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);
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

		UI::texture(render_params, color_buffer2, Vec2(0, 0), Vec2(camera->viewport.width, camera->viewport.height), Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);

		sync->write<RenderOp>(RenderOp::BlendMode);
		sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
	}

	// Bloom
	{
		// Downsample
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(half_fbo1);

		sync->write(RenderOp::Viewport);
		sync->write<ScreenRect>(half_viewport);

		Loader::shader_permanent(Asset::Shader::bloom_downsample);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::bloom_downsample);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(color_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(inv_buffer_size);

		sync->write(RenderOp::Mesh);
		sync->write(screen_quad.mesh);

		// Blur x
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(half_fbo3);

		Loader::shader_permanent(Asset::Shader::blur);
		sync->write(RenderOp::Shader);
		sync->write<AssetID>(Asset::Shader::blur);
		sync->write(RenderTechnique::Default);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(Vec2(inv_half_buffer_size.x, 0));

		sync->write(RenderOp::Mesh);
		sync->write(screen_quad.mesh);

		// Blur y
		sync->write<RenderOp>(RenderOp::BindFramebuffer);
		sync->write<int>(half_fbo2);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::color_buffer);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(half_buffer1);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::inv_buffer_size);
		sync->write(RenderDataType::Vec2);
		sync->write<int>(1);
		sync->write<Vec2>(Vec2(0, inv_half_buffer_size.y));

		sync->write(RenderOp::Mesh);
		sync->write(screen_quad.mesh);
	}

	sync->write(RenderOp::BindFramebuffer);
	sync->write(AssetNull);

	sync->write(RenderOp::Viewport);
	sync->write<ScreenRect>(camera->viewport);

	UI::texture(render_params, color_buffer, Vec2(0, 0), Vec2(camera->viewport.width, camera->viewport.height), Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);

	// Composite bloom
	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Additive);
	UI::texture(render_params, half_buffer2, Vec2(0, 0), Vec2(camera->viewport.width, camera->viewport.height), Vec4(1, 1, 1, 0.5f), screen_quad_uva, screen_quad_uvb);
	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Opaque);

#if DEBUG && DEBUG_RENDER
	// Debug render buffers
	const int buffer_count = 8;
	Vec2 debug_buffer_size = Vec2(camera->viewport.width / (float)buffer_count, camera->viewport.height / (float)buffer_count);
	UI::texture(render_params, color_buffer, Vec2(debug_buffer_size.x * 0, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, normal_buffer, Vec2(debug_buffer_size.x * 1, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, lighting_buffer, Vec2(debug_buffer_size.x * 2, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, depth_buffer, Vec2(debug_buffer_size.x * 3, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, color_buffer2, Vec2(debug_buffer_size.x * 4, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, half_buffer1, Vec2(debug_buffer_size.x * 5, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, half_buffer2, Vec2(debug_buffer_size.x * 6, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
	UI::texture(render_params, half_depth_buffer, Vec2(debug_buffer_size.x * 7, 0), debug_buffer_size, Vec4(1, 1, 1, 1), screen_quad_uva, screen_quad_uvb);
#endif

	sync->write(RenderOp::DepthMask);
	sync->write(true);
	sync->write(RenderOp::DepthTest);
	sync->write(true);
}

void loop(RenderSwapper* swapper, PhysicsSwapper* physics_swapper)
{
	mersenne::srand(time(0));

	RenderSync* sync = swapper->swap<SwapType_Write>();

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
	shadow_buffer = Loader::dynamic_texture_permanent(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, RenderDynamicTextureType::Depth);

	g_fbo = Loader::framebuffer_permanent(3);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color1, normal_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, depth_buffer);

	color_fbo1 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, depth_buffer);

	color_buffer2 = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::Color);
	color_fbo2 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, color_buffer2);

	ui_buffer = Loader::dynamic_texture_permanent(sync->input.width, sync->input.height, RenderDynamicTextureType::ColorMultisample);
	ui_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, ui_buffer);

	lighting_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, lighting_buffer);

	shadow_fbo = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, shadow_buffer);

	half_buffer1 = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Color);
	half_depth_buffer = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Depth);
	half_fbo1 = Loader::framebuffer_permanent(2);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Depth, half_depth_buffer);

	half_buffer2 = Loader::dynamic_texture_permanent(sync->input.width / 2, sync->input.height / 2, RenderDynamicTextureType::Color, RenderTextureFilter::Linear);
	half_fbo2 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer2);

	half_fbo3 = Loader::framebuffer_permanent(1);
	Loader::framebuffer_attach(RenderFramebufferAttachment::Color0, half_buffer1);

	screen_quad.init(sync);

	Update u;

	PhysicsSync* physics_sync = nullptr;

	while (!sync->quit)
	{
		// Update
		if (sync->focus)
		{
			u.input = &sync->input;
			u.time = sync->time;

#if DEBUG
			if (u.input->keys[KEYCODE_F5])
				vi_assert(false);
#endif
			if (physics_sync)
				physics_sync = physics_swapper->next<SwapType_Write>();
			else
				physics_sync = physics_swapper->get();
			physics_sync->time = sync->time;
		}

		Game::update(u);

		physics_swapper->done<SwapType_Write>();

		sync->write(RenderOp::Clear);
		sync->write(true);
		sync->write(true);

		for (int i = 0; i < Camera::max_cameras; i++)
		{
			if (Camera::all[i].active)
				draw(sync, &Camera::all[i]);
		}

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
