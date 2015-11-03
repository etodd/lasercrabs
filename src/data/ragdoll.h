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

	Ragdoll();
	~Ragdoll();
	void awake();

	RigidBody* get_body(const AssetID);
	void update(const Update&);
	void sync_physics_to_armature();
};

}
