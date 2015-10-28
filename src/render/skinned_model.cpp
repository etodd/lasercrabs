#include "skinned_model.h"
#include "load.h"
#include "data/animator.h"
#include "asset/shader.h"

#define DEBUG_SKIN 0

#if DEBUG_SKIN
#include "render/views.h"
#endif

namespace VI
{

SkinnedModel::SkinnedModel()
	: mesh(), shader(), texture(), offset(Mat4::identity), color(0, 0, 0, 0)
{
}

void SkinnedModel::awake()
{
	Mesh* m = Loader::mesh(mesh);
	if (m && color.dot(Vec4(1)) == 0.0f)
		color = m->color;
	Loader::shader(shader);
	Loader::texture(texture);
}

void SkinnedModel::draw(const RenderParams& params)
{
	RenderSync* sync = params.sync;

	Mat4 m;
	get<Transform>()->mat(&m);
	
	m = offset * m;

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
	sync->write(Asset::Uniform::diffuse_map);
	sync->write(RenderDataType::Texture);
	sync->write<int>(1);
	sync->write<RenderTextureType>(RenderTexture2D);
	sync->write<AssetID>(texture);

	Armature* arm = Loader::armature(get<Animator>()->armature);
	Array<Mat4>& bones = get<Animator>()->bones;
	skin_transforms.resize(bones.length);
	for (int i = 0; i < bones.length; i++)
		skin_transforms[i] = arm->inverse_bind_pose[i] * bones[i];

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::bones);
	sync->write(RenderDataType::Mat4);
	sync->write<int>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp::Mesh);
	sync->write(mesh);

#if DEBUG_SKIN
	for (int i = 0; i < bones.length; i++)
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
