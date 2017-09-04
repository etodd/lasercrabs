#include "entities.h"
#include "data/animator.h"
#include "data/components.h"
#include "render/skinned_model.h"
#include "walker.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "asset/mesh.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "mersenne/mersenne-twister.h"
#include "game.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "render/views.h"
#include "drone.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "menu.h"
#include "data/ragdoll.h"
#include "usernames.h"
#include "console.h"
#include "minion.h"
#include "render/particles.h"
#include "strings.h"
#include "data/priority_queue.h"
#include "net.h"
#include "team.h"
#include "parkour.h"
#include "overworld.h"
#include "common.h"
#include "player.h"
#include "load.h"
#include "ease.h"

namespace VI
{


DroneEntity::DroneEntity(AI::Team team, const Vec3& pos)
{
	create<Audio>();
	create<Transform>()->pos = pos;
	create<Drone>();
	create<AIAgent>()->team = team;
	create<Health>(DRONE_HEALTH, DRONE_HEALTH, Game::session.config.drone_shield, Game::session.config.drone_shield)->active_armor_timer = DRONE_INVINCIBLE_TIME;
	create<Shield>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::drone;
	model->shader = Asset::Shader::armature;
	model->team = s8(team);
	model->alpha_if_obstructing();

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::drone;

	create<Target>();
}

Health::Health(s8 hp, s8 hp_max, s8 shield, s8 shield_max)
	: hp(hp),
	hp_max(hp_max),
	shield(shield),
	shield_max(shield_max),
	changed(),
	killed(),
	regen_timer()
{
}

void spawn_sparks(const Vec3& pos, const Quat& rot, Transform* parent)
{
	for (s32 i = 0; i < 15; i++)
	{
		Particles::sparks.add
		(
			pos,
			rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
			Vec4(1, 1, 1, 1)
		);
	}

	EffectLight::add(pos, 3.0f, 0.25f, EffectLight::Type::Spark, parent);
}

template<typename Stream> b8 serialize_health_event(Stream* p, Health* h, HealthEvent* e)
{
	serialize_ref(p, e->source);
	if (h->hp_max > 0)
		serialize_int(p, s8, e->hp, -h->hp_max, h->hp_max);
	else if (Stream::IsReading)
		e->hp = 0;
	if (h->shield_max > 0)
		serialize_int(p, s8, e->shield, -h->shield_max, h->shield_max);
	else if (Stream::IsReading)
		e->shield = 0;
	return true;
}

b8 Health::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Ref<Health> ref;
	serialize_ref(p, ref);

	HealthEvent e;
	if (!serialize_health_event(p, ref.ref(), &e))
		net_error();

	Health* h = ref.ref();
	h->hp += e.hp;
	h->shield += e.shield;
	h->changed.fire(e);
	if (e.hp < 0 && h->hp == 0)
		h->killed.fire(e.source.ref());

	return true;
}

// only called on server
b8 health_send_event(Health* h, HealthEvent* e)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::Health);

	Ref<Health> ref = h;
	serialize_ref(p, ref);

	if (!serialize_health_event(p, ref.ref(), e))
		net_error();

	Net::msg_finalize(p);

	return true;
}

void health_internal_apply_damage(Health* h, Entity* e, s8 damage)
{
	vi_assert(Game::level.local);

	s8 shield_value = h->shield;

	if (h->has<Drone>() && h->get<Drone>()->current_ability == Ability::Sniper) // shield is down while sniping
		shield_value = 0;

	s8 damage_accumulator = damage;
	s8 damage_shield;
	if (damage_accumulator > shield_value)
	{
		damage_shield = shield_value;
		damage_accumulator -= shield_value;
	}
	else
	{
		damage_shield = damage_accumulator;
		damage_accumulator = 0;
	}

	s8 damage_hp;
	if (damage_accumulator > h->hp)
		damage_hp = h->hp;
	else
		damage_hp = damage_accumulator;

	if (damage_hp != 0 || damage_shield != 0)
	{
		h->regen_timer = SHIELD_REGEN_TIME + SHIELD_REGEN_DELAY;

		HealthEvent ev =
		{
			e,
			s8(-damage_hp),
			s8(-damage_shield),
		};
		health_send_event(h, &ev);
	}
}

void Health::update_server(const Update& u)
{
	if (shield < shield_max)
	{
		r32 old_timer = regen_timer;
		regen_timer -= u.time.delta;
		if (regen_timer < SHIELD_REGEN_TIME)
		{
			const r32 regen_interval = SHIELD_REGEN_TIME / r32(shield_max);
			if (s32(old_timer / regen_interval) != s32(regen_timer / regen_interval))
			{
				HealthEvent e =
				{
					nullptr,
					0,
					1,
				};
				health_send_event(this, &e);
			}
		}
	}

	for (s32 i = 0; i < damage_buffer.length; i++)
	{
		BufferedDamage* entry = &damage_buffer[i];
		entry->delay -= u.time.delta;
		if (entry->delay < 0.0f) // IT'S TIME
		{
			Entity* src = entry->source.ref();
			if (src)
			{
				if (src->has<Bolt>())
				{
					if (src->get<Bolt>()->can_damage(entity()))
					{
						health_internal_apply_damage(this, src, entry->damage);
						World::remove_deferred(src);
					}
					else
						src->get<Bolt>()->reflect(entity());
				}
				else if (can_take_damage())
					health_internal_apply_damage(this, src, entry->damage);
				else if (active_armor() && src->has<Drone>() && entry->type != BufferedDamage::Type::Sniper) // kill 'em
					src->get<Health>()->kill(entity());
			}
			damage_buffer.remove(i);
			i--;
		}
	}
}

void Health::update_client(const Update& u)
{
	active_armor_timer = vi_max(0.0f, active_armor_timer - u.time.delta);
}

void Shield::awake()
{
	if (Game::level.local && !inner.ref() && get<Health>()->shield_max > 0)
	{
		AI::Team team = has<CoreModule>() ? get<CoreModule>()->team : get<AIAgent>()->team;

		{
			Entity* shield_entity = World::create<Empty>();
			shield_entity->get<Transform>()->parent = get<Transform>();
			inner = shield_entity;

			View* s = shield_entity->add<View>();
			s->team = s8(team);
			s->mesh = Asset::Mesh::sphere_highres;
			s->offset.scale(Vec3::zero);
			s->shader = Asset::Shader::fresnel;
			s->alpha();
			s->color.w = 0.0f;

			Net::finalize_child(shield_entity);
		}

		{
			// overshield
			vi_assert(!outer.ref());
			Entity* shield_entity = World::create<Empty>();
			shield_entity->get<Transform>()->parent = get<Transform>();
			outer = shield_entity;

			View* s = shield_entity->add<View>();
			s->team = s8(team);
			s->mesh = Asset::Mesh::sphere_highres;
			s->offset.scale(Vec3::zero);
			s->shader = Asset::Shader::fresnel;
			s->alpha();
			s->color.w = 0.0f;

			Net::finalize_child(shield_entity);
		}
	}

	link_arg<const HealthEvent&, &Shield::health_changed>(get<Health>()->changed);
}

// not synced over network
void Shield::set_team(AI::Team team)
{
	if (inner.ref())
	{
		inner.ref()->get<View>()->team = s8(team);
		outer.ref()->get<View>()->team = s8(team);
	}
}

Shield::~Shield()
{
	if (Game::level.local && inner.ref())
	{
		World::remove_deferred(inner.ref());
		World::remove_deferred(outer.ref());
	}
}

void Shield::update_client_all(const Update& u)
{
	static r32 particle_accumulator = 0.0f;
	static r32 particle_interval = 0.05f;
	particle_accumulator += u.time.delta;

	s32 particles = s32(particle_accumulator / particle_interval);
	if (particles > 0)
	{
		particle_accumulator -= particle_interval * particles;
		particle_interval = 0.01f + mersenne::randf_cc() * 0.005f;

		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<Health>()->active_armor())
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				for (s32 j = 0; j < particles; j++)
				{
					s32 cluster = 1 + s32(mersenne::randf_co() * 3.0f);
					Vec3 cluster_center = pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, DRONE_SHIELD_RADIUS);
					for (s32 k = 0; k < cluster; k++)
					{
						Particles::sparkles.add
						(
							cluster_center + Vec3((mersenne::randf_co() - 0.5f) * 0.2f, (mersenne::randf_co() - 0.5f) * 0.2f, (mersenne::randf_co() - 0.5f) * 0.2f),
							Vec3::zero,
							mersenne::randf_co() * PI * 2.0f
						);
					}
				}
			}
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
		i.item()->update_client(u);
}

void Shield::health_changed(const HealthEvent& e)
{
	if (e.shield < 0)
	{
		AkUniqueID event_id;
		if (e.shield < -1)
			event_id = AK::EVENTS::PLAY_DRONE_DAMAGE_LARGE;
		else
			event_id = AK::EVENTS::PLAY_DRONE_DAMAGE_SMALL;

		Entity* src = e.source.ref();
		if (src && src != entity() && has<PlayerControlHuman>() && get<PlayerControlHuman>()->local())
		{
			// spatialized damage sounds
			Vec3 offset = Vec3::normalize(e.source.ref()->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos());
			get<Audio>()->post_offset(event_id, offset * DRONE_SHIELD_RADIUS * 2.0f);
		}
		else
			get<Audio>()->post_unattached(event_id);
	}
	else if (e.shield > 0)
	{
		if (get<Health>()->shield == 1)
			get<Audio>()->post(AK::EVENTS::PLAY_SHIELD_RESTORE_INNER);
		else if (get<Health>()->shield == 2)
			get<Audio>()->post(AK::EVENTS::PLAY_SHIELD_RESTORE_OUTER);
	}
}

void apply_alpha_scale(View* v, const Update& u, const Vec3& offset_pos, r32 target_alpha, r32 target_scale, r32 scale_speed_multiplier)
{
	const r32 anim_time = 0.3f;
	r32 alpha_speed = (DRONE_SHIELD_ALPHA / anim_time) * u.time.delta;
	if (v->color.w > target_alpha)
		v->color.w = vi_max(target_alpha, v->color.w - alpha_speed);
	else
		v->color.w = vi_min(target_alpha, v->color.w + alpha_speed);

	if (v->color.w == 0.0f)
	{
		v->mask = 0;
		v->offset = Mat4::make_scale(Vec3::zero);
	}
	else
	{
		r32 scale_speed = (DRONE_OVERSHIELD_RADIUS * DRONE_SHIELD_VIEW_RATIO / anim_time) * scale_speed_multiplier * u.time.delta;
		r32 existing_scale = v->offset.m[0][0];
		if (existing_scale > target_scale)
			v->offset.make_transform(offset_pos, Vec3(vi_max(target_scale, existing_scale - scale_speed)), Quat::identity);
		else
			v->offset.make_transform(offset_pos, Vec3(vi_min(target_scale, existing_scale + scale_speed)), Quat::identity);
	}
}

void Shield::update_client(const Update& u)
{
	if (!inner.ref() || !outer.ref())
		return;

	Vec3 offset_pos = has<SkinnedModel>() ? get<SkinnedModel>()->offset.translation() : get<View>()->offset.translation();
	RenderMask mask = has<SkinnedModel>() ? get<SkinnedModel>()->mask : get<View>()->mask;

	{
		// inner shield
		View* inner_view = inner.ref()->get<View>();
		inner_view->mask = mask;

		r32 target_alpha;
		r32 target_scale;
		if (has<Drone>() && get<Drone>()->current_ability == Ability::Sniper)
		{
			target_alpha = 0.0f;
			target_scale = 0.0f;
		}
		else if (get<Health>()->shield > 0)
		{
			target_alpha = DRONE_SHIELD_ALPHA;
			target_scale = DRONE_SHIELD_RADIUS * DRONE_SHIELD_VIEW_RATIO;
		}
		else
		{
			target_alpha = 0.0f;
			target_scale = 8.0f;
		}
		apply_alpha_scale(inner_view, u, offset_pos, target_alpha, target_scale, 1.0f);
	}

	{
		// outer shield
		View* outer_view = outer.ref()->get<View>();
		outer_view->mask = mask;

		r32 target_alpha;
		r32 target_scale;
		if (has<Drone>() && get<Drone>()->current_ability == Ability::Sniper)
		{
			target_alpha = 0.0f;
			target_scale = 0.0f;
		}
		else if (get<Health>()->shield > 1)
		{
			target_alpha = DRONE_OVERSHIELD_ALPHA;
			target_scale = DRONE_OVERSHIELD_RADIUS * DRONE_SHIELD_VIEW_RATIO;
		}
		else
		{
			target_alpha = 0.0f;
			target_scale = 10.0f;
		}
		apply_alpha_scale(outer_view, u, offset_pos, target_alpha, target_scale, 1.25f);
	}
}

b8 Health::damage_buffer_required(const Entity* src) const
{
#if SERVER
	return has<PlayerControlHuman>()
		&& !get<PlayerControlHuman>()->local() // we are a remote player
		&& src
		&& (!src->has<PlayerControlHuman>() || !PlayerHuman::players_on_same_client(entity(), src)); // the attacker is remote from the player
#else
	return false;
#endif
}

void Health::damage(Entity* src, s8 damage)
{
	vi_assert(Game::level.local);
	vi_assert(can_take_damage());
	if (hp > 0 && damage > 0)
	{
		if (damage_buffer_required(src))
		{
			// do damage buffering
			BufferedDamage entry;
			entry.source = src;
			entry.damage = damage;
			entry.delay = vi_min(NET_MAX_RTT_COMPENSATION, Net::rtt(get<PlayerControlHuman>()->player.ref())) + Net::tick_rate();
			if (src->has<Drone>() && src->get<Drone>()->current_ability == Ability::Sniper)
				entry.type = BufferedDamage::Type::Sniper;
			else
				entry.type = BufferedDamage::Type::Other;
			damage_buffer.add(entry);
		}
		else // apply damage immediately
			health_internal_apply_damage(this, src, damage);
	}
}

void Health::damage_force(Entity* src, s8 damage)
{
	vi_assert(Game::level.local);
	if (hp > 0 && damage > 0)
		health_internal_apply_damage(this, src, damage);
}

void Health::reset_hp()
{
	if (hp < hp_max)
		add(hp_max - hp);
}

// bypasses all invincibility calculations and damage buffering
void Health::kill(Entity* e)
{
	damage_force(e, hp + shield);
}

void Health::add(s8 amount)
{
	vi_assert(Game::level.local);
	amount = vi_min(amount, s8(hp_max - hp));
	if (amount > 0)
	{
		HealthEvent e =
		{
			nullptr,
			amount,
			0,
		};
		health_send_event(this, &e);
	}
}

s8 Health::total() const
{
	return hp + shield;
}

b8 Health::active_armor() const
{
	if (has<CoreModule>())
		return active_armor_timer > 0.0f || Turret::list.count() > 0 || Game::level.mode != Game::Mode::Pvp;
	else if (has<ForceField>())
		return active_armor_timer > 0.0f || (get<ForceField>()->flags & ForceField::FlagPermanent);
	else if (has<Turret>() && !Game::level.has_feature(Game::FeatureLevel::Turrets))
		return true;
	else
		return active_armor_timer > 0.0f;
}

b8 Health::can_take_damage() const
{
	if (active_armor())
		return false;

	if (has<Drone>())
		return get<Drone>()->state() == Drone::State::Crawl && !UpgradeStation::drone_inside(get<Drone>());
	else
		return true;
}

BatteryEntity::BatteryEntity(const Vec3& p, AI::Team team)
{
	create<Transform>()->pos = p;
	create<Audio>();

	View* model = create<View>();
	model->color = Vec4(0.6f, 0.6f, 0.6f, MATERIAL_NO_OVERRIDE);
	model->mesh = Asset::Mesh::battery;
	model->shader = Asset::Shader::standard;

	create<AICue>(AICue::Type::Sensor);

	if (Game::session.config.enable_battery_stealth)
	{
		PointLight* light = create<PointLight>();
		light->type = PointLight::Type::Override;
		light->team = s8(team);
		light->radius = (team == AI::TeamNone) ? 0.0f : SENSOR_RANGE;

		create<Sensor>()->team = team;
	}

	Target* target = create<Target>();

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	Battery* battery = create<Battery>();
	battery->team = team;
	model->team = s8(team);

	model->offset.scale(Vec3(BATTERY_RADIUS - 0.2f));

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(BATTERY_RADIUS), 0.1f, CollisionTarget, ~CollisionShield & ~CollisionAllTeamsForceField & ~CollisionWalker);
	body->set_damping(0.5f, 0.5f);
	body->set_ccd(true);

	Entity* e = World::create<Empty>();
	e->get<Transform>()->parent = get<Transform>();
	e->add<PointLight>()->radius = 8.0f;
	Net::finalize_child(e);
	battery->light = e;
}

void Battery::health_changed(const HealthEvent& e)
{
	if (team != AI::TeamNone && e.hp + e.shield < 0 && get<Health>()->hp > 0)
	{
		if (PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::BatteryUnderAttack))
		{
			char buffer[UI_TEXT_MAX];
			snprintf(buffer, UI_TEXT_MAX, _(strings::battery_under_attack), s32(id()) + 1);
			PlayerHuman::log_add(buffer, AI::TeamNone, 1 << team);
		}
	}
}

void Battery::killed(Entity* e)
{
	if (Game::level.local)
		set_team(AI::TeamNone, e);
}

Battery* Battery::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Battery* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

s32 Battery::count(AI::TeamMask m)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, m))
			count++;
	}
	return count;
}

r32 Battery::Comparator::priority(const Ref<Battery>& p)
{
	return (p.ref()->get<Transform>()->absolute_pos() - me).length_squared() * (closest_first ? 1.0f : -1.0f);
}

s32 Battery::Comparator::compare(const Ref<Battery>& a, const Ref<Battery>& b)
{
	r32 pa = priority(a);
	r32 pb = priority(b);
	if (pa > pb)
		return 1;
	else if (pa == pb)
		return 0;
	else
		return -1;
}

void Battery::sort_all(const Vec3& pos, Array<Ref<Battery>>* result, b8 closest_first, AI::TeamMask mask)
{
	Comparator key;
	key.me = pos;
	key.closest_first = closest_first;
	result->length = 0;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			result->add(i.item());
	}

	Quicksort::sort<Ref<Battery>, Comparator>(result->data, 0, result->length, &key);
}

void Battery::awake()
{
	link_arg<const TargetEvent&, &Battery::hit>(get<Target>()->target_hit);
	link_arg<Entity*, &Battery::killed>(get<Health>()->killed);
	link_arg<const HealthEvent&, &Battery::health_changed>(get<Health>()->changed);
	set_team_client(team);
	get<Audio>()->post(AK::EVENTS::PLAY_BATTERY_LOOP);
}

Battery::~Battery()
{
	if (Game::level.local && light.ref())
		World::remove_deferred(light.ref());
}

void Battery::hit(const TargetEvent& e)
{
	if (e.hit_by->has<Drone>() && (e.hit_by->get<Drone>()->current_ability != Ability::None && AbilityInfo::list[s32(e.hit_by->get<Drone>()->current_ability)].type == AbilityInfo::Type::Shoot))
		set_team(AI::TeamNone, e.hit_by);
	else
		set_team(e.hit_by->get<AIAgent>()->team, e.hit_by);
}

void Battery::set_team_client(AI::Team t)
{
	if (team != AI::TeamNone && team != t)
	{
		PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::BatteryLost); // notify team that they lost this battery
		char buffer[UI_TEXT_MAX];
		snprintf(buffer, UI_TEXT_MAX, _(strings::battery_lost), s32(id()) + 1);
		PlayerHuman::log_add(buffer, AI::TeamNone, 1 << team);
	}

	team = t;
	get<View>()->team = s8(t);
	if (has<Sensor>())
	{
		get<PointLight>()->team = s8(t);
		get<PointLight>()->radius = (t == AI::TeamNone) ? 0.0f : SENSOR_RANGE;
		get<Sensor>()->team = t;
	}
	spawn_point.ref()->set_team(t);
}

b8 Battery::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Ref<Battery> ref;
	serialize_ref(p, ref);
	AI::Team t;
	serialize_s8(p, t);
	s16 reward_level;
	serialize_s16(p, reward_level);
	Ref<Entity> caused_by;
	serialize_ref(p, caused_by);

	Battery* pickup = ref.ref();
	if (caused_by.ref() && caused_by.ref()->has<AIAgent>() && t == caused_by.ref()->get<AIAgent>()->team)
	{
		if (pickup->team != t)
			caused_by.ref()->get<PlayerCommon>()->manager.ref()->add_energy_and_notify(pickup->reward());
		pickup->get<Audio>()->post(AK::EVENTS::PLAY_BATTERY_CAPTURE);
	}
	else if (t == AI::TeamNone)
		pickup->get<Audio>()->post(AK::EVENTS::PLAY_BATTERY_RESET);

	pickup->reward_level = reward_level;
	pickup->set_team_client(t);

	return true;
}

// returns true if we were successfully captured
// the second parameter is the entity that caused the ownership change
b8 Battery::set_team(AI::Team t, Entity* caused_by)
{
	vi_assert(Game::level.local);

	// must be neutral or owned by an enemy
	get<Health>()->reset_hp();
	if (t != team)
		reward_level++;

	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::Battery);
	Ref<Battery> ref = this;
	serialize_ref(p, ref);
	serialize_s8(p, t);
	serialize_s16(p, reward_level);
	Ref<Entity> caused_by_ref = caused_by;
	serialize_ref(p, caused_by_ref);
	Net::msg_finalize(p);
	return t != team;
}

r32 Battery::particle_accumulator;
void Battery::update_all(const Update& u)
{
	// normal particles
	const r32 interval = 0.1f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();

			Particles::tracers.add
			(
				pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, mersenne::randf_co() * 0.6f),
				Vec3::zero,
				0
			);
		}
	}
}

// instant reward for capturing the battery
s16 Battery::reward() const
{
	return s16(vi_max(1, vi_min(10, s32(reward_level / 2) + 1)) * 5);
}

// applied to every player on the owning team every ENERGY_INCREMENT_INTERVAL seconds
s16 Battery::increment() const
{
	s32 multiplier;
	if (PlayerManager::list.count() >= 6)
		multiplier = 2;
	else if (PlayerManager::list.count() > 2)
		multiplier = 3;
	else
		multiplier = 5;
	return s16(vi_max(1, vi_min(4, s32(reward_level / 2) + 1)) * multiplier);
}

SpawnPointEntity::SpawnPointEntity(AI::Team team, b8 visible)
{
	create<Transform>();

	SpawnPoint* sp = create<SpawnPoint>();
	sp->team = team;

	if (visible)
	{
		View* view = create<View>();
		view->mesh = Asset::Mesh::spawn_main;
		view->shader = Asset::Shader::culled;
		view->team = s8(team);

		PointLight* light = create<PointLight>();
		light->offset.z = 2.0f;
		light->radius = 12.0f;
		light->team = s8(team);

		Entity* upgrade_station = World::create<UpgradeStationEntity>(sp);
		upgrade_station->get<Transform>()->parent = get<Transform>();
		Net::finalize_child(upgrade_station);

		create<RigidBody>(RigidBody::Type::Mesh, Vec3(1.0f), 0.0f, CollisionStatic | CollisionParkour, ~CollisionStatic & ~CollisionAudio & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric, Asset::Mesh::spawn_collision);
	}
}

void SpawnPoint::set_team(AI::Team t)
{
	team = t;
	get<View>()->team = s8(t);
	get<PointLight>()->team = s8(t);
}

SpawnPoint* SpawnPoint::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	SpawnPoint* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

s32 SpawnPoint::count(AI::TeamMask mask)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			count++;
	}
	return count;
}

SpawnPoint* SpawnPoint::first(AI::TeamMask mask)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			return i.item();
	}
	return nullptr;
}

b8 drone_can_spawn(const Vec3& pos)
{
	for (auto i = Drone::list.iterator(); !i.is_last(); i.next())
	{
		if (LMath::ray_sphere_intersect(pos + Vec3(0, 1, 0), pos, i.item()->get<Target>()->absolute_pos(), DRONE_SHIELD_RADIUS))
			return false;
	}
	return true;
}

SpawnPosition SpawnPoint::spawn_position() const
{
	SpawnPosition result;
	Quat rot;
	get<Transform>()->absolute(&result.pos, &rot);
	Vec3 dir = rot * Vec3(0, 1, 0);
	result.angle = atan2f(dir.x, dir.z);

	s32 j = 0;
	Vec3 p = result.pos;
	while (!drone_can_spawn(p) && j < 4)
	{
		p = result.pos + Quat::euler(0, result.angle + (r32(j + 1) * PI * 0.5f), 0) * Vec3(0, 0, SPAWN_POINT_RADIUS * 0.5f);
		j++;
	}
	result.pos = p;
	return result;
}

void SpawnPoint::update_server_all(const Update& u)
{
	if (Game::level.mode == Game::Mode::Pvp
		&& Game::level.has_feature(Game::FeatureLevel::All)
		&& Team::match_state == Team::MatchState::Active
		&& Game::session.config.enable_minions)
	{
		const s32 minion_group = 3;
		const r32 minion_initial_delay = Game::session.config.game_type == GameType::Deathmatch ? 45.0f : 20.0f;
		const r32 minion_spawn_interval = 8.0f;
		const r32 minion_group_interval = minion_spawn_interval * 12.0f; // must be a multiple of minion_spawn_interval
		r32 t = Team::match_time - minion_initial_delay;
		if (t > 0.0f)
		{
			s32 index = t / minion_spawn_interval;
			s32 index_last = (t - u.time.delta) / minion_spawn_interval;
			if (index != index_last && (index % s32(minion_group_interval / minion_spawn_interval)) <= minion_group)
			{
				// spawn points owned by a team will spawn a minion
				for (auto i = list.iterator(); !i.is_last(); i.next())
				{
					if (i.item()->team != AI::TeamNone)
					{
						SpawnPosition pos = i.item()->spawn_position();
						Net::finalize(World::create<MinionEntity>(pos.pos + Vec3(0, 1, 0), Quat::euler(0, pos.angle, 0), i.item()->team, nullptr));
						ParticleEffect::spawn(ParticleEffect::Type::SpawnMinion, pos.pos, Quat::look(Vec3(0, 1, 0)));
					}
				}
			}
		}
	}
}

namespace UpgradeStationNet
{
	b8 set_drone(UpgradeStation* u, Drone* d)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::UpgradeStation);

		{
			Ref<UpgradeStation> ref = u;
			serialize_ref(p, ref);
		}

		{
			Ref<Drone> ref = d;
			serialize_ref(p, ref);
		}

		Net::msg_finalize(p);
		return true;
	}
}

#define UPGRADE_STATION_ANIM_TIME 0.5f
b8 UpgradeStation::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	Ref<UpgradeStation> ref;
	serialize_ref(p, ref);
	Ref<Drone> drone;
	serialize_ref(p, drone);

	UpgradeStation* u = ref.ref();
	if (u)
	{
		Drone* d = drone.ref();
		if ((!u->drone.ref() && d
			&& d->state() == Drone::State::Crawl)
			|| (u->drone.ref() && !d))
		{
#if SERVER
			if (src == Net::MessageSource::Remote)
			{
				// repeat this message to all clients, including ourselves
				// we will process the message in the `else` statement below
				UpgradeStationNet::set_drone(u, d);
			}
			else
#endif
			{
				if (d)
				{
					if (ref.ref()->drone.ref() != d)
					{
						ref.ref()->drone = d;
						ref.ref()->timer = UPGRADE_STATION_ANIM_TIME - ref.ref()->timer;
						ref.ref()->mode = Mode::Activating;
						ref.ref()->get<Audio>()->post(AK::EVENTS::PLAY_UPGRADE_STATION_ENTER);
					}
				}
				else if (ref.ref()->mode == Mode::Activating)
				{
					ref.ref()->timer = UPGRADE_STATION_ANIM_TIME - ref.ref()->timer;
					ref.ref()->mode = Mode::Deactivating;
					ref.ref()->get<Audio>()->post(AK::EVENTS::PLAY_UPGRADE_STATION_EXIT);
				}
			}
		}
	}

	return true;
}

// returns the upgrade station the given drone is in range of, if any
UpgradeStation* UpgradeStation::drone_at(const Drone* drone)
{
	if (!drone)
		return nullptr;

	AI::Team team = drone->get<AIAgent>()->team;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->spawn_point.ref()->team == team && i.item()->get<PlayerTrigger>()->is_triggered(drone->entity()))
			return i.item();
	}

	return nullptr;
}

// returns the upgrade station the given drone is currently inside, if any
UpgradeStation* UpgradeStation::drone_inside(const Drone* drone)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->drone.ref() == drone)
			return i.item();
	}
	return nullptr;
}

UpgradeStation* UpgradeStation::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	UpgradeStation* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->spawn_point.ref()->team, mask))
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

#define UPGRADE_STATION_OFFSET Vec3(0, 0, -0.2f)
void UpgradeStation::update_client(const Update& u)
{
	if (mode == Mode::Activating && !drone.ref())
	{
		// drone disappeared on us; flip back over automatically
		mode = Mode::Deactivating;
		timer = UPGRADE_STATION_ANIM_TIME;
		get<Audio>()->post(AK::EVENTS::PLAY_UPGRADE_STATION_EXIT);
	}

	if (timer > 0.0f)
	{
		timer = vi_max(0.0f, timer - u.time.delta);
		if (timer == 0.0f && mode == Mode::Deactivating)
			drone = nullptr;
	}

	if (drone.ref() && mode != Mode::Deactivating)
		get<View>()->team = drone.ref()->get<AIAgent>()->team;
	else
		get<View>()->team = AI::TeamNone;

	if (timer == 0.0f)
	{
		get<View>()->mesh = Asset::Mesh::spawn_collision;
		get<View>()->offset = Mat4::identity;
	}
	else
	{
		get<View>()->mesh = Asset::Mesh::spawn_upgrade_station;
		get<View>()->offset.make_transform(timer > 0.0f ? UPGRADE_STATION_OFFSET : Vec3::zero, Vec3(1.0f), rotation());
	}
}

Quat UpgradeStation::rotation() const
{
	r32 blend = timer / UPGRADE_STATION_ANIM_TIME;
	if (mode == Mode::Activating)
		blend = Ease::quad_out<r32>(1.0f - blend);
	else
		blend = Ease::quad_in<r32>(blend);
	return Quat::euler(0, blend * PI, 0);
}

void UpgradeStation::transform(Vec3* pos, Quat* rot) const
{
	get<Transform>()->to_local(pos, rot);

	*pos -= UPGRADE_STATION_OFFSET;

	Quat my_rot = rotation();

	*pos = my_rot * *pos;
	*rot = my_rot * *rot;

	*pos += UPGRADE_STATION_OFFSET;

	get<Transform>()->to_world(pos, rot);
}

void UpgradeStation::drone_enter(Drone* d)
{
	UpgradeStationNet::set_drone(this, d);
}

void UpgradeStation::drone_exit()
{
	if (drone.ref())
		UpgradeStationNet::set_drone(this, nullptr);
}

UpgradeStationEntity::UpgradeStationEntity(SpawnPoint* p)
{
	create<Transform>();
	create<Audio>();

	create<PlayerTrigger>()->radius = UPGRADE_STATION_RADIUS;

	UpgradeStation* u = create<UpgradeStation>();
	u->spawn_point = p;

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn_collision;
	view->shader = Asset::Shader::culled;
	view->team = AI::TeamNone;
	view->color.w = MATERIAL_NO_OVERRIDE;
}

SensorEntity::SensorEntity(AI::Team team, const Vec3& abs_pos, const Quat& abs_rot)
{
	Transform* transform = create<Transform>();
	transform->pos = abs_pos;
	transform->rot = abs_rot;

	create<Audio>();

	View* model = create<View>();
	model->mesh = Asset::Mesh::sphere;
	model->color = Team::color_enemy;
	model->team = s8(team);
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(SENSOR_RADIUS));

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	PointLight* light = create<PointLight>();
	light->type = PointLight::Type::Override;
	light->team = s8(team);
	light->radius = SENSOR_RANGE;

	create<Sensor>(team);

	create<Target>();

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(SENSOR_RADIUS), 0.0f, CollisionTarget, ~CollisionShield);
}

Sensor::Sensor(AI::Team t)
	: team(t)
{
}

void Sensor::awake()
{
	if (!has<Battery>())
	{
		link_arg<Entity*, &Sensor::killed_by>(get<Health>()->killed);
		link_arg<const TargetEvent&, &Sensor::hit_by>(get<Target>()->target_hit);
	}
}

void Sensor::hit_by(const TargetEvent& e)
{
	vi_assert(!has<Battery>());
	get<Health>()->kill(e.hit_by);
}

void Sensor::killed_by(Entity* e)
{
	vi_assert(!has<Battery>());
	PlayerManager::entity_killed_by(entity(), e);
	if (Game::level.local)
		World::remove_deferred(entity());
}

void Sensor::update_client_all(const Update& u)
{
	r32 time = u.time.total;
	r32 last_time = time - u.time.delta;
	const r32 sensor_shockwave_interval = 5.0f;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != AI::TeamNone)
		{
			r32 offset = i.index * sensor_shockwave_interval * 0.3f;
			if (s32((time + offset) / sensor_shockwave_interval) != s32((last_time + offset) / sensor_shockwave_interval))
			{
				EffectLight::add(i.item()->get<Transform>()->absolute_pos(), 10.0f, 1.5f, EffectLight::Type::Shockwave);
				i.item()->get<Audio>()->post(AK::EVENTS::PLAY_SENSOR_PING);
			}
		}
	}
}

void Sensor::set_team(AI::Team t)
{
	// not synced over network
	team = t;
	get<View>()->team = s8(t);
	get<PointLight>()->team = s8(t);
}

b8 Sensor::can_see(AI::Team team, const Vec3& pos, const Vec3& normal)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team)
		{
			Vec3 to_sensor = i.item()->get<Transform>()->absolute_pos() - pos;
			if (to_sensor.length_squared() < SENSOR_RANGE * SENSOR_RANGE && to_sensor.dot(normal) > 0.0f)
				return true;
		}
	}
	return false;
}

Sensor* Sensor::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	Sensor* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

AICue::AICue(TypeMask t)
	: type(t)
{
}

AICue::AICue() : type() {}

// returns the closest sensor interest point within range of the given position, or null
AICue* AICue::in_range(AICue::TypeMask mask, const Vec3& pos, r32 radius, s32* count)
{
	AICue* closest = nullptr;
	r32 closest_distance = radius * radius;

	s32 c = 0;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (mask & i.item()->type)
		{
			r32 d = (i.item()->get<Transform>()->absolute_pos() - pos).length_squared();
			if (d < closest_distance)
			{
				closest = i.item();
				closest_distance = d;
			}

			if (d < radius * radius)
				c++;
		}
	}

	if (count)
		*count = c;

	return closest;
}

CoreModuleEntity::CoreModuleEntity(AI::Team team, Transform* parent, const Vec3& pos, const Quat& rot)
{
	Transform* transform = create<Transform>();
	transform->parent = parent;
	transform->absolute(pos + rot * Vec3(0, 0, DRONE_RADIUS), rot);

	create<Health>(DRONE_HEALTH, DRONE_HEALTH, DRONE_SHIELD_AMOUNT, DRONE_SHIELD_AMOUNT);

	View* model = create<View>();
	model->mesh = Asset::Mesh::core_module;
	model->shader = Asset::Shader::standard;
	model->team = s8(team);

	create<Target>();
	create<MinionTarget>();

	create<CoreModule>()->team = team;

	create<Shield>();

	create<Audio>();
}

void CoreModule::awake()
{
	link_arg<Entity*, &CoreModule::killed>(get<Health>()->killed);
}

// not synced over network
void CoreModule::set_team(AI::Team t)
{
	team = t;
	get<View>()->team = s8(t);
	get<Shield>()->set_team(t);
}

s32 CoreModule::count(AI::TeamMask mask)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
			count++;
	}
	return count;
}

void CoreModule::killed(Entity* e)
{
	if (list.count() > 1)
	{
		// let everyone know what happened
		char buffer[512];
		sprintf(buffer, _(strings::core_modules_remaining), list.count() - 1);

		PlayerHuman::log_add(buffer);
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			// it's a good thing if you're not on the defending team
			b8 good = i.item()->get<PlayerManager>()->team.ref()->team() != 0;
			i.item()->msg(buffer, good);
		}
	}

	PlayerManager::entity_killed_by(entity(), e);

	if (Game::level.local)
		destroy();
}

void CoreModule::destroy()
{
	vi_assert(Game::level.local);
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
	World::remove_deferred(entity());
}

#define TURRET_COOLDOWN 1.5f
#define TURRET_TARGET_CHECK_TIME 0.75f
#define TURRET_RADIUS 0.5f
TurretEntity::TurretEntity(AI::Team team)
{
	create<Transform>();
	create<Audio>();
	create<MinionTarget>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::turret_top;
	view->shader = Asset::Shader::culled;
	view->team = s8(team);
	
	create<Turret>()->team = team;

	create<Target>();

	create<Health>(TURRET_HEALTH, TURRET_HEALTH);
	create<Shield>();

	PointLight* light = create<PointLight>();
	light->team = s8(team);
	light->type = PointLight::Type::Normal;
	light->radius = TURRET_RANGE * 0.5f;
}

void Turret::awake()
{
	vi_assert(id() < MAX_TURRETS);
	target_check_time = mersenne::randf_oo() * TURRET_TARGET_CHECK_TIME;
	link_arg<Entity*, &Turret::killed>(get<Health>()->killed);
	link_arg<const HealthEvent&, &Turret::health_changed>(get<Health>()->changed);
}

// not synced over network
void Turret::set_team(AI::Team t)
{
	team = t;
	get<View>()->team = s8(t);
	get<PointLight>()->team = s8(t);
}

AssetID Turret::name() const
{
	static const AssetID names[MAX_TURRETS] =
	{
		strings::turret1,
		strings::turret2,
		strings::turret3,
		strings::turret4,
		strings::turret5,
		strings::turret6,
	};
	return names[id()];
}

void Turret::health_changed(const HealthEvent& e)
{
	if (e.hp + e.shield < 0 && get<Health>()->hp > 0)
	{
		if (PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::TurretUnderAttack))
		{
			char buffer[UI_TEXT_MAX];
			snprintf(buffer, UI_TEXT_MAX, _(strings::turret_under_attack), _(name()));
			PlayerHuman::log_add(buffer, AI::TeamNone, 1 << team);
		}
	}
}

void Turret::killed(Entity* by)
{
	PlayerManager::entity_killed_by(entity(), by);

	{
		// let everyone know what happened
		char buffer[UI_TEXT_MAX];
		if (list.count() > 1)
			snprintf(buffer, UI_TEXT_MAX, _(strings::turrets_remaining), list.count() - 1);
		else
			strcpy(buffer, _(strings::core_vulnerable));

		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			// it's a good thing if you're not on the defending team
			b8 good = i.item()->get<PlayerManager>()->team.ref()->team() != 0;
			i.item()->msg(buffer, good);
		}
	}

	PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::TurretDestroyed);

	if (Game::level.local)
	{
		Vec3 pos;
		Quat rot;
		get<Transform>()->absolute(&pos, &rot);
		ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);

		if (list.count() <= 1)
		{
			// core is vulnerable; remove permanent ForceField protecting it
			for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->flags & ForceField::FlagPermanent)
					i.item()->destroy();
			}
		}

		World::remove_deferred(entity());
	}
}

namespace TurretNet
{
	b8 update_target(Turret* turret, Entity* target)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Turret);
		{
			Ref<Turret> ref_turret = turret;
			serialize_ref(p, ref_turret);
		}
		{
			Ref<Entity> ref_target = target;
			serialize_ref(p, ref_target);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 Turret::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	Ref<Turret> ref;
	serialize_ref(p, ref);
	Ref<Entity> target;
	serialize_ref(p, target);
	if (ref.ref())
		ref.ref()->target = target;
	return true;
}

b8 Turret::can_see(Entity* target) const
{
	if ((target->has<AIAgent>() && target->get<AIAgent>()->stealth)
		|| (target->has<Drone>() && target->get<Drone>()->state() != Drone::State::Crawl))
		return false;

	Vec3 pos = get<Transform>()->absolute_pos();

	Vec3 target_pos = target->has<Target>() ? target->get<Target>()->absolute_pos() : target->get<Transform>()->absolute_pos();

	if (!target->has<ForceField>() && ForceField::hash(team, pos) != ForceField::hash(team, target_pos))
		return false;

	Vec3 to_target = target_pos - pos;
	float distance_to_target = to_target.length();
	if (distance_to_target < TURRET_RANGE)
	{
		RaycastCallbackExcept ray_callback(pos, target_pos, entity());
		Physics::raycast(&ray_callback, ~Team::force_field_mask(team));
		if (!ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == target->id())
			return true;
	}
	return false;
}

s32 turret_priority(Entity* e)
{
	if (e->has<Battery>() || e->has<SpawnPoint>())
		return -1; // never attack
	else if (e->has<Minion>())
		return 2;
	else
		return 1;
}

void Turret::check_target()
{
	// if we are targeting an enemy
	// make sure we still want to do that
	if (target.ref() && can_see(target.ref()))
		return;

	// find a new target
	Ref<Entity> target_old = target;

	Ref<Entity> target_new = nullptr;

	r32 target_priority = 0;
	for (auto i = Health::list.iterator(); !i.is_last(); i.next())
	{
		Entity* e = i.item()->entity();
		AI::Team e_team;
		AI::entity_info(e, team, &e_team);
		if (e_team != AI::TeamNone
			&& e_team != team
			&& can_see(i.item()->entity()))
		{
			r32 candidate_priority = turret_priority(i.item()->entity());
			if (candidate_priority > target_priority)
			{
				target_new = i.item()->entity();
				target_priority = candidate_priority;
			}
		}
	}

	if (!target_new.equals(target_old))
		TurretNet::update_target(this, target_new.ref());
}

void Turret::update_server(const Update& u)
{
	if (cooldown > 0.0f)
		cooldown -= u.time.delta;

	if (Game::level.has_feature(Game::FeatureLevel::All))
	{
		target_check_time -= u.time.delta;
		if (target_check_time < 0.0f)
		{
			target_check_time += TURRET_TARGET_CHECK_TIME;
			check_target();
		}
	}

	if (target.ref() && cooldown <= 0.0f)
	{
		if (can_see(target.ref()))
		{
			Vec3 gun_pos = get<Transform>()->absolute_pos();
			Vec3 aim_pos;
			if (!target.ref()->has<Target>() || !target.ref()->get<Target>()->predict_intersection(gun_pos, BOLT_SPEED_TURRET, nullptr, &aim_pos))
				aim_pos = target.ref()->get<Transform>()->absolute_pos();
			gun_pos += Vec3::normalize(aim_pos - gun_pos) * TURRET_RADIUS;
			Net::finalize(World::create<BoltEntity>(team, nullptr, entity(), Bolt::Type::Turret, gun_pos, aim_pos - gun_pos));
			cooldown += TURRET_COOLDOWN;
		}
		else
			target_check_time = 0.0f;
	}
}

r32 Turret::particle_accumulator;
void Turret::update_client_all(const Update& u)
{
	const r32 interval = 0.05f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			b8 has_target = b8(i.item()->target.ref());
			if (has_target)
			{
				// spawn particle effect
				Vec3 offset = Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 1.5f);
				Particles::fast_tracers.add
				(
					i.item()->get<Transform>()->absolute_pos() + offset,
					offset * -5.0f,
					0
				);
			}

			if (has_target != i.item()->charging)
			{
				if (has_target)
					i.item()->get<Audio>()->post(AK::EVENTS::PLAY_TURRET_CHARGE);
				else
					i.item()->get<Audio>()->stop(AK::EVENTS::STOP_TURRET_CHARGE);
				i.item()->charging = has_target;
			}
		}
	}
}

// returns true if the given position is inside an enemy force field
ForceField* ForceField::inside(AI::TeamMask mask, const Vec3& pos)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask) && i.item()->contains(pos))
			return i.item();
	}
	return nullptr;
}

b8 ForceField::contains(const Vec3& pos) const
{
	return (pos - get<Transform>()->absolute_pos()).length_squared() < FORCE_FIELD_RADIUS * FORCE_FIELD_RADIUS;
}

// describes which enemy force fields you are currently inside
u32 ForceField::hash(AI::Team my_team, const Vec3& pos)
{
	u32 result = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != my_team && (pos - i.item()->get<Transform>()->absolute_pos()).length_squared() < FORCE_FIELD_RADIUS * FORCE_FIELD_RADIUS)
		{
			if (result == 0)
				result = 1;
			result += MAX_ENTITIES % (i.index + 37); // todo: learn how to math
		}
	}
	return result;
}

ForceField::ForceField()
	: team(AI::TeamNone),
	flags(),
	collision(),
	obstacle_id(-1)
{
}

void ForceField::awake()
{
	link_arg<const TargetEvent&, &ForceField::hit_by>(get<Target>()->target_hit);
	link_arg<Entity*, &ForceField::killed>(get<Health>()->killed);
	link_arg<const HealthEvent&, &ForceField::health_changed>(get<Health>()->changed);
	get<Audio>()->post(AK::EVENTS::PLAY_FORCE_FIELD_LOOP);
	if (Game::level.local)
		obstacle_id = AI::obstacle_add(get<Transform>()->to_world(Vec3(0, 0, FORCE_FIELD_BASE_OFFSET * -0.5f)) + Vec3(0, FORCE_FIELD_BASE_OFFSET * -0.5f, 0), FORCE_FIELD_BASE_OFFSET * 0.5f, FORCE_FIELD_BASE_OFFSET);
}

ForceField::~ForceField()
{
	if (Game::level.local && collision.ref())
		World::remove_deferred(collision.ref()->entity());
	get<Audio>()->stop(AK::EVENTS::STOP_FORCE_FIELD_LOOP);
	if (obstacle_id != u32(-1))
		AI::obstacle_remove(obstacle_id);
}

Vec3 ForceField::base_pos() const
{
	return get<Transform>()->to_world(Vec3(0, 0, -FORCE_FIELD_BASE_OFFSET));
}

// not synced over network
void ForceField::set_team(AI::Team t)
{
	team = t;
	get<View>()->team = s8(t);
	collision.ref()->get<View>()->team = s8(t);
	collision.ref()->get<RigidBody>()->set_collision_masks(CollisionGroup(1 << (8 + team)), CollisionDroneIgnore);
}

void ForceField::hit_by(const TargetEvent& e)
{
	if (!(flags & FlagPermanent) && get<Health>()->can_take_damage())
		get<Health>()->damage(e.hit_by, 1);
}

void ForceField::health_changed(const HealthEvent& e)
{
	if (e.hp + e.shield < 0
		&& get<Health>()->hp > 0
		&& PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::ForceFieldUnderAttack))
	{
		PlayerHuman::log_add(_(strings::force_field_under_attack), AI::TeamNone, 1 << team);
	}
}

void ForceField::killed(Entity* e)
{
	PlayerHuman::notification(entity(), team, PlayerHuman::Notification::Type::ForceFieldDestroyed);
	PlayerHuman::log_add(_(strings::force_field_destroyed), AI::TeamNone, 1 << team);
	PlayerManager::entity_killed_by(entity(), e);
	if (Game::level.local)
		World::remove_deferred(entity());
}

ForceField* ForceField::closest(AI::TeamMask mask, const Vec3& pos, r32* distance)
{
	ForceField* closest = nullptr;
	r32 closest_distance = FLT_MAX;

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (AI::match(i.item()->team, mask))
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

r32 ForceField::particle_accumulator;
void ForceField::update_all(const Update& u)
{
	const r32 interval = 0.1f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();

			// spawn particle effect
			Particles::eased_particles.add
			(
				pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 2.0f),
				pos,
				0
			);
		}
	}
}

void ForceField::destroy()
{
	vi_assert(Game::level.local);
	Vec3 pos;
	Quat rot;
	get<Transform>()->absolute(&pos, &rot);
	ParticleEffect::spawn(ParticleEffect::Type::Explosion, pos, rot);
	World::remove_deferred(entity());
}

#define FORCE_FIELD_BASE_RADIUS 0.385f
ForceFieldEntity::ForceFieldEntity(Transform* parent, const Vec3& abs_pos, const Quat& abs_rot, AI::Team team, ForceField::Type type)
{
	Transform* transform = create<Transform>();
	transform->absolute(abs_pos, abs_rot);
	transform->reparent(parent);

	create<Audio>();

	// destroy any overlapping friendly force field
	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team == team
			&& !(i.item()->flags & ForceField::FlagPermanent)
			&& (i.item()->get<Transform>()->absolute_pos() - abs_pos).length_squared() < FORCE_FIELD_RADIUS * 2.0f * FORCE_FIELD_RADIUS * 2.0f)
		{
			i.item()->destroy();
		}
	}

	View* model = create<View>();
	model->team = team;
	model->mesh = Asset::Mesh::force_field_base;
	model->shader = Asset::Shader::standard;

	create<Target>();
	create<Health>(FORCE_FIELD_HEALTH, FORCE_FIELD_HEALTH);
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(FORCE_FIELD_BASE_RADIUS), 0.0f, CollisionTarget, ~CollisionStatic & ~CollisionAudio & ~CollisionShield & ~CollisionParkour & ~CollisionInaccessible & ~CollisionAllTeamsForceField & ~CollisionElectric);

	ForceField* field = create<ForceField>();
	field->team = team;
	if (type == ForceField::Type::Permanent)
		field->flags |= ForceField::FlagPermanent;

	// collision
	Entity* f = World::create<Empty>();
	f->get<Transform>()->absolute_pos(abs_pos);
	ForceFieldCollision* collision = f->add<ForceFieldCollision>();
	collision->field = field;

	View* view = f->add<View>();
	view->team = s8(team);
	view->mesh = Asset::Mesh::force_field_sphere;
	view->shader = Asset::Shader::fresnel;
	view->alpha();
	view->color.w = 0.35f;

	CollisionGroup team_group = CollisionGroup(1 << (8 + team));

	f->add<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, team_group, CollisionTarget, view->mesh);

	Net::finalize_child(f);

	field->collision = collision;
}

r32 Bolt::speed(Type t)
{
	if (t == Type::DroneBolter)
		return BOLT_SPEED_DRONE_BOLTER;
	else if (t == Type::DroneShotgun)
		return BOLT_SPEED_DRONE_SHOTGUN;
	else if (t == Type::Turret)
		return BOLT_SPEED_TURRET;
	else
	{
		vi_assert(t == Type::Minion);
		return BOLT_SPEED_MINION;
	}
}

#define TELEPORTER_RADIUS 0.5f
#define BOLT_THICKNESS 0.05f
BoltEntity::BoltEntity(AI::Team team, PlayerManager* player, Entity* owner, Bolt::Type type, const Vec3& abs_pos, const Vec3& velocity)
{
	Vec3 dir = Vec3::normalize(velocity);
	Transform* transform = create<Transform>();
	transform->absolute_pos(abs_pos);
	transform->absolute_rot(Quat::look(dir));

	if (type != Bolt::Type::DroneShotgun)
	{
		PointLight* light = create<PointLight>();
		light->radius = BOLT_LIGHT_RADIUS;
		light->color = Vec3(1, 1, 1);
	}

	r32 speed = Bolt::speed(type);

	Bolt* b = create<Bolt>();
	b->remaining_lifetime = (DRONE_MAX_DISTANCE * 0.99f) / speed;
	b->team = team;
	b->owner = owner;
	b->player = player;
	b->velocity = dir * speed;
	b->type = type;
}

namespace BoltNet
{
	b8 reflect(Bolt* b)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Bolt);
		{
			Ref<Bolt> ref = b;
			serialize_ref(p, ref);
		}
		b8 change_team = false;
		serialize_bool(p, change_team);
		Net::msg_finalize(p);
		return true;
	}

	b8 reflect(Bolt* b, AI::Team team, const PlayerManager* player, const Entity* owner)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Bolt);
		{
			Ref<Bolt> ref = b;
			serialize_ref(p, ref);
		}
		b8 change_team = true;
		serialize_bool(p, change_team);
		serialize_s8(p, team);
		{
			Ref<PlayerManager> ref = (PlayerManager*)player;
			serialize_ref(p, ref);
		}
		{
			Ref<Entity> ref = (Entity*)(owner);
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 Bolt::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	Ref<Bolt> ref;
	b8 change_team;
	AI::Team team;
	Ref<PlayerManager> player;
	Ref<Entity> owner;

	serialize_ref(p, ref);
	serialize_bool(p, change_team);
	if (change_team)
	{
		serialize_s8(p, team);
		serialize_ref(p, player);
		serialize_ref(p, owner);
	}

	if (ref.ref())
	{
		Audio::post_global(AK::EVENTS::PLAY_DRONE_REFLECT, ref.ref()->get<Transform>()->absolute_pos());
		if (change_team)
		{
			ref.ref()->reflected = true;
			ref.ref()->team = team;
			ref.ref()->player = player;
			ref.ref()->owner = owner;
		}
	}
	return true;
}

void Bolt::awake()
{
	last_pos = get<Transform>()->absolute_pos();

	if (owner.ref() && owner.ref()->has<Turret>())
		owner.ref()->get<Audio>()->post(AK::EVENTS::PLAY_BOLT_SPAWN);
}

b8 Bolt::visible() const
{
	return velocity.length_squared() > 0.0f;
}

b8 Bolt::raycast(const Vec3& trace_start, const Vec3& trace_end, s16 mask, Hit* out_hit, Net::StateFrame* state_frame)
{
	out_hit->entity = nullptr;
	r32 closest_hit_distance_sq = FLT_MAX;

	{
		btCollisionWorld::ClosestRayResultCallback ray_callback(trace_start, trace_end);
		Physics::raycast(&ray_callback, mask);
		if (ray_callback.hasHit())
		{
			out_hit->point = ray_callback.m_hitPointWorld;
			out_hit->normal = ray_callback.m_hitNormalWorld;
			out_hit->entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
			closest_hit_distance_sq = (out_hit->point - trace_start).length_squared();
		}
	}

	// check target collisions
	for (auto i = Target::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 p;
		if (state_frame)
		{
			Vec3 pos;
			Quat rot;
			Net::transform_absolute(*state_frame, i.item()->get<Transform>()->id(), &pos, &rot);
			p = pos + (rot * i.item()->local_offset); // todo possibly: rewind local_offset as well?
		}
		else
			p = i.item()->absolute_pos();

		Vec3 intersection;
		if (LMath::ray_sphere_intersect(trace_start, trace_end, p, i.item()->radius(), &intersection))
		{
			r32 distance_sq = (intersection - trace_start).length_squared();
			if (distance_sq < closest_hit_distance_sq)
			{
				out_hit->point = intersection;
				out_hit->normal = Vec3::normalize(intersection - p);
				out_hit->entity = i.item()->entity();
				closest_hit_distance_sq = distance_sq;
			}
		}
	}

	return out_hit->entity;
}

// returns true if the bolt hit something
b8 Bolt::simulate(r32 dt, Hit* out_hit, Net::StateFrame* state_frame)
{
	if (!visible()) // waiting for damage buffer
		return false;

	Vec3 pos = get<Transform>()->absolute_pos();

	remaining_lifetime -= dt;
	if (!state_frame && remaining_lifetime < 0.0f)
	{
		ParticleEffect::spawn(ParticleEffect::Type::Fizzle, pos, Quat::look(Vec3::normalize(velocity)));
		World::remove_deferred(entity());
		return false;
	}

	Vec3 next_pos = pos + velocity * dt;
	Vec3 trace_end = next_pos + Vec3::normalize(velocity) * BOLT_LENGTH;

	Hit hit;
	if (raycast(pos, trace_end, CollisionStatic | (CollisionAllTeamsForceField & Bolt::raycast_mask(team)), &hit, state_frame))
	{
		if (out_hit)
			*out_hit = hit;
		if (!state_frame) // if the server is fast-forward simulating us, we can't register the hit ourselves
			hit_entity(hit);
		return true;
	}
	else
		get<Transform>()->absolute_pos(next_pos);

	return false;
}

r32 Bolt::particle_accumulator;
void Bolt::update_client_all(const Update& u)
{
	const r32 particle_interval = 0.025f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > particle_interval)
	{
		particle_accumulator -= particle_interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->reflected)
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				Particles::tracers.add
				(
					Vec3::lerp(particle_accumulator / vi_max(0.0001f, u.time.delta), i.item()->last_pos, pos),
					Vec3::zero,
					0
				);
			}
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
		i.item()->last_pos = i.item()->get<Transform>()->absolute_pos();
}

s16 Bolt::raycast_mask(AI::Team team)
{
	return ~Team::force_field_mask(team);
}

b8 Bolt::can_damage(const Entity* e) const
{
	return e->has<Health>()
		&& e->get<Health>()->can_take_damage()
		&& (!e->has<Drone>() // not a drone; we can always damage them
			|| type == Type::DroneBolter || type == Type::DroneShotgun // we're a drone shooting at a drone; can always do damage
			|| e->get<Drone>()->state() == Drone::State::Crawl); // the drone is flying or dashing; it's invincible to minions and turrets
}

void Bolt::reflect(const Entity* hit_object, ReflectionType reflection_type, const Vec3& normal)
{
	vi_assert(Game::level.local);
	Transform* transform = get<Transform>();

	Vec3 dir;
	if (reflection_type == ReflectionType::Homing)
	{
		if (owner.ref())
			dir = Vec3::normalize(owner.ref()->get<Target>()->absolute_pos() - transform->absolute_pos());
		else
			dir = transform->absolute_rot() * Vec3(0, 0, -1.0f);
	}
	else // simple reflection
		dir = (transform->absolute_rot() * Vec3(0, 0, 1.0f)).reflect(normal);

	velocity = dir * 2.0f * speed(type);
	remaining_lifetime = (DRONE_MAX_DISTANCE * 0.99f) / (2.0f * speed(type));
	transform->absolute_rot(Quat::look(dir));
	transform->absolute_pos(transform->absolute_pos() + dir * BOLT_LENGTH);

	if (reflection_type == ReflectionType::Homing)
	{
		// change team
		AI::Team reflect_team;
		AI::entity_info(hit_object, team, &reflect_team);
		BoltNet::reflect(this, reflect_team, PlayerManager::owner(hit_object), hit_object);
	}
	else
		BoltNet::reflect(this);
}

void Bolt::hit_entity(const Hit& hit)
{
	Entity* hit_object = hit.entity;

	b8 destroy = true;

	b8 hit_force_field_collision = false;
	if (hit_object->has<ForceFieldCollision>())
	{
		hit_object = hit_object->get<ForceFieldCollision>()->field.ref()->entity();
		hit_force_field_collision = true;
	}

	Vec3 basis;
	if (hit_object->has<Health>())
	{
		basis = Vec3::normalize(velocity);
		s8 damage = 1;
		switch (type)
		{
			case Type::DroneBolter:
			{
				if (hit_object->has<Turret>() || hit_object->has<Drone>() || hit_object->has<ForceField>() || hit_object->has<CoreModule>())
					damage = 1;
				else if (hit_object->has<Minion>())
					damage = 3;
				else
					damage = 2;
				break;
			}
			case Type::DroneShotgun:
			{
				if (hit_object->has<Minion>())
					damage = MINION_HEALTH;
				else if (hit_object->has<Turret>() || hit_object->has<CoreModule>())
					damage = 1;
				else if (hit_object->has<ForceField>())
					damage = mersenne::rand() % 3 > 0 ? 1 : 0; // expected value: 0.66
				else
					damage = 2;
				break;
			}
			case Type::Minion:
			{
				if (hit_object->has<Turret>())
					damage = 2;
				break;
			}
			case Type::Turret:
			{
				if (hit_object->has<Drone>())
					damage = 2;
				break;
			}
			default:
				vi_assert(false);
				break;
		}

		if (reflected)
			damage += 4;

		if (!can_damage(hit_object))
			damage = 0;

		if (hit_force_field_collision)
		{
			if (!reflected)
			{
				destroy = false;
				reflect(hit_object, ReflectionType::Simple, hit.normal);
			}
		}
		else if (hit_object->get<Health>()->active_armor())
		{
			damage = 0;
			if (hit_object->has<Shield>())
			{
				// reflect
				destroy = false;
				reflect(hit_object);
			}
		}

		if (damage > 0)
		{
			if (hit_object->get<Health>()->damage_buffer_required(entity()))
			{
				// wait for damage buffer
				destroy = false;
				velocity = Vec3::zero;
			}
			hit_object->get<Health>()->damage(entity(), damage);
		}

		if (hit_object->has<RigidBody>())
		{
			RigidBody* body = hit_object->get<RigidBody>();
			body->btBody->applyImpulse(velocity * 0.1f, Vec3::zero);
			body->btBody->activate(true);
		}
	}
	else
		basis = hit.normal;

	{
		ParticleEffect::Type particle_type;
		if (hit_object->has<Health>() && type != Type::DroneShotgun)
			particle_type = ParticleEffect::Type::ImpactLarge;
		else
			particle_type = ParticleEffect::Type::ImpactSmall;
		ParticleEffect::spawn(particle_type, hit.point, Quat::look(basis));
	}

	if (destroy)
		World::remove_deferred(entity());
}

b8 ParticleEffect::spawn(Type t, const Vec3& pos, const Quat& rot)
{
	vi_assert(Game::level.local);
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::ParticleEffect);

	serialize_enum(p, Type, t);
	Vec3 pos2 = pos;
	Net::serialize_position(p, &pos2, Net::Resolution::Low);
	Quat rot2 = rot;
	Net::serialize_quat(p, &rot2, Net::Resolution::Low);

	Net::msg_finalize(p);

	return true;
}

b8 ParticleEffect::net_msg(Net::StreamRead* p)
{
	using Stream = Net::StreamRead;
	Type t;
	serialize_enum(p, Type, t);

	Vec3 pos;
	if (!Net::serialize_position(p, &pos, Net::Resolution::Low))
		net_error();
	Quat rot;
	if (!Net::serialize_quat(p, &rot, Net::Resolution::Low))
		net_error();

	if (t == Type::Grenade)
	{
		Audio::post_global(AK::EVENTS::PLAY_DRONE_GRENADE_EXPLO, pos);
		EffectLight::add(pos, GRENADE_RANGE, 0.35f, EffectLight::Type::Alpha);
	}
	else if (t == Type::Explosion)
	{
		Audio::post_global(AK::EVENTS::PLAY_EXPLOSION, pos);
		EffectLight::add(pos, 8.0f, 0.35f, EffectLight::Type::Alpha);
	}
	else if (t == Type::DroneExplosion)
		EffectLight::add(pos, 8.0f, 0.35f, EffectLight::Type::Alpha);
	else if (t == Type::ImpactLarge || t == Type::ImpactSmall)
		Audio::post_global(AK::EVENTS::PLAY_DRONE_BOLT_IMPACT, pos);
	else if (t == Type::Fizzle)
		Audio::post_global(AK::EVENTS::PLAY_FIZZLE, pos);
	else if (t == Type::SpawnMinion)
		Audio::post_global(AK::EVENTS::PLAY_MINION_SPAWN, pos);
	else if (t == Type::SpawnDrone)
		Audio::post_global(AK::EVENTS::PLAY_DRONE_SPAWN, pos);

	if (t == Type::Grenade)
	{
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		{
			r32 distance = (i.item()->get<Transform>()->absolute_pos() - pos).length();
			if (distance < GRENADE_RANGE * 1.5f)
				i.item()->camera_shake(LMath::lerpf(vi_max(0.0f, (distance - (GRENADE_RANGE * 0.66f)) / (GRENADE_RANGE * (1.5f - 0.66f))), 1.0f, 0.0f));
		}

		for (auto i = Health::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->has<RigidBody>())
			{
				Vec3 to_item = i.item()->get<Transform>()->absolute_pos() - pos;
				r32 distance = to_item.length();
				to_item /= distance;
				if (distance < GRENADE_RANGE)
				{
					RigidBody* body = i.item()->get<RigidBody>();
					body->btBody->applyImpulse(to_item * LMath::lerpf(distance / GRENADE_RANGE, 1.0f, 0.0f) * 10.0f, Vec3::zero);
					body->btBody->activate(true);
				}
			}
		}
	}

	if (t == Type::ImpactLarge || t == Type::SpawnMinion || t == Type::SpawnDrone)
		EffectLight::add(pos, GRENADE_RANGE, 1.5f, EffectLight::Type::Shockwave);

	if (t != Type::SpawnMinion && t != Type::SpawnDrone)
	{
		for (s32 i = 0; i < 50; i++)
		{
			Particles::sparks.add
			(
				pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}
	}
	return true;
}

Array<ShellCasing> ShellCasing::list;

#define SHELL_CASING_LIFETIME 3.0f

Vec3 shell_casing_size(ShellCasing::Type type)
{
	switch (type)
	{
		case ShellCasing::Type::Bolter:
			return Vec3(0.04f, 0.04f, 0.06f);
		case ShellCasing::Type::Shotgun:
			return Vec3(0.06f, 0.06f, 0.08f);
		case ShellCasing::Type::Sniper:
			return Vec3(0.04f, 0.04f, 0.12f);
		default:
		{
			vi_assert(false);
			return Vec3::zero;
			break;
		}
	}
}

void ShellCasing::spawn(const Vec3& pos, const Quat& rot, Type type)
{
	if (!Settings::shell_casings)
		return;

	ShellCasing* entry = list.add();
	entry->type = type;
	entry->pos = pos;
	entry->rot = rot;
	entry->timer = SHELL_CASING_LIFETIME;
	Vec3 size = shell_casing_size(type);
	entry->btShape = new btBoxShape(size);
	btVector3 localInertia(0, 0, 0);
	const r32 mass = 0.01f;
	if (mass > 0.0f)
		entry->btShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo info(mass, 0, entry->btShape, localInertia);

	info.m_startWorldTransform = btTransform(entry->rot, entry->pos);
	entry->btBody = new btRigidBody(info);
	entry->btBody->setWorldTransform(btTransform(entry->rot, entry->pos));

	entry->btBody->setRestitution(1.0f);
	entry->btBody->setUserIndex(-1);
	entry->btBody->setCcdMotionThreshold(size.x);
	entry->btBody->setCcdSweptSphereRadius(size.z);

	entry->btBody->setLinearVelocity(rot * Vec3(-0.707f * 4.0f, 0.707f * 4.0f, 0));

	Physics::btWorld->addRigidBody(entry->btBody, CollisionDroneIgnore, CollisionStatic);
}

void ShellCasing::update_all(const Update& u)
{
	// rigid body transforms are synced in Physics::sync_dynamic()

	if (list.length > 0)
	{
		if (Settings::shell_casings)
		{
			for (s32 i = 0; i < list.length; i++)
			{
				ShellCasing* s = &list[i];
				s->timer -= u.time.delta;
				if (s->timer < 0.0f)
				{
					s->cleanup();
					list.remove(i);
					i--;
				}
			}
		}
		else
			clear();
	}
}

void ShellCasing::clear()
{
	for (s32 i = 0; i < list.length; i++)
		list[i].cleanup();
	list.length = 0;
}

void ShellCasing::cleanup()
{
	Physics::btWorld->removeRigidBody(btBody);
	delete btBody;
	delete btShape;
}

Array<Mat4> ShellCasing::instances;
void ShellCasing::draw_all(const RenderParams& params)
{
	if (!params.camera->mask || !Settings::shell_casings)
		return;

	const Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::tri_tube);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	if (params.technique == RenderTechnique::Shadow || Settings::shadow_quality == Settings::ShadowQuality::Off)
	{
		instances.length = 0;
		for (s32 i = 0; i < list.length; i++)
		{
			const ShellCasing& s = list[i];
			Mat4 m;
			m.make_transform(s.pos, Vec3(1), s.rot);

			Vec3 size = shell_casing_size(s.type);
			if (params.camera->visible_sphere(m.translation(), size.z * f_radius))
			{
				m.scale(size);
				instances.add(m);
			}
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader_permanent(Asset::Shader::standard_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_instanced);
	sync->write(params.technique);

	Mat4 vp = params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::vp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::v);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, MATERIAL_NO_OVERRIDE));

	sync->write(params.flags & RenderFlagEdges ? RenderOp::InstancesEdges : RenderOp::Instances);
	sync->write(Asset::Mesh::tri_tube);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

GrenadeEntity::GrenadeEntity(PlayerManager* owner, const Vec3& abs_pos, const Vec3& velocity)
{
	Transform* transform = create<Transform>();
	transform->absolute_pos(abs_pos);

	create<Audio>();

	Grenade* g = create<Grenade>();
	g->owner = owner;
	g->velocity = Vec3::normalize(velocity) * GRENADE_LAUNCH_SPEED;

	create<RigidBody>(RigidBody::Type::Sphere, Vec3(GRENADE_RADIUS * 2.0f), 0.0f, CollisionTarget, ~CollisionShield & ~CollisionParkour & ~CollisionElectric & ~CollisionStatic & ~CollisionAudio & ~CollisionInaccessible & ~CollisionAllTeamsForceField);

	View* model = create<View>();
	model->mesh = Asset::Mesh::grenade_detached;
	model->color = Team::color_enemy;
	model->team = s8(owner->team.ref()->team());
	model->shader = Asset::Shader::standard;
	model->offset.scale(Vec3(GRENADE_RADIUS));

	create<Health>(SENSOR_HEALTH, SENSOR_HEALTH);

	create<Target>();
}

b8 Grenade::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	Ref<Grenade> ref;
	serialize_ref(p, ref);
	if (ref.ref())
	{
		ref.ref()->get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_ARM);
		ref.ref()->active = true;
	}

	return true;
}

namespace GrenadeNet
{
	b8 send_activate(Grenade* g)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Grenade);
		{
			Ref<Grenade> ref = g;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 grenade_trigger_filter(Entity* e, AI::Team team)
{
	return (e->has<AIAgent>() && e->get<AIAgent>()->team != team && !e->get<AIAgent>()->stealth)
		|| (e->has<ForceField>() && e->get<ForceField>()->team != team)
		|| (e->has<ForceFieldCollision>() && e->get<ForceFieldCollision>()->field.ref()->team != team)
		|| (e->has<Sensor>() && !e->has<Battery>() && e->get<Sensor>()->team != team)
		|| (e->has<Turret>() && e->get<Turret>()->team != team)
		|| (e->has<CoreModule>() && e->get<CoreModule>()->team != team);
}

b8 grenade_hit_filter(Entity* e, AI::Team team)
{
	return grenade_trigger_filter(e, team)
		|| (e->has<AIAgent>() && e->get<AIAgent>()->team != team); // don't care if the AIAgent is stealthed or not
}

// returns true if grenade hits something
b8 Grenade::simulate(r32 dt, Bolt::Hit* out_hit, Net::StateFrame* state_frame)
{
	vi_assert(Game::level.local);
	Transform* t = get<Transform>();
	if (!t->parent.ref())
	{
		Vec3 pos = t->absolute_pos();
		Vec3 next_pos;
		{
			Vec3 half_accel = Physics::btWorld->getGravity() * dt * 0.5f;
			velocity += half_accel;
			next_pos = pos + velocity * dt;
			velocity += half_accel;
		}

		if (next_pos.y < Game::level.min_y)
		{
			World::remove_deferred(entity());
			return false;
		}

		if (!btVector3(next_pos - pos).fuzzyZero())
		{
			Vec3 v = velocity;
			if (v.length_squared() > 0.0f)
				v.normalize();

			Bolt::Hit hit;
			if (Bolt::raycast(pos, next_pos, CollisionStatic | (CollisionAllTeamsForceField & Bolt::raycast_mask(team())), &hit, state_frame))
			{
				if (out_hit)
					*out_hit = hit;

				if (!state_frame) // if server is fast-forward simulating us, we can't register the hit ourselves
					hit_entity(hit);

				return true;
			}
		}

		t->pos = next_pos;
	}

	if (!state_frame && active && timer > GRENADE_DELAY)
		explode();

	return false;
}

void Grenade::hit_entity(const Bolt::Hit& hit)
{
	if (grenade_hit_filter(hit.entity, team()))
		explode();
	else
	{
		if (active)
		{
			// attach
			velocity = Vec3::zero;
			get<Transform>()->parent = hit.entity->get<Transform>();
			get<Transform>()->absolute(hit.point + hit.normal * GRENADE_RADIUS * 1.1f, Quat::look(hit.normal));
		}
		else
		{
			// bounce
			velocity = velocity.reflect(hit.normal) * 0.5f;
			GrenadeNet::send_activate(this);
		}
	}
}

void Grenade::explode()
{
	vi_assert(Game::level.local);
	Vec3 me = get<Transform>()->absolute_pos();
	ParticleEffect::spawn(ParticleEffect::Type::Grenade, me, Quat::look(Vec3(0, 1, 0))); // also handles physics impulses

	AI::Team my_team = team();
	u32 my_hash = ForceField::hash(my_team, me);

	for (auto i = Health::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->can_take_damage())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();
			if (i.item()->has<ForceField>() || ForceField::hash(my_team, pos) == my_hash)
			{
				r32 distance = (pos - me).length();

				if (i.item()->has<ForceField>())
					distance -= FORCE_FIELD_RADIUS;

				if (i.item()->has<Drone>())
				{
					r32 multiplier = i.item()->get<AIAgent>()->team == my_team ? 0.5f : 1.0f;
					if (distance < multiplier * GRENADE_RANGE * 0.4f)
						i.item()->damage(entity(), 3);
					else if (distance < multiplier * GRENADE_RANGE * 0.7f)
						i.item()->damage(entity(), 2);
					else if (distance < multiplier * GRENADE_RANGE)
						i.item()->damage(entity(), 1);
				}
				else if (distance < GRENADE_RANGE && !i.item()->has<Battery>())
					i.item()->damage(entity(), distance < GRENADE_RANGE * 0.5f ? 6 : (distance < GRENADE_RANGE * 0.75f ? 3 : 1));
			}
		}
	}

	World::remove_deferred(entity());
}

void Grenade::set_owner(PlayerManager* m)
{
	// not synced over network
	owner = m;
	get<View>()->team = m ? s8(m->team.ref()->team()) : AI::TeamNone;
	if (!m)
		active = false;
}

AI::Team Grenade::team() const
{
	return owner.ref() ? owner.ref()->team.ref()->team() : AI::TeamNone;
}

r32 Grenade::particle_accumulator;
void Grenade::update_client_all(const Update& u)
{
	// normal particles
	const r32 interval = 0.05f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (auto i = list.iterator(); !i.is_last(); i.next())
		{
			Transform* t = i.item()->get<Transform>();
			if (t->parent.ref())
			{
				View* v = i.item()->get<View>();
				if (v->mesh != Asset::Mesh::grenade_attached)
				{
					v->mesh = Asset::Mesh::grenade_attached;
					i.item()->get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_ATTACH);
				}
			}
			else
				Particles::tracers.add(t->absolute_pos(), Vec3::zero, 0);
		}
	}

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->active)
		{
			Vec3 me = i.item()->get<Transform>()->absolute_pos();
			AI::Team my_team = i.item()->team();
			b8 countdown = false;
			for (auto i = Health::list.iterator(); !i.is_last(); i.next())
			{
				if (grenade_trigger_filter(i.item()->entity(), my_team))
				{
					r32 distance = (i.item()->get<Transform>()->absolute_pos() - me).length();
					if (i.item()->has<ForceField>())
						distance -= FORCE_FIELD_RADIUS;
					if (distance < GRENADE_RANGE * 0.8f)
					{
						countdown = true;
						break;
					}
				}
			}
			if (countdown)
			{
				r32 timer_last = i.item()->timer;
				i.item()->timer += u.time.delta;
				const r32 interval = 1.5f;
				if (timer_last == 0.0f || s32(Ease::cubic_in<r32>(i.item()->timer) / interval) != s32(Ease::cubic_in<r32>(timer_last) / interval))
					i.item()->get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_BEEP);
			}
			else
			{
				i.item()->timer = 0.0f;
				const r32 interval = 5.0f;
				if (s32(Game::time.total / interval) != s32((Game::time.total - u.time.delta) / interval))
				{
					EffectLight::add(me, GRENADE_RANGE, 1.5f, EffectLight::Type::Shockwave);
					i.item()->get<Audio>()->post(AK::EVENTS::PLAY_GRENADE_BEEP);
				}
			}
		}
	}
}

void Grenade::awake()
{
	link_arg<Entity*, &Grenade::killed_by>(get<Health>()->killed);
	link_arg<const TargetEvent&, &Grenade::hit_by>(get<Target>()->target_hit);
}

void Grenade::hit_by(const TargetEvent& e)
{
	get<Health>()->kill(e.hit_by);
}

void Grenade::killed_by(Entity* e)
{
	PlayerManager::entity_killed_by(entity(), e);
	if (Game::level.local)
	{
		if (e->has<Grenade>())
			explode();
		else
			World::remove_deferred(entity());
	}
}

Vec3 Target::velocity() const
{
	if (has<Drone>())
		return get<Drone>()->velocity;
	else if (Game::level.local)
	{
		if (has<Parkour>() && !get<PlayerControlHuman>()->local())
			return net_velocity;
		else if (has<RigidBody>())
			return get<RigidBody>()->btBody->getInterpolationLinearVelocity();
		else
			return Vec3::zero;
	}
	else
		return net_velocity;
}

b8 Target::predict_intersection(const Vec3& from, r32 speed, const Net::StateFrame* state_frame, Vec3* intersection) const
{
	Vec3 pos;
	Vec3 v;
	if (state_frame)
	{
		Quat rot;
		Net::transform_absolute(*state_frame, get<Transform>()->id(), &pos, &rot);
		pos += rot * local_offset; // todo possibly: rewind local_offset as well?

		Net::StateFrame state_frame_last;
		Net::state_frame_by_timestamp(&state_frame_last, state_frame->timestamp - Net::tick_rate());
		Vec3 pos_last;
		Quat rot_last;
		Net::transform_absolute(state_frame_last, get<Transform>()->id(), &pos_last, &rot_last);
		pos_last += rot_last * local_offset;

		v = (pos - pos_last) / Net::tick_rate();
	}
	else
	{
		v = velocity();
		pos = absolute_pos();
	}

	Vec3 to_target = pos - from;
	r32 intersect_time_squared = to_target.dot(to_target) / ((speed * speed) - 2.0f * to_target.dot(v) - v.dot(v));
	if (intersect_time_squared > 0.0f)
	{
		*intersection = pos + v * sqrtf(intersect_time_squared);
		return true;
	}
	else
		return false;
}

r32 Target::radius() const
{
	if (has<Walker>())
		return MINION_HEAD_RADIUS;
	else if (has<Turret>())
		return TURRET_RADIUS;
	else if (has<Shield>())
		return DRONE_SHIELD_RADIUS;
	else
		return get<RigidBody>()->size.y;
}

void Target::hit(Entity* hit_by)
{
	TargetEvent e;
	e.hit_by = hit_by;
	e.target = entity();
	target_hit.fire(e);
}

Vec3 Target::absolute_pos() const
{
	return get<Transform>()->to_world(local_offset);
}

PlayerTrigger::PlayerTrigger()
	: entered(), exited(), triggered(), radius(1.0f)
{

}

b8 PlayerTrigger::is_triggered(const Entity* e) const
{
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (e == triggered[i].ref())
			return true;
	}
	return false;
}

b8 PlayerTrigger::is_triggered() const
{
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
			return true;
	}
	return false;
}

void PlayerTrigger::update(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();
	r32 radius_squared = radius * radius;
	for (s32 i = 0; i < max_trigger; i++)
	{
		Entity* e = triggered[i].ref();
		if (e && (e->get<Transform>()->absolute_pos() - pos).length_squared() > radius_squared)
		{
			triggered[i] = nullptr;
			exited.fire(e);
		}
	}

	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		Entity* e = i.item()->entity();
		if ((e->get<Transform>()->absolute_pos() - pos).length_squared() < radius_squared)
		{
			b8 already_triggered = false;
			s32 free_slot = -1;
			for (s32 i = 0; i < max_trigger; i++)
			{
				if (free_slot == -1 && !triggered[i].ref())
					free_slot = i;

				if (triggered[i].ref() == e)
				{
					already_triggered = true;
					break;
				}
			}

			if (!already_triggered && free_slot != -1)
			{
				triggered[free_slot] = e;
				entered.fire(e);
			}
		}
	}
}

s32 PlayerTrigger::count() const
{
	s32 count = 0;
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
			count++;
	}
	return count;
}

Array<Mat4> Rope::instances;

// draw rope segments and bolts
void Rope::draw_all(const RenderParams& params)
{
	if (!params.camera->mask)
		return;

	const Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::tri_tube);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	if (params.technique == RenderTechnique::Shadow || Settings::shadow_quality == Settings::ShadowQuality::Off)
	{
		instances.length = 0;
		// ropes
		{
			static const Vec3 scale = Vec3(ROPE_RADIUS, ROPE_RADIUS, ROPE_SEGMENT_LENGTH * 0.5f);

			for (auto i = list.iterator(); !i.is_last(); i.next())
			{
				Mat4 m;
				i.item()->get<Transform>()->mat(&m);

				if (params.camera->visible_sphere(m.translation(), ROPE_SEGMENT_LENGTH * f_radius))
				{
					m.scale(scale);
					instances.add(m);
				}
			}
		}

		// bolts
		static const Vec3 scale = Vec3(BOLT_THICKNESS, BOLT_THICKNESS, BOLT_LENGTH * 0.5f);
		static const Mat4 offset = Mat4::make_translation(0, 0, BOLT_LENGTH * 0.5f);
		for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
		{
			Mat4 m;
			i.item()->get<Transform>()->mat(&m);
			m = offset * m;
			if (i.item()->visible() // if the bolt is waiting for damage buffering, don't draw it
				&& params.camera->visible_sphere(m.translation(), BOLT_LENGTH * f_radius))
			{
				m.scale(scale);
				instances.add(m);
			}
		}

		// fake bolts
		for (auto i = EffectLight::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->type == EffectLight::Type::BoltDroneBolter
				|| i.item()->type == EffectLight::Type::BoltDroneShotgun)
			{
				Mat4 m;
				m.make_transform(i.item()->pos, scale, i.item()->rot);
				m = offset * m;
				if (params.camera->visible_sphere(m.translation(), BOLT_LENGTH * f_radius))
					instances.add(m);
			}
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader_permanent(Asset::Shader::flat_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::flat_instanced);
	sync->write(params.technique);

	Mat4 vp = params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::vp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::v);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, 1));

	sync->write(RenderOp::Instances);
	sync->write(Asset::Mesh::tri_tube);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

RigidBody* rope_add(RigidBody* start, const Vec3& start_relative_pos, const Vec3& pos, const Quat& rot, r32 slack, RigidBody::Constraint::Type constraint_type)
{
	RigidBody* last_segment = start;
	Vec3 last_segment_relative_pos = start_relative_pos;
	Vec3 forward = rot * Vec3(0, 0, 1);
	while (true)
	{
		if (last_segment)
		{
			Vec3 last_segment_pos = last_segment->get<Transform>()->to_world(last_segment_relative_pos);
			Vec3 diff = pos - last_segment_pos;
			r32 length = diff.dot(forward);
			r32 rope_interval = ROPE_SEGMENT_LENGTH / (1.0f + slack);
			Vec3 scale = Vec3(ROPE_RADIUS, ROPE_RADIUS, ROPE_SEGMENT_LENGTH * 0.5f);

			if (length > rope_interval * 0.5f)
			{
				if (last_segment->has<Rope>())
					Net::finalize(last_segment->entity());

				Vec3 spawn_pos = last_segment_pos + (diff / length) * rope_interval * 0.5f;
				Entity* box = World::create<PhysicsEntity>(AssetNull, spawn_pos, rot, RigidBody::Type::CapsuleZ, Vec3(ROPE_RADIUS, ROPE_SEGMENT_LENGTH - ROPE_RADIUS * 2.0f, 0.0f), 0.05f, CollisionDroneIgnore, ~CollisionWalker & ~CollisionAllTeamsForceField);
				box->add<Rope>();

				static Quat rotation_a = Quat::look(Vec3(0, 0, 1)) * Quat::euler(0, PI * -0.5f, 0);
				static Quat rotation_b = Quat::look(Vec3(0, 0, -1)) * Quat::euler(PI, PI * -0.5f, 0);

				RigidBody::Constraint constraint = RigidBody::Constraint();
				constraint.type = constraint_type;
				constraint.frame_a = btTransform(rotation_b, Vec3(0, 0, ROPE_SEGMENT_LENGTH * -0.5f));
				constraint.frame_b = btTransform(rotation_a, last_segment_relative_pos),
				constraint.limits = Vec3(PI, PI, 0);
				constraint.a = box->get<RigidBody>(); // this must be constraint A in order for the netcode to pick up on the constraint
				constraint.b = last_segment;
				RigidBody::add_constraint(constraint);

				box->get<RigidBody>()->set_ccd(true);
				box->get<RigidBody>()->set_damping(0.5f, 0.5f);
				last_segment = box->get<RigidBody>();
				last_segment_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
			}
			else
				break;
		}
		else
			break;
	}

	if (last_segment == start) // we didn't add any rope segments
		return nullptr;
	else
		return last_segment;
}

Rope* Rope::start(RigidBody* start, const Vec3& abs_pos, const Vec3& abs_normal, const Quat& abs_rot, r32 slack)
{
	Entity* base = World::create<Prop>(Asset::Mesh::rope_base);
	base->get<Transform>()->absolute(abs_pos, Quat::look(abs_normal));
	base->get<Transform>()->reparent(start->get<Transform>());
	Net::finalize(base);

	// add the first rope segment
	Vec3 p = abs_pos + abs_normal * ROPE_RADIUS;
	Transform* start_trans = start->get<Transform>();
	RigidBody* rope = rope_add(start, start_trans->to_local(p), p + abs_rot * Vec3(0, 0, ROPE_SEGMENT_LENGTH), abs_rot, slack, RigidBody::Constraint::Type::PointToPoint);
	vi_assert(rope); // should never happen
	return rope->get<Rope>();
}

void Rope::end(const Vec3& pos, const Vec3& normal, RigidBody* end, r32 slack, b8 add_cap)
{
	Vec3 abs_pos = pos + normal * ROPE_RADIUS;
	RigidBody* start = get<RigidBody>();
	Vec3 start_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
	RigidBody* last = rope_add(start, start_relative_pos, abs_pos, Quat::look(Vec3::normalize(abs_pos - get<Transform>()->to_world(start_relative_pos))), slack, RigidBody::Constraint::Type::ConeTwist);
	if (!last) // we didn't need to add any rope segments; just attach ourselves to the end point
		last = start;

	if (add_cap)
	{
		Entity* base = World::create<Prop>(Asset::Mesh::rope_base);
		base->get<Transform>()->absolute(pos, Quat::look(normal));
		base->get<Transform>()->reparent(end->get<Transform>());
		Net::finalize(base);
	}

	RigidBody::Constraint constraint = RigidBody::Constraint();
	constraint.type = RigidBody::Constraint::Type::PointToPoint;
	constraint.frame_a = btTransform(Quat::identity, start_relative_pos);
	constraint.frame_b = btTransform(Quat::identity, end->get<Transform>()->to_local(abs_pos));
	constraint.a = last;
	constraint.b = end; // this must be constraint A in order for the netcode to pick up on the constraint
	RigidBody::add_constraint(constraint);

	Net::finalize(last->entity());
}

void Rope::spawn(const Vec3& pos, const Vec3& dir, r32 max_distance, r32 slack, b8 attach_end)
{
	Vec3 dir_normalized = Vec3::normalize(dir);
	Vec3 start_pos = pos;
	Vec3 end = start_pos + dir_normalized * max_distance;
	btCollisionWorld::ClosestRayResultCallback ray_callback(start_pos, end);
	Physics::raycast(&ray_callback, btBroadphaseProxy::AllFilter & ~CollisionAllTeamsForceField);
	if (ray_callback.hasHit())
	{
		Vec3 end2 = start_pos + dir_normalized * -max_distance;

		btCollisionWorld::ClosestRayResultCallback ray_callback2(start_pos, end2);
		Physics::raycast(&ray_callback2, btBroadphaseProxy::AllFilter & ~CollisionAllTeamsForceField);

		if (!attach_end || ray_callback2.hasHit())
		{
			RigidBody* a = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();

			Rope* rope = Rope::start(a, ray_callback.m_hitPointWorld, ray_callback.m_hitNormalWorld, Quat::look(ray_callback.m_hitNormalWorld), slack);

			if (rope)
			{
				if (attach_end && ray_callback2.hasHit())
				{
					// attach on both ends
					RigidBody* b = Entity::list[ray_callback2.m_collisionObject->getUserIndex()].get<RigidBody>();
					rope->end(ray_callback2.m_hitPointWorld, ray_callback2.m_hitNormalWorld, b, slack);
				}
				else
				{
					// only attached on one end
					Vec3 start_relative_pos = Vec3(0, 0, ROPE_SEGMENT_LENGTH * 0.5f);
					rope_add(rope->get<RigidBody>(), start_relative_pos, end2, Quat::look(Vec3::normalize(end2 - rope->get<Transform>()->to_world(start_relative_pos))), slack, RigidBody::Constraint::Type::ConeTwist);
				}
			}
		}
	}
}

WaterEntity::WaterEntity(AssetID mesh_id)
{
	create<Transform>();
	create<Water>(mesh_id);
	create<Audio>();
}

PinArray<EffectLight, MAX_ENTITIES> EffectLight::list;

EffectLight* EffectLight::add(const Vec3& pos, r32 radius, r32 duration, Type t, Transform* parent, Quat rot)
{
	EffectLight* s = list.add();
	s->rot = rot;
	s->pos = parent ? parent->to_local(pos) : pos;
	s->max_radius = radius;
	s->duration = duration;
	s->type = t;
	s->parent = parent;
	s->revision++;
	s->timer = 0.0f;
	return s;
}

void EffectLight::remove(EffectLight* s)
{
	s->revision++;
	list.remove(s->id());
}

void EffectLight::draw_alpha(const RenderParams& params)
{
	// "Light" and "Wave" type shockwaves get rendered in loop.h, not here
	Loader::mesh_permanent(Asset::Mesh::sphere_highres);
	Loader::shader_permanent(Asset::Shader::fresnel);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::fresnel);
	sync->write(params.technique);

	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		r32 radius = i.item()->radius();
		Vec3 pos = i.item()->absolute_pos();
		if (i.item()->type != Type::Alpha || !params.camera->visible_sphere(pos, radius))
			continue;

		Mat4 m;
		m.make_transform(pos, Vec3(radius), Quat::identity);
		Mat4 mvp = m * params.view_projection;

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::mvp);
		sync->write(RenderDataType::Mat4);
		sync->write<s32>(1);
		sync->write<Mat4>(mvp);

		sync->write(RenderOp::Uniform);
		sync->write(Asset::Uniform::diffuse_color);
		sync->write(RenderDataType::Vec4);
		sync->write<s32>(1);
		sync->write<Vec4>(Vec4(1, 1, 1, i.item()->opacity()));

		sync->write(RenderOp::Mesh);
		sync->write(RenderPrimitiveMode::Triangles);
		sync->write(Asset::Mesh::sphere_highres);
	}
}

r32 EffectLight::radius() const
{
	switch (type)
	{
		case Type::BoltDroneBolter:
			return max_radius;
		case Type::BoltDroneShotgun:
			return 0.0f;
		case Type::Shockwave:
			return Ease::cubic_out(timer / duration, 0.0f, max_radius);
		case Type::Spark:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, max_radius * 0.25f, max_radius);
		}
		case Type::Alpha:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 0.0f, max_radius);
		}
		default:
		{
			vi_assert(false);
			return 0.0f;
		}
	}
}

r32 EffectLight::opacity() const
{
	switch (type)
	{
		case Type::BoltDroneBolter:
		case Type::BoltDroneShotgun:
		{
			return 1.0f;
		}
		case Type::Spark:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 1.0f, 0.0f);
		}
		case Type::Shockwave:
		{
			r32 fade_radius = max_radius * (2.0f / 15.0f);
			r32 fade = 1.0f - vi_max(0.0f, ((radius() - (max_radius - fade_radius)) / fade_radius));
			return fade * 0.8f;
		}
		case Type::Alpha:
		{
			r32 blend = Ease::cubic_in<r32>(timer / duration);
			return LMath::lerpf(blend, 0.8f, 0.0f);
		}
		default:
		{
			vi_assert(false);
			return 0.0f;
		}
	}
}

Vec3 EffectLight::absolute_pos() const
{
	Transform* p = parent.ref();
	return p ? p->to_world(pos) : pos;
}

void EffectLight::update(const Update& u)
{
	timer += u.time.delta;
	if (timer > duration)
		remove(this);
	else if (type == Type::BoltDroneBolter)
		pos += rot * Vec3(0, 0, u.time.delta * BOLT_SPEED_DRONE_BOLTER);
	else if (type == Type::BoltDroneShotgun)
		pos += rot * Vec3(0, 0, u.time.delta * BOLT_SPEED_DRONE_SHOTGUN);
}

CollectibleEntity::CollectibleEntity(ID save_id, Resource type, s16 amount)
{
	create<Transform>();

	PointLight* light = create<PointLight>();
	light->radius = 6.0f;
	light->offset = Vec3(0, 0, 0.2f);

	Collectible* c = create<Collectible>();
	c->save_id = save_id;
	c->type = type;
	c->amount = amount;
	switch (type)
	{
		case Resource::AccessKeys:
		case Resource::Energy:
		{
			// simple models
			View* v = create<View>(type == Resource::AccessKeys ? Asset::Mesh::access_key : Asset::Mesh::energy);
			v->shader = Asset::Shader::standard;
			v->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);
			break;
		}
		case Resource::Drones:
		{
			// animated model
			SkinnedModel* model = create<SkinnedModel>();
			model->mesh = Asset::Mesh::drone;
			model->shader = Asset::Shader::armature;
			model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);
			model->offset.translate(Vec3(0, 0, DRONE_RADIUS));

			Animator* anim = create<Animator>();
			anim->armature = Asset::Armature::drone;
			anim->layers[0].behavior = Animator::Behavior::Loop;
			anim->layers[0].animation = Asset::Animation::drone_fly;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void Collectible::give_rewards()
{
	s16 a = amount;
	if (a == 0)
	{
		switch (type)
		{
			case Resource::AccessKeys:
			{
				a = 1;
				break;
			}
			case Resource::Energy:
			{
				a = 100;
				break;
			}
			case Resource::Drones:
			{
				a = 5;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
	if (Game::level.local)
		Overworld::resource_change(type, a);

	Game::save.collectibles.add({ Game::level.id, save_id });

	char msg[512];
	sprintf(msg, _(strings::resource_collected), a, _(Overworld::resource_info[s32(type)].description));
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->msg(msg, true);
	collected.fire();
}

Interactable* Interactable::closest(const Vec3& pos)
{
	r32 distance_sq = SPAWN_POINT_RADIUS * 0.35f * SPAWN_POINT_RADIUS * 0.35f;
	Interactable* result = nullptr;
	// find the closest interactable
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Vec3 i_pos(0.4f, 1.1f, 0);
		Quat i_rot = Quat::identity;
		i.item()->get<Transform>()->to_world(&i_pos, &i_rot);
		Vec3 to_interactable = i_pos - pos;
		r32 d = to_interactable.length_squared();
		if (d < distance_sq)
		{
			distance_sq = d;
			result = i.item();
		}
	}
	return result;
}

Interactable::Interactable(Type t)
	: type(t),
	user_data(),
	interacted()
{
}

void Interactable::awake()
{
	if (has<Animator>())
	{
		link<&Interactable::animation_callback>(get<Animator>()->trigger(Asset::Animation::interactable_interact, 1.916f));
		link<&Interactable::animation_callback>(get<Animator>()->trigger(Asset::Animation::interactable_interact_disable, 1.916f));
	}
	switch (type)
	{
		case Type::Terminal:
		{
			interacted.link(&TerminalInteractable::interacted);
			break;
		}
		case Type::Tram:
		{
			interacted.link(&TramInteractableEntity::interacted);
			break;
		}
		case Type::Shop:
		{
			interacted.link(&ShopInteractable::interacted);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

namespace InteractableNet
{
	b8 send_msg(Interactable* i)
	{
		using Stream = Net::StreamWrite;
		Net::StreamWrite* p = Net::msg_new(Net::MessageType::Interactable);
		{
			Ref<Interactable> ref = i;
			serialize_ref(p, ref);
		}
		Net::msg_finalize(p);
		return true;
	}
}

b8 Interactable::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	Ref<Interactable> ref;
	serialize_ref(p, ref);
	Interactable* i = ref.ref();
	if (i && Game::level.mode == Game::Mode::Parkour)
	{
		if (Game::level.local)
		{
			if (src == Net::MessageSource::Remote)
				InteractableNet::send_msg(i); // need to send out this message to everyone
			else if (src == Net::MessageSource::Loopback)
				i->interacted.fire(i);
			else
				vi_assert(false);
		}
		else // client
		{
			if (src == Net::MessageSource::Loopback)
				i->interacted.fire(i);
		}
	}
	return true;
}

void Interactable::interact()
{
	Animator* anim = get<Animator>();
	if (anim->layers[1].animation == AssetNull)
	{
		if (type == Type::Terminal)
		{
			anim->layers[0].animation = Asset::Animation::interactable_disabled;
			anim->layers[1].play(Asset::Animation::interactable_interact_disable);
		}
		else
		{
			anim->layers[0].animation = Asset::Animation::interactable_enabled;
			anim->layers[1].play(Asset::Animation::interactable_interact);
		}
	}
}

void Interactable::interact_no_animation()
{
	InteractableNet::send_msg(this);
}

void Interactable::animation_callback()
{
	InteractableNet::send_msg(this);
}

b8 Interactable::is_present(Type t)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->type == t)
			return true;
	}
	return false;
}

ShopEntity::ShopEntity()
{
	create<Transform>();

	View* model = create<View>();
	model->mesh = Asset::Mesh::shop;
	model->shader = Asset::Shader::standard;
	model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);

	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAudio & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric, Asset::Mesh::shop_collision);
	body->set_restitution(0.75f);
}

ShopInteractable::ShopInteractable()
{
	create<Transform>();
	create<Interactable>(Interactable::Type::Shop);
}

void ShopInteractable::interacted(Interactable*)
{
}

void TerminalEntity::open()
{
	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	animator->layers[0].play(Asset::Animation::terminal_opened);
	animator->layers[1].play(Asset::Animation::terminal_open);
	animator->get<Audio>()->post(AK::EVENTS::PLAY_TERMINAL_OPEN);
}

void TerminalEntity::close()
{
	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	animator->layers[0].animation = AssetNull;
	animator->layers[1].play(Asset::Animation::terminal_close);
	animator->get<Audio>()->post(AK::EVENTS::PLAY_TERMINAL_CLOSE);
}

void TerminalEntity::closed()
{
	if (Game::level.local)
		Overworld::show(PlayerHuman::list.iterator().item()->camera.ref(), Overworld::State::StoryMode);
}

TerminalEntity::TerminalEntity()
{
	Transform* transform = create<Transform>();
	
	create<Audio>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::terminal;
	model->shader = Asset::Shader::armature;
	model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::terminal;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[0].animation = Game::save.zones[Game::level.id] == ZoneState::Locked ? AssetNull : Asset::Animation::terminal_opened;
	anim->layers[1].blend_time = 0.0f;
	anim->trigger(Asset::Animation::terminal_close, 1.33f).link(&closed);

	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAudio & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric, Asset::Mesh::terminal_collision);
	body->set_restitution(0.75f);
}

TerminalInteractable::TerminalInteractable()
{
	Transform* transform = create<Transform>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::interactable;
	model->shader = Asset::Shader::armature;
	model->alpha();

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::interactable;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].animation = Game::save.zones[Game::level.id] == ZoneState::Locked ? Asset::Animation::interactable_enabled : Asset::Animation::interactable_disabled;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[1].blend_time = 0.0f;

	create<Interactable>(Interactable::Type::Terminal);
}

void TerminalInteractable::interacted(Interactable*)
{
	vi_assert(Game::level.mode == Game::Mode::Parkour);

	Animator* animator = Game::level.terminal.ref()->get<Animator>();
	if (animator->layers[1].animation == AssetNull) // make sure nothing's happening already
	{
		ZoneState zone_state = Game::save.zones[Game::level.id];
		if (zone_state == ZoneState::Locked)
		{
			if (Game::level.local)
				Overworld::zone_change(Game::level.id, ZoneState::PvpHostile);
			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->msg(_(strings::zone_unlocked), true);
			TerminalEntity::open();
		}
		else if (zone_state == ZoneState::PvpHostile)
		{
			if (Game::level.local)
				Overworld::resource_change(Resource::Drones, -DEFAULT_ASSAULT_DRONES);
			TerminalEntity::close();
		}
	}
}

const r32 TRAM_LENGTH = 3.7f * 2.0f;
const r32 TRAM_SPEED_MAX = 10.0f;
const r32 TRAM_WIDTH = TRAM_LENGTH * 0.5f;
const r32 TRAM_HEIGHT = 2.54f;
const r32 TRAM_ROPE_LENGTH = 4.0f;

TramRunnerEntity::TramRunnerEntity(s8 track, b8 is_front)
{
	create<Transform>();
	View* model = create<View>(Asset::Mesh::tram_runner);
	model->shader = Asset::Shader::standard;
	model->color.w = MATERIAL_INACCESSIBLE;
	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAudio & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric, Asset::Mesh::tram_runner);
	body->set_restitution(0.75f);
	TramRunner* r = create<TramRunner>();
	r->track = track;
	r->is_front = is_front;

	create<Audio>();

	const Game::TramTrack& t = Game::level.tram_tracks[track];
	r32 offset;
	if (Game::save.zone_last == t.level && !Game::level.post_pvp)
	{
		offset = t.points[t.points.length - 1].offset - TRAM_LENGTH;
		r->velocity = -TRAM_SPEED_MAX;
		r->state = TramRunner::State::Arriving;
	}
	else
		offset = 0.0f;

	if (is_front)
	{
		offset += TRAM_LENGTH;
		r->target_offset += TRAM_LENGTH;
	}
	r->set(offset);
}

void TramRunner::set(r32 x)
{
	const Game::TramTrack& t = Game::level.tram_tracks[track];
	while (true)
	{
		const Game::TramTrack::Point current = t.points[offset_index];
		if (offset_index > 0 && x < current.offset)
			offset_index--;
		else if (offset_index < t.points.length - 2 && t.points[offset_index + 1].offset < x)
			offset_index++;
		else
			break;
	}

	const Game::TramTrack::Point current = t.points[offset_index];
	const Game::TramTrack::Point next = t.points[offset_index + 1];
	r32 blend = (x - current.offset) / (next.offset - current.offset);
	Transform* transform = get<Transform>();
	transform->pos = Vec3::lerp(blend, current.pos, next.pos);
	transform->rot = Quat::look(Vec3::normalize(next.pos - current.pos));
	offset = x;
}

void TramRunner::go(s8 track, r32 x, State s)
{
	vi_assert(Game::level.local);
	const Game::TramTrack& t = Game::level.tram_tracks[track];
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->track == track)
		{
			i.item()->state = s;
			i.item()->target_offset = x * (t.points[t.points.length - 1].offset - TRAM_LENGTH);
			if (i.item()->is_front)
				i.item()->target_offset += TRAM_LENGTH;
		}
	}
}

namespace TramNet
{
	enum Message
	{
		Entered,
		Exited,
		Arrived,
		DoorsOpen,
		count,
	};

	b8 send(Tram*, Message);
};

void TramRunner::awake()
{
	get<Audio>()->post(AK::EVENTS::PLAY_TRAM_LOOP);
	get<Audio>()->param(AK::GAME_PARAMETERS::TRAM_LOOP, 0.0f);
}

void TramRunner::update_server(const Update& u)
{
	const r32 ACCEL_TIME = 5.0f;
	const r32 ACCEL_MAX = TRAM_SPEED_MAX / ACCEL_TIME;
	const r32 ACCEL_DISTANCE = TRAM_SPEED_MAX * ACCEL_TIME - 0.5f * ACCEL_MAX * (ACCEL_TIME * ACCEL_TIME);

	{
		const Game::TramTrack& t = Game::level.tram_tracks[track];
		if (is_front
			&& state == State::Departing
			&& Game::scheduled_load_level == AssetNull
			&& offset > t.points[t.points.length - 1].offset) // we hit our goal, we are the front runner, we're a local game, and we're exiting the level
			Game::schedule_load_level(t.level, Game::Mode::Parkour);
	}

	r32 error = target_offset - offset;
	r32 distance = fabsf(error);
	r32 dv_half = ACCEL_MAX * u.time.delta * 0.5f;
	if (state == State::Departing || distance > ACCEL_DISTANCE) // accelerating to max speed
	{
		get<RigidBody>()->activate_linked();
		if (error > 0.0f)
			velocity = vi_min(TRAM_SPEED_MAX, velocity + dv_half);
		else
			velocity = vi_max(-TRAM_SPEED_MAX, velocity - dv_half);
		set(offset + velocity * u.time.delta);
		if (error > 0.0f)
			velocity = vi_min(TRAM_SPEED_MAX, velocity + dv_half);
		else
			velocity = vi_max(-TRAM_SPEED_MAX, velocity - dv_half);
	}
	else if (distance == 0.0f) // stopped
	{
		if (state == State::Arriving)
		{
			if (is_front)
				TramNet::send(Tram::by_track(track), TramNet::Message::Arrived);
			state = State::Idle;
		}

		get<RigidBody>()->btBody->setActivationState(ISLAND_SLEEPING);
	}
	else // decelerating
	{
		if (velocity > 0.0f)
		{
			velocity = vi_max(0.01f, velocity - dv_half);
			set(vi_min(target_offset, offset + velocity * u.time.delta));
		}
		else
		{
			velocity = vi_min(-0.01f, velocity + dv_half);
			set(vi_max(target_offset, offset + velocity * u.time.delta));
		}
		if (velocity > 0.0f)
			velocity = vi_max(0.01f, velocity - dv_half);
		else
			velocity = vi_min(-0.01f, velocity + dv_half);
	}
}

void TramRunner::update_client(const Update& u)
{
	get<Audio>()->param(AK::GAME_PARAMETERS::TRAM_LOOP, fabsf(velocity) / TRAM_SPEED_MAX);
	if (get<RigidBody>()->btBody->isActive() && mersenne::randf_co() < u.time.delta / 3.0f)
	{
		b8 left = mersenne::randf_co() < 0.5f;
		Vec3 pos = get<Transform>()->to_world(Vec3(left ? -0.35f : 0.35f, 0.45f, 0));

		Quat rot = get<Transform>()->absolute_rot() * Quat::euler(0, (left ? PI * -0.5f : PI * 0.5f), 0);
		for (s32 i = 0; i < 15; i++)
		{
			Particles::sparks.add
			(
				pos,
				rot * Vec3(mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo() * 2.0f - 1.0f, mersenne::randf_oo()) * 10.0f,
				Vec4(1, 1, 1, 1)
			);
		}

		get<Audio>()->post(AK::EVENTS::PLAY_TRAM_SPARK);
		EffectLight::add(pos + Vec3(0, -0.2f, 0), 3.0f, 0.25f, EffectLight::Type::Spark, get<Transform>());
	}
}

TramEntity::TramEntity(TramRunner* runner_a, TramRunner* runner_b)
{
	Transform* transform = create<Transform>();

	const Mesh* mesh = Loader::mesh(Asset::Mesh::tram_mesh);
	RigidBody* body = create<RigidBody>(RigidBody::Type::Box, (mesh->bounds_max - mesh->bounds_min) * 0.5f, 5.0f, CollisionDroneIgnore, ~CollisionWalker & ~CollisionInaccessible & ~CollisionParkour & ~CollisionStatic & ~CollisionAudio & ~CollisionElectric);
	body->set_restitution(0.75f);
	body->set_damping(0.5f, 0.5f);

	create<Audio>();

	Tram* tram = create<Tram>();
	tram->runner_a = runner_a->get<TramRunner>();
	tram->runner_b = runner_b->get<TramRunner>();

	tram->set_position();
	body->rebuild();

	create<PlayerTrigger>()->radius = 1.3f; // trigger for entering

	View* view = create<View>();
	view->mesh = Asset::Mesh::tram_mesh;
	view->shader = Asset::Shader::standard;
	view->color.w = MATERIAL_INACCESSIBLE;

	{
		Entity* child = World::alloc<StaticGeom>(Asset::Mesh::tram_collision, Vec3::zero, Quat::identity, CollisionInaccessible, ~CollisionDroneIgnore & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric);
		child->get<Transform>()->parent = transform;
		child->get<View>()->mesh = Asset::Mesh::tram_mesh_1;
		child->get<View>()->shader = Asset::Shader::flat;
		child->get<View>()->alpha();
		World::awake(child);
		Net::finalize_child(child);
	}

	{
		Entity* doors = World::alloc<Empty>();
		doors->get<Transform>()->parent = transform;
		doors->get<Transform>()->rot = Quat::identity;

		SkinnedModel* model = doors->create<SkinnedModel>();
		model->mesh = Asset::Mesh::tram_doors;
		model->shader = Asset::Shader::armature;
		model->color = Vec4(1, 1, 1, MATERIAL_INACCESSIBLE);

		Animator* anim = doors->create<Animator>();
		anim->armature = Asset::Armature::tram_doors;
		anim->layers[0].behavior = Animator::Behavior::Loop;
		anim->layers[0].blend_time = 0.0f;
		anim->layers[1].blend_time = 0.0f;

		RigidBody* body = doors->create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAudio & ~CollisionDroneIgnore & ~CollisionInaccessible & ~CollisionParkour & ~CollisionElectric, Asset::Mesh::tram_collision_door);
		body->set_restitution(0.75f);
		
		doors->create<PlayerTrigger>()->radius = 8.0f; // trigger for exiting

		World::awake(doors);
		Net::finalize_child(doors);

		tram->doors = doors;
	}

	{
		runner_a->get<RigidBody>()->rebuild();
		Quat rot_a = runner_a->get<Transform>()->rot;
		Rope* rope1 = Rope::start(runner_a->get<RigidBody>(), runner_a->get<Transform>()->pos + rot_a * Vec3(0, -0.37f, 0), rot_a * Vec3(0, -1, 0), Quat::look(rot_a * Vec3(0, -1, 0)));
		if (rope1)
			rope1->end(transform->to_world(Vec3(0, TRAM_HEIGHT, -TRAM_WIDTH)), transform->rot * (Quat::euler(0, 0, PI * (30.0f / 180.0f)) * Vec3(0, 1, 0)), body, 0.0f, true);
	}
	{
		runner_b->get<RigidBody>()->rebuild();
		Quat rot_b = runner_b->get<Transform>()->rot;
		Rope* rope2 = Rope::start(runner_b->get<RigidBody>(), runner_b->get<Transform>()->pos + rot_b * Vec3(0, -0.37f, 0), rot_b * Vec3(0, -1, 0), Quat::look(rot_b * Vec3(0, -1, 0)));
		if (rope2)
			rope2->end(transform->to_world(Vec3(0, TRAM_HEIGHT, TRAM_WIDTH)), rot_b * (Quat::euler(0, 0, PI * (-30.0f / 180.0f)) * Vec3(0, 1, 0)), body, 0.0f, true);
	}
}

void Tram::awake()
{
	link_arg<Entity*, &Tram::player_entered>(get<PlayerTrigger>()->entered);
	link_arg<Entity*, &Tram::player_exited>(doors.ref()->get<PlayerTrigger>()->exited);
}

void Tram::setup()
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
		i.item()->set_position();
}

void Tram::set_position()
{
	Vec3 pos_a = runner_a.ref()->get<Transform>()->pos;
	Vec3 pos_b = runner_b.ref()->get<Transform>()->pos;
	Transform* transform = get<Transform>();
	transform->pos = (pos_a + pos_b) * 0.5f;
	transform->pos.y += (fabsf(pos_b.y - pos_a.y) * 0.5f) - TRAM_HEIGHT - TRAM_ROPE_LENGTH;
	transform->rot = Quat::look(Vec3::normalize(pos_b - pos_a));
	if (runner_b.ref()->state == TramRunner::State::Arriving)
	{
		get<RigidBody>()->awake(); // create the rigid body if we haven't yet
		get<RigidBody>()->btBody->setLinearVelocity(transform->rot * Vec3(0, 0, -TRAM_SPEED_MAX));
	}
}

namespace TramNet
{
	b8 send(Tram* t, Message m)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Tram);

		{
			Ref<Tram> ref = t;
			serialize_ref(p, ref);
		}

		serialize_enum(p, Message, m);

		Net::msg_finalize(p);

		return true;
	}
}

b8 Tram::net_msg(Net::StreamRead* p, Net::MessageSource)
{
	using Stream = Net::StreamRead;

	Ref<Tram> ref;
	serialize_ref(p, ref);

	TramNet::Message m;
	serialize_enum(p, TramNet::Message, m);

	if (Game::level.mode == Game::Mode::Parkour && ref.ref())
	{
		switch (m)
		{
			case TramNet::Message::Entered:
			{
				if (ref.ref()->departing && ref.ref()->doors_open())
				{
					ref.ref()->doors_open(false);
					ref.ref()->get<Audio>()->post(AK::EVENTS::PLAY_TRAM_START);
					if (Game::level.local)
						TramRunner::go(ref.ref()->track(), 1.0f, TramRunner::State::Departing);
				}
				else if (Game::level.local
					&& ref.ref()->runner_a.ref()->state == TramRunner::State::Idle
					&& !ref.ref()->doors_open())
				{
					// player spawned inside us and we're sitting still
					// open the doors for them
					TramNet::send(ref.ref(), TramNet::Message::DoorsOpen);
				}
				break;
			}
			case TramNet::Message::Exited:
			{
				if (ref.ref()->doors_open())
				{
					ref.ref()->doors_open(false);
					ref.ref()->departing = false;
				}
				break;
			}
			case TramNet::Message::DoorsOpen:
			{
				ref.ref()->doors_open(true);
				break;
			}
			case TramNet::Message::Arrived:
			{
				ref.ref()->get<Audio>()->post(AK::EVENTS::PLAY_TRAM_STOP);
				ref.ref()->doors_open(true);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}

	return true;
}

void Tram::player_entered(Entity* e)
{
	if (e->has<Parkour>()
		&& e->get<PlayerControlHuman>()->local())
	{
		if (departing && doors_open())
		{
			if (Overworld::zone_under_attack() == Game::level.tram_tracks[track()].level) // can't go there if it's under attack
				e->get<PlayerControlHuman>()->player.ref()->msg(_(strings::error_zone_under_attack), false);
			else
				TramNet::send(this, TramNet::Message::Entered);
		}
		else if (!doors_open() && e->get<Walker>()->get_support() == get<RigidBody>())
		{
			// player spawned inside us and we're sitting still
			// open the doors for them
			TramNet::send(this, TramNet::Message::Entered);
		}
	}
}

void Tram::player_exited(Entity* e)
{
	if (!departing
		&& doors_open()
		&& e->has<Parkour>()
		&& e->get<PlayerControlHuman>()->local())
	{
		TramNet::send(this, TramNet::Message::Exited);
	}
}

Tram* Tram::by_track(s8 track)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->runner_b.ref()->track == track)
			return i.item();
	}
	return nullptr;
}

Tram* Tram::player_inside(Entity* player)
{
	Ref<RigidBody> support = player->get<Walker>()->support;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (support.equals(i.item()->get<RigidBody>()))
			return i.item();
	}
	return nullptr;
}

s8 Tram::track() const
{
	return runner_b.ref()->track;
}

b8 Tram::doors_open() const
{
	return doors.ref()->get<RigidBody>()->collision_filter == 0;
}

void Tram::doors_open(b8 open)
{
	Animator* anim = doors.ref()->get<Animator>();
	RigidBody* body = doors.ref()->get<RigidBody>();
	if (open)
	{
		body->set_collision_masks(CollisionStatic | CollisionInaccessible, 0); // disable collision
		anim->layers[0].play(Asset::Animation::tram_doors_opened);
		anim->layers[1].play(Asset::Animation::tram_doors_open);
		get<Audio>()->post(AK::EVENTS::PLAY_TRAM_OPEN);
	}
	else
	{
		body->set_collision_masks(CollisionStatic | CollisionInaccessible, ~CollisionStatic & ~CollisionAudio & ~CollisionDroneIgnore & ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric); // enable collision
		anim->layers[0].animation = AssetNull;
		anim->layers[1].play(Asset::Animation::tram_doors_close);
		get<Audio>()->post(AK::EVENTS::PLAY_TRAM_CLOSE);
	}
}

void TramInteractableEntity::interacted(Interactable* i)
{
	s8 track = s8(i->user_data);
	Tram* tram = Tram::by_track(track);
	if (tram->doors_open())
	{
		tram->departing = false;
		tram->doors_open(false);
	}
	else
	{
		AssetID target_level = Game::level.tram_tracks[track].level;
		if (Game::level.local && Game::save.zones[target_level] == ZoneState::Locked && !Overworld::zone_is_pvp(target_level))
		{
			Overworld::resource_change(Resource::AccessKeys, -1);
			Overworld::zone_change(target_level, ZoneState::ParkourUnlocked);
		}
		tram->departing = true;
		tram->doors_open(true);
	}
}

TramInteractableEntity::TramInteractableEntity(const Vec3& absolute_pos, const Quat& absolute_rot, s8 track)
{
	create<Transform>();

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::interactable;
	model->shader = Asset::Shader::armature;
	model->alpha();

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::interactable;
	anim->layers[0].behavior = Animator::Behavior::Loop;
	anim->layers[0].animation = Asset::Animation::interactable_enabled;
	anim->layers[0].blend_time = 0.0f;
	anim->layers[1].blend_time = 0.0f;

	Interactable* i = create<Interactable>(Interactable::Type::Tram);
	i->user_data = track;

	{
		Entity* collision = World::create<StaticGeom>(Asset::Mesh::interactable_collision, absolute_pos, absolute_rot, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
		collision->get<View>()->color.w = MATERIAL_INACCESSIBLE;
		Net::finalize_child(collision);
	}
}

Array<Ascensions::Entry> Ascensions::entries;
r32 Ascensions::timer;
r32 Ascensions::particle_accumulator;

const r32 ascension_total_time = 20.0f;

Vec3 Ascensions::Entry::pos() const
{
	r32 blend = 1.0f - (timer / ascension_total_time);
	return Quat::euler(Ease::circ_out<r32>(blend) * PI * 0.45f, Game::level.rotation, 0) * Vec3(Game::level.skybox.far_plane * 0.9f, 0, 0);
}

r32 Ascensions::Entry::scale() const
{
	r32 blend = 1.0f - (timer / ascension_total_time);
	return (Game::level.skybox.far_plane / 100.0f) * LMath::lerpf(blend, 1.0f, 0.5f);
}

void Ascensions::update(const Update& u)
{
	if (Game::level.mode != Game::Mode::Special)
	{
		timer -= Game::real_time.delta;
		if (timer < 0.0f)
		{
			timer = 40.0f + mersenne::randf_co() * 200.0f;

			Entry* e = entries.add();
			e->timer = ascension_total_time;
			e->username = Usernames::all[mersenne::rand_u32() % Usernames::count];
		}
	}

	for (s32 i = 0; i < entries.length; i++)
	{
		Entry* e = &entries[i];
		r32 old_timer = e->timer;
		e->timer -= u.time.delta;
		if (e->timer < 0.0f)
		{
			entries.remove(i);
			i--;
		}
		else
		{
			// only show notifications in parkour mode
			if (Game::level.mode == Game::Mode::Parkour
				&& old_timer >= ascension_total_time * 0.85f && e->timer < ascension_total_time * 0.85f)
			{
				char msg[512];
				sprintf(msg, _(strings::player_ascended), e->username);
				PlayerHuman::log_add(msg);
			}
		}
	}

	// particles
	const r32 interval = 0.5f;
	particle_accumulator += u.time.delta;
	while (particle_accumulator > interval)
	{
		particle_accumulator -= interval;
		for (s32 i = 0; i < entries.length; i++)
		{
			const Entry& e = entries[i];
			Particles::tracers_skybox.add(e.pos(), e.scale());
		}
	}
}

void Ascensions::draw_ui(const RenderParams& params)
{
	if (Game::level.mode != Game::Mode::Pvp)
	{
		for (s32 i = 0; i < entries.length; i++)
		{
			if (entries[i].timer < ascension_total_time * 0.85f)
			{
				Vec2 p;
				if (UI::project(params, entries[i].pos(), &p))
				{
					p.y += 8.0f * UI::scale;

					UIText username;
					username.size = 16.0f;
					username.anchor_x = UIText::Anchor::Center;
					username.anchor_y = UIText::Anchor::Min;
					username.color = UI::color_accent();
					username.text_raw(params.camera->gamepad, entries[i].username);
					UI::box(params, username.rect(p).outset(8.0f * UI::scale), UI::color_background);
					username.draw(params, p);
				}
			}
		}
	}
}

void Ascensions::clear()
{
	timer = 40.0f + mersenne::randf_co() * 200.0f;
	entries.length = 0;
}

PinArray<Tile, MAX_ENTITIES> Tile::list;
Array<Mat4> Tile::instances;

void Tile::add(const Vec3& target_pos, const Quat& target_rot, const Vec3& offset, Transform* parent, r32 anim_time)
{
	Tile* t = list.add();
	t->relative_start_pos = target_pos + offset;
	t->relative_start_rot = target_rot * Quat::euler(PI * 0.5f, PI * 0.5f, fmod((Game::time.total + (anim_time * 2.0f)) * 5.0f, PI * 2.0f));
	t->relative_target_pos = target_pos;
	t->relative_target_rot = target_rot;
	if (parent)
	{
		parent->to_local(&t->relative_start_pos, &t->relative_start_rot);
		parent->to_local(&t->relative_target_pos, &t->relative_target_rot);
	}
	t->parent = parent;
	t->timer = 0.0f;
	t->anim_time = anim_time;
}

void Tile::draw_alpha(const RenderParams& params)
{
	instances.length = 0;

	const Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::plane);
	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = vi_max(radius.x, vi_max(radius.y, radius.z));

	{
		for (auto i = Tile::list.iterator(); !i.is_last(); i.next())
		{
			Tile* tile = i.item();
			const r32 size = tile->scale();

			r32 blend = vi_min(tile->timer / tile->anim_time, 1.0f);
			Vec3 pos = Vec3::lerp(blend, tile->relative_start_pos, tile->relative_target_pos) + Vec3(sinf(blend * PI) * 0.25f);
			Quat rot = Quat::slerp(blend, tile->relative_start_rot, tile->relative_target_rot);
			if (tile->parent.ref())
				tile->parent.ref()->to_world(&pos, &rot);

			if (params.camera->visible_sphere(pos, size * f_radius))
			{
				Mat4* m = instances.add();
				m->make_transform(pos, Vec3(size), rot);
			}
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader_permanent(Asset::Shader::standard_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_instanced);
	sync->write(params.technique);

	Mat4 vp = params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::vp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::v);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, 0.5f));

	sync->write(RenderOp::Instances);
	sync->write(Asset::Mesh::plane);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

void Tile::clear()
{
	list.clear();
}

#define TILE_LIFE_TIME 6.0f
#define TILE_ANIM_OUT_TIME 0.3f
void Tile::update(const Update& u)
{
	timer += u.time.delta;
	if (timer > TILE_LIFE_TIME)
		list.remove(id());
}

r32 Tile::scale() const
{
	r32 blend;
	if (timer < TILE_LIFE_TIME - TILE_ANIM_OUT_TIME)
		blend = vi_min(timer / anim_time, 1.0f);
	else
		blend = Ease::quad_in(((timer - (TILE_LIFE_TIME - TILE_ANIM_OUT_TIME)) / TILE_ANIM_OUT_TIME), 1.0f, 0.0f);
	return blend * TILE_SIZE;
}


}
