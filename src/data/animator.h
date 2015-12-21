#pragma once

#include "entity.h"
#include "render/skinned_model.h"
#include "lmath.h"

namespace VI
{

struct Transform;

#define MAX_ANIMATIONS 3

struct Animator : public ComponentType<Animator>
{
	struct AnimatorTransform
	{
		Vec3 pos;
		Quat rot;
		Vec3 scale;

		void blend(float, const AnimatorTransform&);
	};

	struct AnimatorChannel
	{
		int bone;
		AnimatorTransform transform;
	};

	struct Layer
	{
		Layer();
		AssetID animation;
		AssetID last_animation;
		StaticArray<AnimatorChannel, MAX_BONES> last_animation_channels;
		StaticArray<AnimatorChannel, MAX_BONES> channels;
		float weight;
		float blend;
		float blend_time;
		float time;
		float speed;
		bool loop;
		void update(const Update&, const Animator&);
		void changed_animation();
		void play(AssetID);
	};

	struct TriggerEntry
	{
		Link link;
		AssetID animation;
		float time;
	};

	struct BindEntry
	{
		int bone;
		Ref<Transform> transform;
	};

	enum class OverrideMode
	{
		Offset,
		Override,
	};

	Layer layers[MAX_ANIMATIONS];
	AssetID armature;
	StaticArray<Mat4, MAX_BONES> offsets;
	StaticArray<BindEntry, MAX_BONES> bindings;
	StaticArray<TriggerEntry, MAX_BONES> triggers;
	StaticArray<Mat4, MAX_BONES> bones;
	OverrideMode override_mode;

	void update(const Update&);
	void bind(const int, Transform*);
	void unbind(const Transform*);
	void update_world_transforms();
	void bone_transform(const int, Vec3*, Quat*);
	void to_world(const int, Vec3*, Quat*);
	void from_bone_body(const int, const Vec3&, const Quat&, const Vec3&, const Quat&);
	void override_bone(const int, const Vec3&, const Quat&);
	void reset_overrides();
	void awake();
	Link& trigger(const AssetID, float);
	Animator();
};

}
