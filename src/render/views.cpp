#include "views.h"
#include "load.h"
#include "asset/shader.h"

namespace VI
{

View* View::first_alpha = nullptr;

View::View()
	: mesh(AssetNull), shader(AssetNull), texture(AssetNull), offset(Mat4::identity), color(1, 1, 1, 1), alpha_order(-1)
{
}

View::~View()
{
	alpha_disable();
}

void View::draw_opaque(const RenderParams& params)
{
	for (auto i = World::components<View>().iterator(); !i.is_last(); i.next())
	{
		if (i.item()->alpha_order < 0)
			i.item()->draw(params);
	}
}

void View::draw_alpha(const RenderParams& params)
{
	const View* v = first_alpha;
	while (v)
	{
		v->draw(params);
		v = v->next;
	}
}

void View::alpha(int order)
{
	vi_assert(order >= 0);

	if (alpha_order != -1)
		alpha_disable();

	alpha_order = order;

	if (first_alpha)
	{
		View* v = first_alpha;

		if (alpha_order < v->alpha_order)
		{
			insert_before(first_alpha);
			first_alpha = this;
		}
		else
		{
			while (v->next && v->next->alpha_order < alpha_order)
				v = v->next;
			insert_after(v);
		}
	}
	else
		first_alpha = this;
}

void View::alpha_disable()
{
	if (alpha_order != -1)
	{
		alpha_order = -1;
		if (previous)
			previous->next = next;
		else
			first_alpha = nullptr;
		next = nullptr;
	}
}

void View::draw(const RenderParams& params) const
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;
	sync->write(RenderOp_Mesh);
	sync->write(&mesh);
	sync->write(&shader);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params.view_projection;

	sync->write<int>(texture == AssetNull ? 4 : 5); // Uniform count

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

	sync->write(Asset::Uniform::light_position);
	sync->write(RenderDataType_Vec3);
	sync->write<int>(1);
	sync->write(&params.camera->pos);

	if (texture != AssetNull)
	{
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write(&texture);
		sync->write<RenderTextureType>(RenderTexture2D);
	}
}

void View::awake()
{
	Mesh* m = Loader::mesh(mesh);
	if (m)
		color = m->color;
	Loader::shader(shader);
	Loader::texture(texture);
}

AssetID Skybox::texture = AssetNull;
AssetID Skybox::mesh = AssetNull;
AssetID Skybox::shader = AssetNull;
Vec4 Skybox::color = Vec4(1, 1, 1, 1);

void Skybox::set(const Vec4& c, const AssetID& s, const AssetID& m, const AssetID& t)
{
	color = c;

	if (shader != AssetNull && shader != s)
		Loader::shader_free(shader);
	shader = s;
	Loader::shader(s);

	if (mesh != AssetNull && mesh != m)
		Loader::mesh_free(mesh);
	mesh = m;
	Loader::mesh(m);

	if (texture != AssetNull && texture != t)
		Loader::texture_free(texture);
	texture = t;
	Loader::texture(t);
}

bool Skybox::valid()
{
	return shader != AssetNull && mesh != AssetNull;
}

void Skybox::draw(const RenderParams& p)
{
	if (shader == AssetNull || mesh == AssetNull)
		return;

	Loader::shader(shader);
	Loader::mesh(mesh);
	Loader::texture(texture);

	RenderSync* sync = p.sync;

	sync->write(RenderOp_Mesh);
	sync->write(mesh);
	sync->write(shader);

	sync->write<int>(texture == AssetNull ? 2 : 3); // Uniform count

	Mat4 mvp = p.view;
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

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
		sync->write<RenderTextureType>(RenderTexture2D);
	}
}

}
