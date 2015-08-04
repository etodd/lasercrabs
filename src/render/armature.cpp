#include "armature.h"
#include "load.h"

namespace VI
{

Armature::Armature()
	: mesh(), shader(), texture(), bones(), time(), animation(), offset(Mat4::identity), color(1, 1, 1, 1)
{
}

template<typename T>
static int find_keyframe_index(Array<T>& keyframes, float time)
{
	int index;
	for (index = 0; index < keyframes.length - 2; index++)
	{
		if (time < keyframes[index + 1].time)
			break;
	}
	return index;
}

void Armature::update(const Update& u)
{
	time += u.time.delta;
	while (time > animation->duration)
		time -= animation->duration;

	for (int i = 0; i < animation->channels.length; i++)
	{
		Channel* c = &animation->channels[i];

		Vec3 position;
		Vec3 scale;
		Quat rotation;

		int index;
		float last_time;
		float next_time;
		float blend;

		if (c->positions.length == 0)
			position = Vec3::zero;
		else if (c->positions.length == 1)
			position = c->positions[0].value;
		else
		{
			index = find_keyframe_index(c->positions, time);
			last_time = c->positions[index].time;
			next_time = c->positions[index + 1].time;
			blend = fmin(1.0f, (time - last_time) / (next_time - last_time));
			position = Vec3::lerp(blend, c->positions[index].value, c->positions[index + 1].value);
		}

		if (c->scales.length == 0)
			scale = Vec3(1, 1, 1);
		else if (c->scales.length == 1)
			scale = c->scales[0].value;
		else
		{
			index = find_keyframe_index(c->scales, time);
			last_time = c->scales[index].time;
			next_time = c->scales[index + 1].time;
			blend = fmin(1.0f, (time - last_time) / (next_time - last_time));
			scale = Vec3::lerp(blend, c->scales[index].value, c->scales[index + 1].value);
		}

		if (c->rotations.length == 0)
			rotation = Quat::identity;
		else if (c->rotations.length == 1)
			rotation = c->rotations[0].value;
		else
		{
			index = find_keyframe_index(c->rotations, time);
			last_time = c->rotations[index].time;
			next_time = c->rotations[index + 1].time;
			blend = fmin(1.0f, (time - last_time) / (next_time - last_time));
			rotation = Quat::slerp(blend, c->rotations[index].value, c->rotations[index + 1].value);
		}

		c->current_transform.make_transform(position, scale, rotation);
	}

	update_world_transforms();
}

void Armature::update_world_transforms()
{
	Mesh* m = Loader::mesh(mesh);
	bones.resize(animation->channels.length);
	for (int i = 0; i < bones.length; i++)
	{
		int parent = m->bone_hierarchy[i];
		if (parent == -1)
			bones[i] = animation->channels[i].current_transform;
		else
			bones[i] = animation->channels[i].current_transform * bones[parent];
	}
}

void Armature::draw(const RenderParams& params)
{
	SyncData* sync = params.sync;

	Mat4 m;
	get<Transform>()->mat(&m);
	
	m = offset * m;

	sync->write(RenderOp_Mesh);
	sync->write(&mesh);
	sync->write(&shader);
	Mat4 mvp = m * params.view_projection;

	sync->write<int>(5); // Uniform count

	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

	sync->write(Asset::Uniform::m);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&m);

	sync->write(Asset::Uniform::diffuse_map);
	sync->write(RenderDataType_Texture);
	sync->write<int>(1);
	sync->write(&texture);
	sync->write<GLenum>(GL_TEXTURE_2D);

	Mesh* m2 = Loader::mesh(mesh);
	skin_transforms.resize(bones.length);
	for (int i = 0; i < bones.length; i++)
		skin_transforms[i] = m2->inverse_bind_pose[i] * bones[i];

	sync->write(Asset::Uniform::bones);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);

	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType_Vec4);
	sync->write<int>(1);
	sync->write(&color);

	/*
	// Debug
	Loader::mesh(Asset::Model::cube);
	Loader::shader(Asset::Shader::Standard);
	Loader::texture(Asset::Texture::test);
	for (int i = 0; i < bones.length; i++)
	{
		sync->write(RenderOp_Mesh);
		sync->write(Asset::Model::cube);
		sync->write(Asset::Shader::Standard);
		Mat4 mvp = bones[i] * m * params.view * params.projection;

		sync->write<int>(3); // Uniform count

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
	}
	*/
}

void Armature::awake()
{
	Mesh* m = Loader::mesh(mesh);
	color = m->color;
	Loader::shader(shader);
	Loader::texture(texture);
}

}
