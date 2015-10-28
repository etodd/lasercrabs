#pragma once

#include "entity.h"
#include "components.h"
#include "import_common.h"

namespace VI
{

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
	Array<Mat4> bones;
	Array<Mat4> offsets;
	Array<AnimatorChannel> channels;
	Array<BindEntry> bindings;
	Array<TriggerEntry> triggers;
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
