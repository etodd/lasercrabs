#pragma once
#include "entity.h"
#include "array.h"
#include "lmath.h"

namespace VI
{

struct Transform;
struct RigidBody;

#define RAGDOLL_TIME 5.0f

struct Ragdoll : public ComponentType<Ragdoll>
{
	struct BoneBody
	{
		Quat body_to_bone_rot;
		Vec3 body_to_bone_pos;
		Ref<Transform> body;
		AssetID bone;
	};

	enum Impulse : s8
	{
		None,
		Head,
		Feet,
		count,
	};

	static void add(Entity*, Entity*);

	Array<BoneBody> bodies;
	Vec3 impulse;
	r32 timer = RAGDOLL_TIME;
	Impulse impulse_type;

	~Ragdoll();
	void awake();

	RigidBody* get_body(AssetID);
	void update_server(const Update&);
	void update_client(const Update&);
	void apply_impulse(Impulse, const Vec3&);
};

}
