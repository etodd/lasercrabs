#pragma once

#include "entity.h"
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
		Array<AnimatorChannel> last_animation_channels;
		Array<AnimatorChannel> channels;
		Bitmask<MAX_BONES> channel_overlap;
		r32 blend;
		r32 blend_time;
		r32 time;
		r32 time_last;
		r32 speed;
		AssetID animation;
		AssetID last_animation;
		AssetID last_frame_animation;
		Behavior behavior;

		Layer();

		void update(r32, r32, const Animator&);
		void changing_animation(const Armature*);
		void changed_animation();
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

	Array<Mat4> offsets;
	Array<Mat4> bones;
	Array<BindEntry> bindings;
	Array<TriggerEntry> triggers;
	Layer layers[MAX_ANIMATIONS];
	OverrideMode override_mode;
	AssetID armature;

	Animator();
	void awake();

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
	Link& trigger(const AssetID, r32);
};

}
