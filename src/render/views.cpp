#include "views.h"
#include "load.h"
#include "asset.h"

namespace VI
{

View::View()
	: mesh(AssetNull), shader(AssetNull), texture(AssetNull), offset(Mat4::identity), color(1, 1, 1, 1)
{
}

void View::draw(const RenderParams& params)
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);

	SyncData* sync = params.sync;
	sync->write(RenderOp_Mesh);
	sync->write(&mesh);
	sync->write(&shader);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params.view_projection;

	sync->write<int>(texture == AssetNull ? 3 : 4); // Uniform count

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

	sync->write(Asset::Uniform::m);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&m);

	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write(&color);

	if (texture != AssetNull)
	{
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write(&texture);
		sync->write<GLenum>(GL_TEXTURE_2D);
	}
}

void View::awake()
{
	Mesh* m = Loader::mesh(mesh);
	color = m->color;
	Loader::shader(shader);
	Loader::texture(texture);
}

AssetID Skybox::texture = AssetNull;
Vec4 Skybox::color = Vec4(1, 1, 1, 1);

void Skybox::set(const Vec4& c, const AssetID& t)
{
	color = c;
	if (texture != AssetNull)
		Loader::texture_free(texture);
	texture = t;
	Loader::texture(t);
}

void Skybox::draw(const RenderParams& p)
{
	Loader::mesh(Asset::Mesh::skybox);
	Loader::shader(Asset::Shader::flat_texture);
	Loader::texture(texture);

	SyncData* sync = p.sync;

	sync->write(RenderOp_Clear);

	sync->write<GLbitfield>(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	sync->write(RenderOp_DepthMask);
	sync->write<bool>(false);

	sync->write(RenderOp_Mesh);
	sync->write(Asset::Mesh::skybox);
	sync->write(Asset::Shader::flat_texture);

	sync->write<int>(3); // Uniform count

	Mat4 mvp = p.view;
	mvp.translation(Vec3::zero);
	mvp = mvp * p.projection;

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write(&color);

	sync->write(Asset::Uniform::diffuse_map);
	sync->write(RenderDataType_Texture);
	sync->write<int>(1);
	sync->write(&texture);
	sync->write<GLenum>(GL_TEXTURE_2D);

	sync->write(RenderOp_DepthMask);
	sync->write<bool>(true);
}

}
