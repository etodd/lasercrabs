#pragma once

#include "entity.h"
#include "components.h"
#include "import_common.h"

namespace VI
{

struct Animator : public ComponentType<Animator>
{
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
	float time;

	void update(const Update&);
	void bind(const int, Transform*);
	void unbind(const Transform*);
	void update_world_transforms();
	void get_bone(const int, Quat&, Vec3& pos);
	void awake();
	Animator();
};

}
