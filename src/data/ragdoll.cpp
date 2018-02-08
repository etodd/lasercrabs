#include "ragdoll.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "common.h"
#include "load.h"
#include "components.h"
#include "net.h"
#include "game/game.h"
#include "asset/armature.h"
#include "game/parkour.h"
#include "game/drone.h"
#include "game/walker.h"

namespace VI
{

void Ragdoll::add(Entity* src, Entity* killer)
{
	vi_assert(Game::level.local);

	const s32 RAGDOLL_LIMIT = 6;
	if (Ragdoll::list.count() >= 6)
	{
		Ragdoll* oldest_ragdoll = nullptr;
		r32 oldest_timer = RAGDOLL_TIME;
		for (auto i = Ragdoll::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->timer < oldest_timer)
			{
				oldest_timer = i.item()->timer;
				oldest_ragdoll = i.item();
			}
		}
		if (oldest_ragdoll)
			World::remove(oldest_ragdoll->entity());
		else
			return; // all existing ragdolls were created during this frame; there's no use deleting one to replace it with this ragdoll
	}

	Entity* ragdoll = World::create<Empty>();
	ragdoll->get<Transform>()->absolute_pos(src->get<Transform>()->absolute_pos());

	// apply the SkinnedModel::offset rotation to the ragdoll transform to make everything work
	ragdoll->get<Transform>()->absolute_rot(Quat::euler(0, src->get<Walker>()->rotation + PI * 0.5f, 0));

	SkinnedModel* new_skin = ragdoll->add<SkinnedModel>();
	SkinnedModel* old_skin = src->get<SkinnedModel>();
	new_skin->mesh = old_skin->mesh;
	new_skin->mesh_first_person = old_skin->mesh_first_person;
	new_skin->shader = old_skin->shader;
	new_skin->texture = old_skin->texture;
	new_skin->color = old_skin->color;
	new_skin->team = old_skin->team;
	new_skin->mask = old_skin->mask;

	// no rotation
	new_skin->offset.make_transform(
		Vec3(0, -1.1f, 0),
		Vec3(1.0f, 1.0f, 1.0f),
		Quat::identity
	);

	Animator* new_anim = ragdoll->add<Animator>();
	Animator* old_anim = src->get<Animator>();
	new_anim->armature = old_anim->armature;
	new_anim->bones.resize(old_anim->bones.length);
	for (s32 i = 0; i < old_anim->bones.length; i++)
		new_anim->bones[i] = old_anim->bones[i];

	Ragdoll* r = ragdoll->add<Ragdoll>();

	if (killer)
	{
		if (killer->has<Drone>())
			r->apply_impulse(Ragdoll::Impulse::Head, killer->get<Drone>()->velocity * 0.1f);
		else
		{
			Vec3 killer_to_us = src->get<Transform>()->absolute_pos() - killer->get<Transform>()->absolute_pos();
			r->apply_impulse(killer->has<Parkour>() && killer_to_us.y < src->get<Walker>()->capsule_height() ? Ragdoll::Impulse::Feet : Ragdoll::Impulse::Head, Vec3::normalize(killer_to_us) * 10.0f);
		}
	}

	Net::finalize(ragdoll);
}

Ragdoll::~Ragdoll()
{
	if (Game::level.local)
	{
		for (s32 i = 0; i < bodies.length; i++)
		{
			Transform* t = bodies[i].body.ref();
			if (t)
				World::remove(t->entity());
		}
	}
}

void do_impulse(Ragdoll* ragdoll, Ragdoll::Impulse type, const Vec3& i)
{
	switch (type)
	{
		case Ragdoll::Impulse::None:
			break;
		case Ragdoll::Impulse::Head:
		{
			RigidBody* body = ragdoll->get_body(Asset::Bone::character_head);
			if (body->btBody) // if it hasn't been initialized yet, we must be joining a match in progress; forget the impulse
				body->btBody->applyImpulse(i, Vec3::zero);
			break;
		}
		case Ragdoll::Impulse::Feet:
		{
			RigidBody* body = ragdoll->get_body(Asset::Bone::character_shin_L);
			if (body->btBody) // if it hasn't been initialized yet, we must be joining a match in progress; forget the impulse
			{
				body->btBody->applyImpulse(i, Vec3::zero);
				ragdoll->get_body(Asset::Bone::character_shin_R)->btBody->applyImpulse(i, Vec3::zero);
			}
			break;
		}
		default:
			vi_assert(false);
			break;
	}
}

void Ragdoll::awake()
{
	get<Animator>()->override_mode = Animator::OverrideMode::Override;

	if (!Game::level.local)
	{
		// we're a client; the bodies have already been set up
		do_impulse(this, impulse_type, impulse);
		return; 
	}

	Vec3 mesh_offset_scale;
	Quat mesh_offset_rot;
	Vec3 mesh_offset_pos;
	get<SkinnedModel>()->offset.decomposition(&mesh_offset_pos, &mesh_offset_scale, &mesh_offset_rot);

	const Armature* arm = Loader::armature(get<Animator>()->armature);
	Array<Entity*> bone_bodies(arm->hierarchy.length, arm->hierarchy.length);
	for (s32 i = 0; i < arm->bodies.length; i++)
	{
		const BodyEntry& body = arm->bodies[i];
		Vec3 pos = body.pos;
		Quat rot = body.rot;
		get<Animator>()->to_world(body.bone, &pos, &rot);
		Vec3 size = body.size * mesh_offset_scale;
		const r32 density = 0.5f;
		r32 mass = size.dot(Vec3(1)) * density;

		Entity* entity;
		switch (body.type)
		{
			case BodyEntry::Type::Box:
			{
				entity = World::create<PhysicsEntity>(AssetNull, pos, rot, RigidBody::Type::Box, size, mass, CollisionDroneIgnore, ~CollisionAllTeamsForceField, RigidBody::FlagGhost);
				break;
			}
			case BodyEntry::Type::Capsule:
			{
				r32 radius = vi_max(size.y, size.z);
				entity = World::create<PhysicsEntity>(AssetNull, pos, rot, RigidBody::Type::CapsuleX, Vec3(radius, size.x * 2.0f - radius * 2.0f, 0), mass, CollisionDroneIgnore, ~CollisionAllTeamsForceField, RigidBody::FlagGhost);
				break;
			}
			case BodyEntry::Type::Sphere:
			{
				r32 radius = vi_max(size.x, vi_max(size.y, size.z));
				entity = World::create<PhysicsEntity>(AssetNull, pos, rot, RigidBody::Type::Sphere, Vec3(radius), mass, CollisionDroneIgnore, ~CollisionAllTeamsForceField, RigidBody::FlagGhost);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		entity->get<RigidBody>()->set_damping(0.5f, 0.5f);

		bone_bodies[body.bone] = entity;

		BoneBody* entry = bodies.add();
		entry->body = entity->get<Transform>();
		entry->bone = body.bone;
		entry->body_to_bone_pos = -body.pos;
		entry->body_to_bone_rot = body.rot.inverse();
	}

	for (s32 bone_index = 0; bone_index < bone_bodies.length; bone_index++)
	{
		Entity* entity = bone_bodies[bone_index];
		if (entity)
		{
			s32 parent_index = arm->hierarchy[bone_index];
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

					RigidBody::Constraint constraint = RigidBody::Constraint();
					constraint.type = RigidBody::Constraint::Type::ConeTwist;
					constraint.frame_a = frame_a;
					constraint.frame_b = frame_b;
					constraint.limits = Vec3(PI * 0.25f, PI * 0.25f, 0);
					constraint.a = entity->get<RigidBody>();
					constraint.b = parent_entity->get<RigidBody>();
					RigidBody::add_constraint(constraint);
				}
			}
		}
	}

	for (s32 i = 0; i < bodies.length; i++)
		Net::finalize(bodies[i].body.ref()->entity());
}

void Ragdoll::apply_impulse(Impulse type, const Vec3& i)
{
	impulse_type = type;
	impulse = i;
	if (Game::level.local)
		do_impulse(this, type, i);
}

RigidBody* Ragdoll::get_body(AssetID bone)
{
	for (s32 i = 0; i < bodies.length; i++)
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

void Ragdoll::update_server(const Update& u)
{
	timer -= u.time.delta;
	if (timer < 0.0f)
		World::remove_deferred(entity());
}

void Ragdoll::update_client(const Update& u)
{
	{
		Quat rot;
		Vec3 pos;
		bodies[0].body.ref()->absolute(&pos, &rot);
		get<Transform>()->absolute(pos, Quat::identity);
	}

	Animator* anim = get<Animator>();
	for (s32 i = 0; i < bodies.length; i++)
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