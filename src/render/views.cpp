#include "views.h"
#include "load.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "console.h"

namespace VI
{

IntrusiveLinkedList<View>* View::first_additive = nullptr;
IntrusiveLinkedList<View>* View::first_alpha = nullptr;

View::View()
	: mesh(AssetNull), shader(AssetNull), texture(AssetNull), offset(Mat4::identity), color(0, 0, 0, 0), alpha_order(), alpha_enabled(), additive_entry(this), alpha_entry(this)
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
		if (!i.item()->alpha_enabled)
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

void View::draw_alpha(const RenderParams& params)
{
	const IntrusiveLinkedList<View>* v = first_alpha;
	while (v)
	{
		v->object->draw(params);
		v = v->next;
	}
}

void View::alpha(const bool additive, const int order)
{
	if (alpha_enabled)
		alpha_disable();

	alpha_enabled = true;

	alpha_order = order;

	IntrusiveLinkedList<View>*& first = additive ? first_additive : first_alpha;
	IntrusiveLinkedList<View>& entry = additive ? additive_entry : alpha_entry;

	if (first)
	{
		IntrusiveLinkedList<View>* v = first;

		if (alpha_order < v->object->alpha_order)
		{
			entry.insert_before(first);
			first = &entry;
		}
		else
		{
			while (v->next && v->next->object->alpha_order < alpha_order)
				v = v->next;
			entry.insert_after(v);
		}
	}
	else
		first = &entry;
}

void View::alpha_disable()
{
	if (alpha_enabled)
	{
		alpha_enabled = false;

		// Remove additive entry
		if (additive_entry.next)
			additive_entry.next->previous = additive_entry.previous;

		if (additive_entry.previous)
			additive_entry.previous->next = additive_entry.next;
		else if (first_additive == &additive_entry)
			first_additive = additive_entry.next;
		additive_entry.previous = nullptr;
		additive_entry.next = nullptr;

		// Remove alpha entry
		if (alpha_entry.next)
			alpha_entry.next->previous = alpha_entry.previous;

		if (alpha_entry.previous)
			alpha_entry.previous->next = alpha_entry.next;
		else if (first_alpha == &alpha_entry)
			first_alpha = alpha_entry.next;
		alpha_entry.previous = nullptr;
		alpha_entry.next = nullptr;
	}
}

void View::draw(const RenderParams& params) const
{
	if (mesh == AssetNull || shader == AssetNull)
		return;

	Mesh* mesh_data = Loader::mesh(mesh);

	Mat4 m;
	get<Transform>()->mat(&m);
	m = offset * m;

	{
		Vec3 radius = (offset * Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
		if (!params.camera->visible_sphere(m.translation(), fmax(radius.x, fmax(radius.y, radius.z))))
			return;
	}

	Loader::shader(shader);
	Loader::texture(texture);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(shader);
	sync->write(params.technique);

	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	if (texture != AssetNull)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(texture);
	}

	sync->write(RenderOp::Mesh);
	sync->write(mesh);
}

void View::awake()
{
	Mesh* m = Loader::mesh(mesh);
	if (m && color.dot(Vec4(1)) == 0.0f)
		color = m->color;
}

void SkyDecal::draw(const RenderParams& p)
{
	RenderSync* sync = p.sync;

	sync->write<RenderOp>(RenderOp::DepthMask);
	sync->write<bool>(false);

	sync->write<RenderOp>(RenderOp::ColorMask);
	sync->write<bool>(true);
	sync->write<bool>(true);
	sync->write<bool>(true);
	sync->write<bool>(false);

	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Alpha);

	Loader::shader_permanent(Asset::Shader::flat_texture);

	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::flat_texture);
	sync->write(p.technique);

	Loader::mesh_permanent(Asset::Mesh::sky_decal);
	for (auto i = World::components<SkyDecal>().iterator(); !i.is_last(); i.next())
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
		sync->write<int>(1);
		sync->write<Mat4>(mvp);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_color);
		sync->write(RenderDataType::Vec4);
		sync->write<int>(1);
		sync->write<Vec4>(d->color);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(d->texture);

		sync->write(RenderOp::Mesh);
		sync->write(Asset::Mesh::sky_decal);
	}

	sync->write<RenderOp>(RenderOp::DepthMask);
	sync->write<bool>(true);

	sync->write<RenderOp>(RenderOp::ColorMask);
	sync->write<bool>(true);
	sync->write<bool>(true);
	sync->write<bool>(true);
	sync->write<bool>(true);

	sync->write<RenderOp>(RenderOp::BlendMode);
	sync->write<RenderBlendMode>(RenderBlendMode::Opaque);
}

float Skybox::far_plane = 0.0f;
AssetID Skybox::texture = AssetNull;
AssetID Skybox::mesh = AssetNull;
AssetID Skybox::shader = AssetNull;
Vec3 Skybox::color = Vec3(1, 1, 1);
Vec3 Skybox::ambient_color = Vec3(0.1f, 0.1f, 0.1f);
Vec3 Skybox::zenith_color = Vec3(1.0f, 0.4f, 0.9f);

void Skybox::set(const float f, const Vec3& c, const Vec3& ambient, const Vec3& zenith, const AssetID& s, const AssetID& m, const AssetID& t)
{
	far_plane = f;
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
	if (mesh == AssetNull || p.technique != RenderTechnique::Default)
		return;

	Loader::shader(shader);
	Loader::mesh(mesh);
	Loader::texture(texture);

	RenderSync* sync = p.sync;

	sync->write(RenderOp::Shader);
	sync->write(shader);
	sync->write(p.technique);

	Mat4 mvp = p.view * Mat4::make_scale(Vec3(far_plane));
	mvp.translation(Vec3::zero);
	mvp = mvp * p.camera->projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(Vec4(color, 0)); // 0 alpha is a special flag for the compositor

	if (texture != AssetNull)
	{
		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_map);
		sync->write(RenderDataType::Texture);
		sync->write<int>(1);
		sync->write<RenderTextureType>(RenderTextureType::Texture2D);
		sync->write<AssetID>(texture);
	}

	sync->write(RenderOp::Mesh);
	sync->write(mesh);
}

void Cube::draw(const RenderParams& params, const Vec3& pos, const bool alpha, const Vec3& scale, const Quat& rot, const Vec4& color)
{
	Mesh* mesh = Loader::mesh_permanent(Asset::Mesh::cube);
	Loader::shader_permanent(Asset::Shader::flat);

	Vec3 radius = mesh->bounds_radius * scale;
	if (!params.camera->visible_sphere(pos, fmax(radius.x, fmax(radius.y, radius.z))))
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
	sync->write<int>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp::Mesh);
	sync->write(Asset::Mesh::cube);
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

	int indices[] =
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

	Vec2 uvs[] =
	{
		Vec2(uva.x, uva.y),
		Vec2(uvb.x, uva.y),
		Vec2(uva.x, uvb.y),
		Vec2(uvb.x, uvb.y),
	};

	sync->write(RenderOp::UpdateAttribBuffers);
	sync->write(mesh);
	sync->write<int>(4);
	sync->write(vertices, 4);
	sync->write(camera->frustum_rays, 4);
	sync->write(uvs, 4);
}

}
