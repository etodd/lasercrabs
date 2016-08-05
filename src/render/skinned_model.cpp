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

SkinnedModel::SkinnedModel()
	: mesh(),
	shader(AssetNull),
	texture(AssetNull),
	offset(Mat4::identity),
	color(-1, -1, -1, -1),
	mask(RENDER_MASK_DEFAULT),
	team((u8)AI::NoTeam)
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
		if (!list_alpha.get(i.index) && !list_additive.get(i.index))
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

void SkinnedModel::alpha()
{
	list_alpha.set(id(), true);
	list_additive.set(id(), false);
}

void SkinnedModel::additive()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), true);
}

void SkinnedModel::alpha_disable()
{
	list_alpha.set(id(), false);
	list_additive.set(id(), false);
}

void SkinnedModel::draw(const RenderParams& params)
{
	if (!(params.camera->mask & mask))
		return;

	RenderSync* sync = params.sync;

	Mat4 m;
	get<Transform>()->mat(&m);
	
	m = offset * m;

	const Mesh* mesh_data = Loader::mesh(mesh);
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
	StaticArray<Mat4, MAX_BONES>& bones = get<Animator>()->bones;
	skin_transforms.resize(bones.length);
	for (s32 i = 0; i < bones.length; i++)
		skin_transforms[i] = arm->inverse_bind_pose[i] * bones[i];

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::bones);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	if (team == (u8)AI::NoTeam)
		sync->write<Vec4>(color);
	else
		sync->write<Vec4>(Team::color((AI::Team)team, (AI::Team)params.camera->team));

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(mesh);

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