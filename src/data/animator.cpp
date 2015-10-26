#include "animator.h"
#include "load.h"

namespace VI
{

Animator::Animator()
	: bones(), channels(), time(), animation(AssetNull), armature(AssetNull), bindings(), triggers(), offsets()
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

void Animator::update(const Update& u)
{
	Animation* anim = Loader::animation(animation);

	if (anim)
	{
		float old_time = time;
		time += u.time.delta;

		bool looped = false;
		while (time > anim->duration)
		{
			time -= anim->duration;
			looped = true;
		}

		for (int i = 0; i < triggers.length; i++)
		{
			TriggerEntry* trigger = &triggers[i];
			bool trigger_after_old_time = old_time <= trigger->time;
			bool trigger_before_new_time = time >= trigger->time;
			if (animation == trigger->animation &&
				(((looped || trigger_after_old_time) && trigger_before_new_time) || (trigger_after_old_time && (looped || trigger_before_new_time))))
			{
				trigger->link.fire();
			}
		}

		channels.resize(anim->channels.length);
		for (int i = 0; i < anim->channels.length; i++)
		{
			Channel* c = &anim->channels[i];

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

			channels[i].bone = c->bone_index;
			channels[i].transform.make_transform(position, scale, rotation);
		}
	}

	update_world_transforms();
}

void Animator::update_world_transforms()
{
	Armature* arm = Loader::armature(armature);
	bones.resize(arm->hierarchy.length);
	if (offsets.length < arm->hierarchy.length)
	{
		int old_length = offsets.length;
		offsets.resize(arm->hierarchy.length);
		for (int i = old_length; i < offsets.length; i++)
			offsets[i] = Mat4::identity;
	}
	Mat4 transform;
	get<Transform>()->mat(&transform);

	for (int i = 0; i < bones.length; i++)
	{
		bones[i].make_transform(arm->bind_pose[i].pos, Vec3(1, 1, 1), arm->bind_pose[i].rot);
		bones[i] = offsets[i] * bones[i];
	}

	for (int i = 0; i < channels.length; i++)
	{
		int bone_index = channels[i].bone;
		bones[bone_index] = offsets[i] * channels[i].transform;
	}

	for (int i = 0; i < bones.length; i++)
	{
		int parent = arm->hierarchy[i];
		if (parent != -1)
			bones[i] = bones[i] * bones[parent];
	}

	for (int i = 0; i < bindings.length; i++)
	{
		BindEntry& binding = bindings[i];
		Mat4 mat = transform * bones[binding.bone];
		Vec3 pos;
		Quat quat;
		Vec3 scale;
		mat.decomposition(pos, scale, quat);
		Transform* t = binding.transform.ref();
		if (t)
			t->absolute(pos, quat);
		else
		{
			bindings.remove(i);
			i--;
		}
	}
}

void Animator::bind(const int bone, Transform* transform)
{
	BindEntry* entry = bindings.add();
	entry->bone = bone;
	entry->transform = transform;

	Mat4 world_matrix;
	get<Transform>()->mat(&world_matrix);
	Mat4 mat = world_matrix * bones[bone];
	Vec3 pos;
	Quat quat;
	Vec3 scale;
	mat.decomposition(pos, scale, quat);
	transform->absolute(pos, quat);
}

void Animator::unbind(const Transform* transform)
{
	for (int i = 0; i < bindings.length; i++)
	{
		BindEntry& entry = bindings[i];
		if (entry.transform.ref() == transform)
		{
			bindings.remove(i);
			i--;
		}
	}
}

void Animator::bone_transform(const int index, Vec3* pos, Quat* rot)
{
	if (bones.length == 0)
		update_world_transforms();
	Vec3 bone_scale;
	Vec3 bone_pos;
	Quat bone_rot;
	bones[index].decomposition(bone_pos, bone_scale, bone_rot);
	*rot = bone_rot * *rot;
	*pos = (bone_rot * *pos) + bone_pos;
}

void Animator::offset_bone(const int index, const Vec3& pos, const Quat& rot)
{
	if (bones.length == 0)
		update_world_transforms();
	Armature* arm = Loader::armature(armature);
	offsets[index].make_transform(pos, Vec3(1), rot);
}

void Animator::awake()
{
	Loader::armature(armature);
	Loader::animation(animation);
}

Link& Animator::trigger(const AssetID anim, const float time)
{
	TriggerEntry* entry = triggers.add();
	entry->animation = anim;
	entry->time = time;
	return entry->link;
}

}
