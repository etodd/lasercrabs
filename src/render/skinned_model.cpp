#include "skinned_model.h"
#include "load.h"
#include "data/animator.h"
#include "asset/shader.h"

namespace VI
{

SkinnedModel::SkinnedModel()
	: mesh(), shader(), texture(), offset(Mat4::identity), color(1, 1, 1, 1)
{
}

void SkinnedModel::awake()
{
	Mesh* m = Loader::mesh(mesh);
	if (m)
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

	sync->write(RenderOp_Shader);
	sync->write(shader);
	sync->write(params.technique);
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
	sync->write(Asset::Uniform::diffuse_map);
	sync->write(RenderDataType_Texture);
	sync->write<int>(1);
	sync->write<RenderTextureType>(RenderTexture2D);
	sync->write<AssetID>(texture);

	Armature* arm = Loader::armature(get<Animator>()->armature);
	Array<Mat4>& bones = get<Animator>()->bones;
	skin_transforms.resize(bones.length);
	for (int i = 0; i < bones.length; i++)
		skin_transforms[i] = arm->inverse_bind_pose[i] * bones[i];

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::bones);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);

	sync->write(RenderOp_Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp_Mesh);
	sync->write(mesh);
	/*
	// Debug
	Loader::mesh(Asset::Mesh::cube);
	Loader::shader(Asset::Shader::Standard);
	Loader::texture(Asset::Texture::test);
	for (int i = 0; i < bones.length; i++)
	{
		sync->write(Asset::Shader::Standard);
		sync->write(params.technique);
		Mat4 mvp = bones[i] * m * params.view * params.camera->projection;

		sync->write(RenderOp_Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType_Mat4);
		sync->write<int>(1);
		sync->write<Mat4>(mvp);

		sync->write(RenderOp_Uniform);
		sync->write(Asset::Uniform::m);
		sync->write(RenderDataType_Mat4);
		sync->write<int>(1);
		sync->write<Mat4>(m);

		sync->write(RenderOp_Uniform);
		sync->write(Asset::Uniform::diffuse_color);
		sync->write(RenderDataType_Vec4);
		sync->write<int>(1);
		sync->write<Vec4>(color);

		sync->write(RenderOp_Mesh);
		sync->write(Asset::Mesh::cube);
	}
	*/
}

}
