#pragma once
#include "entity.h"
#include "array.h"
#include "physics.h"

namespace VI
{

struct Transform;

struct Ragdoll : public ComponentType<Ragdoll>
{
	struct BoneBody
	{
		AssetID bone;
		Ref<Transform> body;
		Vec3 body_to_bone_pos;
		Quat body_to_bone_rot;
	};

	Array<BoneBody> bodies;
	Vec3 offset;

	RigidBody* get_body(const AssetID);

	Ragdoll();
	~Ragdoll();
	void awake();
	void update(const Update&);
};

}
