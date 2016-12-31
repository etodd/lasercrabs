#include "skinned_model.h"
#include "load.h"
#include "asset/shader.h"
#include "data/components.h"
#include "data/animator.h"
#include "game/team.h"

#define DEBUG_SKIN 0

#if DEBUG_SKIN
#include "render/views.h"
#endif

namespace VI
{

Bitmask<MAX_ENTITIES> SkinnedModel::list_alpha;
Bitmask<MAX_ENTITIES> SkinnedModel::list_additive;
Bitmask<MAX_ENTITIES> SkinnedModel::list_hollow;

SkinnedModel::SkinnedModel()
	: mesh(AssetNull),
	mesh_shadow(AssetNull),
	shader(AssetNull),
	texture(AssetNull),
	offset(Mat4::identity),
	color(-1, -1, -1, -1),
	mask(RENDER_MASK_DEFAULT),
	team((s8)AI::TeamNone)
{
}

void SkinnedModel::awake()
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
	Loader::shader(shader);
	Loader::texture(texture);
}

SkinnedModel::~SkinnedModel()
{
	alpha_disable();
}

void SkinnedModel::draw_opaque(const RenderParams& params)
{
	for (auto i = SkinnedModel::list.iterator(); !i.is_last(); i.next())
	{
		if (!list_alpha.get(i.index) && !list_additive.get(i.index) && !list_hollow.get(i.index))
			i.item()->draw(params);
	}
}

void SkinnedModel::draw_additive(const RenderParams& params)
{
	for (auto i = SkinnedModel::list.iterator(); !i.is_last(); i.next())
	{
		if (list_additive.get(i.index))
			i.item()->draw(params);
	}
}

void SkinnedModel::draw_alpha(const RenderParams& params)
{
	for (auto i = SkinnedModel::list.iterator(); !i.is_last(); i.next())
	{
		if (list_alpha.get(i.index))
			i.item()->draw(params);
	}
}

void SkinnedModel::draw_hollow(const RenderParams& params)
{
	for (auto i = SkinnedModel::list.iterator(); !i.is_last(); i.next())
	{
		if (list_hollow.get(i.index))
			i.item()->draw(params);
	}
}

void SkinnedModel::alpha()
{
	list_alpha.set(id(), true);
	list_additive.set(id(), false);
	list_hollow.set(id(), false);
}

void SkinnedModel::additive()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), true);
	list_hollow.set(id(), false);
}

void SkinnedModel::hollow()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), false);
	list_hollow.set(id(), true);
}

void SkinnedModel::alpha_disable()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), false);
	list_hollow.set(id(), false);
}

AlphaMode SkinnedModel::alpha_mode() const
{
	if (list_alpha.get(id()))
		return AlphaMode::Alpha;
	else if (list_additive.get(id()))
		return AlphaMode::Additive;
	else if (list_hollow.get(id()))
		return AlphaMode::Hollow;
	else
		return AlphaMode::Opaque;
}

void SkinnedModel::alpha_mode(AlphaMode m)
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
		case AlphaMode::Hollow:
		{
			hollow();
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void SkinnedModel::draw(const RenderParams& params)
{
	if (!(params.camera->mask & mask))
		return;

	RenderSync* sync = params.sync;

	Mat4 m;
	get<Transform>()->mat(&m);
	
	m = offset * m;

	AssetID mesh_actual = (params.technique != RenderTechnique::Shadow || params.edges) || mesh_shadow == AssetNull ? mesh : mesh_shadow;

	const Mesh* mesh_data = Loader::mesh(mesh_actual);
	{
		Vec3 radius = (offset * Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
		if (!params.camera->visible_sphere(m.translation(), vi_max(radius.x, vi_max(radius.y, radius.z))))
			return;
	}

	sync->write(RenderOp::Shader);
	sync->write(shader);
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
	sync->write(Asset::Uniform::diffuse_map);
	sync->write(RenderDataType::Texture);
	sync->write<s32>(1);
	sync->write<RenderTextureType>(RenderTextureType::Texture2D);
	sync->write<AssetID>(texture);

	const Armature* arm = Loader::armature(get<Animator>()->armature);
	Array<Mat4>* bones = &get<Animator>()->bones;
	StaticArray<Mat4, MAX_BONES> skin_transforms;
	skin_transforms.resize(bones->length);
	for (s32 i = 0; i < bones->length; i++)
		skin_transforms[i] = arm->inverse_bind_pose[i] * (*bones)[i];

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::bones);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);

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
		if (list_alpha.get(id()) || list_additive.get(id()) || list_hollow.get(id()))
			sync->write<Vec4>(Vec4(team_color.xyz(), color.w));
		else
			sync->write<Vec4>(team_color);
	}

	if (params.edges)
	{
		sync->write(RenderOp::MeshEdges);
		sync->write(mesh_actual);
	}
	else
	{
		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(mesh_actual);
	}

#if DEBUG_SKIN
	for (s32 i = 0; i < bones.length; i++)
	{
		Mat4 bone_transform = bones[i] * m;
		Vec3 pos;
		Vec3 scale;
		Quat rot;
		bone_transform.decomposition(pos, scale, rot);
		Cube::draw(params, pos, false, Vec3(0.02f), rot);
	}
#endif
}

}