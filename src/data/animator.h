#pragma once

#include "entity.h"
#include "components.h"
#include "mesh.h"

namespace VI
{

struct Animator : public ComponentType<Animator>
{
	struct BindEntry
	{
		int bone;
		ID transform;
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
	void bind(const int, const Transform*);
	void unbind(const Transform*);
	void update_world_transforms();
	void awake();
	Animator();
};

}