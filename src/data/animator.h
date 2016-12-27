#pragma once

#include "entity.h"
#include "render/skinned_model.h"
#include "lmath.h"

namespace VI
{

struct Transform;
struct Armature;

#define MAX_ANIMATIONS 4

struct Animator : public ComponentType<Animator>
{
	enum class Behavior
	{
		Default,
		Loop,
		Freeze,
		count,
	};

	struct AnimatorTransform
	{
		Vec3 pos;
		Quat rot;
		Vec3 scale;

		void blend(r32, const AnimatorTransform&);
	};

	struct AnimatorChannel
	{
		s32 bone;
		AnimatorTransform transform;
	};

	struct Layer
	{
		Layer();
		r32 weight;
		r32 blend;
		r32 blend_time;
		r32 time;
		r32 time_last;
		r32 speed;
		StaticArray<AnimatorChannel, MAX_BONES> last_animation_channels;
		StaticArray<AnimatorChannel, MAX_BONES> channels;
		AssetID animation;
		AssetID last_animation;
		AssetID last_frame_animation;
		Behavior behavior;
		void update(r32, r32, const Animator&);
		void changed_animation(const Armature*);
		void play(AssetID);
		void set(AssetID, r32);
	};

	struct TriggerEntry
	{
		r32 time;
		Link link;
		AssetID animation;
	};

	struct BindEntry
	{
		s32 bone;
		Ref<Transform> transform;
	};

	enum class OverrideMode
	{
		Offset,
		Override,
		count,
	};

	OverrideMode override_mode;
	Layer layers[MAX_ANIMATIONS];
	AssetID armature;
	StaticArray<Mat4, MAX_BONES> offsets;
	StaticArray<Mat4, MAX_BONES> bones;
	StaticArray<BindEntry, MAX_BONES> bindings;
	StaticArray<TriggerEntry, MAX_BONES> triggers;

	void update_server(const Update&);
	void update_client_only(const Update&);
	void bind(const s32, Transform*);
	void unbind(const Transform*);
	void update_world_transforms();
	void bone_transform(const s32, Vec3*, Quat* = nullptr);
	void to_local(const s32, Vec3*, Quat* = nullptr);
	void to_world(const s32, Vec3*, Quat* = nullptr);
	void from_bone_body(const s32, const Vec3&, const Quat&, const Vec3&, const Quat&);
	void override_bone(const s32, const Vec3&, const Quat&);
	void reset_overrides();
	void awake();
	Link& trigger(const AssetID, r32);
	Animator();
};

}
