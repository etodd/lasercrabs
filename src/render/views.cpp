#include "views.h"
#include "load.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/texture.h"
#include "console.h"
#include "data/components.h"
#include "game/team.h"
#include "game/game.h"

namespace VI
{

Bitmask<MAX_ENTITIES> View::list_alpha;
Bitmask<MAX_ENTITIES> View::list_additive;
Bitmask<MAX_ENTITIES> View::list_alpha_depth;

View::View(AssetID m)
	: mesh(m),
	shader(AssetNull),
	texture(AssetNull),
	offset(Mat4::identity),
	color(-1, -1, -1, -1),
	mask(RENDER_MASK_DEFAULT),
	team((s8)AI::TeamNone)
{
}

View::~View()
{
	alpha_disable();
}

void View::draw_opaque(const RenderParams& params)
{
	for (auto i = View::list.iterator(); !i.is_last(); i.next())
	{
		if (!list_alpha.get(i.index) && !list_additive.get(i.index) && !list_alpha_depth.get(i.index) && (i.item()->mask & params.camera->mask))
			i.item()->draw(params);
	}
}

void View::draw_additive(const RenderParams& params)
{
	for (auto i = View::list.iterator(); !i.is_last(); i.next())
	{
		if (list_additive.get(i.index) && (i.item()->mask & params.camera->mask))
			i.item()->draw(params);
	}
}

void View::draw_alpha(const RenderParams& params)
{
	for (auto i = View::list.iterator(); !i.is_last(); i.next())
	{
		if (list_alpha.get(i.index) && (i.item()->mask & params.camera->mask))
			i.item()->draw(params);
	}
}

void View::draw_alpha_depth(const RenderParams& params)
{
	for (auto i = View::list.iterator(); !i.is_last(); i.next())
	{
		if (list_alpha_depth.get(i.index) && (i.item()->mask & params.camera->mask))
			i.item()->draw(params);
	}
}

void View::alpha()
{
	list_alpha.set(id(), true);
	list_additive.set(id(), false);
	list_alpha_depth.set(id(), false);
}

void View::additive()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), true);
	list_alpha_depth.set(id(), false);
}

void View::alpha_depth()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), false);
	list_alpha_depth.set(id(), true);
}

void View::alpha_disable()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), false);
	list_alpha_depth.set(id(), false);
}

AlphaMode View::alpha_mode() const
{
	if (list_alpha.get(id()))
		return AlphaMode::Alpha;
	else if (list_additive.get(id()))
		return AlphaMode::Additive;
	else if (list_alpha_depth.get(id()))
		return AlphaMode::AlphaDepth;
	else
		return AlphaMode::Opaque;
}

void View::alpha_mode(AlphaMode m)
{
	switch (m)
	{
		case AlphaMode::Opaque:
		{
			alpha_disable();
			break;
		}
		case AlphaMode::Alpha:
		{
			alpha();
			break;
		}
		case AlphaMode::Additive:
		{
			additive();
			break;
		}
		case AlphaMode::AlphaDepth:
		{
			alpha_depth();
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

#define PVP_ACCESSIBLE Vec4(0.7f, 0.7f, 0.7f, 1.0f)
#define PVP_ACCESSIBLE_NO_OVERRIDE Vec4(0.7f, 0.7f, 0.7f, MATERIAL_NO_OVERRIDE)
#define PVP_INACCESSIBLE Vec4(0.0f, 0.0f, 0.0f, MATERIAL_NO_OVERRIDE)

void View::draw(const RenderParams& params) const
{
	if (mesh == AssetNull || shader == AssetNull)
		return;

	const Mesh* mesh_data = Loader::mesh(mesh);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;

	{
		Vec3 radius = (offset * Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
		if (!params.camera->visible_sphere(m.translation(), vi_max(radius.x, vi_max(radius.y, radius.z))))
			return;
	}

	// if allow_culled_shader is false, replace the culled shader with the standard shader.
	b8 allow_culled_shader = params.camera->cull_range > 0.0f;
	AssetID actual_shader = allow_culled_shader || shader != Asset::Shader::culled ? shader : Asset::Shader::standard;

	Loader::shader(actual_shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(actual_shader);
	sync->write(params.technique);

	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);

	if (team == (s8)AI::TeamNone)
	{
		if (params.camera->colors)
			sync->write<Vec4>(color);
		else if (color.w == MATERIAL_INACCESSIBLE)
			sync->write<Vec4>(PVP_INACCESSIBLE);
		else if (color.w == MATERIAL_NO_OVERRIDE)
			sync->write<Vec4>(PVP_ACCESSIBLE_NO_OVERRIDE);
		else
			sync->write<Vec4>(PVP_ACCESSIBLE);
	}
	else
	{
		const Vec4& team_color = Team::color((AI::Team)team, (AI::Team)params.camera->team);
		if (list_alpha.get(id()) || list_additive.get(id()) || list_alpha_depth.get(id()))
			sync->write<Vec4>(Vec4(team_color.xyz(), color.w));
		else
			sync->write<Vec4>(team_color);
	}

	if (actual_shader == Asset::Shader::culled)
	{
		// write culling info
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::cull_center);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		sync->write<Vec3>(params.camera->range_center);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::cull_radius);
		sync->write(RenderDataType::R32);
		sync->write<s32>(1);
		sync->write<r32>(params.camera->cull_range);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::wall_normal);
		sync->write(RenderDataType::Vec3);
		sync->write<s32>(1);
		sync->write<Vec3>(params.camera->clip_planes[0].normal);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::cull_behind_wall);
		sync->write(RenderDataType::S32);
		sync->write<s32>(1);
		sync->write<s32>(params.camera->cull_behind_wall);
	}

	if (texture != AssetNull)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(texture);
	}

	if (params.edges)
	{
		sync->write(RenderOp::MeshEdges);
		sync->write(mesh);
	}
	else
	{
		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(mesh);
	}
}

void View::awake()
{
	const Mesh* m = Loader::mesh(mesh);
	if (m)
	{
		if (color.x < 0.0f)
			color.x = m->color.x;
		if (color.y < 0.0f)
			color.y = m->color.y;
		if (color.z < 0.0f)
			color.z = m->color.z;
		if (color.w < 0.0f)
			color.w = m->color.w;
	}
}

r32 fog_start(const RenderParams& p)
{
	return Game::level.skybox.fog_start;
}

void SkyDecal::draw_alpha(const RenderParams& p)
{
	RenderSync* sync = p.sync;

	Loader::shader_permanent(Asset::Shader::sky_decal);

	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::sky_decal);
	sync->write(p.technique);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::p);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(p.camera->projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::frustum);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(4);
	sync->write<Vec3>(p.camera->frustum_rays, 4);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::fog_start);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(p.camera->far_plane * 0.25f);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::fog_extent);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(p.camera->far_plane * 0.75f);

	Vec2 inv_buffer_size = Vec2(1.0f / (r32)p.sync->input.width, 1.0f / (r32)p.sync->input.height);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_offset);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(p.camera->viewport.pos * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_scale);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(p.camera->viewport.size * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::depth_buffer);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(p.depth_buffer);

	sync->write(RenderOp::DepthTest);
	sync->write<b8>(false);

	Loader::mesh_permanent(Asset::Mesh::sky_decal);
	for (auto i = SkyDecal::list.iterator(); !i.is_last(); i.next())
	{
		SkyDecal* d = i.item();

		Loader::texture(d->texture);

		Quat rot = d->get<Transform>()->absolute_rot();
		Mat4 m;
		m.make_transform(rot * Vec3(0, 0, 1), Vec3(d->scale), rot);
		Mat4 v = p.view;
		v.translation(Vec3::zero);

		Mat4 mvp = m * (v * p.camera->projection);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(mvp);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_color);
		sync->write(RenderDataType::Vec4);
		sync->write<s32>(1);
		if (p.camera->colors)
			sync->write<Vec4>(d->color);
		else
			sync->write<Vec4>(LMath::desaturate(d->color));

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(d->texture);

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::sky_decal);
	}

	sync->write(RenderOp::DepthTest);
	sync->write<b8>(true);
}

b8 Skybox::Config::valid() const
{
	return shader != AssetNull && mesh != AssetNull;
}

void Skybox::draw_alpha(const RenderParams& p)
{
	if (Game::level.skybox.mesh == AssetNull || p.technique != RenderTechnique::Default)
		return;

	Loader::shader(Game::level.skybox.shader);
	Loader::mesh(Game::level.skybox.mesh);
	Loader::texture(Game::level.skybox.texture);

	RenderSync* sync = p.sync;

	sync->write<RenderOp>(RenderOp::DepthTest);
	sync->write<b8>(false);

	sync->write(RenderOp::Shader);
	sync->write(Game::level.skybox.shader);

	b8 volumetric_lighting = p.shadow_buffer != AssetNull && p.camera->fog;

	if (volumetric_lighting)
		sync->write(RenderTechnique::Shadow);
	else
		sync->write(RenderTechnique::Default);

	Mat4 mvp = p.view * Mat4::make_scale(Vec3(p.camera->far_plane));
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(1);
	if (p.camera->colors)
		sync->write<Vec3>(Game::level.skybox.color);
	else
		sync->write<Vec3>(LMath::desaturate(Game::level.skybox.color));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::p);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(p.camera->projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::frustum);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(4);
	sync->write<Vec3>(p.camera->frustum_rays, 4);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::fog_start);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(fog_start(p));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::fog_extent);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(p.camera->far_plane - fog_start(p));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::far_plane);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(p.camera->far_plane);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::fog);
	sync->write(RenderDataType::S32);
	sync->write<s32>(1);
	sync->write<s32>(p.camera->fog);

	Vec2 inv_buffer_size = Vec2(1.0f / (r32)p.sync->input.width, 1.0f / (r32)p.sync->input.height);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_offset);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(p.camera->viewport.pos * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::uv_scale);
	sync->write(RenderDataType::Vec2);
	sync->write<s32>(1);
	sync->write<Vec2>(p.camera->viewport.size * inv_buffer_size);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::depth_buffer);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(p.depth_buffer);

	if (volumetric_lighting)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::shadow_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(p.shadow_buffer);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::light_vp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		Mat4 view_rotation = p.view;
		view_rotation.translation(Vec3::zero);
		sync->write<Mat4>(view_rotation.inverse() * p.shadow_vp);

		Loader::texture_permanent(Asset::Texture::noise, RenderTextureWrap::Repeat, RenderTextureFilter::Nearest);
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::noise_sampler);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(Asset::Texture::noise);
	}

	if (Game::level.skybox.texture != AssetNull)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<s32>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(Game::level.skybox.texture);
	}

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write<AssetID>(Game::level.skybox.mesh);

	sync->write<RenderOp>(RenderOp::DepthTest);
	sync->write<b8>(true);
}

void SkyPattern::draw_opaque(const RenderParams& p)
{
	// only render depth

	if (p.technique != RenderTechnique::Default)
		return;

	Loader::shader_permanent(Asset::Shader::standard_flat);
	Loader::mesh_permanent(Asset::Mesh::sky_pattern);

	RenderSync* sync = p.sync;

	sync->write(RenderOp::ColorMask);
	sync->write<s8>(0);

	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_flat);
	sync->write(p.technique);

	Mat4 mvp = p.view * Mat4::make_scale(Vec3(p.camera->far_plane));
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(0, 0, 0, 1));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(Asset::Mesh::sky_pattern);

	sync->write(RenderOp::ColorMask);
	sync->write<s8>(RENDER_COLOR_MASK_DEFAULT);
}

void SkyPattern::draw_alpha(const RenderParams& p)
{
	if (p.technique != RenderTechnique::Default)
		return;

	Loader::shader_permanent(Asset::Shader::flat);
	Loader::mesh_permanent(Asset::Mesh::sky_pattern);

	RenderSync* sync = p.sync;

	sync->write<RenderOp>(RenderOp::FillMode);
	sync->write(RenderFillMode::Point);
	sync->write<RenderOp>(RenderOp::PointSize);
	sync->write<r32>(4.0f * UI::scale);

	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::flat);
	sync->write(p.technique);

	Mat4 mvp = p.view * Mat4::make_scale(Vec3(p.camera->far_plane - 1.0f));
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, 1));

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(Asset::Mesh::sky_pattern);

	sync->write<RenderOp>(RenderOp::FillMode);
	sync->write(RenderFillMode::Fill);
}

Water::Water(AssetID mesh_id)
	: mesh(mesh_id),
	texture(Asset::Texture::water_normal),
	color(-1, -1, -1, -1),
	displacement_horizontal(2.0f),
	displacement_vertical(0.75f)
{

}

void Water::awake()
{
	const Mesh* m = Loader::mesh(mesh);
	if (m)
	{
		if (color.x < 0.0f)
			color.x = m->color.x;
		if (color.y < 0.0f)
			color.y = m->color.y;
		if (color.z < 0.0f)
			color.z = m->color.z;
		if (color.w < 0.0f)
			color.w = m->color.w;
	}
}

void Water::draw_opaque(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	const Mesh* mesh_data = Loader::mesh(mesh);

	Mat4 m;
	get<Transform>()->mat(&m);

	if (!params.camera->visible_sphere(m.translation(), mesh_data->bounds_radius))
		return;

	Loader::shader(Asset::Shader::water);
	Loader::texture(texture);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::water);
	sync->write(params.technique);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view_projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::time);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(params.sync->time.total);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::displacement);
	sync->write(RenderDataType::Vec3);
	sync->write<s32>(1);
	sync->write<Vec3>(Vec3(displacement_horizontal, displacement_vertical, displacement_horizontal));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::normal_map);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(Asset::Texture::water_normal);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	if (params.camera->colors)
		sync->write<Vec4>(color);
	else
		sync->write<Vec4>(PVP_INACCESSIBLE);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(mesh);
}

void Water::draw_alpha(const RenderParams& params)
{
	const Mesh* mesh_data = Loader::mesh(mesh);

	Mat4 m;
	get<Transform>()->mat(&m);
	m.translate(Vec3(0, 0.02f, 0));

	if (!params.camera->visible_sphere(m.translation(), mesh_data->bounds_radius))
		return;

	Loader::shader(Asset::Shader::water);
	Loader::texture(Asset::Texture::water_normal);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::water);
	sync->write(params.technique);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view_projection);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1));

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::time);
	sync->write(RenderDataType::R32);
	sync->write<s32>(1);
	sync->write<r32>(params.sync->time.total);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::normal_map);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(Asset::Texture::water_normal);

	sync->write(RenderOp::FillMode);
	sync->write(RenderFillMode::Point);

	sync->write(RenderOp::PointSize);
	sync->write<r32>(4.0f * UI::scale);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Points);
	sync->write(mesh);

	sync->write(RenderOp::FillMode);
	sync->write(RenderFillMode::Fill);
}

void Cube::draw(const RenderParams& params, const Vec3& pos, const b8 alpha, const Vec3& scale, const Quat& rot, const Vec4& color)
{
	const Mesh* mesh = Loader::mesh_permanent(Asset::Mesh::cube);
	Loader::shader_permanent(Asset::Shader::flat);

	Vec3 radius = mesh->bounds_radius * scale;
	if (!params.camera->visible_sphere(pos, vi_max(radius.x, vi_max(radius.y, radius.z))))
		return;

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(alpha ? Asset::Shader::flat : Asset::Shader::standard);
	sync->write(params.technique);

	Mat4 m;
	m.make_transform(pos, scale, rot);
	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(color);

	if (params.edges)
	{
		sync->write(RenderOp::MeshEdges);
		sync->write(Asset::Mesh::cube);
	}
	else
	{
		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::cube);
	}
}

ScreenQuad::ScreenQuad()
	: mesh(AssetNull)
{
}

void ScreenQuad::init(RenderSync* sync)
{
	mesh = Loader::dynamic_mesh_permanent(3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType::Vec2);

	s32 indices[] =
	{
		0,
		1,
		2,
		1,
		3,
		2
	};

	sync->write(RenderOp::UpdateIndexBuffer);
	sync->write(mesh);
	sync->write<s32>(6);
	sync->write(indices, 6);
}

void ScreenQuad::set(RenderSync* sync, const Rect2& r, const Camera* camera, const Rect2& uv)
{
	Vec3 vertices[] =
	{
		Vec3(r.pos.x, r.pos.y, 0),
		Vec3(r.pos.x + r.size.x, r.pos.y, 0),
		Vec3(r.pos.x, r.pos.y + r.size.y, 0),
		Vec3(r.pos.x + r.size.x, r.pos.y + r.size.y, 0),
	};

	Vec2 uvs[] =
	{
		Vec2(uv.pos.x, uv.pos.y),
		Vec2(uv.pos.x + uv.size.x, uv.pos.y),
		Vec2(uv.pos.x, uv.pos.y + uv.size.y),
		Vec2(uv.pos.x + uv.size.x, uv.pos.y + uv.size.y),
	};

	sync->write(RenderOp::UpdateAttribBuffers);
	sync->write(mesh);
	sync->write<s32>(4);
	sync->write(vertices, 4);
	sync->write(camera->frustum_rays, 4);
	sync->write(uvs, 4);
}

}
