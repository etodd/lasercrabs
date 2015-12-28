#include "animator.h"
#include "load.h"
#include "components.h"

namespace VI
{

void Animator::AnimatorTransform::blend(float x, const AnimatorTransform& b)
{
	pos = Vec3::lerp(x, pos, b.pos);
	rot = Quat::slerp(x, rot, b.rot);
	scale = Vec3::lerp(x, scale, b.scale);
}

Animator::Layer::Layer()
	: channels(),
	last_animation_channels(),
	time(),
	weight(1.0f),
	blend(1.0f),
	blend_time(0.25f),
	animation(AssetNull),
	last_animation(AssetNull),
	loop(true),
	speed(1.0f)
{
}

Animator::Animator()
	: armature(AssetNull),
	layers(),
	bindings(),
	triggers(),
	offsets(),
	override_mode(),
	bones()
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

void Animator::Layer::play(AssetID a)
{
	animation = a;
	time = 0.0f;
}

void Animator::Layer::update(const Update& u, const Animator& animator)
{
	Animation* anim = Loader::animation(animation);

	blend = fmin(1.0f, blend + u.time.delta / blend_time);

	if (anim)
	{
		float old_time = time;
		time += u.time.delta * speed;

		bool looped = false;
		if (time > anim->duration)
		{
			if (loop)
			{
				time = fmod(time, anim->duration);
				looped = true;
			}
			else
			{
				animation = AssetNull;
				changed_animation();
				channels.resize(0);
				return;
			}
		}

		if (animation != last_animation)
			changed_animation();

		for (int i = 0; i < animator.triggers.length; i++)
		{
			const TriggerEntry* trigger = &animator.triggers[i];
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
			channels[i].transform.pos = position;
			channels[i].transform.rot = rotation;
			channels[i].transform.scale = scale;
		}
	}
	else
		channels.resize(0);
}

void Animator::Layer::changed_animation()
{
	if (channels.length > 0)
	{
		last_animation_channels.length = channels.length;
		memcpy(&last_animation_channels[0], &channels[0], sizeof(AnimatorChannel) * channels.length);
		blend = 1.0f - blend;
	}
	last_animation = animation;
}

void Animator::update(const Update& u)
{
	for (int i = 0; i < MAX_ANIMATIONS; i++)
		layers[i].update(u, *this);
	update_world_transforms();
}

void Animator::update_world_transforms()
{
	Armature* arm = Loader::armature(armature);
	bones.resize(arm->hierarchy.length);
	if (offsets.length < arm->hierarchy.length)
	{
		int old_length = offsets.length;
		offsets.length = arm->hierarchy.length;
		for (int i = old_length; i < offsets.length; i++)
		{
			if (override_mode == OverrideMode::Offset)
				offsets[i] = Mat4::identity;
			else
				offsets[i].make_transform(arm->bind_pose[i].pos, Vec3(1, 1, 1), arm->bind_pose[i].rot);
		}
	}

	if (override_mode == OverrideMode::Offset)
	{
		AnimatorTransform bone_channels[MAX_BONES];
		for (int i = 0; i < bones.length; i++)
		{
			bone_channels[i].pos = arm->bind_pose[i].pos;
			bone_channels[i].rot = arm->bind_pose[i].rot;
			bone_channels[i].scale = Vec3(1, 1, 1);
		}

		for (int l = 0; l < MAX_ANIMATIONS; l++)
		{
			Layer& layer = layers[l];

			if (layer.blend < 1.0f)
			{
				float blend = layer.weight * (1.0f - layer.blend);
				for (int i = 0; i < layer.last_animation_channels.length; i++)
				{
					AnimatorChannel& channel = layer.last_animation_channels[i];
					bone_channels[channel.bone].blend(blend * layer.weight, channel.transform);
				}
			}

			{
				float blend = layer.weight * layer.blend;
				for (int i = 0; i < layer.channels.length; i++)
				{
					AnimatorChannel& channel = layer.channels[i];
					bone_channels[channel.bone].blend(blend, channel.transform);
				}
			}
		}

		for (int i = 0; i < bones.length; i++)
		{
			bones[i].make_transform(bone_channels[i].pos, bone_channels[i].scale, bone_channels[i].rot);
			bones[i] = offsets[i] * bones[i];
		}
	}
	else
	{
		for (int i = 0; i < bones.length; i++)
			bones[i] = offsets[i];
	}

	for (int i = 0; i < bones.length; i++)
	{
		int parent = arm->hierarchy[i];
		if (parent != -1)
			bones[i] = bones[i] * bones[parent];
	}

	Mat4 transform;
	get<Transform>()->mat(&transform);
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

void Animator::to_world(const int index, Vec3* pos, Quat* rot)
{
	bone_transform(index, pos, rot);
	*pos = (get<SkinnedModel>()->offset * Vec4(*pos)).xyz();
	*rot = get<SkinnedModel>()->offset.extract_quat() * *rot;
	rot->normalize();
	get<Transform>()->to_world(pos, rot);
}

void Animator::from_bone_body(const int index, const Vec3& pos, const Quat& rot, const Vec3& body_to_bone_pos, const Quat& body_to_bone_rot)
{
	update_world_transforms();

	Mat4 offset_inverse = get<SkinnedModel>()->offset.inverse();
	Quat offset_inverse_quat = offset_inverse.extract_quat();
	offset_inverse_quat.normalize();

	Vec3 offset_pos = (offset_inverse * Vec4(pos)).xyz();
	Quat offset_rot = offset_inverse_quat * rot;

	Vec3 parent_pos = Vec3::zero;
	Quat parent_rot = Quat::identity;
	Armature* arm = Loader::armature(armature);
	int parent = arm->hierarchy[index];
	if (parent != -1)
		bone_transform(parent, &parent_pos, &parent_rot);
	Quat parent_rot_inverse = parent_rot.inverse();

	Vec3 local_pos = parent_rot_inverse * (offset_pos - parent_pos);
	Quat local_rot = parent_rot_inverse * offset_rot;

	Vec3 bone_pos = local_pos + local_rot * (body_to_bone_rot * body_to_bone_pos);
	Quat bone_rot = local_rot * body_to_bone_rot;

	override_bone(index, bone_pos, bone_rot);
}

void Animator::override_bone(const int index, const Vec3& pos, const Quat& rot)
{
	if (bones.length == 0)
		update_world_transforms();
	offsets[index].make_transform(pos, Vec3(1), rot);
}

void Animator::reset_overrides()
{
	Armature* arm = Loader::armature(armature);
	if (override_mode == OverrideMode::Offset)
	{
		for (int i = 0; i < offsets.length; i++)
			offsets[i] = Mat4::identity;
	}
	else
	{
		for (int i = 0; i < offsets.length; i++)
			offsets[i].make_transform(arm->bind_pose[i].pos, Vec3(1, 1, 1), arm->bind_pose[i].rot);
	}
}

void Animator::awake()
{
	Loader::armature(armature);
}

Link& Animator::trigger(const AssetID anim, const float time)
{
	TriggerEntry* entry = triggers.add();
	entry->animation = anim;
	entry->time = time;
	return entry->link;
}

}
