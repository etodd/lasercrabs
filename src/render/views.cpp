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
	sync->write(mesh);
	sync->write(shader);
	sync->write(params.technique);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params.view_projection;

	sync->write<int>(texture == AssetNull ? 3 : 4); // Uniform count

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(Asset::Uniform::m);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(m);

	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	if (texture != AssetNull)
	{
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write<AssetID>(texture);
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
	sync->write(p.technique);

	sync->write<int>(texture == AssetNull ? 2 : 3); // Uniform count

	Mat4 mvp = p.view;
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->proj;

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	if (texture != AssetNull)
	{
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write<AssetID>(texture);
		sync->write<RenderTextureType>(RenderTexture2D);
	}
}

ScreenQuad::ScreenQuad()
	: mesh(AssetNull)
{
}

void ScreenQuad::init(RenderSync* sync)
{
	mesh = Loader::dynamic_mesh_permanent(3, false);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec3);
	Loader::dynamic_mesh_attrib(RenderDataType_Vec2);

	int indices[] =
	{
		0,
		1,
		2,
		1,
		3,
		2
	};

	sync->write(RenderOp_UpdateIndexBuffer);
	sync->write(mesh);
	sync->write<int>(6);
	sync->write(indices, 6);
}

void ScreenQuad::set(RenderSync* sync, const Vec2& a, const Vec2& b, const Camera* camera, const Vec2& uva, const Vec2& uvb)
{
	Vec3 vertices[] =
	{
		Vec3(a.x, a.y, 0),
		Vec3(b.x, a.y, 0),
		Vec3(a.x, b.y, 0),
		Vec3(b.x, b.y, 0),
	};

	Mat4 p_inverse = camera->proj.inverse();

	Vec4 rays[] =
	{
		p_inverse * Vec4(-1, -1, 0, 1),
		p_inverse * Vec4(1, -1, 0, 1),
		p_inverse * Vec4(-1, 1, 0, 1),
		p_inverse * Vec4(1, 1, 0, 1),
	};
	rays[0] /= rays[0].w;
	rays[0] /= rays[0].z;
	rays[1] /= rays[1].w;
	rays[1] /= rays[1].z;
	rays[2] /= rays[2].w;
	rays[2] /= rays[2].z;
	rays[3] /= rays[3].w;
	rays[3] /= rays[3].z;

	Vec3 rays3[] =
	{
		camera->rot * rays[0].xyz(),
		camera->rot * rays[1].xyz(),
		camera->rot * rays[2].xyz(),
		camera->rot * rays[3].xyz(),
	};

	Vec2 uvs[] =
	{
		Vec2(uva.x, uva.y),
		Vec2(uvb.x, uva.y),
		Vec2(uva.x, uvb.y),
		Vec2(uvb.x, uvb.y),
	};

	sync->write(RenderOp_UpdateAttribBuffers);
	sync->write(mesh);
	sync->write<int>(4);
	sync->write(vertices, 4);
	sync->write(rays3, 4);
	sync->write(uvs, 4);
}

}
