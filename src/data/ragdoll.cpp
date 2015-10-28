#include "ragdoll.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "common.h"
#include "asset/mesh.h"
#include "load.h"
#include "components.h"

namespace VI
{

Ragdoll::Ragdoll()
	: bodies(), offset()
{
}

Ragdoll::~Ragdoll()
{
	for (int i = 0; i < bodies.length; i++)
	{
		Transform* t = bodies[i].body.ref();
		if (t)
			World::remove(t->entity());
	}
}

void Ragdoll::awake()
{
	get<Animator>()->override_mode = Animator::OverrideMode::Override;

	Vec3 mesh_offset_scale;
	Quat mesh_offset_rot;
	Vec3 mesh_offset_pos;
	get<SkinnedModel>()->offset.decomposition(mesh_offset_pos, mesh_offset_scale, mesh_offset_rot);

	Armature* arm = Loader::armature(get<Animator>()->armature);
	Array<Entity*> bone_bodies(arm->hierarchy.length, arm->hierarchy.length);
	for (int i = 0; i < arm->bodies.length; i++)
	{
		BodyEntry& body = arm->bodies[i];
		Vec3 pos = body.pos;
		Quat rot = body.rot;
		get<Animator>()->to_world(body.bone, &pos, &rot);
		Vec3 size = body.size * mesh_offset_scale;
		const float density = 0.5f;
		float mass = size.dot(Vec3(1)) * density;

		Entity* entity;
		switch (body.type)
		{
			case BodyEntry::Type::Box:
			{
				entity = World::create<PhysicsEntity>(pos, rot, AssetNull, mass, new btBoxShape(size), size, CollisionTarget, btBroadphaseProxy::AllFilter);
				break;
			}
			case BodyEntry::Type::Capsule:
			{
				float radius = fmax(size.y, size.z);
				entity = World::create<PhysicsEntity>(pos, rot, AssetNull, mass, new btCapsuleShapeX(radius, size.x * 2.0f - radius * 2.0f), size, CollisionTarget, btBroadphaseProxy::AllFilter);
				break;
			}
			case BodyEntry::Type::Sphere:
			{
				float radius = fmax(size.x, fmax(size.y, size.z));
				entity = World::create<PhysicsEntity>(pos, rot, AssetNull, mass, new btSphereShape(radius), Vec3(radius), CollisionTarget, btBroadphaseProxy::AllFilter);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
		bone_bodies[body.bone] = entity;

		BoneBody* entry = bodies.add();
		entry->body = entity->get<Transform>();
		entry->bone = body.bone;
		entry->body_to_bone_pos = -body.pos;
		entry->body_to_bone_rot = body.rot.inverse();
	}

	for (int bone_index = 0; bone_index < bone_bodies.length; bone_index++)
	{
		Entity* entity = bone_bodies[bone_index];
		if (entity)
		{
			int parent_index = arm->hierarchy[bone_index];
			while (parent_index != -1 && !bone_bodies[parent_index])
				parent_index = arm->hierarchy[parent_index];

			if (parent_index != -1)
			{
				Entity* parent_entity = bone_bodies[parent_index];

				btRigidBody* body = entity->get<RigidBody>()->btBody;
				btRigidBody* parent_body = parent_entity->get<RigidBody>()->btBody;

				btCollisionWorld::ClosestRayResultCallback rayCallback(parent_body->getWorldTransform().getOrigin(), body->getWorldTransform().getOrigin());

				btTransform ray_start = parent_body->getWorldTransform();
				btTransform ray_end = body->getWorldTransform();
				Physics::btWorld->rayTestSingle
				(
					ray_start, ray_end,
					body,
					body->getCollisionShape(),
					body->getWorldTransform(),
					rayCallback
				);

				if (rayCallback.hasHit())
				{
					btTransform frame_a = body->getWorldTransform().inverse() * btTransform(Quat::look(rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld);
					btTransform frame_b = parent_body->getWorldTransform().inverse() * btTransform(Quat::look(-rayCallback.m_hitNormalWorld), rayCallback.m_hitPointWorld);
					frame_a.setRotation(frame_a.getRotation() * Quat::euler(0, PI * -0.5f, 0));
					frame_b.setRotation(frame_b.getRotation() * Quat::euler(PI, PI * -0.5f, 0));

					Quat a = frame_a.getRotation();
					Quat b = frame_b.getRotation();

					if (Quat::angle(a, b) > 0.1f)
						frame_b.setRotation(frame_b.getRotation() * Quat::euler(0, 0, PI));

					btConeTwistConstraint* constraint = new btConeTwistConstraint
					(
						*body,
						*parent_body,
						frame_a,
						frame_b
					);
					constraint->setLimit(PI * 0.25f, PI * 0.25f, 0);
					parent_body->setDamping(0.5f, 0.5f);
					body->setDamping(0.5f, 0.5f);
					Physics::btWorld->addConstraint(constraint);
					body->addConstraintRef(constraint);
					parent_body->addConstraintRef(constraint);
				}
			}
		}
	}

	Vec3 pos = Vec3::zero;
	Quat rot = Quat::identity;
	int bone = bodies[0].bone;
	while (bone != -1)
	{
		rot = arm->bind_pose[bone].rot * rot;
		pos = (arm->bind_pose[bone].rot * pos) + arm->bind_pose[bone].pos;
		bone = arm->hierarchy[bone];
	}
	offset = (get<SkinnedModel>()->offset * Vec4(pos)).xyz();
}

RigidBody* Ragdoll::get_body(const AssetID bone)
{
	for (int i = 0; i < bodies.length; i++)
	{
		BoneBody& body = bodies[i];
		if (body.bone == bone)
		{
			Transform* t = body.body.ref();
			if (t)
				return t->get<RigidBody>();
			else
				return nullptr;
		}
	}
	return nullptr;
}

void Ragdoll::update(const Update& u)
{
	{
		Quat rot;
		Vec3 pos;
		bodies[0].body.ref()->absolute(&pos, &rot);
		get<Transform>()->absolute(pos, Quat::identity);
	}

	Animator* anim = get<Animator>();
	for (int i = 0; i < bodies.length; i++)
	{
		BoneBody& bone_body = bodies[i];
		Quat rot;
		Vec3 pos;
		bone_body.body.ref()->absolute(&pos, &rot);

		get<Transform>()->to_local(&pos, &rot);

		anim->from_bone_body(bone_body.bone, pos, rot, bone_body.body_to_bone_pos, bone_body.body_to_bone_rot);
	}
}

}
