#pragma once

#include "entity.h"
#include "components.h"
#include "import_common.h"

namespace VI
{

#define MAX_BONES 100

struct Animator : public ComponentType<Animator>
{
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

	struct AnimatorChannel
	{
		int bone;
		Mat4 transform;
	};

	enum class OverrideMode
	{
		Offset,
		Override,
	};

	AssetID armature;
	AssetID animation;
	AssetID last_animation;
	StaticArray<Mat4, MAX_BONES> bones;
	StaticArray<Mat4, MAX_BONES> last_animation_bones;
	StaticArray<Mat4, MAX_BONES> offsets;
	StaticArray<AnimatorChannel, MAX_BONES> channels;
	StaticArray<BindEntry, MAX_BONES> bindings;
	StaticArray<TriggerEntry, MAX_BONES> triggers;
	float blend;
	float blend_time;
	float time;
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
