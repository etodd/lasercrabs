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
#define LERP_TRANSLATION_SPEED 3.0f
#define MAX_FLIGHT_TIME 4.0f
#define DRONE_LEG_LENGTH (0.277f - 0.101f)
#define DRONE_LEG_BLEND_SPEED (1.0f / 0.03f)
#define DRONE_MIN_LEG_BLEND_SPEED (DRONE_LEG_BLEND_SPEED * 0.1f)
#define DRONE_REFLECTION_TIME_TOLERANCE 0.1f

const r32 Drone::cooldown_thresholds[DRONE_CHARGES] = { DRONE_COOLDOWN * 0.4f, DRONE_COOLDOWN * 0.1f, 0.0f, };

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

void particle_trail(const Vec3& start, const Vec3& dir, r32 distance, r32 interval = 2.0f)
{
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
			vi_min(0.25f, i * 0.05f)
		);
	}
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
		drone->get<Audio>()->post_event(drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_DRONE_REFLECT_PLAYER : AK::EVENTS::PLAY_DRONE_REFLECT);

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

b8 players_on_same_client(const Entity* a, const Entity* b)
{
#if SERVER
	return a->has<PlayerControlHuman>()
		&& b->has<PlayerControlHuman>()
		&& Net::Server::client_id(a->get<PlayerControlHuman>()->player.ref()) == Net::Server::client_id(b->get<PlayerControlHuman>()->player.ref());
#else
	return true;
#endif
}

s32 impact_damage(const Drone* drone, const Entity* target_drone)
{
	Net::StateFrame state_frame;

	Vec3 target_pos;

	if (!players_on_same_client(drone->entity(), target_drone) && drone->net_state_frame(&state_frame))
	{
		Vec3 pos;
		Quat rot;
		Net::transform_absolute(state_frame, target_drone->get<Transform>()->id(), &pos, &rot);
		target_pos = pos + (rot * target_drone->get<Target>()->local_offset); // todo possibly: rewind local_offset as well?
	}
	else
		target_pos = target_drone->get<Target>()->absolute_pos();

	Vec3 ray_start = drone->get<Transform>()->absolute_pos();
	Vec3 ray_dir = Vec3::normalize(drone->velocity);

	Vec3 intersection;
	if (LMath::ray_sphere_intersect(ray_start, ray_start + ray_dir * DRONE_MAX_DISTANCE, target_pos, DRONE_SHIELD_RADIUS, &intersection))
	{
		r32 dot = Vec3::normalize(intersection - target_pos).dot(ray_dir);
		if (dot < -0.95f)
			return 3;
		else if (dot < -0.75f)
			return 2;
	}
	return 1;
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

			if (apply_msg)
			{
				if (drone->charges > 0 || flag == DroneNet::FlyFlag::CancelExisting)
				{
					drone->dash_combo = false;
					drone->dash_timer = 0.0f;
					drone->velocity = dir * DRONE_FLY_SPEED;
					drone->detaching.fire();
					drone->get<Transform>()->absolute_pos(drone->get<Transform>()->absolute_pos() + dir * DRONE_RADIUS * 0.5f);
					drone->get<Transform>()->absolute_rot(Quat::look(dir));

					{
						b8 local = drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local();
						drone->get<Audio>()->post_event(local ? AK::EVENTS::PLAY_DRONE_LAUNCH_PLAYER : AK::EVENTS::PLAY_DRONE_LAUNCH);
						drone->get<Audio>()->post_event(local ? AK::EVENTS::PLAY_DRONE_FLY_PLAYER : AK::EVENTS::PLAY_DRONE_FLY);
					}

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
				drone->remote_reflection_timer = 0.0f;

				drone->attach_time = Game::time.total;
				drone->cooldown_setup();

				for (s32 i = 0; i < DRONE_LEGS; i++)
					drone->footing[i].parent = nullptr;
				drone->get<Animator>()->reset_overrides();
				drone->get<Animator>()->layers[0].animation = Asset::Animation::drone_dash;

				drone->particle_accumulator = 0;

				{
					b8 local = drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local();
					drone->get<Audio>()->post_event(local ? AK::EVENTS::PLAY_DRONE_LAUNCH_PLAYER : AK::EVENTS::PLAY_DRONE_LAUNCH);
					drone->get<Audio>()->post_event(local ? AK::EVENTS::PLAY_DRONE_FLY_PLAYER : AK::EVENTS::PLAY_DRONE_FLY);
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
				client_hit_effects(drone, target.ref(), DroneHitType::Target);

			// damage messages

			if (target.ref()->has<Shield>())
			{
				// check if we can damage them
				if (target.ref()->get<Health>()->invincible() || (target.ref()->has<Drone>() && target.ref()->get<Drone>()->state() != State::Crawl && drone->state() != State::Crawl))
				{
					// we didn't hurt them
					if (Game::level.local)
					{
						if (target.ref()->get<Health>()->invincible() && target.ref()->get<Health>()->invincible_timer <= ACTIVE_ARMOR_TIME) // they were invincible; they should damage us
							drone->get<Health>()->damage(target.ref(), DRONE_HEALTH + DRONE_SHIELD);
					}

					if (drone->has<PlayerControlHuman>())
						drone->get<PlayerControlHuman>()->player.ref()->msg(_(strings::no_effect), false);
				}
				else
				{
					// we hurt them
					if (Game::level.local) // if we're a client, this has already been handled by the server
						target.ref()->get<Health>()->damage(drone->entity(), impact_damage(drone, target.ref()));

					if (drone->has<PlayerControlHuman>() && drone->state() != State::Crawl)
					{
						// if the target got killed, we'll get a notification about the energy reward
						// but if it was only damaged, we need to let the player know
						if (target.ref()->get<Health>()->hp > 0) // target is still around
							drone->get<PlayerControlHuman>()->player.ref()->msg(_(strings::target_damaged), true);
					}
				}
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

			if (!info.rapid_fire)
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
					particle_trail(my_pos, dir_normalized, (pos - my_pos).length());
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
					particle_trail(my_pos, dir_normalized, (pos - my_pos).length());
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
					particle_trail(my_pos, dir_normalized, (pos - my_pos).length());
					EffectLight::add(npos, 8.0f, 1.5f, EffectLight::Type::Shockwave);

					break;
				}
				case Ability::Sniper:
				{
					drone->get<Audio>()->post_event(AK::EVENTS::PLAY_SNIPER_FIRE);

					drone->hit_targets.length = 0;

					Vec3 pos = drone->get<Transform>()->absolute_pos();
					Vec3 ray_start = pos + dir_normalized * -DRONE_RADIUS;
					Vec3 ray_end = pos + dir_normalized * drone->range();
					drone->velocity = dir_normalized * DRONE_FLY_SPEED;
					r32 distance;
					if (Game::level.local)
						distance = drone->movement_raycast(ray_start, ray_end);
					else
					{
						Hits hits;
						drone->raycast(RaycastMode::Default, ray_start, ray_end, nullptr, &hits);
						distance = hits.fraction_end * drone->range();
						if (hits.index_end != -1)
							sniper_hit_effects(hits.hits[hits.index_end]);
					}

					// whiff sfx
					r32 closest_projected_camera_distance = distance;
					for (auto i = Camera::list.iterator(); !i.is_last(); i.next())
					{
						r32 projected_camera_distance = (i.item()->pos - ray_start).dot(dir_normalized);
						if (projected_camera_distance > 0.0f && projected_camera_distance < closest_projected_camera_distance)
							closest_projected_camera_distance = projected_camera_distance;
					}
					if (closest_projected_camera_distance < distance)
						Audio::post_global_event(AK::EVENTS::PLAY_SNIPER_WHIFF, ray_start + dir_normalized * closest_projected_camera_distance);

					// effects
					particle_trail(ray_start, dir_normalized, distance);

					// everyone instantly knows where we are
					AI::Team team = drone->get<AIAgent>()->team;
					for (auto i = Team::list.iterator(); !i.is_last(); i.next())
					{
						if (i.item()->team() != team)
							i.item()->track(drone->get<PlayerCommon>()->manager.ref(), drone->entity());
					}

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
					if (Game::level.local)
					{
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
								Vec3 pos_bolt_next = pos_bolt + dir_normalized * (BOLT_SPEED_DRONE * SIMULATION_STEP);
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

							Entity* bolt = World::create<BoltEntity>(manager->team.ref()->team(), manager, drone->entity(), Bolt::Type::Drone, pos_bolt, dir_normalized);
							Net::finalize(bolt);
							if (closest_hit_entity) // we hit something, register it instantly
								bolt->get<Bolt>()->hit_entity(closest_hit_entity, closest_hit, closest_hit_normal);
						}
						else
						{
							// not a remote player; no lag compensation needed
							Net::finalize(World::create<BoltEntity>(manager->team.ref()->team(), manager, drone->entity(), Bolt::Type::Drone, my_pos + dir_normalized * DRONE_SHIELD_RADIUS, dir_normalized));
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
					drone->bolter_charge_counter++;
					if (drone->bolter_charge_counter >= BOLTS_PER_DRONE_CHARGE)
					{
						drone->bolter_charge_counter = 0;
						drone->cooldown_setup();
					}
					break;
				}
				case Ability::ActiveArmor:
				{
					if (drone)
					{
						drone->get<Health>()->invincible_timer = vi_max(drone->get<Health>()->invincible_timer, ACTIVE_ARMOR_TIME);
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

			if (!info.rapid_fire)
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

	get<Audio>()->post_event(has<PlayerControlHuman>() && get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_DRONE_LAND_PLAYER : AK::EVENTS::PLAY_DRONE_LAND);
	get<Audio>()->post_event(AK::EVENTS::STOP_DRONE_FLY);
	attach_time = Game::time.total;
	dash_timer = 0.0f;
	dash_combo = false;

	remote_reflection_timer = 0.0f;

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
	remote_reflection_timer(),
	reflection_source_remote(),
	remote_reflection_entity(),
	bolter_charge_counter()
{
}

void Drone::awake()
{
	get<Animator>()->layers[0].behavior = Animator::Behavior::Loop;
	link_arg<Entity*, &Drone::killed>(get<Health>()->killed);
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
	if (hit_targets.length < hit_targets.capacity())
		hit_targets.add(target);

	if (current_ability == Ability::None && get<Health>()->invincible() && target->has<Shield>())
		get<Health>()->invincible_timer = 0.0f; // damaging a Shield cancels our invincibility

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
	// killed; notify everyone
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
	const Hit* environment_hit = nullptr;
	b8 allow_further_end = false; // allow drone to shoot if we're aiming at an enemy drone in range but the backing behind it is out of range
	b8 hit_target_value = false;
	for (s32 i = 0; i < hits.hits.length; i++)
	{
		const Hit& hit = hits.hits[i];
		if (hit.type == Hit::Type::Environment || hit.type == Hit::Type::ForceField)
			environment_hit = &hit;
		if (hit.fraction * DRONE_SNIPE_DISTANCE < r)
		{
			if (hit.type == Hit::Type::Shield)
			{
				allow_further_end = true;
				if (hit.fraction <= hits.fraction_end)
					hit_target_value = true;
			}
			else if (hit.type == Hit::Type::Target && hit.fraction <= hits.fraction_end)
				hit_target_value = true;
		}
	}

	if (environment_hit)
	{
		// need to check that the environment hit is within range
		// however if we are shooting at an Drone, we can tolerate further environment hits
		b8 can_shoot = false;
		if (allow_further_end || environment_hit->fraction * DRONE_SNIPE_DISTANCE < r)
			can_shoot = true;
		else
		{
			// check drone target predictions
			r32 end_distance_sq = vi_min(r * r, hits.fraction_end * DRONE_SNIPE_DISTANCE * hits.fraction_end * DRONE_SNIPE_DISTANCE);
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
							can_shoot = true;
							hit_target_value = true;
							break;
						}
					}
				}
			}
		}

		if (can_shoot)
		{
			if (final_pos)
				*final_pos = hits.hits[hits.index_end].pos;
			if (final_normal)
				*final_normal = hits.hits[hits.index_end].normal;
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

	RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
	Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionAllTeamsForceField);
	if (AbilityInfo::list[s32(a)].type == AbilityInfo::Type::Shoot)
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
		b8 can_spawn = ray_callback.hasHit()
			&& !(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK);
		if (can_spawn)
		{
			if (final_pos)
				*final_pos = ray_callback.m_hitPointWorld;
			if (final_normal)
				*final_normal = ray_callback.m_hitNormalWorld;
			if (hit_parent)
				*hit_parent = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
			if (hit_target)
				*hit_target = false;
		}
		return can_spawn;
	}
}

void Drone::ability(Ability a)
{
	if (a != current_ability)
		DroneNet::ability_select(this, a);
}

void Drone::cooldown_setup()
{
	if (Game::level.local)
	{
		vi_assert(charges > 0);
		charges--;
	}

#if SERVER
	if (has<PlayerControlHuman>())
		cooldown = DRONE_COOLDOWN - get<PlayerControlHuman>()->rtt;
	else
#endif
		cooldown = DRONE_COOLDOWN;
}

void Drone::ensure_detached()
{
	hit_targets.length = 0;
	remote_reflection_timer = 0.0f;

	if (get<Transform>()->parent.ref())
	{
		attach_time = Game::time.total;
		get<Transform>()->reparent(nullptr);
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
			return false;
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
				return false;
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
			return BOLT_SPEED_DRONE;
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
		return false;

	Vec3 dir_normalized = Vec3::normalize(dir);

	if (a == Ability::None)
	{
		{
			Net::StateFrame* state_frame = nullptr;
			Net::StateFrame state_frame_data;
			if (net_state_frame(&state_frame_data))
				state_frame = &state_frame_data;
			if (!can_shoot(dir, nullptr, nullptr, state_frame))
				return false;
		}
		DroneNet::start_flying(this, dir_normalized, DroneNet::FlyFlag::None);
	}
	else
	{
		if (!get<PlayerCommon>()->manager.ref()->ability_valid(a))
			return false;

		if (!can_spawn(a, dir_normalized))
			return false;

		if (Game::level.local)
			DroneNet::ability_spawn(this, dir_normalized, a);
		else if (a == Ability::ActiveArmor)
			get<Health>()->invincible_timer = ACTIVE_ARMOR_TIME; // client-side prediction; show invincibility sparkles instantly
		else if (a == Ability::Bolter)
		{
			// client-side prediction; create fake bolt
			if (fake_bolts.length == fake_bolts.capacity())
			{
				EffectLight* bolt = fake_bolts[0].ref();
				if (bolt) // might have already been removed
					EffectLight::remove(bolt);
				fake_bolts.remove_ordered(0);
			}
			fake_bolts.add
			(
				EffectLight::add
				(
					get<Transform>()->absolute_pos() + dir_normalized * DRONE_SHIELD_RADIUS,
					BOLT_LIGHT_RADIUS,
					0.5f,
					EffectLight::Type::Bolt,
					nullptr,
					Quat::look(dir_normalized)
				)
			);
		}
	}

	return true;
}

#define REFLECTION_TRIES 32 // try x raycasts. if they all fail, just shoot off into space.

void drone_reflection_execute(Drone* a, Entity* reflected_off, const Vec3& dir)
{
	{
		r32 l = dir.length_squared();
		vi_assert(l > 0.98f * 0.98f && l < 1.02f * 1.02f);
	}
	a->get<Transform>()->reparent(nullptr);
	a->dash_timer = 0.0f;
	a->dash_combo = false;
	a->get<Animator>()->layers[0].animation = Asset::Animation::drone_fly;

	if (!reflected_off || !reflected_off->has<Target>()) // target hit effects are handled separately
	{
		if (Game::level.local)
			DroneNet::reflection_effects(a, reflected_off); // let everyone know we're doing reflection effects
		else
		{
			// client-side prediction
			vi_assert(a->has<PlayerControlHuman>() && a->get<PlayerControlHuman>()->local());
			client_hit_effects(a, reflected_off, DroneHitType::Reflection);
		}
	}

	DroneReflectEvent e;
	e.entity = reflected_off;
	e.new_velocity = dir * DRONE_FLY_SPEED;
	a->reflecting.fire(e);
	a->get<Transform>()->rot = Quat::look(Vec3::normalize(e.new_velocity));
	a->velocity = e.new_velocity;
	a->remote_reflection_timer = 0.0f;
}

void Drone::reflect(Entity* entity, const Vec3& hit, const Vec3& normal, const Net::StateFrame* state_frame)
{
	vi_assert(velocity.length_squared() > 0.0f);

	// it's possible to reflect off a shield while we are dashing (still parented to an object)
	// so we need to make sure we're not dashing anymore
	Vec3 reflection_pos = hit + normal * DRONE_RADIUS * 0.5f;
	get<Transform>()->parent = nullptr;
	get<Transform>()->absolute_pos(reflection_pos);

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

		// if we're bouncing off a target (a drone most likely), we can bounce backward through the target.
		// otherwise, if it's a force field or something, we don't want to bounce through it
		b8 allow_reflect_against_normal = entity->has<Target>();

		for (s32 i = 0; i < REFLECTION_TRIES; i++)
		{
			Vec3 candidate_dir = target_quat * (Quat::euler(PI + (mersenne::randf_co() - 0.5f) * random_range, (PI * 0.5f) + (mersenne::randf_co() - 0.5f) * random_range, 0) * Vec3(1, 0, 0));
			if (allow_reflect_against_normal || candidate_dir.dot(normal) > 0.0f)
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

	if (state_frame)
	{
		// this drone is being controlled by a remote
		if (remote_reflection_timer > 0.0f)
		{
			// the remote already told us about the reflection
			// so go the direction they told us to
			get<Transform>()->absolute_pos(remote_reflection_pos);
			drone_reflection_execute(this, entity, remote_reflection_dir);
		}
		else
		{
			// store our reflection result and wait for the remote to tell us which way to go
			// if we don't hear from them in a certain amount of time, forget anything happened
			remote_reflection_dir = velocity; // HACK: not normalized. this will be restored to the velocity variable if the client does not acknowledge the hit
			remote_reflection_pos = reflection_pos;
			remote_reflection_timer = DRONE_REFLECTION_TIME_TOLERANCE;
			remote_reflection_entity = entity;
			reflection_source_remote = false; // this hit was detected locally
			velocity = Vec3::zero;
		}
	}
	else
	{
		// locally controlled; bounce instantly
		drone_reflection_execute(this, entity, new_dir);
		if (!Game::level.local && has<PlayerControlHuman>())
			remote_reflection_timer = Net::rtt(get<PlayerControlHuman>()->player.ref()) + NET_INTERPOLATION_DELAY + DRONE_REFLECTION_TIME_TOLERANCE;
	}
}

void Drone::handle_remote_reflection(Entity* entity, const Vec3& reflection_pos, const Vec3& reflection_dir)
{
	if (reflection_dir.length() == 0.0f)
		return;

	vi_assert(Game::level.local);

	Vec3 reflection_dir_normalized = Vec3::normalize(reflection_dir);

	// we're a server; the client is notifying us that it did a reflection

	// check if they're roughly where we think they should be
	if ((get<Transform>()->absolute_pos() - reflection_pos).length() < DRONE_SHIELD_RADIUS * 6.0f)
	{
		if (remote_reflection_timer == 0.0f)
		{
			// we haven't reflected off anything on the server yet; save this info and wait for us to hit something
			remote_reflection_dir = reflection_dir_normalized;
			remote_reflection_pos = reflection_pos;
			remote_reflection_timer = DRONE_REFLECTION_TIME_TOLERANCE;
			remote_reflection_entity = entity;
			reflection_source_remote = true; // this hit came from the remote
		}
		else
		{
			// we HAVE already detected a reflection off something; let's do it now
			r32 original_timer = remote_reflection_timer;

			get<Transform>()->absolute_pos(reflection_pos);
			drone_reflection_execute(this, remote_reflection_entity.ref(), reflection_dir_normalized);

			// fast forward the amount of time we've been sitting here waiting for the client to acknowledge the reflection
			Vec3 dir = velocity * (DRONE_REFLECTION_TIME_TOLERANCE - original_timer);
			Vec3 old_pos = get<Transform>()->absolute_pos();
			Vec3 new_pos = old_pos + dir;
			get<Transform>()->absolute_pos(new_pos);
			movement_raycast(old_pos + Vec3::normalize(dir) * -DRONE_RADIUS, new_pos);
		}
	}
	else
	{
		// client is not where we think they should be. they might be hacking. don't reflect
		vi_debug("%s", "Remote attempted to force an invalid drone reflection. Ignoring.");
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
						charges++;
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

		Vec3 position = get<Transform>()->absolute_pos();
		Vec3 next_position;

		if (btVector3(velocity).fuzzyZero()) // HACK. why does this happen
			velocity = get<Transform>()->absolute_rot() * Vec3(0, 0, s == State::Dash ? DRONE_DASH_SPEED : DRONE_FLY_SPEED);

		if (s == State::Dash)
		{
			crawl(velocity, vi_min(dash_timer, u.time.delta));
			next_position = get<Transform>()->absolute_pos();
			dash_timer = vi_max(0.0f, dash_timer - u.time.delta);
			if (dash_timer == 0.0f)
			{
				if (dash_combo)
					DroneNet::start_flying(this, dash_target - next_position, DroneNet::FlyFlag::CancelExisting);
				else
					DroneNet::finish_dashing(this);
				return;
			}
		}
		else
		{
			next_position = position + velocity * u.time.delta;
			get<Transform>()->absolute_pos(next_position);
		}

		{
			Vec3 dir = Vec3::normalize(velocity);
			Vec3 ray_start = position + dir * -DRONE_RADIUS;
			Vec3 ray_end = next_position + dir * DRONE_RADIUS;
			movement_raycast(ray_start, ray_end);
		}
	}

#if SERVER
	if (remote_reflection_timer > 0.0f)
	{
		if (s == Drone::State::Crawl)
			remote_reflection_timer = 0.0f; // cancel
		else
		{
			remote_reflection_timer = vi_max(0.0f, remote_reflection_timer - u.time.delta);
			if (remote_reflection_timer == 0.0f)
			{
				// time's up, we have to do something
				vi_assert(remote_reflection_dir.length_squared() > 0.0f);
				if (reflection_source_remote)
				{
					// the remote told us about this reflection. go ahead and do it even though we never detected the hit locally
					get<Transform>()->absolute_pos(remote_reflection_pos);
					drone_reflection_execute(this, remote_reflection_entity.ref(), remote_reflection_dir);
				}
				else
				{
					// we detected the hit locally, but the client never acknowledged it. ignore the reflection and keep going straight.
					velocity = remote_reflection_dir; // restore original velocity
				}
				Vec3 position = get<Transform>()->absolute_pos();
				Vec3 next_position = position + velocity * DRONE_REFLECTION_TIME_TOLERANCE;
				get<Transform>()->absolute_pos(next_position);
				Vec3 dir = Vec3::normalize(velocity);
				Vec3 ray_start = position + dir * -DRONE_RADIUS;
				Vec3 ray_end = next_position + dir * DRONE_RADIUS;
				movement_raycast(ray_start, ray_end);
			}
		}
	}
#else
	remote_reflection_timer = vi_max(0.0f, remote_reflection_timer - u.time.delta);
#endif
}

r32 Drone::particle_accumulator;
void Drone::update_client_all(const Update& u)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
		i.item()->update_client(u);

	const r32 particle_interval = 0.05f;
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
				r32 particle_start_delay = DRONE_THIRD_PERSON_OFFSET / i.item()->velocity.length();
				if (u.time.total - particle_accumulator > i.item()->attach_time + particle_start_delay)
				{
					Particles::tracers.add
					(
						Vec3::lerp((particle_accumulator - particle_start_delay) / vi_max(0.0001f, u.time.delta), i.item()->last_pos, i.item()->lerped_pos),
						Vec3::zero,
						0
					);
				}
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

void Drone::update_client(const Update& u)
{
	State s = state();

	if (s == Drone::State::Crawl)
	{
		// crawling

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
						get<Audio>()->post_event(has<PlayerControlHuman>() && get<PlayerControlHuman>()->local() ? AK::EVENTS::PLAY_DRONE_FOOTSTEP_PLAYER : AK::EVENTS::PLAY_DRONE_FOOTSTEP);
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

void Drone::raycast(RaycastMode mode, const Vec3& ray_start, const Vec3& ray_end, const Net::StateFrame* state_frame, Hits* result) const
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
			|| (i.item()->has<Drone>() && UpgradeStation::drone_inside(i.item()->get<Drone>()))) // ignore drones inside upgrade stations
			continue;

		Vec3 p;
		// do rewinding, unless we're checking collisions between two players on the same client
		if (state_frame && !players_on_same_client(entity(), i.item()->entity()))
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

	// determine which collision is the one we stop at
	result->fraction_end = 1.0f;
	result->index_end = -1;
	for (s32 i = 0; i < result->hits.length; i++)
	{
		const Hit& hit = result->hits[i];
		if (hit.fraction < result->fraction_end)
		{
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
					if ((hit.entity.ref()->has<Drone>() && hit.entity.ref()->get<Drone>()->state() != Drone::State::Crawl && state() != State::Crawl) // it's flying or dashing; always bounce off
						|| hit.entity.ref()->get<Health>()->invincible()) // it's invincible; always bounce off
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
				result->fraction_end = hit.fraction;
				result->index_end = i;
			}
		}
	}
}

r32 Drone::movement_raycast(const Vec3& ray_start, const Vec3& ray_end)
{
	State s = state();

	const Net::StateFrame* state_frame = nullptr;
	Net::StateFrame state_frame_data;
	if (net_state_frame(&state_frame_data))
		state_frame = &state_frame_data;

	Hits hits;
	raycast(RaycastMode::Default, ray_start, ray_end, state_frame, &hits);

	// handle collisions
	for (s32 i = 0; i < hits.hits.length; i++)
	{
		const Hit& hit = hits.hits[i];
		if (i == hits.index_end || hit.fraction < hits.fraction_end)
		{
			if (current_ability == Ability::Sniper)
				sniper_hit_effects(hit);

			if (hit.type == Hit::Type::Target)
				hit_target(hit.entity.ref());
			else if (hit.type == Hit::Type::Shield)
			{
				b8 do_reflect;
				if (!Game::level.local && has<PlayerControlHuman>() && get<PlayerControlHuman>()->local())
				{
					// client-side prediction
					do_reflect = hit_target(hit.entity.ref())
						&& (hit.entity.ref()->get<Health>()->total() > impact_damage(this, hit.entity.ref()) // will they still be alive after we hit them? if so, reflect
							|| ((hit.entity.ref()->has<Drone>() && hit.entity.ref()->get<Drone>()->state() != State::Crawl && s != State::Crawl) || hit.entity.ref()->get<Health>()->invincible()));
				}
				else
				{
					// server
					do_reflect = hit_target(hit.entity.ref()) // go through them if we've already hit them once on this flight
						&& hit.entity.ref()
						&& hit.entity.ref()->get<Health>()->total() > 0; // if we didn't destroy them, then bounce off
				}
				if (do_reflect
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
				if (s == State::Fly)
				{
					if (hit_target(hit.entity.ref()))
						reflect(hit.entity.ref(), hit.pos, hit.normal, state_frame);
				}
				else if (current_ability == Ability::Sniper)
					hit_target(hit.entity.ref());
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
	}

	return hits.fraction_end * (ray_end - ray_start).length();
}

}
