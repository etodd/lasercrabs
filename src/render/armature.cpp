#include "armature.h"
#include "load.h"

Armature::Armature()
	: mesh(), shader(), texture(), bones(), time(), animation(), scale(1, 1, 1)
{
}

template<typename T>
static size_t find_keyframe_index(Array<T>& keyframes, float time)
{
	size_t index;
	for (index = 0; index < keyframes.length - 2; index++)
	{
		if (time < keyframes[index + 1].time)
			break;
	}
	return index;
}

void Armature::update(Update u)
{
	time += u.time.delta;
	while (time > animation->duration)
		time -= animation->duration;

	for (size_t i = 0; i < animation->channels.length; i++)
	{
		Channel* c = &animation->channels[i];

		Vec3 position;
		Vec3 scale;
		Quat rotation;

		size_t index;
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
	for (size_t i = 0; i < bones.length; i++)
	{
		int parent = m->bone_hierarchy[i];
		if (parent == -1)
			bones[i] = animation->channels[i].current_transform;
		else
			bones[i] = animation->channels[i].current_transform * bones[parent];
	}
}

void Armature::draw(RenderParams* params)
{
	SyncData* sync = params->sync;

	Mat4 m;
	get<Transform>()->mat(&m);
	m.scale(scale);

	sync->write(RenderOp_View);
	sync->write(&mesh);
	sync->write(&shader);
	sync->write(&texture);
	Mat4 mvp = m * params->view * params->projection;

	sync->write<int>(4); // Uniform count

	sync->write(Asset::Uniform::MVP);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&mvp);

	sync->write(Asset::Uniform::M);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&m);

	sync->write(Asset::Uniform::V);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(1);
	sync->write(&params->view);

	Mesh* m2 = Loader::mesh(mesh);
	skin_transforms.resize(bones.length);
	for (size_t i = 0; i < bones.length; i++)
		skin_transforms[i] = m2->inverse_bind_pose[i] * bones[i];

	sync->write(Asset::Uniform::Bones);
	sync->write(RenderDataType_Mat4);
	sync->write<int>(skin_transforms.length);
	sync->write(skin_transforms.data, skin_transforms.length);
}

void Armature::awake()
{
	Loader::mesh(mesh);
	Loader::shader(shader);
	Loader::texture(texture);
}