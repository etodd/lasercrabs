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
#include "walker.h"

namespace VI
{

#define LERP_ROTATION_SPEED 10.0f
#define LERP_TRANSLATION_SPEED 4.0f
#define DRONE_LEG_LENGTH (0.277f - 0.101f)
#define DRONE_LEG_BLEND_SPEED (1.0f / 0.03f)
#define DRONE_MIN_LEG_BLEND_SPEED (DRONE_LEG_BLEND_SPEED * 0.1f)
#define DRONE_REFLECTION_TIME_TOLERANCE 0.1f
#define DRONE_REFLECTION_POSITION_TOLERANCE (DRONE_SHIELD_RADIUS * 10.0f)
#define DRONE_SHOTGUN_PELLETS 13

#define DEBUG_REFLECTIONS 0
#define DEBUG_NET_SYNC 0

Vec3 drone_shotgun_dirs[DRONE_SHOTGUN_PELLETS];
r32 drone_shotgun_offsets[DRONE_SHOTGUN_PELLETS];

void Drone::init()
{
	drone_shotgun_dirs[0] = Vec3(0, 0, 1);
	drone_shotgun_offsets[0] = DRONE_SHIELD_RADIUS * 1.5f;
	{
		Vec3 d = Quat::euler(0.0f, 0.0f, PI * -0.02f) * Vec3(0, 0, 1);
		for (s32 i = 0; i < 3; i++)
		{
			drone_shotgun_dirs[1 + i] = Quat::euler(r32(i) * (PI * 2.0f / 3.0f), 0.0f, 0.0f) * d;
			drone_shotgun_offsets[1 + i] = DRONE_SHIELD_RADIUS * 0.5f;
		}
	}

	{
		Vec3 d = Quat::euler(0.0f, 0.0f, PI * 0.05f) * Vec3(0, 0, 1);
		for (s32 i = 0; i < 9; i++)
		{
			drone_shotgun_dirs[4 + i] = Quat::euler(r32(i) * (PI * 2.0f / 9.0f), 0.0f, 0.0f) * d;
			drone_shotgun_offsets[4 + i] = 0.0f;
		}
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
	s32 particle_count = s32(distance / interval);
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
	if (type == DroneHitType::Reflection || (target && target->has<Shield>()))
	{
		Entity* audio_source = (target && target->has<Audio>() && drone->current_ability == Ability::Sniper) ? target : drone->entity();
		audio_source->get<Audio>()->post_unattached(AK::EVENTS::PLAY_DRONE_REFLECT);
	}

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
	Audio::post_global(AK::EVENTS::PLAY_SNIPER_IMPACT, hit.pos);
}

s32 impact_damage(const Drone* drone, const Entity* target_shield)
{
	if (target_shield->has<Drone>()
		&& (target_shield->get<Drone>()->state() != Drone::State::Crawl || target_shield->get<AIAgent>()->team == drone->get<AIAgent>()->team))
		return 0;

	Vec3 ray_dir;
	{
		r32 speed = drone->velocity.length();
		if (speed == 0.0f)
			return 1;
		else
			ray_dir = drone->velocity / speed;
	}

	Net::StateFrame state_frame;

	Quat target_rot;
	Vec3 target_pos;

	if (Game::net_transform_filter(target_shield, Game::level.mode)
		&& !PlayerHuman::players_on_same_client(drone->entity(), target_shield)
		&& drone->net_state_frame(&state_frame))
	{
		Vec3 pos;
		Quat rot;
		Net::transform_absolute(state_frame, target_shield->get<Transform>()->id(), &pos, &rot);
		target_pos = pos + (rot * target_shield->get<Target>()->local_offset); // todo possibly: rewind local_offset as well?
		target_rot = rot;
	}
	else
	{
		target_pos = target_shield->get<Target>()->absolute_pos();
		target_rot = target_shield->get<Transform>()->absolute_rot();
	}

	Vec3 me = drone->get<Transform>()->absolute_pos();

	s32 result = 1;

	Vec3 intersection;
	if (LMath::ray_sphere_intersect(me + ray_dir * -FORCE_FIELD_RADIUS, me + ray_dir * DRONE_SNIPE_DISTANCE, target_pos, target_shield->has<ForceField>() ? FORCE_FIELD_RADIUS : DRONE_SHIELD_RADIUS, &intersection))
	{
		Vec3 intersection_dir = Vec3::normalize(intersection - target_pos);
		r32 dot = intersection_dir.dot(ray_dir);

		if (target_shield->has<ForceField>())
		{
			if (drone->current_ability == Ability::Sniper)
				result = 8;
			else
				result = 5;
		}
		else if (drone->current_ability == Ability::Sniper)
		{
			// require more precision
			if (dot < -0.85f)
				result = 3;
			else if (dot < -0.75f)
				result = 2;

			if (target_shield->has<MinionSpawner>()
				|| target_shield->has<Rectifier>()
				|| target_shield->has<Turret>())
				result *= 2;
		}
		else
		{
			// flying hit; be more tolerant
			if (dot < -0.75f)
				result = 3;
			else
				result = 2;
		}

		if (target_shield->has<MinionSpawner>()
			|| target_shield->has<Rectifier>()
			|| target_shield->has<Turret>()
			|| target_shield->has<ForceField>())
			result *= 2;
	}
	else
	{
		// our velocity vector didn't actually intersect the target shield

		// but our shield may have, in which case it still does 1 damage

		// snipers however, must actually hit the shield
		if (drone->current_ability == Ability::Sniper)
			result = 0;
	}

	return result;
}

void drone_remove_fake_projectile(Drone* drone, EffectLight::Type type)
{
	for (s32 i = 0; i < drone->fake_projectiles.length; i++)
	{
		EffectLight* p = drone->fake_projectiles[i].ref();

		if (!p) // might have already been removed
		{
			drone->fake_projectiles.remove_ordered(i);
			i--;
			continue;
		}

		if (p->type == type)
			EffectLight::remove(p);
		drone->fake_projectiles.remove_ordered(i);
		break;
	}
}

void drone_bolt_spawn(Drone* drone, const Vec3& my_pos, const Vec3& dir_normalized, Bolt::Type type)
{
	if (Game::level.local)
	{
		PlayerManager* manager = drone->get<PlayerCommon>()->manager.ref();

		Entity* bolt_entity = World::create<BoltEntity>(manager->team.ref()->team(), manager, drone->entity(), type, my_pos, dir_normalized);

		if (manager->has<PlayerHuman>() && !manager->get<PlayerHuman>()->local())
		{
			// step 1. rewind the world to the point where the remote player fired
			// step 2. step forward in increments of 1/60th of a second until we reach the present,
			// checking for obstacles along the way.
			// step 3. spawn the bolt at the final position
			// step 4. if a target was hit, tell the bolt to register it

			r32 timestamp;
#if SERVER
			timestamp = Net::timestamp() - (vi_min(NET_MAX_RTT_COMPENSATION, drone->get<PlayerControlHuman>()->rtt) - Net::interpolation_delay(manager->get<PlayerHuman>())) * Game::session.effective_time_scale();
#else
			timestamp = Net::timestamp();
#endif

			Bolt* bolt = bolt_entity->get<Bolt>();

			Bolt::Hit hit = {};

			const r32 SIMULATION_STEP = Net::tick_rate() * Game::session.effective_time_scale();

			b8 rewound = false;
			Net::StateFrame state_frame;
			while (timestamp < Net::timestamp())
			{
				rewound = true;
				Net::state_frame_by_timestamp(&state_frame, timestamp);

				if (bolt->simulate(SIMULATION_STEP, &hit, &state_frame))
					break; // hit something

				timestamp += SIMULATION_STEP;
			}

			Net::finalize(bolt_entity);

			if (hit.entity) // we hit something, register it instantly
				bolt->hit_entity(hit, rewound ? &state_frame : nullptr);
		}
		else
		{
			// not a remote player; no lag compensation needed
			Net::finalize(bolt_entity);
		}
	}
	else
	{
		// we're a client; if this is a local player who has already spawned a fake bolt for client-side prediction,
		// we need to delete that fake bolt, since the server has spawned a real one.
		EffectLight::Type effect_light_type = type == Bolt::Type::DroneBolter ? EffectLight::Type::BoltDroneBolter : EffectLight::Type::BoltDroneShotgun;
		drone_remove_fake_projectile(drone, effect_light_type);
	}
}

void drone_sniper_effects(Drone* drone, const Vec3& dir_normalized, const Drone::Hits* hits = nullptr)
{
	Vec3 pos = drone->get<Transform>()->absolute_pos();

	ShellCasing::spawn(pos, Quat::look(dir_normalized), ShellCasing::Type::Sniper);
	drone->get<Audio>()->post(AK::EVENTS::PLAY_SNIPER_FIRE);
	EffectLight::add(pos + dir_normalized * DRONE_RADIUS * 2.0f, DRONE_RADIUS * 2.0f, 0.1f, EffectLight::Type::MuzzleFlash);

	drone->hit_targets.length = 0;
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
		Camera* closest_camera = nullptr;
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
				r32 t = (i.item()->pos - line_start).dot(line_normalized);
				if (t > 0.0f && t < line_length)
				{
					Vec3 pos = line_start + line_normalized * t;
					r32 distance = (pos - i.item()->pos).length_squared();
					if (distance < closest_distance)
					{
						closest_distance = distance;
						closest_camera = i.item();
					}
				}

				line_start = hit.pos;
			}
		}
		if (closest_camera)
		{
			Vec3 whiff_pos;
			Vec3 diff = pos - closest_camera->pos;
			if (diff.length_squared() > 0.0f)
				whiff_pos = closest_camera->pos + Vec3::normalize(diff) * closest_distance;
			else
				whiff_pos = pos;
			Audio::post_global(AK::EVENTS::PLAY_SNIPER_WHIFF, whiff_pos, nullptr, AudioEntry::FlagEnableObstructionOcclusion);
		}
	}

	// shatter glass
	for (s32 i = 0; i < hits->index_end; i++)
	{
		const Drone::Hit& hit = hits->hits[i];
		if (hit.type == Drone::Hit::Type::Glass && hit.entity.ref())
			hit.entity.ref()->get<Glass>()->shatter(hit.pos, ray_end - ray_start);
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
}

// velocity to give the grenade
Vec3 grenade_velocity_dir(const Vec3& dir_normalized)
{
	Vec3 dir_adjusted = dir_normalized;
	dir_adjusted.y += 0.2f;
	dir_adjusted.normalize();
	return dir_adjusted;
}

// direction to offset the grenade spawn position
Vec3 grenade_spawn_dir(const Vec3& dir_normalized)
{
	Vec3 dir_adjusted = dir_normalized;
	dir_adjusted.y += 0.5f;
	dir_adjusted.normalize();
	return dir_adjusted;
}

DroneCollisionState Drone::collision_state() const
{
	if (get<Health>()->active_armor_timer > 0.0f)
		return DroneCollisionState::ActiveArmor;
	else if (UpgradeStation::drone_inside(this))
		return DroneCollisionState::UpgradeStation; // invincible inside upgrade station
	else if (get<Drone>()->state() != Drone::State::Crawl)
		return DroneCollisionState::FlyingDashing; // only drone bolts can damage us while flying or dashing
	else
		return DroneCollisionState::Default; // anything can damage us
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
	b8 apply_msg;
	{
#if SERVER
		apply_msg = src == Net::MessageSource::Loopback;
#else
		b8 local = Game::level.local || (drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local());
		apply_msg = drone && (local == (src == Net::MessageSource::Loopback));
#endif
	}

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
				drone->dash_combo = false;
				drone->dash_timer = 0.0f;
				drone->velocity = dir * DRONE_FLY_SPEED;

				drone->detaching.fire();
				drone->get<Transform>()->absolute_pos(drone->get<Transform>()->absolute_pos() + dir * DRONE_RADIUS * 0.5f);
				drone->get<Transform>()->absolute_rot(Quat::look(dir));

				if (flag != DroneNet::FlyFlag::CancelExisting)
				{
					drone->get<Audio>()->post(AK::EVENTS::PLAY_DRONE_LAUNCH);
					drone->cooldown_recoil_setup(Ability::None);
				}

				drone->ensure_detached();
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

			if (apply_msg)
			{
				drone->velocity = dir * DRONE_DASH_SPEED;

				drone->dashing.fire();

				drone->dash_timer = dash_time;

				drone->attach_time = Game::time.total;

				for (s32 i = 0; i < DRONE_LEGS; i++)
					drone->footing[i].parent = nullptr;
				drone->get<Animator>()->reset_overrides();
				drone->get<Animator>()->layers[0].animation = Asset::Animation::drone_dash;

				if (drone->current_ability == Ability::None)
				{
					drone->get<Audio>()->post(AK::EVENTS::PLAY_DRONE_LAUNCH);
					drone->cooldown_recoil_setup(Ability::None);
				}
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

				drone->done_flying.fire();
			}
			break;
		}
		case DroneNet::Message::HitTarget:
		{
			Ref<Entity> target;
			serialize_ref(p, target);

			if (apply_msg)
			{
				client_hit_effects(drone, target.ref(), DroneHitType::Target);

				if (Game::level.local && target.ref()->has<Shield>())
				{
					// check if we can damage them
					Net::StateFrame state_frame_data;
					const Net::StateFrame* state_frame = nullptr;
					if (drone->net_state_frame(&state_frame_data))
						state_frame = &state_frame_data;

					if (target.ref()->get<Health>()->can_take_damage(drone->entity(), state_frame))
					{
						// we hurt them
						target.ref()->get<Health>()->damage(drone->entity(), impact_damage(drone, target.ref()), state_frame);
					}
					else
					{
						// we didn't hurt them
						// check if they had active armor on and so should damage us
						if (drone->state() != State::Crawl
							&& target.ref()->get<Health>()->active_armor(state_frame)
							&& target.ref()->has<AIAgent>()
							&& target.ref()->get<AIAgent>()->team != drone->get<AIAgent>()->team)
							drone->get<Health>()->damage_force(target.ref(), DRONE_HEALTH + Game::session.config.ruleset.drone_shield);
					}
				}
			}
			break;
		}
		case DroneNet::Message::AbilitySelect:
		{
			Ability ability;
			serialize_int(p, Ability, ability, 0, s32(Ability::count) + 1); // must be +1 for Ability::None
			if (apply_msg)
			{
				const AbilityInfo& info = AbilityInfo::list[s32(ability)];
				if (info.type != AbilityInfo::Type::Passive)
				{
					drone->get<Audio>()->post(info.equip_sound);
					drone->cooldown_ability_switch = info.cooldown_switch;
					drone->cooldown_ability_switch_last_local_change = Game::real_time.total;
					if (info.type != AbilityInfo::Type::Other)
						drone->current_ability = ability;
				}
			}
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

			const AbilityInfo& info = AbilityInfo::list[s32(ability)];

			Vec3 pos;
			Vec3 normal;
			RigidBody* parent;
			{
				Net::StateFrame state_frame_data;
				const Net::StateFrame* state_frame = nullptr;
				if (drone->net_state_frame(&state_frame_data))
					state_frame = &state_frame_data;
				if (!drone->can_spawn(ability, dir_normalized, state_frame, &pos, &normal, &parent))
					return true;
			}

			Quat rot = Quat::look(normal);

			if ((Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())) // only do cooldowns for remote drones or AI drones; local players will have already done this
				drone->cooldown_recoil_setup(ability);

			if (Game::level.local == (src == Net::MessageSource::Loopback))
				manager->ability_cooldown_apply(ability);

			Vec3 my_pos;
			Quat my_rot;
			drone->get<Transform>()->absolute(&my_pos, &my_rot);

			switch (ability)
			{
				case Ability::Rectifier:
				{
					// place a rectifier
					if (Game::level.local)
						ParticleEffect::spawn(ParticleEffect::Type::SpawnRectifier, pos + rot * Vec3(0, 0, RECTIFIER_RADIUS), rot, parent->get<Transform>(), manager);

					// effects
					particle_trail(my_pos, pos);

					break;
				}
				case Ability::MinionSpawner:
				{
					// place a minion spawner
					if (Game::level.local)
						ParticleEffect::spawn(ParticleEffect::Type::SpawnMinionSpawner, pos + rot * Vec3(0, 0, MINION_SPAWNER_RADIUS), rot, parent->get<Transform>(), manager);

					// effects
					particle_trail(my_pos, pos);

					break;
				}
				case Ability::Turret:
				{
					// place a turret
					if (Game::level.local)
						ParticleEffect::spawn(ParticleEffect::Type::SpawnTurret, pos + rot * Vec3(0, 0, TURRET_RADIUS), rot, parent->get<Transform>(), manager);

					// effects
					particle_trail(my_pos, pos);

					break;
				}
				case Ability::ForceField:
				{
					// spawn a force field
					if (Game::level.local)
						ParticleEffect::spawn(ParticleEffect::Type::SpawnForceField, pos + rot * Vec3(0, 0, FORCE_FIELD_BASE_OFFSET), rot, parent->get<Transform>(), manager);

					// effects
					particle_trail(my_pos, pos);

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
					if (Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
					{
						drone->get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_SPAWN);
						EffectLight::add(my_pos + dir_normalized * DRONE_RADIUS * 2.0f, DRONE_RADIUS * 1.5f, 0.1f, EffectLight::Type::MuzzleFlash);
					}

					if (Game::level.local)
					{
						PlayerManager* manager = drone->get<PlayerCommon>()->manager.ref();

						Entity* grenade_entity = World::create<GrenadeEntity>(manager, my_pos + grenade_spawn_dir(dir_normalized) * (DRONE_SHIELD_RADIUS + GRENADE_RADIUS + 0.01f), grenade_velocity_dir(dir_normalized));

						if (manager->has<PlayerHuman>() && !manager->get<PlayerHuman>()->local())
						{
							// step 1. rewind the world to the point where the remote player fired
							// step 2. step forward in increments of 1/60th of a second until we reach the present,
							// checking for obstacles along the way.
							// step 3. spawn the grenade at the final position
							// step 4. if a target was hit, tell the grenade to register it

							r32 timestamp;
#if SERVER
							timestamp = Net::timestamp() - (vi_min(NET_MAX_RTT_COMPENSATION, drone->get<PlayerControlHuman>()->rtt) - Net::interpolation_delay(manager->get<PlayerHuman>())) * Game::session.effective_time_scale();
#else
							timestamp = Net::timestamp();
#endif

							Grenade* grenade = grenade_entity->get<Grenade>();

							Bolt::Hit hit = {};

							const r32 SIMULATION_STEP = Net::tick_rate() * Game::session.effective_time_scale();

							b8 rewound = false;
							Net::StateFrame state_frame;
							while (timestamp < Net::timestamp())
							{
								rewound = true;
								Net::state_frame_by_timestamp(&state_frame, timestamp);

								if (grenade->simulate(SIMULATION_STEP, &hit, &state_frame))
									break; // hit something

								timestamp += SIMULATION_STEP;
							}

							Net::finalize(grenade_entity);

							if (hit.entity) // we hit something, register it instantly
								grenade->hit_entity(hit, rewound ? &state_frame : nullptr);
						}
						else
						{
							// not a remote player; no lag compensation needed
							Net::finalize(grenade_entity);
						}
					}
					else // client
						drone_remove_fake_projectile(drone, EffectLight::Type::Grenade);
					break;
				}
				case Ability::Bolter:
				{
					drone_bolt_spawn(drone, my_pos, dir_normalized, Bolt::Type::DroneBolter);
					if (Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
					{
						drone->get<Audio>()->post(AK::EVENTS::PLAY_BOLT_SPAWN);
						EffectLight::add(my_pos + dir_normalized * DRONE_RADIUS * 2.0f, DRONE_RADIUS * 1.5f, 0.1f, EffectLight::Type::MuzzleFlash);
						ShellCasing::spawn(drone->get<Transform>()->absolute_pos(), Quat::look(dir_normalized), ShellCasing::Type::Bolter);
					}
					break;
				}
				case Ability::Shotgun:
				{
					Quat target_quat = Quat::look(dir_normalized);
					for (s32 i = 0; i < DRONE_SHOTGUN_PELLETS; i++)
					{
						Vec3 d = target_quat * drone_shotgun_dirs[i];
						drone_bolt_spawn(drone, my_pos + d * drone_shotgun_offsets[i], d, Bolt::Type::DroneShotgun);
					}
					if (Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local())
					{
						drone->get<Audio>()->post(AK::EVENTS::PLAY_DRONE_SHOTGUN_FIRE);
						Vec3 my_pos = drone->get<Transform>()->absolute_pos();
						ShellCasing::spawn(my_pos, Quat::look(dir_normalized), ShellCasing::Type::Shotgun);
						EffectLight::add(my_pos + dir_normalized * DRONE_RADIUS * 3.0f, DRONE_RADIUS * 3.0f, 0.1f, EffectLight::Type::MuzzleFlash);
						drone->dash_start(-dir_normalized, my_pos, DRONE_DASH_TIME * 0.25f); // HACK: set target to current position so it is not used
					}
					break;
				}
				case Ability::ActiveArmor:
				{
					if ((Game::level.local || !drone->has<PlayerControlHuman>() || !drone->get<PlayerControlHuman>()->local()))
					{
						drone->get<Health>()->active_armor_timer = vi_max(drone->get<Health>()->active_armor_timer, ACTIVE_ARMOR_TIME);
						drone->get<Audio>()->post(AK::EVENTS::PLAY_DRONE_ACTIVE_ARMOR);
					}
					break;
				}
				default:
					vi_assert(false);
					break;
			}

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

	get<Audio>()->post(AK::EVENTS::PLAY_DRONE_LAND);
	attach_time = Game::time.total;
	hit_targets.length = 0;
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
	last_ability_fired(),
	dash_combo(),
	attach_time(Game::time.total),
	footing(),
	last_footstep(),
	reflecting(),
	hit_targets(),
	cooldown(),
	cooldown_last_local_change(),
	current_ability(Ability::None),
	fake_projectiles(),
	ability_spawned(),
	dash_target(),
	reflections(),
	flag(),
	lerped_rotation(),
	lerped_pos(),
	last_pos(),
	hit()
{
}

Drone::~Drone()
{
	if (Game::level.local && flag.ref())
	{
		flag.ref()->drop();
		flag = nullptr;
	}
}

void Drone::awake()
{
	get<Animator>()->layers[0].behavior = Animator::Behavior::Loop;
	link_arg<Entity*, &Drone::killed>(get<Health>()->killed);
	get<Transform>()->absolute(&lerped_pos, &lerped_rotation);
	update_offset();
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
	for (s32 i = 0; i < hit_targets.length; i++)
	{
		if (hit_targets[i].ref() == target)
			return false; // we've already hit this target once during this flight
	}
	hit_targets.add(target);

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
	PlayerHuman::notification(get<Transform>()->absolute_pos(), get<AIAgent>()->team, PlayerHuman::Notification::Type::DroneDestroyed);

	PlayerManager::entity_killed_by(entity(), e);

	get<Audio>()->post(AK::EVENTS::PLAY_DRONE_DAMAGE_EXPLODE);

	if (Game::level.local)
	{
		Vec3 pos;
		Quat rot;
		get<Transform>()->absolute(&pos, &rot);
		ParticleEffect::spawn(ParticleEffect::Type::DroneExplosion, pos, rot);
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
		timestamp = Net::timestamp() - (vi_min(NET_MAX_RTT_COMPENSATION, get<PlayerControlHuman>()->rtt) - Net::interpolation_delay(get<PlayerControlHuman>()->player.ref())) * Game::session.effective_time_scale();
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
						&& LMath::ray_sphere_intersect(trace_start, trace_end, intersection, DRONE_SHIELD_RADIUS * 2.0f))
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

Vec3 target_position(Entity* me, const Net::StateFrame* state_frame, Target* target)
{
	// do rewinding, unless we're checking collisions between two players on the same client
	if (state_frame
		&& Game::net_transform_filter(target->entity(), Game::level.mode)
		&& !PlayerHuman::players_on_same_client(me, target->entity()))
	{
		Vec3 pos;
		Quat rot;
		Net::transform_absolute(*state_frame, target->get<Transform>()->id(), &pos, &rot);
		return pos + (rot * target->local_offset); // todo possibly: rewind local_offset as well?
	}
	else
		return target->absolute_pos();
}

b8 Drone::can_spawn(Ability a, const Vec3& dir, const Net::StateFrame* state_frame, Vec3* final_pos, Vec3* final_normal, RigidBody** hit_parent, b8* hit_target) const
{
	const AbilityInfo& info = AbilityInfo::list[s32(a)];

	vi_assert(info.type != AbilityInfo::Type::Passive);

	if (info.type == AbilityInfo::Type::Other)
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

	struct RayHit
	{
		Entity* entity;
		Vec3 pos;
		Vec3 normal;
		b8 hit;
	};

	RayHit ray_callback;

	{
		RaycastCallbackExcept physics_ray_callback(trace_start, trace_end, entity());
		s16 force_field_mask = info.type == AbilityInfo::Type::Build
			? ~ally_force_field_mask() // only ignore friendly force fields; we don't want to build something on a force field
			: ~CollisionGlass & ~CollisionAllTeamsForceField; // ignore all force fields
		Physics::raycast(&physics_ray_callback, ~CollisionDroneIgnore & force_field_mask);

		ray_callback.hit = physics_ray_callback.hasHit();
		if (ray_callback.hit)
		{
			ray_callback.pos = physics_ray_callback.m_hitPointWorld;
			ray_callback.entity = &Entity::list[physics_ray_callback.m_collisionObject->getUserIndex()];
			ray_callback.normal = physics_ray_callback.m_hitNormalWorld;
		}
	}

	// check targets
	for (auto i = Target::list.iterator(); !i.is_last(); i.next())
	{
		if (should_collide(i.item(), state_frame)
			|| i.item()->has<Minion>()) // raycast against friendly minions so we can easily spawn minions next to each other
		{
			{
				// check actual position
				Vec3 target_pos = target_position(entity(), state_frame, i.item());
				Vec3 intersection;
				if (LMath::ray_sphere_intersect(trace_start, trace_end, target_pos, i.item()->radius(), &intersection)
					&& (intersection - trace_start).length_squared() < (ray_callback.pos - trace_start).length_squared())
				{
					ray_callback.hit = true;
					ray_callback.pos = intersection;
					Vec3 diff = intersection - target_pos;
					r32 length = diff.length();
					if (length > 0.0f)
						ray_callback.normal = diff / length;
					else
						ray_callback.normal = Vec3(0, 1, 0);
					ray_callback.entity = i.item()->entity();
				}
			}

			if (info.type == AbilityInfo::Type::Shoot)
			{
				// check predicted intersection
				Vec3 target_pos;
				if (predict_intersection(i.item()->get<Target>(), nullptr, &target_pos, target_prediction_speed()))
				{
					Vec3 intersection;
					if (LMath::ray_sphere_intersect(trace_start, trace_end, target_pos, i.item()->radius(), &intersection)
						&& (intersection - trace_start).length_squared() < (ray_callback.pos - trace_start).length_squared())
					{
						ray_callback.hit = true;
						ray_callback.pos = intersection;
						Vec3 diff = intersection - target_pos;
						r32 length = diff.length();
						if (length > 0.0f)
							ray_callback.normal = diff / length;
						else
							ray_callback.normal = Vec3(0, 1, 0);
						ray_callback.entity = i.item()->entity();
					}
				}
			}
		}
	}

	if (ray_callback.hit)
	{
		if (final_pos)
			*final_pos = ray_callback.pos;
		if (final_normal)
			*final_normal = ray_callback.normal;
		if (hit_target)
			*hit_target = ray_callback.entity->has<Target>();
		if (hit_parent)
			*hit_parent = ray_callback.entity->has<RigidBody>() ? ray_callback.entity->get<RigidBody>() : nullptr;
	}
	else
	{
		if (final_pos)
			*final_pos = trace_end;
		if (final_normal)
			*final_normal = Vec3::zero;
		if (hit_target)
			*hit_target = false;
		if (hit_parent)
			*hit_parent = nullptr;
	}

	if (info.type == AbilityInfo::Type::Shoot)
		return true; // we can always spawn these abilities, even if we're aiming into space
	else
	{
		// build-type ability
		if (ray_callback.hit)
		{
			{
				// check if this thing we're building will intersect with an invincible force field
				r32 radius = (a == Ability::ForceField) ? FORCE_FIELD_RADIUS : 0.0f;
				if (!ForceField::can_spawn(get<AIAgent>()->team, ray_callback.pos, radius))
					return false;
			}

			if (ray_callback.entity->has<Minion>())
				return !ray_callback.entity->get<Minion>()->carrying.ref(); // minion can only carry one thing at a time
			else if (ray_callback.entity->has<Target>()
				|| (ray_callback.entity->get<RigidBody>()->collision_group & (DRONE_INACCESSIBLE_MASK | CollisionGlass)))
				return false;
			else
			{
				// make sure there's enough room
				Vec3 space_check_pos = ray_callback.pos;
				Vec3 space_check_dir = ray_callback.normal;
				r32 required_space;
				switch (a)
				{
					case Ability::ForceField:
						required_space = FORCE_FIELD_BASE_OFFSET;
						break;
					case Ability::Rectifier:
						required_space = DRONE_SHIELD_RADIUS;
						break;
					case Ability::MinionSpawner:
						required_space = 0.5f + WALKER_HEIGHT + WALKER_SUPPORT_HEIGHT + WALKER_MINION_RADIUS * 2.0f;
						break;
					case Ability::Turret:
						required_space = TURRET_RADIUS;
						break;
					default:
					{
						required_space = 0.0f;
						vi_assert(false);
						break;
					}
				}

				space_check_pos += space_check_dir * 0.01f;

				Vec3 ray_hit = ray_callback.pos + ray_callback.normal * 0.01f;
				if ((space_check_pos - ray_hit).length_squared() > 0.01f * 0.01f)
				{
					RaycastCallbackExcept physics_ray_callback(ray_hit, space_check_pos, entity());
					Physics::raycast(&physics_ray_callback, ~CollisionDroneIgnore & ~ally_force_field_mask());
					if (physics_ray_callback.hasHit())
						return false; // something in the way
				}

				{
					RaycastCallbackExcept physics_ray_callback(space_check_pos, space_check_pos + space_check_dir * required_space, entity());
					Physics::raycast(&physics_ray_callback, ~CollisionDroneIgnore & ~ally_force_field_mask());
					if (physics_ray_callback.hasHit())
						return false; // not enough space
				}
				return true;
			}
		}
		else // nowhere to build
			return false;
	}
}

void Drone::ability(Ability a)
{
	if (a != current_ability)
		DroneNet::ability_select(this, a);
}

void Drone::cooldown_recoil_setup(Ability a)
{
	const AbilityInfo& info = AbilityInfo::list[s32(a)];
	cooldown = vi_min(cooldown + info.cooldown_movement, DRONE_COOLDOWN_MAX);
	cooldown_last_local_change = Game::real_time.total;
	last_ability_fired = Game::time.total;
	get<PlayerCommon>()->recoil_add(info.recoil_velocity);
}

// if the property in question is remote controlled, adjustment is the amount that should be subtracted due to network lag
b8 is_remote_controlled(const Drone* drone, r32 last_local_change, r32* adjustment)
{
	if (Game::level.local)
	{
		if (adjustment)
			*adjustment = 0.0f;
		return false;
	}
	else
	{
		if (drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local())
		{
			// cooldown is remote controlled unless it changed recently
			// this facilitates client-side prediction
			PlayerHuman* player = drone->get<PlayerControlHuman>()->player.ref();
			r32 rtt = Net::rtt(player);
			if (Game::real_time.total - last_local_change > (rtt + Net::interpolation_delay(player)) + Net::tick_rate() * 2.0f)
			{
				if (adjustment)
					*adjustment = rtt * DRONE_COOLDOWN_SPEED * Game::session.effective_time_scale();
				return true;
			}
			else
			{
				if (adjustment)
					*adjustment = 0.0f;
				return false;
			}
		}
		else
		{
			// permanently remote controlled drone
			if (adjustment)
				*adjustment = 0.0f;
			return true;
		}
	}
}

// if the cooldown is remote controlled, adjustment is the amount that should be subtracted due to network lag
b8 Drone::cooldown_remote_controlled(r32* adjustment) const
{
	return is_remote_controlled(this, cooldown_last_local_change, adjustment);
}

// if the cooldown is remote controlled, adjustment is the amount that should be subtracted due to network lag
b8 Drone::cooldown_ability_switch_remote_controlled(r32* adjustment) const
{
	return is_remote_controlled(this, cooldown_ability_switch_last_local_change, adjustment);
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

b8 Drone::dash_start(const Vec3& dir, const Vec3& target, r32 max_time)
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
	else if (state() == State::Fly)
		return false;

	Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
	Vec3 dir_normalized = Vec3::normalize(dir);
	Vec3 pos = get<Transform>()->absolute_pos();

	// determine how long we'll be dashing, and whether it will be a combo
	r32 time = 0.0f;
	b8 combo = false;

	Vec3 dir_flattened = dir_normalized - wall_normal * wall_normal.dot(dir_normalized);

	// check for obstacles
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(pos, pos + dir_flattened * DRONE_DASH_SPEED * max_time);
		Physics::raycast(&ray_callback, ~DRONE_PERMEABLE_MASK & ~ally_force_field_mask());
		if (ray_callback.hasHit())
			max_time = ray_callback.m_closestHitFraction * max_time;
	}
	Vec3 next_pos = pos;
	const r32 time_increment = Net::tick_rate() * 0.5f;
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
		if ((target - pos).length_squared() > 0.0f
			&& could_shoot(next_pos, target_dir, &final_pos, &final_normal)
			&& (final_pos - target).dot(target_dir) > DRONE_RADIUS * -2.0f
			&& (next_pos - final_pos).dot(final_normal) > DRONE_RADIUS * 2.0f)
		{
			combo = true;
			break;
		}

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
		time = vi_max(time, DRONE_DASH_TIME / r32(1 << 16));
		dash_combo = combo;
		dash_target = target;
		DroneNet::start_dashing(this, dir_normalized, time);
	}

	return true;
}

b8 Drone::cooldown_can_shoot() const
{
	r32 epsilon;
#if SERVER
	epsilon = DRONE_REFLECTION_TIME_TOLERANCE;
#else
	epsilon = 0.0f;
#endif
	return cooldown < DRONE_COOLDOWN_THRESHOLD + epsilon
		&& cooldown_ability_switch <= epsilon;
}

r32 Drone::target_prediction_speed() const
{
	switch (current_ability)
	{
		case Ability::Sniper:
			return 0.0f;
		case Ability::Bolter:
			return BOLT_SPEED_DRONE_BOLTER;
		case Ability::Shotgun:
			return BOLT_SPEED_DRONE_SHOTGUN;
		default:
			return DRONE_FLY_SPEED;
	}
}

r32 Drone::range() const
{
	return current_ability == Ability::Sniper ? DRONE_SNIPE_DISTANCE : DRONE_MAX_DISTANCE;
}

b8 Drone::go(const Vec3& dir)
{
	Ability a = current_ability;

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
		if (!get<PlayerCommon>()->manager.ref()->ability_valid(a)
			|| (a == Ability::Bolter && !bolter_can_fire()))
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
		else
		{
			// client-side prediction
			cooldown_recoil_setup(a);

			if (a == Ability::Shotgun)
			{
				// create fake bolts
				get<Audio>()->post(AK::EVENTS::PLAY_DRONE_SHOTGUN_FIRE);
				Quat target_quat = Quat::look(dir_normalized);
				Vec3 my_pos = get<Transform>()->absolute_pos();
				ShellCasing::spawn(my_pos, target_quat, ShellCasing::Type::Shotgun);
				EffectLight::add(my_pos + dir_normalized * DRONE_RADIUS * 3.0f, DRONE_RADIUS * 3.0f, 0.1f, EffectLight::Type::MuzzleFlash);
				for (s32 i = 0; i < DRONE_SHOTGUN_PELLETS; i++)
				{
					Vec3 d = target_quat * drone_shotgun_dirs[i];
					fake_projectiles.add
					(
						EffectLight::add
						(
							my_pos,
							BOLT_LIGHT_RADIUS,
							0.5f,
							EffectLight::Type::BoltDroneShotgun,
							nullptr,
							Quat::look(d)
						)
					);
				}
				dash_start(-dir_normalized, get<Transform>()->absolute_pos(), DRONE_DASH_TIME * 0.25f); // HACK: set target to current position so it is not used
			}
			else if (a == Ability::Bolter)
			{
				// create fake bolt
				get<Audio>()->post(AK::EVENTS::PLAY_BOLT_SPAWN);
				Vec3 my_pos = get<Transform>()->absolute_pos();
				EffectLight::add(my_pos + dir_normalized * DRONE_RADIUS * 2.0f, DRONE_RADIUS * 1.5f, 0.1f, EffectLight::Type::MuzzleFlash);
				ShellCasing::spawn(my_pos, Quat::look(dir_normalized), ShellCasing::Type::Bolter);
				fake_projectiles.add
				(
					EffectLight::add
					(
						my_pos,
						BOLT_LIGHT_RADIUS,
						0.5f,
						EffectLight::Type::BoltDroneBolter,
						nullptr,
						Quat::look(dir_normalized)
					)
				);
			}
			else if (a == Ability::Grenade)
			{
				// create fake grenade
				get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_SPAWN);
				Vec3 my_pos = get<Transform>()->absolute_pos();
				Vec3 dir_spawn = grenade_spawn_dir(dir_normalized);

				EffectLight::add(my_pos + dir_spawn * DRONE_RADIUS * 2.0f, DRONE_RADIUS * 1.5f, 0.1f, EffectLight::Type::MuzzleFlash);
				fake_projectiles.add
				(
					EffectLight::add
					(
						my_pos + dir_spawn * (DRONE_SHIELD_RADIUS + GRENADE_RADIUS + 0.01f),
						BOLT_LIGHT_RADIUS,
						0.5f,
						EffectLight::Type::Grenade,
						nullptr,
						Quat::look(grenade_velocity_dir(dir_normalized))
					)
				);
			}
			else if (a == Ability::Sniper)
				drone_sniper_effects(this, dir_normalized);
			else if (a == Ability::ActiveArmor)
			{
				get<Health>()->active_armor_timer = ACTIVE_ARMOR_TIME; // show invincibility sparkles instantly
				get<Audio>()->post(AK::EVENTS::PLAY_DRONE_ACTIVE_ARMOR);
			}
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
	d->ensure_detached();
	d->get<Transform>()->absolute_pos(reflection.pos);
	d->attach_time = Game::time.total;
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

	d->get<Transform>()->rot = Quat::look(reflection.dir);
	d->velocity = e.new_velocity;

	d->reflections.remove_ordered(0);
}

void drone_environment_raycast(const Drone* drone, RaycastCallbackExcept* ray_callback, Drone::RaycastMode mode)
{
	s16 mask;
	switch (mode)
	{
		case Drone::RaycastMode::Default:
			mask = (CollisionStatic | CollisionAllTeamsForceField) & ~drone->ally_force_field_mask();
			break;
		case Drone::RaycastMode::IgnoreForceFields:
			mask = CollisionStatic;
			break;
		default:
		{
			mask = 0;
			vi_assert(false);
			break;
		}
	}
	Physics::raycast(ray_callback, mask);
}

void drone_dash_fly_simulate(Drone* d, r32 dt, Net::StateFrame* state_frame = nullptr)
{
	Drone::State s = d->state();

	Vec3 position = d->get<Transform>()->absolute_pos();

	if (s == Drone::State::Fly)
	{
		// we might enter an invalid state for various reasons while in flight
		// in that case we should try and find a new direction to fly
		b8 find_new_dir = false;

		if (btVector3(d->velocity).fuzzyZero())
		{
			// we're flying but have no velocity
			// we could be waiting for a reflection to be confirmed...
			if (d->reflections.length == 0)
				find_new_dir = true; // apparently not. we're dead in the water.
		}
		else
		{
			// we're moving, but we might be heading somewhere we don't want to go
			RaycastCallbackExcept ray_callback(position, position + Vec3::normalize(d->velocity) * DRONE_SNIPE_DISTANCE, d->entity());
			drone_environment_raycast(d, &ray_callback, Drone::RaycastMode::IgnoreForceFields);
			if (!ray_callback.hasHit() // heading toward empty space
				|| (ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & CollisionInaccessible)) // heading toward inaccessible surface
				find_new_dir = true;
		}

		if (find_new_dir)
		{
			b8 found = false;
			for (s32 i = 0; i < REFLECTION_TRIES; i++)
			{
				Vec3 candidate_dir = Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 1);
				if (d->can_shoot(candidate_dir, nullptr, nullptr, state_frame))
				{
					d->velocity = candidate_dir * DRONE_FLY_SPEED;
					found = true;
					break;
				}
			}

			if (!found) // couldn't find a new direction; kill self
			{
				d->get<Health>()->kill(d->entity());
				return;
			}
		}
	}

	Vec3 next_position;
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

	r32 timestamp = Net::timestamp() - amount * Game::session.effective_time_scale();
	const r32 SIMULATION_STEP = Net::tick_rate() * Game::session.effective_time_scale();
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

		new_dir = -Vec3::normalize(velocity);

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
				random_range = vi_min(PI, random_range + (1.5f * PI / r32(REFLECTION_TRIES)));
			}
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

void Drone::stealth(Entity* e, b8 stealthing)
{
	if (stealthing)
	{
		e->get<SkinnedModel>()->alpha();
		e->get<SkinnedModel>()->color.w = 0.7f;
		e->get<SkinnedModel>()->mask = 1 << s32(e->get<AIAgent>()->team); // only display to fellow teammates
	}
	else
	{
		if (!e->has<Drone>() || e->get<Drone>()->state() == State::Crawl)
			e->get<SkinnedModel>()->alpha_if_obstructing();
		else
			e->get<SkinnedModel>()->alpha_disable();
		e->get<SkinnedModel>()->color.w = MATERIAL_NO_OVERRIDE;
		e->get<SkinnedModel>()->mask = RENDER_MASK_DEFAULT; // display to everyone
	}
}

void Drone::update_server(const Update& u)
{
	State s = state();

	if (!cooldown_remote_controlled())
	{
		cooldown = vi_max(0.0f, cooldown - u.time.delta * DRONE_COOLDOWN_SPEED);
		cooldown_ability_switch = vi_max(0.0f, cooldown_ability_switch - u.time.delta * DRONE_COOLDOWN_SPEED);
	}

	if (s != Drone::State::Crawl)
		drone_dash_fly_simulate(this, u.time.delta);

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

			// fast forward whatever amount of time we've been sitting here waiting on this reflection
			drone_fast_forward(this, fast_forward);
		}
		else // we detected the hit locally, but the client never acknowledged it. ignore the reflection and keep going straight.
		{
#if DEBUG_REFLECTIONS
			vi_debug("%f remote never confirmed local reflection, continuing as normal", Game::real_time.total);
#endif
			reflections.remove_ordered(0);
			if (state() == State::Dash)
			{
				// can't restore original velocity
				// i mean i could but i'd rather not add the complexity of saving it in another member variable
				// instead just stop the dash
				velocity = Vec3::zero;
				dash_timer = 0.0f;
				DroneNet::finish_dashing(this);
			}
			else
			{
				// restore original velocity
				velocity = get<Transform>()->absolute_rot() * Vec3(0, 0, DRONE_FLY_SPEED);
				// fast forward whatever amount of time we've been sitting here waiting on this reflection
				drone_fast_forward(this, fast_forward);
			}
		}
	}
#else
	// client
	vi_assert(reflections.length == 0);
#endif
}

r32 Drone::particle_accumulator;
void Drone::update_all(const Update& u)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (Game::level.local || (i.item()->has<PlayerControlHuman>() && i.item()->get<PlayerControlHuman>()->local()))
			i.item()->update_server(u);
		i.item()->update_client(u);
	}

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
		if (i.item()->state() == State::Crawl)
			i.item()->velocity = i.item()->velocity * 0.9f + ((pos - i.item()->last_pos) / vi_max(0.0001f, u.time.delta)) * 0.1f;
		i.item()->last_pos = pos;
	}
}

b8 Drone::bolter_can_fire() const
{
	r32 interval;
#if SERVER
	if (has<PlayerControlHuman>())
		interval = BOLTER_INTERVAL * 0.5f; // server-side forgiveness
	else
#endif
	{
		interval = BOLTER_INTERVAL;
	}

	return Game::time.total - last_ability_fired > interval;
}

Vec3 Drone::rotation_clamp() const
{
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
				if (i.item()->local() && i.item()->get<PlayerManager>()->team.ref()->team() != my_team)
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
						get<Audio>()->post(AK::EVENTS::PLAY_DRONE_FOOTSTEP);
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

		if (get<Animator>()->layers[0].animation == AssetNull)
		{
			// this means that we were crawling, but were interrupted.
#if DEBUG_NET_SYNC
			vi_debug_break();
#endif
			ensure_detached();
		}

		get<Transform>()->absolute(&lerped_pos, &lerped_rotation);
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

b8 Drone::should_collide(const Target* target, const Net::StateFrame* state_frame) const
{
	if (target == get<Target>())
		return false; // don't collide with self
	else if (target->has<Drone>())
	{
		DroneCollisionState target_collision_state;
		if (state_frame
			&& state_frame->drones[target->get<Drone>()->id()].active
			&& !PlayerHuman::players_on_same_client(entity(), target->entity()))
			target_collision_state = state_frame->drones[target->get<Drone>()->id()].collision_state;
		else
			target_collision_state = target->get<Drone>()->collision_state();

		return target_collision_state != DroneCollisionState::UpgradeStation;
	}
	else
	{
		AI::Team my_team = get<AIAgent>()->team;
		return (!target->has<Rectifier>() || target->get<Rectifier>()->team != my_team) // ignore friendly rectifiers
			&& (!target->has<ForceField>() || target->get<ForceField>()->team != my_team) // ignore friendly force fields
			&& (!target->has<Minion>() || target->get<AIAgent>()->team != my_team) // ignore friendly minions
			&& (!target->has<Grenade>() || target->get<Grenade>()->team != my_team || current_ability == Ability::Sniper); // ignore friendly grenades unless we're sniping them
	}
}

void Drone::raycast(RaycastMode mode, const Vec3& ray_start, const Vec3& ray_end, const Net::StateFrame* state_frame, Hits* result, s32 recursion_level, Entity* ignore) const
{
	r32 distance_total = (ray_end - ray_start).length();

	// check environment
	{
		RaycastCallbackExcept ray_callback(ray_start, ray_end, entity());
		drone_environment_raycast(this, &ray_callback, mode);

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

			Entity* entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			if (entity->has<ForceFieldCollision>())
				entity = entity->get<ForceFieldCollision>()->field.ref()->entity();

			Hit hit =
			{
				ray_callback.m_hitPointWorld,
				ray_callback.m_hitNormalWorld,
				(ray_callback.m_hitPointWorld - ray_start).length() / distance_total,
				entity,
				type,
			};
			result->hits.add(hit);
		}
	}

	// check targets
	{
		AI::Team my_team = get<AIAgent>()->team;
		for (auto i = Target::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->entity() == ignore // don't collide with ignored entity
				|| !should_collide(i.item(), state_frame))
				continue;

			Vec3 p = target_position(entity(), state_frame, i.item());

			r32 target_radius = i.item()->radius();
			r32 raycast_radius = (current_ability == Ability::None && i.item()->has<Shield>()) ? DRONE_SHIELD_RADIUS : 0.0f;
			Vec3 intersection;
			if (LMath::ray_sphere_intersect(ray_start, ray_end, p, target_radius + raycast_radius, &intersection))
			{
				Vec3 normal = Vec3::normalize(intersection - p);
				intersection = p + normal * target_radius;

				// check if the intersection point is actually accessible or if it's inside the environment
				b8 hit;

				if (LMath::ray_sphere_intersect(ray_start, ray_end, p, target_radius + 0.09f, &intersection))
				{
					hit = true; // the center of our drone actually hit the enemy drone

					// update intersection point
					normal = Vec3::normalize(intersection - p);
					intersection = p + normal * target_radius;
				}
				else
				{
					// it's a shield-shield collision
					// so make sure the intersection point is not inside the environment geometry
					RaycastCallbackExcept ray_callback(ray_start, intersection, entity());
					drone_environment_raycast(this, &ray_callback, mode);
					hit = !ray_callback.hasHit();
				}

				if (hit)
				{
					result->hits.add(
					{
						intersection,
						normal,
						(intersection - ray_start).length() / distance_total,
						i.item()->entity(),
						i.item()->has<Shield>() ? Hit::Type::Shield : Hit::Type::Target,
					});
				}
			}
		}
	}

	// glass
	{
		btCollisionWorld::AllHitsRayResultCallback ray_callback(ray_start, ray_end);
		Physics::raycast(&ray_callback, CollisionGlass);
		for (s32 i = 0; i < ray_callback.m_collisionObjects.size(); i++)
		{
			result->hits.add(
			{
				ray_callback.m_hitPointWorld[i],
				ray_callback.m_hitNormalWorld[i],
				(ray_callback.m_hitPointWorld[i] - ray_start).length() / distance_total,
				&Entity::list[ray_callback.m_collisionObjects[i]->getUserIndex()],
				Hit::Type::Glass,
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
		if (hit.type == Hit::Type::Shield || hit.type == Hit::Type::ForceField)
		{
			b8 already_hit = false;

			if (hit.type == Hit::Type::Shield)
			{
				// if we've already hit this shield once, we must ignore it
				for (s32 i = 0; i < hit_targets.length; i++)
				{
					if (hit_targets[i].equals(hit.entity))
					{
						already_hit = true;
						break;
					}
				}
			}

			if (!already_hit)
			{
				Entity* e = hit.entity.ref();
				if (e->has<Health>())
				{
					const Health* health = e->get<Health>();
					if (!health->can_take_damage(entity(), state_frame)) // it's invincible; always bounce off
						stop = true;
					else if (s32(health->total()) > impact_damage(this, e))
						stop = true; // it has health or shield to spare; we'll bounce off
				}
				else
					stop = true;
			}
		}
		else if (hit.type == Hit::Type::Target || hit.type == Hit::Type::Glass)
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
			if (s == State::Fly && i == hits.index_end)
				reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
		}
		else if (hit.type == Hit::Type::Glass)
			hit.entity.ref()->get<Glass>()->shatter(hit.pos, ray_end - ray_start);
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
