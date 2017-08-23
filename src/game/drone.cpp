#include "drone.h"
#include "data/components.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "entities.h"
#include "render/skinned_model.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "player.h"
#include "mersenne/mersenne-twister.h"
#include "common.h"
#include "asset/mesh.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "data/animator.h"
#include "render/views.h"
#include "game.h"
#include "console.h"
#include "minion.h"
#include "strings.h"
#include "render/particles.h"
#include "net.h"
#include "team.h"
#include "load.h"
#include "ease.h"

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 4.0f
#define MAX_FLIGHT_TIME 4.0f
#define DRONE_LEG_LENGTH (0.277f - 0.101f)
#define DRONE_LEG_BLEND_SPEED (1.0f / 0.03f)
#define DRONE_MIN_LEG_BLEND_SPEED (DRONE_LEG_BLEND_SPEED * 0.1f)
#define DRONE_REFLECTION_TIME_TOLERANCE 0.1f
#define DRONE_REFLECTION_POSITION_TOLERANCE (DRONE_SHIELD_RADIUS * 10.0f)
#define DRONE_SHOTGUN_PELLETS 13

#define DEBUG_REFLECTIONS 0
#define DEBUG_NET_SYNC 0

const r32 Drone::cooldown_thresholds[DRONE_CHARGES] = { DRONE_COOLDOWN * 0.4f, DRONE_COOLDOWN * 0.1f, 0.0f, };
Vec3 drone_shotgun_dirs[DRONE_SHOTGUN_PELLETS];

void Drone::init()
{
	drone_shotgun_dirs[0] = Vec3(0, 0, 1);
	{
		Vec3 d = Quat::euler(0.0f, 0.0f, PI * -0.02f) * Vec3(0, 0, 1);
		for (s32 i = 0; i < 3; i++)
			drone_shotgun_dirs[1 + i] = Quat::euler(r32(i) * (PI * 2.0f / 3.0f), 0.0f, 0.0f) * d;
	}

	{
		Vec3 d = Quat::euler(0.0f, 0.0f, PI * 0.05f) * Vec3(0, 0, 1);
		for (s32 i = 0; i < 9; i++)
			drone_shotgun_dirs[4 + i] = Quat::euler(r32(i) * (PI * 2.0f / 9.0f), 0.0f, 0.0f) * d;
	}
}

namespace DroneNet
{

enum class Message : s8
{
	FlyStart,
	FlyDone,
	DashStart,
	DashDone,
	HitTarget,
	ChargeCount,
	AbilitySelect,
	AbilitySpawn,
	ReflectionEffects,
	count,
};

enum class FlyFlag : s8
{
	None,
	CancelExisting,
	count,
};

b8 start_flying(Drone* a, Vec3 dir, FlyFlag flag)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::FlyStart;
		serialize_enum(p, Message, t);
	}
	serialize_enum(p, FlyFlag, flag);
	dir.normalize();
	serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
	Net::msg_finalize(p);
	return true;
}

b8 charge_count(Drone* a, s8 count)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::ChargeCount;
		serialize_enum(p, Message, t);
	}
	serialize_int(p, s8, count, 0, DRONE_CHARGES);
	Net::msg_finalize(p);
	return true;
}

b8 ability_spawn(Drone* a, Vec3 dir, Ability ability)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::AbilitySpawn;
		serialize_enum(p, Message, t);
	}
	dir.normalize();
	serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
	serialize_enum(p, Ability, ability);
	Net::msg_finalize(p);
	return true;
}

b8 ability_select(Drone* d, Ability ability)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = d;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::AbilitySelect;
		serialize_enum(p, Message, t);
	}
	serialize_int(p, Ability, ability, 0, s32(Ability::count) + 1); // must be +1 for Ability::None
	Net::msg_finalize(p);
	return true;
}

b8 start_dashing(Drone* a, Vec3 dir, r32 time)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::DashStart;
		serialize_enum(p, Message, t);
	}
	serialize_r32_range(p, time, 0, DRONE_DASH_TIME, 16);
	dir.normalize();
	serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
	serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
	Net::msg_finalize(p);
	return true;
}

b8 finish_flying(Drone* a)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::FlyDone;
		serialize_enum(p, Message, t);
	}
	Net::msg_finalize(p);
	return true;
}

b8 finish_dashing(Drone* a)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::DashDone;
		serialize_enum(p, Message, t);
	}
	Net::msg_finalize(p);
	return true;
}

b8 hit_target(Drone* a, Entity* target)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::HitTarget;
		serialize_enum(p, Message, t);
	}
	{
		Ref<Entity> ref = target;
		serialize_ref(p, ref);
	}
	Net::msg_finalize(p);
	return true;
}

b8 reflection_effects(Drone* a, Entity* reflected_off)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new_local(Net::MessageType::Drone);
	{
		Ref<Drone> ref = a;
		serialize_ref(p, ref);
	}
	{
		Message t = Message::ReflectionEffects;
		serialize_enum(p, Message, t);
	}
	{
		Ref<Entity> ref = reflected_off;
		serialize_ref(p, ref);
	}
	Net::msg_finalize(p);
	return true;
}

}

r32 particle_trail(const Vec3& start, const Vec3& end, r32 offset = 0.0f)
{
	Vec3 dir = end - start;
	r32 distance = dir.length();
	dir /= distance;
	const r32 interval = 1.0f;
	s32 particle_count = distance / interval;
	Vec3 interval_pos = dir * interval;
	Vec3 pos = start;
	for (s32 i = 0; i < particle_count; i++)
	{
		pos += interval_pos;
		Particles::tracers.add
		(
			pos,
			Vec3::zero,
			0,
			vi_min(0.25f, offset + i * 0.05f)
		);
	}
	return offset + particle_count * 0.05f;
}

enum class DroneHitType
{
	Target,
	Reflection,
	count,
};

void client_hit_effects(Drone* drone, Entity* target, DroneHitType type)
{
	if (type == DroneHitType::Reflection)
		drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_REFLECT);

	Vec3 pos = drone->get<Transform>()->absolute_pos();

	Quat rot = Quat::look(Vec3::normalize(drone->velocity));
	for (s32 i = 0; i < 50; i++)
	{
		Particles::sparks.add
		(
			pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}

	EffectLight::add(pos, 8.0f, 1.5f, EffectLight::Type::Shockwave);

	// controller vibration, etc.
	drone->hit.fire(target);
}

void sniper_hit_effects(const Drone::Hit& hit)
{
	// we just shot at something; spawn some particles
	Quat rot = Quat::look(hit.normal);
	for (s32 j = 0; j < 50; j++)
	{
		Particles::sparks.add
		(
			hit.pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}
	Audio::post_global_event(AK::EVENTS::PLAY_SNIPER_IMPACT, hit.pos);
}

s32 impact_damage(const Drone* drone, const Entity* target_shield)
{
	Vec3 ray_dir;
	{
		r32 speed = drone->velocity.length();
		if (speed == 0.0f)
			return 0;
		else
			ray_dir = drone->velocity / speed;
	}

	Net::StateFrame state_frame;

	Vec3 target_pos;

	if (Game::net_transform_filter(target_shield, Game::level.mode)
		&& !PlayerHuman::players_on_same_client(drone->entity(), target_shield)
		&& drone->net_state_frame(&state_frame))
	{
		Vec3 pos;
		Quat rot;
		Net::transform_absolute(state_frame, target_shield->get<Transform>()->id(), &pos, &rot);
		target_pos = pos + (rot * target_shield->get<Target>()->local_offset); // todo possibly: rewind local_offset as well?
	}
	else
		target_pos = target_shield->get<Target>()->absolute_pos();

	Vec3 ray_start = drone->get<Transform>()->absolute_pos();

	Vec3 intersection;
	if (LMath::ray_sphere_intersect(ray_start, ray_start + ray_dir * drone->range(), target_pos, DRONE_SHIELD_RADIUS, &intersection))
	{
		r32 dot = Vec3::normalize(intersection - target_pos).dot(ray_dir);

		b8 allow_direct_hit = drone->current_ability == Ability::Sniper ? true : !target_shield->has<Turret>();

		if (dot < -0.95f && allow_direct_hit)
			return 3;
		else if (dot < -0.75f)
			return 2;
	}
	return 1;
}

void drone_bolt_spawn(Drone* drone, const Vec3& my_pos, const Vec3& dir_normalized, Bolt::Type type)
{
	if (Game::level.local)
	{
		PlayerManager* manager = drone->get<PlayerCommon>()->manager.ref();
		if (manager->has<PlayerHuman>() && !manager->get<PlayerHuman>()->local)
		{
			// step 1. rewind the world to the point where the remote player fired
			// step 2. step forward in increments of 1/60th of a second until we reach the present,
			// checking for obstacles along the way.
			// step 3. spawn the bolt at the final position
			// step 4. if a target was hit, delete the bolt and apply any damage effects

			Vec3 pos_bolt = my_pos + dir_normalized * DRONE_SHIELD_RADIUS;
			r32 timestamp;
#if SERVER
			timestamp = Net::timestamp() - drone->get<PlayerControlHuman>()->rtt;
#else
			timestamp = Net::timestamp();
#endif

			Entity* closest_hit_entity = nullptr;
			Vec3 closest_hit;
			Vec3 closest_hit_normal;

			while (timestamp < Net::timestamp())
			{
				const r32 SIMULATION_STEP = Net::tick_rate();
				Net::StateFrame state_frame;
				Net::state_frame_by_timestamp(&state_frame, timestamp);
				Vec3 pos_bolt_next = pos_bolt + dir_normalized * (Bolt::speed(type) * SIMULATION_STEP);
				Vec3 pos_bolt_next_ray = pos_bolt_next + dir_normalized * BOLT_LENGTH;

				r32 closest_hit_distance_sq = FLT_MAX;

				// check environment collisions
				{
					btCollisionWorld::ClosestRayResultCallback ray_callback(pos_bolt, pos_bolt_next_ray);
					Physics::raycast(&ray_callback, CollisionStatic | (CollisionAllTeamsForceField & Bolt::raycast_mask(drone->get<AIAgent>()->team)));
					if (ray_callback.hasHit())
					{
						closest_hit = ray_callback.m_hitPointWorld;
						closest_hit_normal = ray_callback.m_hitNormalWorld;
						closest_hit_entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
						closest_hit_distance_sq = (closest_hit - pos_bolt).length_squared();
					}
				}

				// check target collisions
				for (auto i = Target::list.iterator(); !i.is_last(); i.next())
				{
					if (i.item() == drone->get<Target>())
						continue;

					Vec3 p;
					{
						Vec3 pos;
						Quat rot;
						Net::transform_absolute(state_frame, i.item()->get<Transform>()->id(), &pos, &rot);
						p = pos + (rot * i.item()->local_offset); // todo possibly: rewind local_offset as well?
					}

					Vec3 intersection;
					if (LMath::ray_sphere_intersect(pos_bolt, pos_bolt_next_ray, p, i.item()->radius(), &intersection))
					{
						r32 distance_sq = (intersection - pos_bolt).length_squared();
						if (distance_sq < closest_hit_distance_sq)
						{
							closest_hit = intersection;
							closest_hit_normal = Vec3::normalize(intersection - p);
							closest_hit_entity = i.item()->entity();
							closest_hit_distance_sq = distance_sq;
						}
					}
				}
				pos_bolt = pos_bolt_next;

				timestamp += SIMULATION_STEP;

				if (closest_hit_entity) // we hit something; stop simulating
					break;
			}

			Entity* bolt = World::create<BoltEntity>(manager->team.ref()->team(), manager, drone->entity(), type, pos_bolt, dir_normalized);
			Net::finalize(bolt);
			if (closest_hit_entity) // we hit something, register it instantly
				bolt->get<Bolt>()->hit_entity(closest_hit_entity, closest_hit, closest_hit_normal);
		}
		else
		{
			// not a remote player; no lag compensation needed
			Net::finalize(World::create<BoltEntity>(manager->team.ref()->team(), manager, drone->entity(), type, my_pos + dir_normalized * DRONE_SHIELD_RADIUS, dir_normalized));
		}
	}
	else
	{
		// we're a client; if this is a local player who has already spawned a fake bolt for client-side prediction,
		// we need to delete that fake bolt, since the server has spawned a real one.
		if (drone->fake_bolts.length > 0)
		{
			EffectLight* bolt = drone->fake_bolts[0].ref();
			if (bolt) // might have already been removed
				EffectLight::remove(bolt);
			drone->fake_bolts.remove_ordered(0);
		}
	}
}

void drone_sniper_effects(Drone* drone, const Vec3& dir_normalized, const Drone::Hits* hits = nullptr)
{
	drone->cooldown_setup();

	drone->get<Audio>()->post_event(AK::EVENTS::PLAY_SNIPER_FIRE);

	drone->hit_targets.length = 0;
	Vec3 pos = drone->get<Transform>()->absolute_pos();
	Vec3 ray_start = pos + dir_normalized * -DRONE_RADIUS;
	Vec3 ray_end = pos + dir_normalized * drone->range();
	drone->velocity = dir_normalized * DRONE_FLY_SPEED;

	Drone::Hits hits_storage;
	if (!hits)
	{
		drone->raycast(Drone::RaycastMode::Default, ray_start, ray_end, nullptr, &hits_storage);
		hits = &hits_storage;
	}
	for (s32 i = 0; i < hits->index_end + 1; i++)
		sniper_hit_effects(hits->hits[i]);

	// whiff sfx
	{
		Vec3 closest_position = ray_start;
		r32 closest_distance = FLT_MAX;
		for (auto i = Camera::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 line_start = ray_start;
			for (s32 j = 0; j < hits->index_end + 1; j++)
			{
				const Drone::Hit& hit = hits->hits[j];
				Vec3 line = hit.pos - line_start;
				r32 line_length = line.length();
				Vec3 line_normalized = line / line_length;
				r32 t = vi_min((i.item()->pos - line_start).dot(line_normalized), line_length);
				if (t > 0.0f)
				{
					Vec3 pos = line_start + line_normalized * t;
					r32 distance = (pos - i.item()->pos).length_squared();
					if (distance < closest_distance)
					{
						closest_distance = distance;
						closest_position = pos;
					}
				}

				line_start = hit.pos;
			}
		}
		Audio::post_global_event(AK::EVENTS::PLAY_SNIPER_WHIFF, closest_position);
	}

	// effects
	{
		Vec3 line_start = ray_start;
		r32 offset = 0.0f;
		for (s32 i = 0; i < hits->index_end + 1; i++)
		{
			const Vec3& p = hits->hits[i].pos;
			offset = particle_trail(line_start, p, offset);
			line_start = p;
		}
	}

	// everyone instantly knows where we are
	AI::Team team = drone->get<AIAgent>()->team;
	for (auto i = Team::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team() != team)
			i.item()->track(drone->get<PlayerCommon>()->manager.ref(), drone->entity());
	}
}

void drone_bolter_cooldown_setup(Drone* drone)
{
	drone->bolter_charge_counter++;
	if (drone->bolter_charge_counter >= BOLTS_PER_DRONE_CHARGE)
	{
		drone->bolter_charge_counter = 0;
		drone->cooldown_setup();
	}
}

b8 Drone::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	Drone* drone;
	{
		Ref<Drone> ref;
		serialize_ref(p, ref);
		drone = ref.ref();
	}

	DroneNet::Message type;
	serialize_enum(p, DroneNet::Message, type);

	// should we actually pay attention to this message?
	// if it's a message from a remote, but we are a local entity, then ignore the message.
	b8 apply_msg = drone && (src == Net::MessageSource::Loopback || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local());

	switch (type)
	{
		case DroneNet::Message::FlyStart:
		{
			DroneNet::FlyFlag flag;
			serialize_enum(p, DroneNet::FlyFlag, flag);

			Vec3 dir;
			serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
			dir.normalize();

			if (apply_msg)
			{
				if (drone->charges > 0 || flag == DroneNet::FlyFlag::CancelExisting)
				{
					drone->dash_combo = false;
					drone->dash_timer = 0.0f;
					drone->velocity = dir * DRONE_FLY_SPEED;
					drone->rotation_clamp_vector = dir;

					drone->detaching.fire();
					drone->hit_targets.length = 0;
					drone->get<Transform>()->absolute_pos(drone->get<Transform>()->absolute_pos() + dir * DRONE_RADIUS * 0.5f);
					drone->get<Transform>()->absolute_rot(Quat::look(dir));

					drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_LAUNCH);
					drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_FLY);

					if (flag != DroneNet::FlyFlag::CancelExisting)
						drone->cooldown_setup();
					drone->ensure_detached();
				}
			}

			break;
		}
		case DroneNet::Message::DashStart:
		{
			r32 dash_time;
			serialize_r32_range(p, dash_time, 0, DRONE_DASH_TIME, 16);

			Vec3 dir;
			serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);

			if (apply_msg && drone->charges > 0)
			{
				drone->velocity = dir * DRONE_DASH_SPEED;

				drone->dashing.fire();

				drone->dash_timer = dash_time;

				drone->hit_targets.length = 0;

				drone->attach_time = Game::time.total;
				drone->cooldown_setup();

				for (s32 i = 0; i < DRONE_LEGS; i++)
					drone->footing[i].parent = nullptr;
				drone->get<Animator>()->reset_overrides();
				drone->get<Animator>()->layers[0].animation = Asset::Animation::drone_dash;

				drone->particle_accumulator = 0;

				drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_LAUNCH);
				drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_FLY);
			}

			break;
		}
		case DroneNet::Message::DashDone:
		{
			if (apply_msg)
			{
				drone->finish_flying_dashing_common();
				drone->done_dashing.fire();
			}
			break;
		}
		case DroneNet::Message::FlyDone:
		{
			if (apply_msg)
			{
				drone->finish_flying_dashing_common();

				// rotation clamp
				{
					/*
					Quat absolute_rot = drone->get<Transform>()->absolute_rot();
					Vec3 wall_normal = absolute_rot * Vec3(0, 0, 1);

					// set the rotation clamp to be perpendicular to the camera, so we can ease the camera gently away from the wall
					Vec3 direction = drone->get<PlayerCommon>()->look_dir();

					Vec3 clamped;
					if (direction.dot(wall_normal) == -1.0f)
						clamped = wall_normal;
					else
					{
						clamped = Vec3::normalize(direction - wall_normal * wall_normal.dot(direction));
						if (fabsf(wall_normal.y) > 0.707f)
						{
							Vec3 right = Vec3::normalize(direction.cross(Vec3(0, 1, 0)));
							clamped = Vec3::normalize(clamped - right * clamped.dot(right));
						}

						// if the clamped vector is too vertical, force it to be more horizontal
						const r32 threshold = fabsf(wall_normal.y) + 0.25f;
						clamped.y = LMath::clampf(clamped.y, -threshold, threshold);
						clamped.normalize();
					}

					drone->rotation_clamp_vector = clamped;
					*/
					drone->rotation_clamp_vector = drone->get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
				}

				drone->done_flying.fire();
			}
			break;
		}
		case DroneNet::Message::HitTarget:
		{
			Ref<Entity> target;
			serialize_ref(p, target);

			if (apply_msg)
				client_hit_effects(drone, target.ref(), DroneHitType::Target);

			// damage messages

			if (target.ref()->has<Shield>())
			{
				// check if we can damage them
				if (target.ref()->get<Health>()->can_take_damage())
				{
					// we hurt them
					if (Game::level.local) // if we're a client, this has already been handled by the server
						target.ref()->get<Health>()->damage(drone->entity(), impact_damage(drone, target.ref()));
				}
				else
				{
					// we didn't hurt them
					if (Game::level.local)
					{
						// check if they had active armor on and so should damage us
						if (drone->state() != State::Crawl
							&& target.ref()->get<Health>()->active_armor()
							&& target.ref()->has<AIAgent>()
							&& target.ref()->get<AIAgent>()->team != drone->get<AIAgent>()->team)
							drone->get<Health>()->kill(target.ref());
					}
				}
			}
			break;
		}
		case DroneNet::Message::ChargeCount:
		{
			s8 charges;
			serialize_int(p, s8, charges, 0, DRONE_CHARGES);
			if (apply_msg || !Game::level.local)
			{
				drone->charges = charges;
				drone->bolter_charge_counter = 0; // hack; this will result in some de-syncs between client and server
			}
			break;
		}
		case DroneNet::Message::AbilitySelect:
		{
			Ability ability;
			serialize_int(p, Ability, ability, 0, s32(Ability::count) + 1); // must be +1 for Ability::None
			if (apply_msg && (ability == Ability::None || AbilityInfo::list[s32(ability)].type != AbilityInfo::Type::Other))
				drone->current_ability = ability;
			break;
		}
		case DroneNet::Message::AbilitySpawn:
		{
			Vec3 dir;
			serialize_r32_range(p, dir.x, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.y, -1.0f, 1.0f, 16);
			serialize_r32_range(p, dir.z, -1.0f, 1.0f, 16);
			Ability ability;
			serialize_enum(p, Ability, ability);

			if (!drone)
				break;

			Vec3 dir_normalized;
			{
				r32 length = dir.length();
				if (length == 0.0f)
					net_error();
				dir_normalized = dir / length;
			}

			PlayerManager* manager = drone->get<PlayerCommon>()->manager.ref();

#if SERVER
			if (!manager->ability_valid(ability))
				return true;
#endif

			const AbilityInfo& info = AbilityInfo::list[(s32)ability];

			Vec3 pos;
			Vec3 normal;
			RigidBody* parent;
			if (!drone->can_spawn(ability, dir_normalized, &pos, &normal, &parent))
				return true;

			Quat rot = Quat::look(normal);

			manager->add_energy(-info.spawn_cost);

			if (ability != Ability::Bolter
				&& ability != Ability::Shotgun
				&& ability != Ability::Sniper) // these weapons handle cooldowns manually
				drone->cooldown_setup();

			Vec3 my_pos;
			Quat my_rot;
			drone->get<Transform>()->absolute(&my_pos, &my_rot);

			switch (ability)
			{
				case Ability::Sensor:
				{
					// place a sensor
					if (Game::level.local)
						Net::finalize(World::create<SensorEntity>(manager->team.ref()->team(), pos + rot * Vec3(0, 0, SENSOR_RADIUS), rot));

					Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, pos);

					// effects
					particle_trail(my_pos, pos);
					EffectLight::add(pos + rot * Vec3(0, 0, ROPE_SEGMENT_LENGTH), 8.0f, 1.5f, EffectLight::Type::Shockwave);

					break;
				}
				case Ability::Minion:
				{
					// spawn a minion
					Vec3 forward = rot * Vec3(0, 0, 1.0f);
					Vec3 npos = pos + forward;
					forward.y = 0.0f;

					if (Game::level.local)
					{
						r32 angle;
						if (forward.length_squared() > 0.0f)
							angle = atan2f(forward.x, forward.z);
						else
						{
							Vec3 forward2 = dir_normalized;
							forward2.y = 0.0f;
							r32 length = forward2.length();
							if (length > 0.0f)
							{
								forward2 /= length;
								angle = atan2f(forward2.x, forward2.z);
							}
							else
								angle = 0.0f;
						}
						Net::finalize(World::create<MinionEntity>(npos, Quat::euler(0, angle, 0), drone->get<AIAgent>()->team, manager));
					}

					// effects
					particle_trail(my_pos, pos);
					EffectLight::add(npos, 8.0f, 1.5f, EffectLight::Type::Shockwave);

					Audio::post_global_event(AK::EVENTS::PLAY_MINION_SPAWN, npos);
					break;
				}
				case Ability::ForceField:
				{
					// spawn a force field
					Vec3 npos = pos + rot * Vec3(0, 0, FORCE_FIELD_BASE_OFFSET);

					Audio::post_global_event(AK::EVENTS::PLAY_SENSOR_SPAWN, npos);

					if (Game::level.local)
						Net::finalize(World::create<ForceFieldEntity>(parent->get<Transform>(), npos, rot, drone->get<AIAgent>()->team));

					// effects
					particle_trail(my_pos, pos);
					EffectLight::add(npos, 8.0f, 1.5f, EffectLight::Type::Shockwave);

					break;
				}
				case Ability::Sniper:
				{
					if (Game::level.local)
					{
						drone->hit_targets.length = 0;
						Vec3 pos = drone->get<Transform>()->absolute_pos();
						Vec3 ray_start = pos + dir_normalized * -DRONE_RADIUS;
						Vec3 ray_end = pos + dir_normalized * drone->range();
						drone->velocity = dir_normalized * DRONE_FLY_SPEED;
						Hits hits;
						drone->movement_raycast(ray_start, ray_end, &hits);
						drone_sniper_effects(drone, dir_normalized, &hits);
					}
					else if (!drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
						drone_sniper_effects(drone, dir_normalized);
					break;
				}
				case Ability::Grenade:
				{
					drone->get<Audio>()->post_event(AK::EVENTS::PLAY_GRENADE_SPAWN);
					if (Game::level.local)
					{
						Vec3 dir_adjusted = dir_normalized;
						if (dir_adjusted.y > -0.25f)
							dir_adjusted.y += 0.35f;
						Net::finalize(World::create<GrenadeEntity>(manager, my_pos + dir_adjusted * (DRONE_SHIELD_RADIUS + GRENADE_RADIUS + 0.01f), dir_adjusted));
					}
					break;
				}
				case Ability::Bolter:
				{
					drone_bolt_spawn(drone, my_pos, dir_normalized, Bolt::Type::DroneBolter);
					if (Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
						drone_bolter_cooldown_setup(drone);
					break;
				}
				case Ability::Shotgun:
				{
					if (drone->charges >= DRONE_SHOTGUN_CHARGES)
					{
						Quat target_quat = Quat::look(dir_normalized);
						for (s32 i = 0; i < DRONE_SHOTGUN_PELLETS; i++)
						{
							Vec3 d = target_quat * drone_shotgun_dirs[i];
							drone_bolt_spawn(drone, my_pos, d, Bolt::Type::DroneShotgun);
						}
						if (Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
							drone->cooldown_setup(DRONE_SHOTGUN_CHARGES);
					}
					break;
				}
				case Ability::ActiveArmor:
				{
					if (drone)
					{
						drone->get<Health>()->active_armor_timer = vi_max(drone->get<Health>()->active_armor_timer, ACTIVE_ARMOR_TIME);
						drone->get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_ACTIVE_ARMOR);
					}
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			if (ability != Ability::Bolter)
				drone->current_ability = Ability::None;
			drone->ability_spawned.fire(ability);
			break;
		}
		case DroneNet::Message::ReflectionEffects:
		{
			Ref<Entity> reflected_off;
			serialize_ref(p, reflected_off);
			if (apply_msg)
				client_hit_effects(drone, reflected_off.ref(), DroneHitType::Reflection);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	return true;
}

void Drone::finish_flying_dashing_common()
{
	get<Animator>()->layers[0].animation = AssetNull;

	get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_LAND);
	get<Audio>()->post_event(AK::EVENTS::STOP_DRONE_FLY);
	attach_time = Game::time.total;
	dash_timer = 0.0f;
	dash_combo = false;

	velocity = Vec3::zero;
	get<Transform>()->absolute(&lerped_pos, &lerped_rotation);
	last_pos = lerped_pos;
}

Drone* Drone::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Drone* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->get<AIAgent>()->team, mask))
		{
			r32 d = (i.item()->get<Transform>()->absolute_pos() - pos).length_squared();
			if (d < closest_distance)
			{
				closest = i.item();
				closest_distance = d;
			}
		}
	}

	if (distance)
	{
		if (closest)
			*distance = sqrtf(closest_distance);
		else
			*distance = FLT_MAX;
	}

	return closest;
}

Drone::Drone()
	: velocity(0.0f, -DRONE_FLY_SPEED, 0.0f),
	done_flying(),
	done_dashing(),
	detaching(),
	dashing(),
	dash_timer(),
	dash_combo(),
	rotation_clamp_vector(0, 1, 0),
	attach_time(Game::time.total),
	footing(),
	last_footstep(),
	reflecting(),
	hit_targets(),
	cooldown(),
	charges(DRONE_CHARGES),
	current_ability(Ability::None),
	fake_bolts(),
	ability_spawned(),
	dash_target(),
	reflections(),
	bolter_charge_counter()
{
}

void Drone::awake()
{
	get<Animator>()->layers[0].behavior = Animator::Behavior::Loop;
	link_arg<Entity*, &Drone::killed>(get<Health>()->killed);
	lerped_pos = get<Transform>()->absolute_pos();
}

Drone::~Drone()
{
	get<Audio>()->post_event(AK::EVENTS::STOP_DRONE_FLY);
}

Drone::State Drone::state() const
{
	if (dash_timer > 0.0f)
		return State::Dash;
	else if (get<Transform>()->parent.ref())
		return State::Crawl;
	else
		return State::Fly;
}

s16 Drone::ally_force_field_mask() const
{
	return Team::force_field_mask(get<AIAgent>()->team);
}

Vec3 Drone::center_lerped() const
{
	return lerped_pos;
}

Vec3 Drone::attach_point(r32 offset) const
{
	Quat rot;
	Vec3 pos;
	get<Transform>()->absolute(&pos, &rot);
	return pos + rot * Vec3(0, 0, offset + -DRONE_RADIUS);
}

// returns true if it's a valid hit
b8 Drone::hit_target(Entity* target)
{
	if (target->has<ForceFieldCollision>())
		target = target->get<ForceFieldCollision>()->field.ref()->entity();

	for (s32 i = 0; i < hit_targets.length; i++)
	{
		if (hit_targets[i].ref() == target)
			return false; // we've already hit this target once during this flight
	}
	hit_targets.add(target);

	if (current_ability == Ability::None && get<Health>()->active_armor() && (target->has<Shield>() || target->has<ForceField>()))
		get<Health>()->active_armor_timer = 0.0f; // damaging a Shield cancels our invincibility

	if (!Game::level.local) // then we are a local player on a client
	{
		// target hit events are synced across the network
		// so just spawn some particles if needed, but don't do anything else
		client_hit_effects(this, target, DroneHitType::Target);
		return true;
	}

	DroneNet::hit_target(this, target);

	if (target->has<Target>())
	{
		Ref<Target> t = target->get<Target>();

		t.ref()->hit(entity());

		if (t.ref() && t.ref()->has<RigidBody>()) // is it still around and does it have a rigidbody?
		{
			RigidBody* body = t.ref()->get<RigidBody>();
			body->btBody->applyImpulse(velocity * 0.1f, Vec3::zero);
			body->btBody->activate(true);
		}
	}

	return true;
}

b8 Drone::predict_intersection(const Target* target, const Net::StateFrame* state_frame, Vec3* intersection, r32 speed) const
{
	if (speed == 0.0f) // instant bullet travel time
	{
		*intersection = target->absolute_pos();
		return true;
	}
	else
	{
		Vec3 me;
		if (state_frame)
			Net::transform_absolute(*state_frame, get<Transform>()->id(), &me);
		else
			me = get<Transform>()->absolute_pos();
		return target->predict_intersection(me, speed, state_frame, intersection);
	}
}

void Drone::killed(Entity* e)
{
	PlayerHuman::notification(entity(), get<AIAgent>()->team, PlayerHuman::Notification::Type::DroneDestroyed);

	PlayerManager::entity_killed_by(entity(), e);

	if (Game::level.local)
	{
		Vec3 pos;
		Quat rot;
		get<Transform>()->absolute(&pos, &rot);
		ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
		World::remove_deferred(entity());
	}
}

b8 Drone::can_dash(const Target* target, Vec3* out_intersection) const
{
	Vec3 intersection;
	if (predict_intersection(target, nullptr, &intersection, DRONE_DASH_SPEED))
	{
		// the Target is situated at the base of the enemy Drone, where it attaches to the surface.
		// we need to calculate the vector starting from our own base attach point, otherwise the dot product will be messed up.
		Vec3 me = get<Target>()->absolute_pos();
		Vec3 to_intersection = intersection - me;
		r32 distance = to_intersection.length();
		to_intersection /= distance;
		if (distance < DRONE_DASH_DISTANCE)
		{
			Vec3 dash_to_intersection = intersection - me;
			r32 dot = to_intersection.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1));
			if (fabsf(dot) < 0.1f)
			{
				if (out_intersection)
					*out_intersection = intersection;
				return true;
			}
		}
	}
	return false;
}

b8 Drone::can_shoot(const Target* target, Vec3* out_intersection, r32 speed, const Net::StateFrame* state_frame) const
{
	Vec3 intersection;
	if (predict_intersection(target, state_frame, &intersection, speed))
	{
		Vec3 me = get<Transform>()->absolute_pos();
		Vec3 to_intersection = intersection - me;
		r32 distance = to_intersection.length();
		to_intersection /= distance;

		Vec3 final_pos;
		b8 hit_target;
		if (can_shoot(to_intersection, &final_pos, &hit_target, state_frame))
		{
			if (hit_target || (final_pos - me).length() > distance - DRONE_RADIUS * 2.0f)
			{
				if (out_intersection)
					*out_intersection = intersection;
				return true;
			}
		}
	}
	return false;
}

b8 Drone::can_hit(const Target* target, Vec3* out_intersection, r32 speed) const
{
	// first try to dash there
	if (can_dash(target, out_intersection))
		return true;

	// now try to fly there
	if (can_shoot(target, out_intersection, speed))
		return true;

	return false;
}

b8 Drone::direction_is_toward_attached_wall(const Vec3& dir) const
{
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	return dir.dot(wall_normal) < 0.0f;
}

// get the state frame representing the world according to this player, if one exists
// return false if no state frame is necessary
b8 Drone::net_state_frame(Net::StateFrame* state_frame) const
{
	if (Game::level.local && has<PlayerControlHuman>() && !get<PlayerControlHuman>()->local()) // this Drone is being controlled remotely; we need to rewind the world state to what it looks like from their side
	{
		r32 timestamp;
#if SERVER
		timestamp = Net::timestamp() - get<PlayerControlHuman>()->rtt;
#else
		// should never happen on client
		timestamp = 0.0f;
		vi_assert(false);
#endif
		return Net::state_frame_by_timestamp(state_frame, timestamp);
	}
	else
		return false;
}

b8 Drone::can_shoot(const Vec3& dir, Vec3* final_pos, b8* hit_target, const Net::StateFrame* state_frame) const
{
	// if we're attached to a wall, make sure we're not shooting into the wall
	if (state() == Drone::State::Crawl && direction_is_toward_attached_wall(dir))
		return false;

	return could_shoot(get<Transform>()->absolute_pos(), dir, final_pos, nullptr, hit_target, state_frame);
}

b8 Drone::could_shoot(const Vec3& trace_start, const Vec3& dir, Vec3* final_pos, Vec3* final_normal, b8* hit_target, const Net::StateFrame* state_frame) const
{
	Vec3 trace_dir = Vec3::normalize(dir);

	// can't shoot straight up or straight down
	// HACK: if it's a local player, let them do what they want because it's frustrating
	// in certain cases where the drone won't let you go where you should be able to go
	// due to the third-person camera offset
	// the AI however needs to know whether it can hit actually hit a target
	if (!has<PlayerControlHuman>() && fabsf(trace_dir.y) > DRONE_VERTICAL_DOT_LIMIT)
		return false;

	Vec3 trace_end = trace_start + trace_dir * DRONE_SNIPE_DISTANCE;

	Net::StateFrame state_frame_data;
	if (!state_frame && net_state_frame(&state_frame_data))
		state_frame = &state_frame_data;

	Hits hits;
	raycast(RaycastMode::IgnoreForceFields, trace_start, trace_end, state_frame, &hits);

	r32 r = range();
	b8 allow_further_end = false; // allow drone to shoot if we're aiming at an enemy drone in range but the backing behind it is out of range
	b8 hit_target_value = false;
	for (s32 i = 0; i < hits.index_end + 1; i++)
	{
		const Hit& hit = hits.hits[i];
		if (hit.fraction * DRONE_SNIPE_DISTANCE < r)
		{
			if (hit.type == Hit::Type::Shield)
			{
				allow_further_end = true;
				hit_target_value = true;
			}
			else if (hit.type == Hit::Type::Target)
				hit_target_value = true;
		}
	}

#if SERVER
	if (state_frame)
		allow_further_end = true; // be more lenient on server to prevent glitching
#endif

	const Hit& final_hit = hits.hits[hits.hits.length - 1];

	if (!allow_further_end || !hit_target_value)
	{
		// check drone target predictions
		r32 end_distance_sq = vi_min(r * r, final_hit.fraction * DRONE_SNIPE_DISTANCE * final_hit.fraction * DRONE_SNIPE_DISTANCE);
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item() != this && (i.item()->get<Target>()->absolute_pos() - trace_start).length_squared() > DRONE_SHIELD_RADIUS * 2.0f * DRONE_SHIELD_RADIUS * 2.0f)
			{
				Vec3 intersection;
				if (predict_intersection(i.item()->get<Target>(), state_frame, &intersection, target_prediction_speed()))
				{
					if ((intersection - trace_start).length_squared() <= end_distance_sq
						&& LMath::ray_sphere_intersect(trace_start, trace_end, intersection, DRONE_SHIELD_RADIUS))
					{
						allow_further_end = true;
						hit_target_value = true;
						break;
					}
				}
			}
		}
	}

	if (final_hit.type == Hit::Type::Environment)
	{
		// need to check that the environment hit is within range
		// however if we are shooting at a Drone, we can tolerate further environment hits
		if (allow_further_end || final_hit.fraction * DRONE_SNIPE_DISTANCE < r)
		{
			if (final_pos)
				*final_pos = final_hit.pos;
			if (final_normal)
				*final_normal = final_hit.normal;
			if (hit_target)
				*hit_target = hit_target_value;
			return true;
		}
	}
	return false;
}

b8 Drone::can_spawn(Ability a, const Vec3& dir, Vec3* final_pos, Vec3* final_normal, RigidBody** hit_parent, b8* hit_target) const
{
	if (AbilityInfo::list[s32(a)].type == AbilityInfo::Type::Other)
	{
		if (final_pos)
			*final_pos = Vec3::zero;
		if (final_normal)
			*final_normal = Vec3::zero;
		if (hit_parent)
			*hit_parent = nullptr;
		if (hit_target)
			*hit_target = false;
		return true;
	}

	Vec3 trace_dir = Vec3::normalize(dir);

	// can't shoot straight up or straight down
	// HACK: if it's a local player, let them do what they want because it's frustrating
	// in certain cases where the drone won't let you go where you should be able to go
	// due to the third-person camera offset
	// the AI however needs to know whether it can hit actually hit a target
	if (!has<PlayerControlHuman>() && fabsf(trace_dir.y) > DRONE_VERTICAL_DOT_LIMIT)
		return false;

	Vec3 trace_start = get<Transform>()->absolute_pos();
	Vec3 trace_end = trace_start + trace_dir * range();

	AbilityInfo::Type type = AbilityInfo::list[s32(a)].type;
	RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
	s16 force_field_mask = type == AbilityInfo::Type::Build
		? ~ally_force_field_mask() // only ignore friendly force fields; we don't want to build something on a force field
		: ~CollisionAllTeamsForceField; // ignore all force fields
	Physics::raycast(&ray_callback, ~CollisionDroneIgnore & force_field_mask);
	if (type == AbilityInfo::Type::Shoot)
	{
		if (ray_callback.hasHit())
		{
			Entity* e = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (hit_target)
				*hit_target = e->has<Target>();
			if (hit_parent)
				*hit_parent = e->get<RigidBody>();
		}
		else
		{
			if (final_pos)
				*final_pos = trace_end;
			if (hit_target)
				*hit_target = false;
			if (hit_parent)
				*hit_parent = nullptr;
		}
		return true; // we can always spawn these abilities, even if we're aiming into space
	}
	else
	{
		// build-type ability
		if (ray_callback.hasHit())
		{
			Entity* hit_entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (!hit_entity->has<Target>()
				&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
			{
				if (final_pos)
					*final_pos = ray_callback.m_hitPointWorld;
				if (final_normal)
					*final_normal = ray_callback.m_hitNormalWorld;
				if (hit_parent)
					*hit_parent = hit_entity->get<RigidBody>();
				if (hit_target)
					*hit_target = false;
				return true;
			}
		}
		return false;
	}
}

void Drone::ability(Ability a)
{
	if (a != current_ability)
		DroneNet::ability_select(this, a);
}

void Drone::cooldown_setup(s8 amount)
{
	vi_assert(charges >= amount);
	charges -= amount;

#if SERVER
	DroneNet::charge_count(this, charges);

	if (has<PlayerControlHuman>())
		cooldown = DRONE_COOLDOWN - get<PlayerControlHuman>()->rtt;
	else
#endif
		cooldown = DRONE_COOLDOWN;
}

void Drone::ensure_detached()
{
	if (get<Transform>()->parent.ref())
	{
		attach_time = Game::time.total;
		get<Transform>()->reparent(nullptr);
		Vec3 p = get<Transform>()->absolute_pos();
	}

	get<SkinnedModel>()->offset = Mat4::identity;

	for (s32 i = 0; i < DRONE_LEGS; i++)
		footing[i].parent = nullptr;
	get<Animator>()->reset_overrides();
	get<Animator>()->layers[0].animation = Asset::Animation::drone_fly;
}

b8 Drone::dash_start(const Vec3& dir, const Vec3& target)
{
	if (state() == State::Dash)
	{
#if SERVER
		// add some forgiveness
		if (dash_timer > DRONE_REFLECTION_TIME_TOLERANCE)
#endif
		{
#if DEBUG_NET_SYNC
			vi_debug_break();
#endif
			return false;
		}
	}
	else if (state() == State::Fly || current_ability != Ability::None)
		return false;

	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	Vec3 dir_normalized = Vec3::normalize(dir);
	Vec3 pos = get<Transform>()->absolute_pos();

	// determine how long we'll be dashing, and whether it will be a combo
	r32 time = 0.0f;
	b8 combo = false;

	Vec3 dir_flattened = dir_normalized - wall_normal * wall_normal.dot(dir_normalized);

	// check for obstacles
	r32 max_time = DRONE_DASH_TIME;
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(pos, pos + dir_flattened * DRONE_DASH_DISTANCE);
		Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
		if (ray_callback.hasHit())
			max_time = ray_callback.m_closestHitFraction * DRONE_DASH_TIME;
	}
	Vec3 next_pos = pos;
	while (time < max_time)
	{
		// recalculate dir_flattened in case wall_normal has changed as we've been dashing
		dir_flattened = dir_normalized - wall_normal * wall_normal.dot(dir_normalized);

		// check if we still have wall to dash on
		Vec3 wall_ray_start = next_pos + wall_normal * DRONE_RADIUS;
		Vec3 wall_ray_end = next_pos + wall_normal * DRONE_RADIUS * -2.0f;
		btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray_start, wall_ray_end);
		Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
		if (ray_callback.hasHit())
		{
			Vec3 other_wall_normal = ray_callback.m_hitNormalWorld;
			if (other_wall_normal.dot(wall_normal) > 0.707f
				&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
				wall_normal = other_wall_normal; // allow the drone to keep dashing around the corner
			else
				break; // no more wall
		}
		else
			break; // no more wall

		// check if we can hit the target now
		Vec3 target_dir = Vec3::normalize(target - next_pos);
		Vec3 final_pos;
		Vec3 final_normal;
		if (could_shoot(next_pos, target_dir, &final_pos, &final_normal)
			&& (final_pos - target).dot(target_dir) > DRONE_RADIUS * -2.0f
			&& (next_pos - final_pos).dot(final_normal) > DRONE_RADIUS * 2.0f)
		{
			combo = true;
			break;
		}

		const r32 time_increment = Net::tick_rate() * 0.1f;
		next_pos += dir_flattened * DRONE_DASH_SPEED * time_increment;
		time += time_increment;
	}

	if (combo && time <= DRONE_DASH_TIME / r32(1 << 16))
	{
		// don't dash; just start flying
		{
			Net::StateFrame* state_frame = nullptr;
			Net::StateFrame state_frame_data;
			if (net_state_frame(&state_frame_data))
				state_frame = &state_frame_data;
			if (!could_shoot(pos, dir, nullptr, nullptr, nullptr, state_frame))
			{
#if DEBUG_NET_SYNC && SERVER
				if (has<PlayerControlHuman>())
					vi_debug_break();
#endif
				return false;
			}
		}
		DroneNet::start_flying(this, dir_normalized, DroneNet::FlyFlag::None);
	}
	else
	{
		dash_combo = combo;
		dash_target = target;
		DroneNet::start_dashing(this, dir_normalized, time);
	}

	return true;
}

b8 Drone::cooldown_can_shoot() const
{
	return charges > 0;
}

r32 Drone::target_prediction_speed() const
{
	switch (current_ability)
	{
		case Ability::Sniper:
		{
			return 0.0f;
		}
		case Ability::Bolter:
		{
			return BOLT_SPEED_TURRET;
		}
		default:
		{
			return DRONE_FLY_SPEED;
		}
	}
}

r32 Drone::range() const
{
	return current_ability == Ability::Sniper ? DRONE_SNIPE_DISTANCE : DRONE_MAX_DISTANCE;
}

b8 Drone::go(const Vec3& dir)
{
	Ability a = current_ability;
	if (AbilityInfo::list[s32(a)].type == AbilityInfo::Type::Other)
		current_ability = Ability::None;

	if (!cooldown_can_shoot())
	{
#if SERVER && DEBUG_NET_SYNC
		if (has<PlayerControlHuman>())
			vi_debug_break();
#endif
		return false;
	}

	Vec3 dir_normalized = Vec3::normalize(dir);

	if (a == Ability::None)
	{
		{
			Net::StateFrame* state_frame = nullptr;
			Net::StateFrame state_frame_data;
			if (net_state_frame(&state_frame_data))
				state_frame = &state_frame_data;
			if (!can_shoot(dir, nullptr, nullptr, state_frame))
			{
#if SERVER && DEBUG_NET_SYNC
				if (has<PlayerControlHuman>())
					vi_debug_break();
#endif
				return false;
			}
		}
		DroneNet::start_flying(this, dir_normalized, DroneNet::FlyFlag::None);
	}
	else
	{
		if (!get<PlayerCommon>()->manager.ref()->ability_valid(a))
		{
#if SERVER && DEBUG_NET_SYNC
			if (has<PlayerControlHuman>())
				vi_debug_break();
#endif
			return false;
		}

		if (!can_spawn(a, dir_normalized))
		{
#if SERVER && DEBUG_NET_SYNC
			if (has<PlayerControlHuman>())
				vi_debug_break();
#endif
			return false;
		}

		if (Game::level.local)
			DroneNet::ability_spawn(this, dir_normalized, a);
		else if (a == Ability::Sniper) // client-side prediction; effects
			drone_sniper_effects(this, dir_normalized);
		else if (a == Ability::ActiveArmor)
			get<Health>()->active_armor_timer = ACTIVE_ARMOR_TIME; // client-side prediction; show invincibility sparkles instantly
		else if (a == Ability::Shotgun)
		{
			// client-side prediction; create fake bolts
			Quat target_quat = Quat::look(dir_normalized);
			for (s32 i = 0; i < DRONE_SHOTGUN_PELLETS; i++)
			{
				Vec3 d = target_quat * drone_shotgun_dirs[i];
				fake_bolts.add
				(
					EffectLight::add
					(
						get<Transform>()->absolute_pos() + d * DRONE_SHIELD_RADIUS,
						BOLT_LIGHT_RADIUS,
						0.5f,
						EffectLight::Type::BoltDroneShotgun,
						nullptr,
						Quat::look(d)
					)
				);
			}
			cooldown_setup(DRONE_SHOTGUN_CHARGES);
		}
		else if (a == Ability::Bolter)
		{
			// client-side prediction; create fake bolt
			fake_bolts.add
			(
				EffectLight::add
				(
					get<Transform>()->absolute_pos() + dir_normalized * DRONE_SHIELD_RADIUS,
					BOLT_LIGHT_RADIUS,
					0.5f,
					EffectLight::Type::BoltDroneBolter,
					nullptr,
					Quat::look(dir_normalized)
				)
			);
			drone_bolter_cooldown_setup(this);
		}
	}

	return true;
}

#define REFLECTION_TRIES 32 // try x raycasts. if they all fail, just shoot off into space.

// removes the first reflection in the queue and executes it
void drone_reflection_execute(Drone* d)
{
	const Drone::Reflection& reflection = d->reflections[0];

#if DEBUG_REFLECTIONS
	vi_debug
	(
		"%f executing %s reflection at %fs (%f %f %f).",
		Game::real_time.total,
		reflection.src == Net::MessageSource::Loopback ? "local" : "remote",
		Game::time.total - d->attach_time,
		reflection.pos.x,
		reflection.pos.y,
		reflection.pos.z
	);
#endif

	{
		r32 l = reflection.dir.length_squared();
		vi_assert(l > 0.98f * 0.98f && l < 1.02f * 1.02f);
	}
	d->get<Transform>()->reparent(nullptr);
	d->get<Transform>()->absolute_pos(reflection.pos);
	d->dash_timer = 0.0f;
	d->dash_combo = false;
	d->get<Animator>()->layers[0].animation = Asset::Animation::drone_fly;

	Entity* reflected_off = reflection.entity.ref();
	if (!reflected_off || !reflected_off->has<Target>()) // target hit effects are handled separately
	{
		if (Game::level.local)
			DroneNet::reflection_effects(d, reflected_off); // let everyone know we're doing reflection effects
		else
		{
			// client-side prediction
			vi_assert(d->has<PlayerControlHuman>() && d->get<PlayerControlHuman>()->local());
			client_hit_effects(d, reflected_off, DroneHitType::Reflection);
		}
	}

	DroneReflectEvent e;
	e.entity = reflected_off;
	e.new_velocity = reflection.dir * DRONE_FLY_SPEED;
	d->reflecting.fire(e);

	d->rotation_clamp_vector = reflection.dir;
	d->get<Transform>()->rot = Quat::look(reflection.dir);
	d->velocity = e.new_velocity;

	d->reflections.remove_ordered(0);
}

void drone_dash_fly_simulate(Drone* d, r32 dt, Net::StateFrame* state_frame = nullptr)
{
	Drone::State s = d->state();

	Vec3 position = d->get<Transform>()->absolute_pos();
	Vec3 next_position;

	if (btVector3(d->velocity).fuzzyZero() && d->reflections.length == 0) // HACK. why does this happen
		d->velocity = d->get<Transform>()->absolute_rot() * Vec3(0, 0, s == Drone::State::Dash ? DRONE_DASH_SPEED : DRONE_FLY_SPEED);

	if (s == Drone::State::Dash)
	{
		d->crawl(d->velocity, vi_min(d->dash_timer, dt));
		next_position = d->get<Transform>()->absolute_pos();
		if (d->velocity.length_squared() > 0.0f)
			d->dash_timer = vi_max(0.0f, d->dash_timer - dt);
		if (d->dash_timer == 0.0f)
		{
			if (d->dash_combo)
				DroneNet::start_flying(d, d->dash_target - next_position, DroneNet::FlyFlag::CancelExisting);
			else
				DroneNet::finish_dashing(d);
			return;
		}
	}
	else
	{
		next_position = position + d->velocity * dt;
		d->get<Transform>()->absolute_pos(next_position);
	}

	{
		Vec3 dir = Vec3::normalize(d->velocity);
		Vec3 ray_start = position + dir * -DRONE_RADIUS;
		Vec3 ray_end = next_position + dir * DRONE_RADIUS;
		d->movement_raycast(ray_start, ray_end, nullptr, state_frame);
	}
}

void drone_fast_forward(Drone* d, r32 amount)
{
	vi_assert(Game::level.local && d->state() != Drone::State::Crawl && d->velocity.length_squared() > 0.0f);
#if DEBUG_REFLECTIONS
	vi_debug("%f fast-forwarding %fs", Game::real_time.total, amount);
#endif

	r32 timestamp = Net::timestamp() - amount;
	const r32 SIMULATION_STEP = Net::tick_rate();
	s32 reflection_count = d->reflections.length;
	while (timestamp < Net::timestamp()
		&& d->reflections.length == reflection_count
		&& d->state() != Drone::State::Crawl) // stop if we reflect off anything; movement_raycast will have handled fast-forwarding from that point
	{
		Net::StateFrame state_frame;
		Net::state_frame_by_timestamp(&state_frame, timestamp);
		drone_dash_fly_simulate(d, SIMULATION_STEP, &state_frame);
		timestamp += SIMULATION_STEP;
	}
}

void Drone::reflect(Entity* entity, const Vec3& hit, const Vec3& normal, const Net::StateFrame* state_frame)
{
	vi_assert(velocity.length_squared() > 0.0f);

	Vec3 last_pos = get<Transform>()->absolute_pos();

	// it's possible to reflect off a shield while we are dashing (still parented to an object)
	// so we need to make sure we're not dashing anymore
	get<Transform>()->parent = nullptr;
	get<Transform>()->absolute_pos(hit);

	// the actual direction we end up going
	Vec3 new_dir;

	{
		// make sure we have somewhere to land.

		// our goal
		Vec3 target_dir = Vec3::normalize(velocity.reflect(normal));

		new_dir = target_dir;

		Quat target_quat = Quat::look(target_dir);

		r32 random_range = 0.0f;
		r32 best_score = DRONE_MAX_DISTANCE;
		const r32 goal_distance = DRONE_MAX_DISTANCE * 0.25f;

		// if we're bouncing off a Shield, we can bounce backward through the target.
		// otherwise, it's probably an inaccessible surface or a force field, so we don't want to bounce through it
		b8 allow_reflect_against_normal = entity->has<Shield>();

		for (s32 i = 0; i < REFLECTION_TRIES; i++)
		{
			Vec3 candidate_dir = target_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * random_range, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * random_range, 0) * Vec3(1, 0, 0));
			if (allow_reflect_against_normal || candidate_dir.dot(normal) > 0.05f)
			{
				Vec3 next_hit;
				if (can_shoot(candidate_dir, &next_hit, nullptr, state_frame))
				{
					r32 distance = (next_hit - hit).length();
					r32 score = fabsf(distance - goal_distance);

					if (score < best_score)
					{
						new_dir = candidate_dir;
						best_score = score;
					}

					if (distance > goal_distance && score < DRONE_MAX_DISTANCE * 0.4f)
					{
						new_dir = candidate_dir;
						best_score = score;
						break;
					}
				}
				random_range += PI / r32(REFLECTION_TRIES);
			}
			else
				i--; // candidate dir was against the normal; try again
		}
	}

	if (!state_frame // this drone is locally controlled
		|| reflections.length == 0) // or we're a server and this drone is remote controlled, but the remote has not told us about this reflection yet
	{
		Reflection* reflection = reflections.add();
		reflection->pos = hit;
		reflection->timer = DRONE_REFLECTION_TIME_TOLERANCE + Game::time.delta;
		reflection->entity = entity;
		reflection->src = Net::MessageSource::Loopback; // this hit was detected locally
		reflection->additional_fast_forward_time = Game::time.delta - (hit - last_pos).length() / velocity.length();
		reflection->dir = new_dir;

		if (state_frame)
		{
#if DEBUG_REFLECTIONS
			vi_debug
			(
				"%f detected local reflection at %fs (%f %f %f), waiting for remote...",
				Game::real_time.total,
				Game::time.total - attach_time,
				reflection->pos.x,
				reflection->pos.y,
				reflection->pos.z
			);
#endif
			vi_assert(reflections.length == 1);
			// store our reflection result and wait for the remote to tell us which way to go
			// if we don't hear from them in a certain amount of time, forget anything happened
			velocity = Vec3::zero;
		}
	}

	if (!state_frame // locally controlled; reflect instantly
		|| reflections[0].src == Net::MessageSource::Remote) // remotely controlled, but the remote already told us about this reflection
	{
		const Reflection& reflection = reflections[0];

		b8 valid;
		if (state_frame)
		{
			if ((reflection.pos - hit).length_squared() < DRONE_REFLECTION_POSITION_TOLERANCE * DRONE_REFLECTION_POSITION_TOLERANCE)
			{
				valid = true;
#if DEBUG_REFLECTIONS
				vi_debug("%f local confirmed remote reflection %fs later, executing", Game::real_time.total, vi_max(0.0f, DRONE_REFLECTION_TIME_TOLERANCE - reflection.timer));
#endif
			}
			else
			{
				valid = false;
				vi_debug("%s", "Remote attempted to force an invalid drone reflection. Ignoring.");
			}
		}
		else
			valid = true;

		// if we have a state frame, we're on the server and this is a remote drone
		// so reflections should only come from the remote
		// if we're locally controlled, reflections should only come from us
		vi_assert(b8(state_frame) == (reflection.src == Net::MessageSource::Remote));

		r32 fast_forward = vi_max(0.0f, DRONE_REFLECTION_TIME_TOLERANCE - reflection.timer);
		if (valid)
			drone_reflection_execute(this);
		else
			reflections.remove_ordered(0); // ignore it
		if (state_frame) // only need to fast forward on server
			drone_fast_forward(this, fast_forward);
	}
}

void Drone::handle_remote_reflection(Entity* entity, const Vec3& reflection_pos, const Vec3& reflection_dir)
{
	if (reflection_dir.length() == 0.0f)
	{
#if DEBUG_REFLECTIONS
		vi_debug("%s", "Rejected zero-length remote reflection.");
#endif
		return;
	}

	if (state() == State::Crawl)
	{
#if DEBUG_REFLECTIONS
		vi_debug("%s", "Rejected remote reflection because local drone is in Crawl state.");
#endif
		return;
	}

	vi_assert(Game::level.local); // reflect messages only go from client to server

	Vec3 reflection_dir_normalized = Vec3::normalize(reflection_dir);

	// we're a server; the client is notifying us that it did a reflection

	if (reflections.length == 0 || reflections[0].src == Net::MessageSource::Remote)
	{
		// we haven't reflected off anything on the server yet; save this info and wait for us to hit something
#if DEBUG_REFLECTIONS
		vi_debug("%f received remote reflection, waiting for confirmation", Game::real_time.total);
#endif
		Reflection* reflection = reflections.add();
		reflection->dir = reflection_dir_normalized;
		reflection->pos = reflection_pos;
		reflection->timer = DRONE_REFLECTION_TIME_TOLERANCE;
		reflection->entity = entity;
		reflection->additional_fast_forward_time = 0.0f;
		reflection->src = Net::MessageSource::Remote;
	}
	else
	{
		// we HAVE already detected a reflection off something; let's do it now
		Reflection* reflection = &reflections[0];

		// check if they're roughly where we think they should be
		if ((reflection->pos - reflection_pos).length_squared() < DRONE_REFLECTION_POSITION_TOLERANCE * DRONE_REFLECTION_POSITION_TOLERANCE)
		{
			r32 original_timer = reflection->timer;
#if DEBUG_REFLECTIONS
			vi_debug("%f remote confirmed local reflection %fs later, executing", Game::real_time.total, vi_max(0.0f, DRONE_REFLECTION_TIME_TOLERANCE - original_timer));
#endif
			// replace local reflection data with remote data
			reflection->dir = reflection_dir_normalized;
			reflection->pos = reflection_pos;
			reflection->entity = entity;
			reflection->src = Net::MessageSource::Remote;

			drone_reflection_execute(this); // automatically removes the reflection from the queue

			// fast forward the amount of time we've been sitting here waiting for the client to acknowledge the reflection
			drone_fast_forward(this, vi_max(0.0f, DRONE_REFLECTION_TIME_TOLERANCE - original_timer));
		}
		else
		{
			// client is not where we think they should be. they might be hacking. don't reflect
			vi_debug("%s", "Remote attempted to force an invalid drone reflection. Ignoring.");
		}
	}
}

void Drone::crawl_wall_edge(const Vec3& dir, const Vec3& other_wall_normal, r32 dt, r32 speed)
{
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);

	Vec3 orthogonal = wall_normal.cross(other_wall_normal);

	Vec3 dir_flattened = orthogonal * orthogonal.dot(dir);

	r32 dir_flattened_length = dir_flattened.length();
	if (dir_flattened_length > 0.1f)
	{
		dir_flattened /= dir_flattened_length;
		Vec3 pos = get<Transform>()->absolute_pos();
		Vec3 next_pos = pos + dir_flattened * dt * speed;
		Vec3 wall_ray_start = next_pos + wall_normal * DRONE_RADIUS;
		Vec3 wall_ray_end = next_pos + wall_normal * DRONE_RADIUS * -2.0f;

		btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray_start, wall_ray_end);
		Physics::raycast(&ray_callback, ~DRONE_INACCESSIBLE_MASK & ~ally_force_field_mask());

		if (ray_callback.hasHit() && !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
		{
			// check for obstacles
			btCollisionWorld::ClosestRayResultCallback ray_callback2(pos, next_pos + dir_flattened * DRONE_RADIUS);
			Physics::raycast(&ray_callback2, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
			if (!ray_callback2.hasHit())
			{
				// all good, go ahead
				move
				(
					ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * DRONE_RADIUS,
					Quat::look(ray_callback.m_hitNormalWorld),
					ray_callback.m_collisionObject->getUserIndex()
				);
			}
		}
	}
}

// return true if we actually switched to the other wall
b8 Drone::transfer_wall(const Vec3& dir, const btCollisionWorld::ClosestRayResultCallback& ray_callback)
{
	// check to make sure that our movement direction won't get flipped if we switch walls.
	// this prevents jittering back and forth between walls all the time.
	// also, don't crawl onto inaccessible surfaces.
	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	Vec3 other_wall_normal = ray_callback.m_hitNormalWorld;
	Vec3 dir_flattened_other_wall = dir - other_wall_normal * other_wall_normal.dot(dir);

	if (dir_flattened_other_wall.dot(wall_normal) > 0.0f
		&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
	{
		// check for obstacles
		btCollisionWorld::ClosestRayResultCallback obstacle_ray_callback(ray_callback.m_hitPointWorld, ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * (DRONE_RADIUS * 1.1f));
		Physics::raycast(&obstacle_ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
		if (!obstacle_ray_callback.hasHit())
		{
			move
			(
				ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * DRONE_RADIUS,
				Quat::look(ray_callback.m_hitNormalWorld),
				ray_callback.m_collisionObject->getUserIndex()
			);
			return true;
		}
	}
	return false;
}

void Drone::move(const Vec3& new_pos, const Quat& new_rotation, const ID entity_id)
{
	get<Transform>()->absolute(new_pos, new_rotation);
	Entity* entity = &Entity::list[entity_id];
	if (entity->get<Transform>() != get<Transform>()->parent.ref())
		get<Transform>()->reparent(entity->get<Transform>());
	update_offset();
}

void Drone::crawl(const Vec3& dir_raw, r32 dt)
{
	r32 dir_length = dir_raw.length();

	State s = state();
	if (s != State::Fly && dir_length > 0.0f)
	{
		Vec3 dir_normalized = dir_raw / dir_length;

		r32 speed = s == State::Dash ? DRONE_DASH_SPEED : (vi_min(dir_length, 1.0f) * DRONE_CRAWL_SPEED);

		Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
		Vec3 pos = get<Transform>()->absolute_pos();

		if (dir_normalized.dot(wall_normal) > 0.0f)
		{
			// first, try to climb in the actual direction requested
			Vec3 next_pos = pos + dir_normalized * dt * speed;
			
			// check for obstacles
			Vec3 ray_end = next_pos + (dir_normalized * DRONE_RADIUS * 1.5f);
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, ray_end);
			Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());

			if (ray_callback.hasHit())
			{
				if (transfer_wall(dir_normalized, ray_callback))
					return;
			}
		}

		Vec3 dir_flattened = dir_normalized - wall_normal * wall_normal.dot(dir_normalized);
		r32 dir_flattened_length = dir_flattened.length();
		if (dir_flattened_length < 0.005f)
			return;

		dir_flattened /= dir_flattened_length;

		Vec3 next_pos = pos + dir_flattened * dt * speed;

		// check for obstacles
		{
			Vec3 ray_end = next_pos + (dir_flattened * DRONE_RADIUS);
			btCollisionWorld::ClosestRayResultCallback ray_callback(pos, ray_end);
			Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());

			if (ray_callback.hasHit())
			{
				if (!transfer_wall(dir_normalized, ray_callback))
				{
					// Stay on our current wall
					crawl_wall_edge(dir_normalized, ray_callback.m_hitNormalWorld, dt, speed);
				}
				return;
			}
		}

		// no obstacle. check if we still have wall to walk on.

		Vec3 wall_ray_start = next_pos + wall_normal * DRONE_RADIUS;
		Vec3 wall_ray_end = next_pos + wall_normal * DRONE_RADIUS * -2.0f;

		btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray_start, wall_ray_end);
		Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());

		if (ray_callback.hasHit())
		{
			// all good, go ahead

			Vec3 other_wall_normal = ray_callback.m_hitNormalWorld;
			Vec3 dir_flattened_other_wall = dir_normalized - other_wall_normal * other_wall_normal.dot(dir_normalized);
			// check to make sure that our movement direction won't get flipped if we switch walls.
			// this prevents jittering back and forth between walls all the time.
			if (dir_flattened_other_wall.dot(dir_flattened) > 0.0f
				&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
			{
				Vec3 to_next_wall = Vec3(ray_callback.m_hitPointWorld) - attach_point();
				b8 next_wall_curves_away = wall_normal.dot(to_next_wall) < 0.0f;
				r32 dir_flattened_dot = dir_flattened_other_wall.dot(wall_normal);
				if ((next_wall_curves_away && dir_flattened_dot < 0.01f)
					|| (!next_wall_curves_away && dir_flattened_dot > -0.01f))
				{
					move
					(
						ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * DRONE_RADIUS,
						Quat::look(ray_callback.m_hitNormalWorld),
						ray_callback.m_collisionObject->getUserIndex()
					);
				}
				else
				{
					// stay on our current wall
					crawl_wall_edge(dir_normalized, other_wall_normal, dt, speed);
				}
			}
		}
		else
		{
			// no wall left
			// see if we can walk around the corner
			Vec3 wall_ray2_start = next_pos + wall_normal * DRONE_RADIUS * -1.25f;
			Vec3 wall_ray2_end = wall_ray2_start + dir_flattened * DRONE_RADIUS * -2.0f;

			btCollisionWorld::ClosestRayResultCallback ray_callback(wall_ray2_start, wall_ray2_end);
			Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());

			if (ray_callback.hasHit())
			{
				// walk around the corner

				// check to make sure that our movement direction won't get flipped if we switch walls.
				// this prevents jittering back and forth between walls all the time.
				if (dir_normalized.dot(wall_normal) < 0.05f
					&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
				{
					// transition to the other wall
					move
					(
						ray_callback.m_hitPointWorld + ray_callback.m_hitNormalWorld * DRONE_RADIUS,
						Quat::look(ray_callback.m_hitNormalWorld),
						ray_callback.m_collisionObject->getUserIndex()
					);
				}
				else
				{
					// stay on our current wall
					Vec3 other_wall_normal = Vec3(ray_callback.m_hitNormalWorld);
					crawl_wall_edge(dir_normalized, other_wall_normal, dt, speed);
				}
			}
		}
	}
}
const AssetID drone_legs[DRONE_LEGS] =
{
	Asset::Bone::drone_a1,
	Asset::Bone::drone_b1,
	Asset::Bone::drone_c1,
};

const AssetID drone_outer_legs[DRONE_LEGS] =
{
	Asset::Bone::drone_a2,
	Asset::Bone::drone_b2,
	Asset::Bone::drone_c2,
};

const r32 drone_outer_leg_rotation[DRONE_LEGS] =
{
	-1,
	1,
	1,
};

void Drone::set_footing(const s32 index, const Transform* parent, const Vec3& pos)
{
	if (footing[index].parent.ref())
	{
		footing[index].blend = 0.0f;
		footing[index].last_abs_pos = footing[index].parent.ref()->to_world(footing[index].pos);
	}
	else
		footing[index].blend = 1.0f;
	footing[index].parent = parent;
	footing[index].pos = footing[index].parent.ref()->to_local(pos);
}

void Drone::get_offset(Mat4* mat, OffsetMode mode) const
{
	Quat offset_rot = lerped_rotation;
	Vec3 offset_pos = lerped_pos;

	if (mode == OffsetMode::WithUpgradeStation)
	{
		UpgradeStation* station = UpgradeStation::drone_inside(this);
		if (station) // make it look like we're attached to this upgrade station
			station->transform(&offset_pos, &offset_rot);
	}

	get<Transform>()->to_local(&offset_pos, &offset_rot);

	if (state() != State::Crawl)
		offset_pos = Vec3::zero;

	mat->rotation(offset_rot);
	mat->translation(offset_pos);
}

void Drone::update_offset()
{
	get_offset(&get<SkinnedModel>()->offset);
}

void Drone::stealth(Entity* e, b8 enable)
{
	if (enable != e->get<AIAgent>()->stealth)
	{
		if (enable)
		{
			e->get<AIAgent>()->stealth = true;
			e->get<SkinnedModel>()->alpha();
			e->get<SkinnedModel>()->color.w = 0.7f;
			e->get<SkinnedModel>()->mask = 1 << s32(e->get<AIAgent>()->team); // only display to fellow teammates
		}
		else
		{
			e->get<AIAgent>()->stealth = false;
			if (!e->has<Drone>() || e->get<Drone>()->state() == State::Crawl)
				e->get<SkinnedModel>()->alpha_if_obstructing();
			else
				e->get<SkinnedModel>()->alpha_disable();
			e->get<SkinnedModel>()->color.w = MATERIAL_NO_OVERRIDE;
			e->get<SkinnedModel>()->mask = RENDER_MASK_DEFAULT; // display to everyone
		}
	}
}

void Drone::update_server(const Update& u)
{
	State s = state();

	if (cooldown > 0.0f)
	{
		r32 cooldown_last = cooldown;
		cooldown = vi_max(0.0f, cooldown - u.time.delta);
		for (s32 i = 0; i < DRONE_CHARGES; i++)
		{
			if (charges < DRONE_CHARGES && cooldown <= cooldown_thresholds[i])
			{
				if (cooldown_last > cooldown_thresholds[i])
				{
					// we just crossed the threshold this frame
					if (Game::level.local)
					{
						charges++;
#if SERVER
						DroneNet::charge_count(this, charges);
#endif
					}
					bolter_charge_counter = 0;
				}
			}
		}
	}

	if (s != Drone::State::Crawl)
	{
		// flying or dashing
		if (Game::level.local && u.time.total - attach_time > MAX_FLIGHT_TIME)
		{
			get<Health>()->kill(entity()); // kill self
			return;
		}

		drone_dash_fly_simulate(this, u.time.delta);
	}

#if SERVER
	for (s32 i = 0; i < reflections.length; i++)
		reflections[i].timer -= u.time.delta;

	while (reflections.length > 0 && reflections[0].timer <= 0.0f)
	{
		// time's up on this reflection, we have to do something
		const Reflection& reflection = reflections[0];
		r32 fast_forward = vi_max(0.0f, DRONE_REFLECTION_TIME_TOLERANCE - reflection.timer) + reflection.additional_fast_forward_time;

		if (reflection.src == Net::MessageSource::Remote) // the remote told us about this reflection. go ahead and do it even though we never detected the hit locally
		{
#if DEBUG_REFLECTIONS
			vi_debug("%f remote reflection timed out without confirmation, executing anyway", Game::real_time.total);
#endif
			drone_reflection_execute(this); // automatically removes the reflection
		}
		else // we detected the hit locally, but the client never acknowledged it. ignore the reflection and keep going straight.
		{
#if DEBUG_REFLECTIONS
			vi_debug("%f remote never confirmed local reflection, continuing as normal", Game::real_time.total);
#endif
			velocity = get<Transform>()->absolute_rot() * Vec3(0, 0, state() == State::Dash ? DRONE_DASH_SPEED : DRONE_FLY_SPEED); // restore original velocity
			reflections.remove_ordered(0);
		}

		// fast forward whatever amount of time we've been sitting here waiting on this reflection
		drone_fast_forward(this, fast_forward);
	}
#else
	// client
	vi_assert(reflections.length == 0);
#endif
}

r32 Drone::particle_accumulator;
void Drone::update_client_all(const Update& u)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
		i.item()->update_client(u);

	const r32 particle_interval = 0.025f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > particle_interval)
	{
		particle_accumulator -= particle_interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->state() != State::Crawl)
			{
				// emit particles
				// but don't start until the drone has cleared the camera radius
				// we do this so that the particles don't block the camera
				Particles::tracers.add
				(
					Vec3::lerp(particle_accumulator / vi_max(0.0001f, u.time.delta), i.item()->last_pos, i.item()->lerped_pos),
					Vec3::zero,
					0
				);
			}
		}
	}

	// update velocity and last_pos
	// this has to happen after the particles so that we can lerp between last_pos and lerped_pos
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Vec3 pos = i.item()->lerped_pos;
		r32 speed;
		if (i.item()->state() == State::Crawl)
		{
			i.item()->velocity = i.item()->velocity * 0.9f + ((pos - i.item()->last_pos) / vi_max(0.0001f, u.time.delta)) * 0.1f;
			speed = vi_max(0.0f, vi_min(1.0f, i.item()->velocity.length() / DRONE_CRAWL_SPEED));
		}
		else
			speed = 0.0f;
		i.item()->get<Audio>()->param(AK::GAME_PARAMETERS::DRONE_SPEED, speed);
		i.item()->last_pos = pos;
	}
}

Vec3 Drone::rotation_clamp() const
{
	r32 length = rotation_clamp_vector.length();
	if (length > 0.0f)
		return rotation_clamp_vector / length;
	else
		return lerped_rotation * Vec3(0, 0, 1);
}

Vec3 Drone::camera_center() const
{
	return center_lerped() + lerped_rotation * Vec3(0, 0, 0.5f);
}

void Drone::update_client(const Update& u)
{
	// update audio perspective
	{
		r32 perspective;
		if (!Game::level.noclip && has<PlayerControlHuman>() && get<PlayerControlHuman>()->local()) // we're a local player
			perspective = 0.5f;
		else
		{
			// not a local player; assume we're an ally, unless we find out we're enemies with one of the local players
			perspective = 0;
			AI::Team my_team = get<AIAgent>()->team;
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->local && i.item()->get<PlayerManager>()->team.ref()->team() != my_team)
				{
					perspective = 1;
					break;
				}
			}
		}
		get<Audio>()->param(AK::GAME_PARAMETERS::PERSPECTIVE, perspective);
	}

	State s = state();

	if (s == Drone::State::Crawl)
	{
		// crawling

		// rotation clamp
		{
			Vec3 target = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
			r32 distance = (target - rotation_clamp_vector).length();
			if (distance > 0.0f)
				rotation_clamp_vector = Vec3::lerp(vi_min(1.0f, (6.0f / distance) * u.time.delta), rotation_clamp_vector, target);
		}

		if (!get<AIAgent>()->stealth)
			get<SkinnedModel>()->alpha_if_obstructing();

		{
			// update lerped pos so we crawl smoothly
			Quat abs_rot;
			Vec3 abs_pos;
			get<Transform>()->absolute(&abs_pos, &abs_rot);
			{
				r32 angle = Quat::angle(lerped_rotation, abs_rot);
				if (angle > 0)
				{
					const r32 tolerance = PI * 0.7f;
					while (angle > tolerance)
					{
						lerped_rotation = Quat::slerp(0.5f / angle, lerped_rotation, abs_rot);
						angle = Quat::angle(lerped_rotation, abs_rot);
					}
					lerped_rotation = Quat::slerp(vi_min(1.0f, (LERP_ROTATION_SPEED / angle) * u.time.delta), lerped_rotation, abs_rot);
				}
			}

			{
				r32 distance = (abs_pos - lerped_pos).length();
				if (distance > 0.0f)
				{
					const r32 tolerance = DRONE_SHIELD_RADIUS;
					if (distance < tolerance)
						lerped_pos = Vec3::lerp(vi_min(1.0f, (LERP_TRANSLATION_SPEED / distance) * u.time.delta), lerped_pos, abs_pos);
					else // keep the lerped pos within tolerance of the absolute pos
						lerped_pos = Vec3::lerp(vi_min(1.0f, (tolerance * 0.9f) / distance), lerped_pos, abs_pos);
				}
			}
		}
		update_offset();

		if (get<Animator>()->layers[0].animation != AssetNull)
		{
			// this means that we were flying or dashing, but we were interrupted. the animation is still playing.
			// this probably happened because the server never started flying or dashing in the first place, while we did
			// and now we're snapping back to the server's state
#if DEBUG_NET_SYNC
			vi_debug_break();
#endif
			finish_flying_dashing_common();
			done_dashing.fire();
		}

		// update footing

		Mat4 inverse_offset;
		{
			// feet ignore upgrade station animation
			Mat4 offset = Mat4::identity;
			get_offset(&offset, OffsetMode::WithoutUpgradeStation);
			inverse_offset = offset.inverse();
		}

		r32 leg_blend_speed = vi_max(DRONE_MIN_LEG_BLEND_SPEED, DRONE_LEG_BLEND_SPEED * (velocity.length() / DRONE_CRAWL_SPEED));

		const Armature* arm = Loader::armature(get<Animator>()->armature);
		for (s32 i = 0; i < DRONE_LEGS; i++)
		{
			b8 find_footing = false;

			Vec3 relative_target;

			Vec3 find_footing_offset;

			if (footing[i].parent.ref())
			{
				Vec3 target = footing[i].parent.ref()->to_world(footing[i].pos);
				Vec3 relative_target = get<Transform>()->to_local(target);
				Vec3 target_leg_space = (arm->inverse_bind_pose[drone_legs[i]] * Vec4(relative_target, 1.0f)).xyz();
				// x axis = lengthwise along the leg
				// y axis = left and right rotation of the leg
				// z axis = up and down rotation of the leg
				if (target_leg_space.x < DRONE_LEG_LENGTH * 0.25f && target_leg_space.z > DRONE_LEG_LENGTH * -1.25f)
				{
					find_footing_offset = Vec3(DRONE_LEG_LENGTH * 2.0f, -target_leg_space.y, 0);
					find_footing = true;
				}
				else if (target_leg_space.x > DRONE_LEG_LENGTH * 1.75f)
				{
					find_footing_offset = Vec3(DRONE_LEG_LENGTH * 0.5f, -target_leg_space.y, 0);
					find_footing = true;
				}
				else if (target_leg_space.y < DRONE_LEG_LENGTH * -1.5f || target_leg_space.y > DRONE_LEG_LENGTH * 1.5f
					|| target_leg_space.length_squared() > (DRONE_LEG_LENGTH * 2.0f) * (DRONE_LEG_LENGTH * 2.0f))
				{
					find_footing_offset = Vec3(vi_max(target_leg_space.x, DRONE_LEG_LENGTH * 0.25f), -target_leg_space.y, 0);
					find_footing = true;
				}
			}
			else
			{
				find_footing = true;
				find_footing_offset = Vec3(DRONE_LEG_LENGTH, 0, 0);
			}

			if (find_footing)
			{
				Mat4 bind_pose_mat = arm->abs_bind_pose[drone_legs[i]];
				Vec3 ray_start = get<Transform>()->to_world((bind_pose_mat * Vec4(0, 0, DRONE_LEG_LENGTH * 1.75f, 1)).xyz());
				Vec3 ray_end = get<Transform>()->to_world((bind_pose_mat * Vec4(find_footing_offset + Vec3(0, 0, DRONE_LEG_LENGTH * -1.0f), 1)).xyz());
				btCollisionWorld::ClosestRayResultCallback ray_callback(ray_start, ray_end);
				Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
				if (ray_callback.hasHit())
					set_footing(i, Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<Transform>(), ray_callback.m_hitPointWorld);
				else
				{
					Vec3 new_ray_start = get<Transform>()->to_world((bind_pose_mat * Vec4(DRONE_LEG_LENGTH * 1.5f, 0, 0, 1)).xyz());
					Vec3 new_ray_end = get<Transform>()->to_world((bind_pose_mat * Vec4(DRONE_LEG_LENGTH * -1.0f, find_footing_offset.y, DRONE_LEG_LENGTH * -1.0f, 1)).xyz());
					btCollisionWorld::ClosestRayResultCallback ray_callback(new_ray_start, new_ray_end);
					Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
					if (ray_callback.hasHit())
						set_footing(i, Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<Transform>(), ray_callback.m_hitPointWorld);
					else
						footing[i].parent = nullptr;
				}
			}

			Quat leg_rot_inner;
			Quat leg_rot_outer;

			if (footing[i].parent.ref())
			{
				Vec3 target = footing[i].parent.ref()->to_world(footing[i].pos);
				Vec3 relative_target = (inverse_offset * Vec4(get<Transform>()->to_local(target), 1)).xyz();
				Vec3 target_leg_space = (arm->inverse_bind_pose[drone_legs[i]] * Vec4(relative_target, 1.0f)).xyz();

				if (footing[i].blend < 1.0f)
				{
					Vec3 last_relative_target = (inverse_offset * Vec4(get<Transform>()->to_local(footing[i].last_abs_pos), 1)).xyz();
					Vec3 last_target_leg_space = (arm->inverse_bind_pose[drone_legs[i]] * Vec4(last_relative_target, 1.0f)).xyz();

					footing[i].blend = vi_min(1.0f, footing[i].blend + u.time.delta * leg_blend_speed);
					target_leg_space = Vec3::lerp(footing[i].blend, last_target_leg_space, target_leg_space);
					if (footing[i].blend == 1.0f && Game::real_time.total - last_footstep > 0.07f)
					{
						get<Audio>()->post_event(AK::EVENTS::PLAY_DRONE_FOOTSTEP);
						last_footstep = Game::real_time.total;
					}
				}

				r32 angle = atan2f(-target_leg_space.y, target_leg_space.x);

				r32 angle_x = acosf((target_leg_space.length() * 0.5f) / DRONE_LEG_LENGTH);

				if (target_leg_space.x < 0.0f)
					angle += PI;

				Vec2 xy_offset = Vec2(target_leg_space.x, target_leg_space.y);
				r32 angle_x_offset = -atan2f(target_leg_space.z, xy_offset.length() * (target_leg_space.x < 0.0f ? -1.0f : 1.0f));

				leg_rot_inner = Quat::euler(-angle, 0, 0) * Quat::euler(0, angle_x_offset - angle_x, 0);
				leg_rot_outer = Quat::euler(0, angle_x * 2.0f * drone_outer_leg_rotation[i], 0);
			}
			else
			{
				leg_rot_inner = Quat::euler(0, PI * -0.1f, 0);
				leg_rot_outer =  Quat::euler(0, PI * 0.75f * drone_outer_leg_rotation[i], 0);
			}

			{
				const Animator::Layer& layer = get<Animator>()->layers[0];
				r32 animation_strength = Ease::quad_out<r32>(layer.blend);
				get<Animator>()->override_bone(drone_legs[i], Vec3::zero, Quat::slerp(animation_strength, Quat::identity, leg_rot_inner));
				get<Animator>()->override_bone(drone_outer_legs[i], Vec3::zero, Quat::slerp(animation_strength, Quat::identity, leg_rot_outer));
			}
		}
	}
	else
	{
		// flying or dashing

		if (!get<AIAgent>()->stealth)
			get<SkinnedModel>()->alpha_disable();

		if (get<Animator>()->layers[0].animation == AssetNull)
		{
			// this means that we were crawling, but were interrupted.
#if DEBUG_NET_SYNC
			vi_debug_break();
#endif
			ensure_detached();
		}

		Quat rot;
		Vec3 pos;
		get<Transform>()->absolute(&pos, &rot);

		lerped_pos = pos;
		lerped_rotation = rot;
		update_offset();
	}
}

s32 Drone::Hit::Comparator::compare(const Hit& a, const Hit& b)
{
	if (a.fraction > b.fraction)
		return 1;
	else if (a.fraction == b.fraction)
		return 0;
	else
		return -1;
}

void Drone::raycast(RaycastMode mode, const Vec3& ray_start, const Vec3& ray_end, const Net::StateFrame* state_frame, Hits* result, s32 recursion_level, Entity* ignore) const
{
	r32 distance_total = (ray_end - ray_start).length();

	// check environment
	{
		RaycastCallbackExcept ray_callback(ray_start, ray_end, entity());
		{
			s16 mask;
			switch (mode)
			{
				case RaycastMode::Default:
				{
					mask = (CollisionStatic | CollisionAllTeamsForceField) & ~ally_force_field_mask();
					break;
				}
				case RaycastMode::IgnoreForceFields:
				{
					mask = CollisionStatic;
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}
			Physics::raycast(&ray_callback, mask);
		}

		if (ray_callback.hasHit())
		{
			Hit::Type type;
			s16 group = ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
			if (group & CollisionInaccessible)
				type = Hit::Type::Inaccessible;
			else if (group & CollisionAllTeamsForceField)
				type = Hit::Type::ForceField;
			else
				type = Hit::Type::Environment;

			Hit hit =
			{
				ray_callback.m_hitPointWorld,
				ray_callback.m_hitNormalWorld,
				(ray_callback.m_hitPointWorld - ray_start).length() / distance_total,
				&Entity::list[ray_callback.m_collisionObject->getUserIndex()],
				type,
			};
			result->hits.add(hit);

			if (hit.entity.ref()->has<Turret>())
			{
				hit.type = Hit::Type::Target;
				hit.fraction -= 0.01f; // make sure this hit will be registered before we stop at the original hit
				result->hits.add(hit);
			}
		}
	}

	// check targets
	for (auto i = Target::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item() == get<Target>() // don't collide with self
			|| i.item()->entity() == ignore // don't collide with ignored entity
			|| (i.item()->has<Drone>() && UpgradeStation::drone_inside(i.item()->get<Drone>()))) // ignore drones inside upgrade stations
			continue;

		Vec3 p;
		// do rewinding, unless we're checking collisions between two players on the same client
		if (state_frame
			&& Game::net_transform_filter(i.item()->entity(), Game::level.mode)
			&& !PlayerHuman::players_on_same_client(entity(), i.item()->entity()))
		{
			Vec3 pos;
			Quat rot;
			Net::transform_absolute(*state_frame, i.item()->get<Transform>()->id(), &pos, &rot);
			p = pos + (rot * i.item()->local_offset); // todo possibly: rewind local_offset as well?
		}
		else
			p = i.item()->absolute_pos();

		Vec3 intersection;
		if (LMath::ray_sphere_intersect(ray_start, ray_end, p, i.item()->radius(), &intersection))
		{
			result->hits.add(
			{
				intersection,
				Vec3::normalize(intersection - p),
				(intersection - ray_start).length() / distance_total,
				i.item()->entity(),
				i.item()->has<Shield>() ? Hit::Type::Shield : Hit::Type::Target,
			});
		}
	}

	// sort collisions
	Hit::Comparator comparator;
	Quicksort::sort<Hit, Hit::Comparator>(result->hits.data, 0, result->hits.length, &comparator);

	// make sure we don't hit anything after hitting the environment
	for (s32 i = 0; i < result->hits.length; i++)
	{
		if (result->hits[i].type == Hit::Type::Environment)
		{
			result->hits.length = i + 1;
			break;
		}
	}

	// determine which collision is the one we stop at
	result->index_end = -1; // do we need to add a Hit::Type::None to the end of the hit list?
	for (s32 i = 0; i < result->hits.length; i++)
	{
		const Hit& hit = result->hits[i];

		b8 stop = false;
		if (hit.type == Hit::Type::Shield)
		{
			// if we've already hit this shield once, we must ignore it
			b8 already_hit = false;
			for (s32 i = 0; i < hit_targets.length; i++)
			{
				if (hit_targets[i].equals(hit.entity))
				{
					already_hit = true;
					break;
				}
			}

			if (!already_hit)
			{
				if (!hit.entity.ref()->get<Health>()->can_take_damage()) // it's invincible; always bounce off
					stop = true;
				else if (hit.entity.ref()->get<Health>()->total() > impact_damage(this, hit.entity.ref()))
					stop = true; // it has health or shield to spare; we'll bounce off
			}
		}
		else if (hit.type == Hit::Type::Target)
		{
			// go through it, don't stop
		}
		else
			stop = true; // we can't go through it

		if (stop)
		{
			// stop raycast here
			result->index_end = i;
			break;
		}
	}

	if (result->index_end == -1)
	{
		Hit hit;
		hit.fraction = 1.0f;
		hit.entity = nullptr;
		hit.normal = Vec3::zero;
		hit.pos = ray_end;
		hit.type = Hit::Type::None;
		result->hits.add(hit);
		result->index_end = result->hits.length - 1;
	}
	else if (mode == RaycastMode::Default
		&& recursion_level < 5
		&& current_ability == Ability::Sniper)
	{
		const Hit& hit_end = result->hits[result->index_end];
		if (hit_end.type == Hit::Type::ForceField
			|| hit_end.type == Hit::Type::Shield
			|| hit_end.type == Hit::Type::Inaccessible)
		{
			// reflect off of it
			Vec3 dir = Vec3::normalize((ray_end - ray_start).reflect(hit_end.normal));

			Entity* ignore;
			if (hit_end.type == Hit::Type::Shield)
				ignore = hit_end.entity.ref();
			else
				ignore = nullptr;

			Hits hits2;
			raycast(mode, hit_end.pos + dir * DRONE_RADIUS, hit_end.pos + dir * DRONE_SNIPE_DISTANCE, state_frame, &hits2, recursion_level + 1, ignore);
			if (hits2.hits.length < result->hits.capacity() - result->hits.length)
			{
				// append hits2 to result->hits

				// remove any hits after index_end
				result->hits.length = result->index_end + 1;
				result->index_end = result->hits.length + hits2.index_end;

				for (s32 i = 0; i < hits2.hits.length; i++)
				{
					Hit hit = hits2.hits[i];
					hit.fraction += 1.0f;
					result->hits.add(hit);
				}
			}
		}
	}
}

void Drone::movement_raycast(const Vec3& ray_start, const Vec3& ray_end, Hits* hits_out, const Net::StateFrame* state_frame)
{
	State s = state();

	Net::StateFrame state_frame_data;
	if (!state_frame)
	{
		if (net_state_frame(&state_frame_data))
			state_frame = &state_frame_data;
	}

	Hits hits;
	raycast(RaycastMode::Default, ray_start, ray_end, state_frame, &hits);

	// handle collisions
	for (s32 i = 0; i < hits.index_end + 1; i++)
	{
		const Hit& hit = hits.hits[i];
		if (current_ability == Ability::Sniper && hit.type != Hit::Type::None)
			sniper_hit_effects(hit);

		if (hit.type == Hit::Type::Target)
			hit_target(hit.entity.ref());
		else if (hit.type == Hit::Type::Shield)
		{
			if (hit_target(hit.entity.ref())
				&& i == hits.index_end // make sure this is the hit we thought we would stop on
				&& s != State::Crawl) // make sure we're flying or dashing
				reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
		}
		else if (hit.type == Hit::Type::Inaccessible)
		{
			if (s == State::Fly)
				reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
		}
		else if (hit.type == Hit::Type::ForceField)
		{
			hit_target(hit.entity.ref());
			if (s == State::Fly)
				reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
		}
		else if (hit.type == Hit::Type::Environment)
		{
			vi_assert(i == hits.index_end); // no more hits should come after we hit an environment surface
			if (s == State::Fly)
			{
				// we hit a normal surface; attach to it

				Vec3 point = hit.pos;
				// check for obstacles
				{
					btCollisionWorld::ClosestRayResultCallback obstacle_ray_callback(hit.pos, hit.pos + hit.normal * (DRONE_RADIUS * 1.1f));
					Physics::raycast(&obstacle_ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
					if (obstacle_ray_callback.hasHit())
					{
						// push us away from the obstacle
						Vec3 obstacle_normal_flattened = obstacle_ray_callback.m_hitNormalWorld - hit.normal * hit.normal.dot(obstacle_ray_callback.m_hitNormalWorld);
						point += obstacle_normal_flattened * DRONE_RADIUS;
					}
				}

				get<Transform>()->parent = hit.entity.ref()->get<Transform>();
				get<Transform>()->absolute(point + hit.normal * DRONE_RADIUS, Quat::look(hit.normal));

				DroneNet::finish_flying(this);
			}
		}
	}

	if (hits_out)
		*hits_out = hits;
}

}
