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
		Transform* transform;
	};

	struct AnimatorChannel
	{
		int bone;
		Mat4 transform;
	};

	AssetID armature;
	AssetID animation;
	Array<Mat4> bones;
	Array<AnimatorChannel> channels;
	Array<BindEntry> bindings;
	Array<TriggerEntry> triggers;
	float time;

	void update(const Update&);
	void bind(const int, Transform*);
	void unbind(const Transform*);
	void update_world_transforms();
	void bone_transform(const int, Vec3*, Quat*);
	void awake();
	Link& trigger(const AssetID, float);
	Animator();
};

}
