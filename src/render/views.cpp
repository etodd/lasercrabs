#include "views.h"
#include "load.h"
#include "asset/shader.h"
#include "asset/mesh.h"

namespace VI
{

IntrusiveLinkedList<View>* View::first_additive = nullptr;

View::View()
	: mesh(AssetNull), shader(AssetNull), texture(AssetNull), offset(Mat4::identity), color(0, 0, 0, 0), alpha_order(-1), additive_entry(this)
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

void View::draw_additive(const RenderParams& params)
{
	const IntrusiveLinkedList<View>* v = first_additive;
	while (v)
	{
		v->object->draw(params);
		v = v->next;
	}
}

void View::alpha(int order)
{
	vi_assert(order >= 0);

	if (alpha_order != -1)
		alpha_disable();

	alpha_order = order;

	if (first_additive)
	{
		IntrusiveLinkedList<View>* v = first_additive;

		if (alpha_order < v->object->alpha_order)
		{
			additive_entry.insert_before(first_additive);
			first_additive = &additive_entry;
		}
		else
		{
			while (v->next && v->next->object->alpha_order < alpha_order)
				v = v->next;
			additive_entry.insert_after(v);
		}
	}
	else
		first_additive = &additive_entry;
}

void View::alpha_disable()
{
	if (alpha_order != -1)
	{
		alpha_order = -1;

		if (additive_entry.next)
			additive_entry.next->previous = additive_entry.previous;

		if (additive_entry.previous)
			additive_entry.previous->next = additive_entry.next;
		else
			first_additive = additive_entry.next;
		additive_entry.previous = nullptr;
		additive_entry.next = nullptr;
	}
}

void View::draw(const RenderParams& params) const
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;
	sync->write(RenderOp_Shader);
	sync->write(shader);
	sync->write(params.technique);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;
	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	if (texture != AssetNull)
	{
		sync->write(RenderOp_Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(texture);
	}

	sync->write(RenderOp_Mesh);
	sync->write(mesh);
}

void View::awake()
{
	Mesh* m = Loader::mesh(mesh);
	if (m && color.dot(Vec4(1)) == 0.0f)
		color = m->color;
	Loader::shader(shader);
	Loader::texture(texture);
}

AssetID Skybox::texture = AssetNull;
AssetID Skybox::mesh = AssetNull;
AssetID Skybox::shader = AssetNull;
Vec3 Skybox::color = Vec3(1, 1, 1);
Vec3 Skybox::ambient_color = Vec3(0.1f, 0.1f, 0.1f);
Vec3 Skybox::zenith_color = Vec3(1.0f, 0.4f, 0.9f);

void Skybox::set(const Vec3& c, const Vec3& ambient, const Vec3& zenith, const AssetID& s, const AssetID& m, const AssetID& t)
{
	color = c;
	ambient_color = ambient;
	zenith_color = zenith;

	if (shader != AssetNull && shader != s)
		Loader::shader_free(shader);
	if (mesh != AssetNull && mesh != m)
		Loader::mesh_free(mesh);
	if (texture != AssetNull && texture != t)
		Loader::texture_free(texture);

	shader = s;
	Loader::shader(s);

	mesh = m;
	Loader::mesh(m);

	texture = t;
	Loader::texture(t);
}

bool Skybox::valid()
{
	return shader != AssetNull && mesh != AssetNull;
}

void Skybox::draw(const RenderParams& p)
{
	if (shader == AssetNull || mesh == AssetNull || p.technique != RenderTechnique_Default)
		return;

	Loader::shader(shader);
	Loader::mesh(mesh);
	Loader::texture(texture);

	RenderSync* sync = p.sync;

	sync->write(RenderOp_Shader);
	sync->write(shader);
	sync->write(p.technique);

	Mat4 mvp = p.view;
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(Vec4(color, 0)); // 0 alpha is a special flag for the compositor

	if (texture != AssetNull)
	{
		sync->write(RenderOp_Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType_Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTexture2D);
		sync->write<AssetID>(texture);
	}

	sync->write(RenderOp_Mesh);
	sync->write(mesh);
}

void Cube::draw(const RenderParams& params, const Vec3& pos, const Vec3& scale, const Quat& rot, const Vec4& color)
{
	if (params.technique != RenderTechnique_Default)
		return;

	Loader::mesh_permanent(Asset::Mesh::cube);
	Loader::shader_permanent(Asset::Shader::flat);

	RenderSync* sync = params.sync;
	sync->write(RenderOp_Shader);
	sync->write(Asset::Shader::flat);
	sync->write(params.technique);

	Mat4 m;
	m.make_transform(pos, scale, rot);
	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp_Mesh);
	sync->write(Asset::Mesh::cube);
}

ScreenQuad::ScreenQuad()
	: mesh(AssetNull)
{
}

void ScreenQuad::init(RenderSync* sync)
{
	mesh = Loader::dynamic_mesh_permanent(3);
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

	Vec3 frustum[4];
	camera->projection_frustum(frustum);

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
	sync->write(frustum, 4);
	sync->write(uvs, 4);
}

}
