#define WIN32_LEAN_AND_MEAN
#include "net.h"
#include "platform/sock.h"
#include "game/game.h"
#if SERVER
#include "asset/level.h"
#endif
#include "mersenne/mersenne-twister.h"
#include "common.h"
#include "ai.h"
#include "render/views.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "data/ragdoll.h"
#include "physics.h"
#include "game/drone.h"
#include "game/minion.h"
#include "game/audio.h"
#include "game/player.h"
#include "game/walker.h"
#include "game/ai_player.h"
#include "game/team.h"
#include "game/player.h"
#include "game/parkour.h"
#include "game/overworld.h"
#include "game/scripts.h"
#include "game/master.h"
#include "console.h"
#include "asset/armature.h"
#include "load.h"
#include "settings.h"
#include "cjson/cJSON.h"
#include <array>
#include "data/import_common.h"

#define DEBUG_MSG 0
#define DEBUG_ENTITY 0
#define DEBUG_TRANSFORMS 0
#define DEBUG_LAG 0
#define DEBUG_LAG_AMOUNT 0.1f
#define DEBUG_PACKET_LOSS 0
#define DEBUG_PACKET_LOSS_AMOUNT 0.05f

#define MASTER_AUTH_TIMEOUT 8.0f
#define NET_EXPECTED_CLIENT_TIMEOUT 30.0f

namespace VI
{


namespace Net
{

b8 show_stats;

// borrows heavily from https://github.com/networkprotocol/libyojimbo

struct MessageFrame // container for the amount of messages that can come in a single frame
{
	union
	{
		StreamRead read;
		StreamWrite write;
	};
	r32 timestamp;
	s32 bytes;
	SequenceID sequence_id;
	SequenceID remote_sequence_id;

	MessageFrame() : read(), sequence_id(), remote_sequence_id(), timestamp(), bytes() {}
	MessageFrame(r32 t, s32 bytes) : read(), sequence_id(), remote_sequence_id(), timestamp(t), bytes(bytes) {}
	~MessageFrame() {}
};

enum class ClientPacket : s8
{
	Connect,
	Update,
	Disconnect,
	Ping,
	count,
};

enum class ServerPacket : s8
{
	Init,
	Update,
	Disconnect,
	PingResponse,
	count,
};

struct StateHistory
{
	StaticArray<StateFrame, NET_HISTORY_SIZE> frames;
	s32 current_index;
};

struct Ack
{
	u64 previous_sequences;
	SequenceID sequence_id;
};

struct MessageHistory
{
	StaticArray<MessageFrame, NET_HISTORY_SIZE> msg_frames;
	s32 current_index;
};

struct SequenceHistoryEntry
{
	r32 timestamp;
	SequenceID id;
};

// keeps track of which message frame we have processed so far, and which one we need to process next
struct MessageFrameState
{
	SequenceID sequence_id;
	b8 starting; // are we looking for the first message frame?
};

typedef StaticArray<SequenceHistoryEntry, NET_SEQUENCE_RESEND_BUFFER> SequenceHistory;
typedef Array<StreamWrite> MessageBuffer;

// server/client data
struct StateCommon
{
	MessageHistory msgs_out_history;
	MessageBuffer msgs_out;
	SequenceID local_sequence_id;
	StateHistory state_history;
	s32 bandwidth_in;
	s32 bandwidth_out;
	s32 bandwidth_in_counter;
	s32 bandwidth_out_counter;
	r32 timestamp;
};
StateCommon state_common;

struct StatePersistent
{
	Sock::Handle sock;
	Master::Messenger master;
	Sock::Address master_addr;
};
StatePersistent state_persistent;

#if SERVER
namespace Server
{
	void packet_sent(const StreamWrite&, const Sock::Address&);
}
#endif

void packet_send(const StreamWrite& p, const Sock::Address& address)
{
	Sock::udp_send(&state_persistent.sock, address, p.data.data, p.bytes_written());
	state_common.bandwidth_out_counter += p.bytes_written();
#if SERVER
	Server::packet_sent(p, address);
#endif
}

b8 master_send(Master::Message msg)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, msg);
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

void master_init()
{
	Sock::Address::get(&state_persistent.master_addr, Settings::master_server, NET_MASTER_PORT);
	master_send(Master::Message::Disconnect);
	state_persistent.master.reset();
	{
		char buffer[NET_MAX_ADDRESS];
		state_persistent.master_addr.str(buffer);
		vi_debug("Connecting to master server at %s...", buffer);
	}
}

b8 msg_process(StreamRead*, MessageSource);
StreamWrite* msg_new(MessageBuffer*, MessageType);

template<typename Stream, typename View> b8 serialize_view_skinnedmodel(Stream* p, View* v)
{
	b8 is_identity;
	if (Stream::IsWriting)
	{
		is_identity = true;
		for (s32 i = 0; i < 4; i++)
		{
			for (s32 j = 0; j < 4; j++)
			{
				if (v->offset.m[i][j] != Mat4::identity.m[i][j])
				{
					is_identity = false;
					break;
				}
			}
		}
	}
	serialize_bool(p, is_identity);
	if (is_identity)
	{
		if (Stream::IsReading)
			v->offset = Mat4::identity;
	}
	else
	{
		for (s32 i = 0; i < 4; i++)
		{
			for (s32 j = 0; j < 4; j++)
				serialize_r32(p, v->offset.m[i][j]);
		}
	}
	serialize_r32_range(p, v->color.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.z, 0.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.w, 0.0f, 1.0f, 8);
	serialize_r32_range(p, v->radius, 0.0f, 20.0f, 8);
	serialize_s16(p, v->mask);
	serialize_s16(p, v->mesh);
	serialize_s16(p, v->mesh_shadow);
	serialize_asset(p, v->shader, Loader::shader_count);
	serialize_asset(p, v->texture, Loader::static_texture_count);
	serialize_s8(p, v->team);
	{
		AlphaMode m;
		if (Stream::IsWriting)
			m = v->alpha_mode();
		serialize_enum(p, AlphaMode, m);
		if (Stream::IsReading)
			v->alpha_mode(m);
	}
	return true;
}

template<typename Stream> b8 serialize_player_control(Stream* p, PlayerControlHuman::RemoteControl* control)
{
	b8 moving;
	if (Stream::IsWriting)
		moving = control->movement.length_squared() > 0.0f;
	serialize_bool(p, moving);
	if (moving)
	{
		serialize_r32_range(p, control->movement.x, -1.0f, 1.0f, 16);
		serialize_r32_range(p, control->movement.y, -1.0f, 1.0f, 16);
		serialize_r32_range(p, control->movement.z, -1.0f, 1.0f, 16);
	}
	else if (Stream::IsReading)
		control->movement = Vec3::zero;
	serialize_ref(p, control->parent);
	if (!serialize_position(p, &control->pos, Resolution::High))
		net_error();
	if (!serialize_quat(p, &control->rot, Resolution::High))
		net_error();
	return true;
}

template<typename Stream> b8 serialize_constraint(Stream* p, RigidBody::Constraint* c)
{
	serialize_enum(p, RigidBody::Constraint::Type, c->type);
	serialize_ref(p, c->b);

	Vec3 pos;
	if (Stream::IsWriting)
		pos = c->frame_a.getOrigin();
	if (!serialize_position(p, &pos, Resolution::High))
		net_error();
	Quat rot;
	if (Stream::IsWriting)
		rot = c->frame_a.getRotation();
	if (!serialize_quat(p, &rot, Resolution::High))
		net_error();
	if (Stream::IsReading)
		c->frame_a = btTransform(rot, pos);

	if (Stream::IsWriting)
		pos = c->frame_b.getOrigin();
	if (!serialize_position(p, &pos, Resolution::High))
		net_error();
	if (Stream::IsWriting)
		rot = c->frame_b.getRotation();
	if (!serialize_quat(p, &rot, Resolution::High))
		net_error();
	if (Stream::IsReading)
		c->frame_b = btTransform(rot, pos);

	if (!serialize_position(p, &c->limits, Resolution::Medium))
		net_error();

	return true;
}

template<typename Stream> b8 serialize_entity(Stream* p, Entity* e)
{
	const ComponentMask mask = Transform::component_mask
		| RigidBody::component_mask
		| View::component_mask
		| Animator::component_mask
		| Rope::component_mask
		| SpawnPoint::component_mask
		| UpgradeStation::component_mask
		| SkyDecal::component_mask
		| AIAgent::component_mask
		| Health::component_mask
		| PointLight::component_mask
		| SpotLight::component_mask
		| CoreModule::component_mask
		| Walker::component_mask
		| Turret::component_mask
		| Ragdoll::component_mask
		| Target::component_mask
		| PlayerTrigger::component_mask
		| SkinnedModel::component_mask
		| Bolt::component_mask
		| Grenade::component_mask
		| Battery::component_mask
		| Generator::component_mask
		| ForceField::component_mask
		| ForceFieldCollision::component_mask
		| Drone::component_mask
		| Shield::component_mask
		| Audio::component_mask
		| Team::component_mask
		| PlayerHuman::component_mask
		| PlayerManager::component_mask
		| PlayerCommon::component_mask
		| PlayerControlHuman::component_mask
		| Minion::component_mask
		| Parkour::component_mask
		| Interactable::component_mask
		| Tram::component_mask
		| TramRunner::component_mask
		| Collectible::component_mask
		| Water::component_mask
		| Flag::component_mask;

	if (Stream::IsWriting)
	{
		ComponentMask m = e->component_mask;
		m &= mask;
		serialize_u64(p, m);
	}
	else
		serialize_u64(p, e->component_mask);
	serialize_s16(p, e->revision);

#if DEBUG_ENTITY
	{
		char components[MAX_FAMILIES + 1] = {};
		for (s32 i = 0; i < MAX_FAMILIES; i++)
			components[i] = (e->component_mask & (ComponentMask(1) << i) & mask) ? '1' : '0';
		vi_debug("Entity %d rev %d: %s", s32(e->id()), s32(e->revision), components);
	}
#endif

	for (s32 i = 0; i < MAX_FAMILIES; i++)
	{
		if (e->component_mask & mask & (ComponentMask(1) << i))
		{
			serialize_int(p, ID, e->components[i], 0, MAX_ENTITIES - 1);
			ID component_id = e->components[i];
			Revision r;
			if (Stream::IsWriting)
				r = World::component_pools[i]->revision(component_id);
			serialize_s16(p, r);
			if (Stream::IsReading)
				World::component_pools[i]->net_add(component_id, e->id(), r);
		}
	}

	if (e->has<Transform>())
	{
		Transform* t = e->get<Transform>();
		if (!serialize_position(p, &t->pos, Resolution::High))
			net_error();
		b8 is_identity_quat;
		if (Stream::IsWriting)
			is_identity_quat = Quat::angle(t->rot, Quat::identity) == 0.0f;
		serialize_bool(p, is_identity_quat);
		if (!is_identity_quat)
		{
			if (!serialize_quat(p, &t->rot, Resolution::High))
				net_error();
		}
		else if (Stream::IsReading)
			t->rot = Quat::identity;
		serialize_ref(p, t->parent);
	}

	if (e->has<RigidBody>())
	{
		RigidBody* r = e->get<RigidBody>();
		serialize_r32_range(p, r->size.x, 0, 5.0f, 8);
		serialize_r32_range(p, r->size.y, 0, 5.0f, 8);
		serialize_r32_range(p, r->size.z, 0, 5.0f, 8);
		serialize_r32_range(p, r->damping.x, 0, 1.0f, 2);
		serialize_r32_range(p, r->damping.y, 0, 1.0f, 2);
		serialize_enum(p, RigidBody::Type, r->type);
		serialize_r32_range(p, r->mass, 0, 20.0f, 16);
		serialize_r32_range(p, r->restitution, 0, 1, 8);
		serialize_asset(p, r->mesh_id, Loader::static_mesh_count);
		serialize_s16(p, r->collision_group);
		serialize_s16(p, r->collision_filter);
		serialize_s8(p, r->flags);

		if (Stream::IsWriting)
		{
			b8 b = false;
			for (auto i = RigidBody::global_constraints.iterator(); !i.is_last(); i.next())
			{
				RigidBody::Constraint* c = i.item();
				if (c->a.ref() == r)
				{
					b = true;
					serialize_bool(p, b);
					if (!serialize_constraint(p, c))
						net_error();
				}
			}
			b = false;
			serialize_bool(p, b);
		}
		else
		{
			b8 has_constraint;
			serialize_bool(p, has_constraint);
			while (has_constraint)
			{
				RigidBody::Constraint* c = RigidBody::net_add_constraint();
				c->a = r;
				c->btPointer = nullptr;

				if (!serialize_constraint(p, c))
					net_error();

				serialize_bool(p, has_constraint);
			}
		}
	}

	if (e->has<View>())
	{
		if (!serialize_view_skinnedmodel(p, e->get<View>()))
			net_error();
	}

	if (e->has<Animator>())
	{
		Animator* a = e->get<Animator>();
		for (s32 i = 0; i < MAX_ANIMATIONS; i++)
		{
			Animator::Layer* l = &a->layers[i];
			serialize_r32_range(p, l->blend, 0, 1, 8);
			serialize_r32_range(p, l->blend_time, 0, 8, 16);
			serialize_r32(p, l->time);
			serialize_r32_range(p, l->speed, 0, 8, 16);
			serialize_asset(p, l->animation, Loader::animation_count);
			if (Stream::IsReading)
			{
				l->last_animation = l->last_frame_animation = l->animation;
				l->time_last = l->time;
			}
			serialize_enum(p, Animator::Behavior, l->behavior);
		}
		serialize_asset(p, a->armature, Loader::armature_count);
		serialize_enum(p, Animator::OverrideMode, a->override_mode);
	}

	if (e->has<AIAgent>())
	{
		AIAgent* a = e->get<AIAgent>();
		serialize_s8(p, a->team);
		serialize_bool(p, a->stealth);
	}

	if (e->has<Drone>())
	{
		Drone* a = e->get<Drone>();
		serialize_r32_range(p, a->cooldown, 0, DRONE_COOLDOWN_MAX, 8);
		serialize_int(p, Ability, a->current_ability, 0, s32(Ability::count) + 1);
	}

	if (e->has<Minion>())
	{
		Minion* m = e->get<Minion>();
		serialize_ref(p, m->owner);
	}

	if (e->has<Turret>())
	{
		Turret* t = e->get<Turret>();
		serialize_s8(p, t->team);
		serialize_ref(p, t->target);
	}

	if (e->has<Flag>())
	{
		Flag* f = e->get<Flag>();
		serialize_s8(p, f->team);
		serialize_r32_range(p, f->timer, 0.0f, FLAG_RESTORE_TIME, 16);
		serialize_bool(p, f->at_base);
	}

	if (e->has<Health>())
	{
		Health* h = e->get<Health>();
		serialize_r32_range(p, h->active_armor_timer, 0, 5, 8);
		serialize_r32_range(p, h->regen_timer, 0, 10, 8);
		serialize_s8(p, h->shield);
		serialize_s8(p, h->shield_max);
		serialize_s8(p, h->hp);
		serialize_s8(p, h->hp_max);
	}

	if (e->has<Shield>())
	{
		Shield* s = e->get<Shield>();
		serialize_ref(p, s->inner);
		serialize_ref(p, s->outer);
	}

	if (e->has<CoreModule>())
	{
		CoreModule* c = e->get<CoreModule>();
		serialize_s8(p, c->team);
	}

	if (e->has<PointLight>())
	{
		PointLight* l = e->get<PointLight>();
		serialize_r32_range(p, l->color.x, 0, 1, 8);
		serialize_r32_range(p, l->color.y, 0, 1, 8);
		serialize_r32_range(p, l->color.z, 0, 1, 8);
		serialize_r32_range(p, l->offset.x, -5, 5, 8);
		serialize_r32_range(p, l->offset.y, -5, 5, 8);
		serialize_r32_range(p, l->offset.z, -5, 5, 8);
		serialize_r32_range(p, l->radius, 0, 50, 8);
		serialize_enum(p, PointLight::Type, l->type);
		serialize_s16(p, l->mask);
		serialize_s8(p, l->team);
	}

	if (e->has<SpotLight>())
	{
		SpotLight* l = e->get<SpotLight>();
		serialize_r32_range(p, l->color.x, 0, 1, 8);
		serialize_r32_range(p, l->color.y, 0, 1, 8);
		serialize_r32_range(p, l->color.z, 0, 1, 8);
		serialize_r32_range(p, l->radius, 0, 50, 8);
		serialize_r32_range(p, l->fov, 0, PI, 8);
		serialize_s16(p, l->mask);
		serialize_s8(p, l->team);
	}

	if (e->has<SpawnPoint>())
	{
		SpawnPoint* s = e->get<SpawnPoint>();
		serialize_s8(p, s->team);
	}

	if (e->has<UpgradeStation>())
	{
		UpgradeStation* u = e->get<UpgradeStation>();
		serialize_ref(p, u->spawn_point);
		serialize_ref(p, u->drone);
		serialize_enum(p, UpgradeStation::Mode, u->mode);
	}

	if (e->has<Walker>())
	{
		Walker* w = e->get<Walker>();
		serialize_r32_range(p, w->speed, 0, 10, 16);
		serialize_r32_range(p, w->max_speed, 0, 10, 16);
		serialize_r32_range(p, w->rotation_speed, 0, 20.0f, 16);
		serialize_bool(p, w->auto_rotate);
		r32 r;
		if (Stream::IsWriting)
			r = LMath::angle_range(w->rotation);
		serialize_r32_range(p, r, -PI, PI, 8);
		if (Stream::IsReading)
		{
			w->rotation = r;
			w->target_rotation = r;
		}
	}

	if (e->has<Ragdoll>())
	{
		Ragdoll* r = e->get<Ragdoll>();
		serialize_enum(p, Ragdoll::Impulse, r->impulse_type);
		serialize_r32_range(p, r->impulse.x, -15.0f, 15.0f, 8);
		serialize_r32_range(p, r->impulse.y, -15.0f, 15.0f, 8);
		serialize_r32_range(p, r->impulse.z, -15.0f, 15.0f, 8);
		s32 bone_count;
		if (Stream::IsWriting)
			bone_count = r->bodies.length;
		serialize_int(p, s32, bone_count, 0, MAX_BONES);
		if (Stream::IsReading)
			r->bodies.resize(bone_count);
		for (s32 i = 0; i < bone_count; i++)
		{
			Ragdoll::BoneBody* bone = &r->bodies[i];
			serialize_ref(p, bone->body);
			serialize_asset(p, bone->bone, Asset::Bone::count);
			if (!serialize_position(p, &bone->body_to_bone_pos, Resolution::Medium))
				net_error();
			if (!serialize_quat(p, &bone->body_to_bone_rot, Resolution::Medium))
				net_error();
		}
	}

	if (e->has<Target>())
	{
		Target* t = e->get<Target>();
		serialize_r32_range(p, t->local_offset.x, -5, 5, 16);
		serialize_r32_range(p, t->local_offset.y, -5, 5, 16);
		serialize_r32_range(p, t->local_offset.z, -5, 5, 16);
	}

	if (e->has<PlayerTrigger>())
	{
		PlayerTrigger* t = e->get<PlayerTrigger>();
		serialize_r32(p, t->radius);
	}

	if (e->has<SkinnedModel>())
	{
		if (!serialize_view_skinnedmodel(p, e->get<SkinnedModel>()))
			net_error();
	}

	if (e->has<Bolt>())
	{
		Bolt* x = e->get<Bolt>();
		serialize_s8(p, x->team);
		serialize_ref(p, x->owner);
		serialize_ref(p, x->player);
		serialize_r32(p, x->velocity.x);
		serialize_r32(p, x->velocity.y);
		serialize_r32(p, x->velocity.z);
		serialize_enum(p, Bolt::Type, x->type);
		serialize_bool(p, x->reflected);
	}

	if (e->has<Grenade>())
	{
		Grenade* g = e->get<Grenade>();
		serialize_ref(p, g->owner);
		serialize_enum(p, Grenade::State, g->state);
	}

	if (e->has<Battery>())
	{
		Battery* b = e->get<Battery>();
		serialize_s8(p, b->team);
		serialize_ref(p, b->light);
		serialize_ref(p, b->spawn_point);
	}

	if (e->has<Generator>())
	{
		Generator* s = e->get<Generator>();
		serialize_s8(p, s->team);
	}

	if (e->has<ForceField>())
	{
		ForceField* c = e->get<ForceField>();
		serialize_ref(p, c->collision);
		serialize_s8(p, c->team);
		serialize_s8(p, c->flags);
	}

	if (e->has<ForceFieldCollision>())
	{
		ForceFieldCollision* c = e->get<ForceFieldCollision>();
		serialize_ref(p, c->field);
	}

	if (e->has<Water>())
	{
		Water* w = e->get<Water>();
		serialize_r32_range(p, w->config.color.x, 0, 1.0f, 8);
		serialize_r32_range(p, w->config.color.y, 0, 1.0f, 8);
		serialize_r32_range(p, w->config.color.z, 0, 1.0f, 8);
		serialize_r32_range(p, w->config.color.w, 0, 1.0f, 8);
		serialize_r32_range(p, w->config.displacement_horizontal, 0, 10, 8);
		serialize_r32_range(p, w->config.displacement_vertical, 0, 10, 8);
		serialize_s16(p, w->config.mesh);
		serialize_asset(p, w->config.texture, Loader::static_texture_count);
	}

	if (e->has<SkyDecal>())
	{
		SkyDecal* d = e->get<SkyDecal>();
		serialize_r32_range(p, d->color.x, 0, 1.0f, 8);
		serialize_r32_range(p, d->color.y, 0, 1.0f, 8);
		serialize_r32_range(p, d->color.z, 0, 1.0f, 8);
		serialize_r32_range(p, d->color.w, 0, 1.0f, 8);
		serialize_r32_range(p, d->scale, 0, 10.0f, 8);
		serialize_asset(p, d->texture, Loader::static_texture_count);
	}

	if (e->has<PlayerHuman>())
	{
		PlayerHuman* ph = e->get<PlayerHuman>();
		serialize_u64(p, ph->uuid);
		serialize_u32(p, ph->master_id);
		if (Stream::IsReading)
		{
			ph->flag(PlayerHuman::FlagLocal, false);
#if !SERVER // when replaying, all players are remote
			if (Client::replay_mode() == Client::ReplayMode::Replaying)
				ph->gamepad = s8(ph->id());
			else
#endif
			{
				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					if (ph->uuid == Game::session.local_player_uuids[i])
					{
						ph->flag(PlayerHuman::FlagLocal, true);
						ph->gamepad = s8(i);
						break;
					}
				}
			}
		}
	}

	if (e->has<PlayerManager>())
	{
		PlayerManager* m = e->get<PlayerManager>();
		serialize_s32(p, m->upgrades);
		for (s32 i = 0; i < MAX_ABILITIES; i++)
			serialize_int(p, Ability, m->abilities[i], 0, s32(Ability::count) + 1);
		serialize_ref(p, m->team);
		serialize_ref(p, m->instance);
		serialize_s16(p, m->energy);
		serialize_s16(p, m->kills);
		serialize_s16(p, m->deaths);
		serialize_s16(p, m->flags_captured);
		s32 username_length;
		if (Stream::IsWriting)
			username_length = strlen(m->username);
		serialize_int(p, s32, username_length, 0, MAX_USERNAME);
		serialize_bytes(p, (u8*)m->username, username_length);
		if (Stream::IsReading)
			m->username[username_length] = '\0';
		serialize_bool(p, m->can_spawn);
		serialize_bool(p, m->is_admin);
	}

	if (e->has<Team>())
	{
		Team* t = e->get<Team>();
		serialize_s16(p, t->kills);
		serialize_s16(p, t->flags_captured);
		serialize_int(p, s16, t->extra_drones, 0, MAX_RESPAWNS);
		serialize_ref(p, t->flag_base);
	}

	if (e->has<PlayerCommon>())
	{
		PlayerCommon* pc = e->get<PlayerCommon>();
		serialize_r32_range(p, pc->angle_horizontal, PI * -2.0f, PI * 2.0f, 16);
		serialize_r32_range(p, pc->angle_vertical, -PI, PI, 16);
		serialize_ref(p, pc->manager);
	}

	if (e->has<PlayerControlHuman>())
	{
		PlayerControlHuman* c = e->get<PlayerControlHuman>();
		serialize_ref(p, c->player);
	}

	if (e->has<Interactable>())
	{
		Interactable* i = e->get<Interactable>();
		serialize_s32(p, i->user_data);
		serialize_enum(p, Interactable::Type, i->type);
	}

	if (e->has<Tram>())
	{
		Tram* t = e->get<Tram>();
		serialize_ref(p, t->runner_a);
		serialize_ref(p, t->runner_b);
		serialize_ref(p, t->doors);
		serialize_bool(p, t->departing);
		serialize_bool(p, t->arrive_only);
	}

	if (e->has<TramRunner>())
	{
		TramRunner* r = e->get<TramRunner>();
		serialize_s8(p, r->track);
		serialize_bool(p, r->is_front);
		serialize_enum(p, TramRunner::State, r->state);
	}

	if (e->has<Collectible>())
	{
		Collectible* c = e->get<Collectible>();
		serialize_s16(p, c->save_id);
		serialize_enum(p, Resource, c->type);
		serialize_s16(p, c->amount);
	}

#if !SERVER
	if (Stream::IsReading && Client::mode() == Client::Mode::Connected)
		World::awake(e);
#endif

	return true;
}

template<typename Stream> b8 serialize_init_packet(Stream* p)
{
	serialize_enum(p, Game::FeatureLevel, Game::level.feature_level);
	serialize_r32_range(p, Game::level.rotation, -2.0f * PI, 2.0f * PI, 16);
	serialize_r32_range(p, Game::level.min_y, -128, 128, 8);
	serialize_r32(p, Game::level.skybox.far_plane);
	serialize_r32(p, Game::level.skybox.fog_start);
	serialize_r32(p, Game::level.skybox.fog_end);
	serialize_asset(p, Game::level.skybox.texture, Loader::static_texture_count);
	serialize_asset(p, Game::level.skybox.shader, Loader::shader_count);
	serialize_asset(p, Game::level.skybox.mesh, Loader::static_mesh_count);
	serialize_r32_range(p, Game::level.skybox.color.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.color.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.color.z, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.ambient_color.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.ambient_color.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.ambient_color.z, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.rain, 0.0f, 1.0f, 8);

	serialize_int(p, u16, Game::level.directional_lights.length, 0, Game::level.directional_lights.capacity());
	for (s32 i = 0; i < Game::level.directional_lights.length; i++)
	{
		DirectionalLight* d = &Game::level.directional_lights[i];
		serialize_r32_range(p, d->color.x, 0.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.y, 0.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.z, 0.0f, 1.0f, 8);
		if (!serialize_quat(p, &d->rot, Net::Resolution::Medium))
			net_error();
		serialize_bool(p, d->shadowed);
	}

	serialize_int(p, u16, Game::level.clouds.length, 0, Game::level.clouds.capacity());
	for (s32 i = 0; i < Game::level.clouds.length; i++)
	{
		Clouds::Config* cloud = &Game::level.clouds[i];
		serialize_r32_range(p, cloud->height, -50.0f, 200.0f, 16);
		serialize_r32_range(p, cloud->velocity.x, -20, 20.0f, 8);
		serialize_r32_range(p, cloud->velocity.y, -20, 20.0f, 8);
		serialize_r32_range(p, cloud->scale, 0.0f, 10.0f, 8);
		serialize_r32_range(p, cloud->color.x, 0.0f, 1.0f, 8);
		serialize_r32_range(p, cloud->color.y, 0.0f, 1.0f, 8);
		serialize_r32_range(p, cloud->color.z, 0.0f, 1.0f, 8);
		serialize_r32_range(p, cloud->color.w, 0.0f, 1.0f, 8);
		serialize_r32_range(p, cloud->shadow, 0.0f, 1.0f, 8);
	}
	serialize_s16(p, Game::level.id);

	{
		s32 ambience_length;
		if (Stream::IsWriting)
			ambience_length = strlen(Game::level.ambience);
		serialize_int(p, s32, ambience_length, 0, MAX_AUDIO_EVENT_NAME);
		serialize_bytes(p, (u8*)Game::level.ambience, ambience_length);
		if (Stream::IsReading)
			Game::level.ambience[ambience_length] = '\0';
	}

	serialize_enum(p, Game::Mode, Game::level.mode);
	serialize_enum(p, SessionType, Game::session.type);
	serialize_ref(p, Game::level.map_view);
	serialize_ref(p, Game::level.core_force_field);
	serialize_ref(p, Game::level.terminal);
	serialize_ref(p, Game::level.terminal_interactable);
	serialize_ref(p, Game::level.shop);
	serialize_int(p, s32, Game::level.max_teams, 0, MAX_TEAMS);
	serialize_int(p, s32, Game::level.tram_tracks.length, 0, Game::level.tram_tracks.capacity());
	for (s32 i = 0; i < Game::level.tram_tracks.length; i++)
		serialize_s16(p, Game::level.tram_tracks[i].level);
	serialize_int(p, u16, Game::level.scripts.length, 0, Game::level.scripts.capacity());
	for (s32 i = 0; i < Game::level.scripts.length; i++)
		serialize_int(p, AssetID, Game::level.scripts[i], 0, Script::count);
	serialize_s16(p, Game::level.multiplayer_level_scheduled);
	if (!Master::serialize_server_config(p, &Game::session.config))
		net_error();
	serialize_r32_range(p, Team::match_time, 0, Game::session.config.time_limit(), 16);
	serialize_r32_range(p, Team::core_module_delay, 0, CORE_MODULE_DELAY, 8);
	serialize_enum(p, Team::MatchState, Team::match_state);
	if (Stream::IsReading)
		Game::level.finder.map.length = 0;
	return true;
}

#if DEBUG
void ack_debug(const char* caption, const Ack& ack)
{
	char str[NET_ACK_PREVIOUS_SEQUENCES + 1] = {};
	for (s32 i = 0; i < NET_ACK_PREVIOUS_SEQUENCES; i++)
		str[i] = (ack.previous_sequences & (u64(1) << i)) ? '1' : '0';
	vi_debug("%s %d %s", caption, s32(ack.sequence_id), str);
}

void msg_history_debug(const MessageHistory& history)
{
	if (history.msg_frames.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			const MessageFrame& msg = history.msg_frames[index];
			vi_debug("%d %f", s32(msg.sequence_id), msg.timestamp);

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msg_frames.length - 1;
			if (index == history.current_index || history.msg_frames[index].timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;
		}
	}
}
#endif

MessageFrame* msg_history_add(MessageHistory* history, r32 timestamp, s32 bytes)
{
	MessageFrame* frame;
	if (history->msg_frames.length < history->msg_frames.capacity())
	{
		frame = history->msg_frames.add();
		history->current_index = history->msg_frames.length - 1;
	}
	else
	{
		history->current_index = (history->current_index + 1) % history->msg_frames.capacity();
		frame = &history->msg_frames[history->current_index];
	}
	new (frame) MessageFrame(timestamp, bytes);
	return frame;
}

template<typename Comparator> SequenceID msg_history_foremost_sequence(const MessageHistory& history, Comparator* comparator)
{
	// find foremost sequence ID we've received
	if (history.msg_frames.length == 0)
		return NET_SEQUENCE_INVALID;

	s32 index = history.current_index;
	SequenceID result = history.msg_frames[index].sequence_id;
	for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
	{
		// loop backward through most recently received frames
		index = index > 0 ? index - 1 : history.msg_frames.length - 1;

		const MessageFrame& msg = history.msg_frames[index];

		if (index == history.current_index || msg.timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
			break;

		if (comparator->compare(msg.sequence_id, result))
			result = msg.sequence_id;
	}
	return result;
}

SequenceID msg_history_most_recent_sequence(const MessageHistory& history)
{
	struct Comparator
	{
		const MessageHistory* history;

		b8 compare(SequenceID a, SequenceID b)
		{
			return sequence_more_recent(a, b);
		}
	};

	Comparator comparator;
	comparator.history = &history;
	return msg_history_foremost_sequence(history, &comparator);
}

MessageFrame* msg_frame_by_sequence(MessageHistory* history, SequenceID sequence_id)
{
	if (history->msg_frames.length > 0)
	{
		s32 index = history->current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			MessageFrame* msg = &history->msg_frames[index];
			if (msg->sequence_id == sequence_id)
				return msg;

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history->msg_frames.length - 1;
			if (index == history->current_index || msg->timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;
		}
	}
	return nullptr;
}

SequenceID msg_history_most_recent_sequence_before(const MessageHistory& history, r32 timestamp_cutoff)
{
	struct Comparator
	{
		const MessageHistory* history;
		r32 timestamp_cutoff;

		b8 compare(SequenceID a, SequenceID b)
		{
			return sequence_more_recent(a, b) && msg_frame_by_sequence((MessageHistory*)(history), a)->timestamp <= timestamp_cutoff;
		}
	};
	Comparator comparator;
	comparator.history = &history;
	comparator.timestamp_cutoff = timestamp_cutoff;
	return msg_history_foremost_sequence(history, &comparator);
}

Ack msg_history_ack(const MessageHistory& history)
{
	Ack ack = { 0, NET_SEQUENCE_INVALID };
	if (history.msg_frames.length > 0)
	{
		ack.sequence_id = msg_history_most_recent_sequence(history);

		s32 index = history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msg_frames.length - 1;

			const MessageFrame& msg = history.msg_frames[index];

			if (index == history.current_index || msg.timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;

			if (msg.sequence_id != ack.sequence_id) // ignore the ack
			{
				s32 sequence_id_relative_to_most_recent = sequence_relative_to(msg.sequence_id, ack.sequence_id);
				if (sequence_id_relative_to_most_recent < 0 // ack.sequence_id should always be the most recent received message
					&& sequence_id_relative_to_most_recent >= -NET_ACK_PREVIOUS_SEQUENCES)
					ack.previous_sequences |= u64(1) << (-sequence_id_relative_to_most_recent - 1);
			}
		}
	}
	return ack;
}

b8 msg_history_contains(const MessageHistory& history, SequenceID sequence_id)
{
	if (history.msg_frames.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			// loop backward through most recently received frames
			const MessageFrame& msg = history.msg_frames[index];

			if (msg.sequence_id == sequence_id)
				return true;

			index = index > 0 ? index - 1 : history.msg_frames.length - 1;

			if (index == history.current_index || msg.timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;
		}
	}
	return false;
}

// consolidate msgs_out into msgs_out_history
b8 msgs_out_consolidate(MessageBuffer* buffer, MessageHistory* history, SequenceID sequence_id)
{
	using Stream = StreamWrite;

	if (buffer->length == 0)
		msg_finalize(msg_new(buffer, MessageType::Noop)); // we have to send SOMETHING every sequence

	s32 bytes = 0;
	s32 msgs = 0;
	{
		for (s32 i = 0; i < buffer->length; i++)
		{
			s32 msg_bytes = (*buffer)[i].bytes_written();

			if (64 + bytes + msg_bytes > NET_MAX_MESSAGES_SIZE)
				break;

			bytes += msg_bytes;
			msgs++;
		}
	}

	MessageFrame* frame = msg_history_add(history, state_common.timestamp, bytes);

	frame->sequence_id = sequence_id;

	serialize_int(&frame->write, s32, bytes, 0, NET_MAX_MESSAGES_SIZE); // message frame size
	if (bytes > 0)
	{
		serialize_int(&frame->write, SequenceID, frame->sequence_id, 0, NET_SEQUENCE_COUNT - 1);
		for (s32 i = 0; i < msgs; i++)
			serialize_bytes(&frame->write, (u8*)((*buffer)[i].data.data), (*buffer)[i].bytes_written());
	}

	frame->write.flush();
	
	for (s32 i = msgs - 1; i >= 0; i--)
		buffer->remove_ordered(i);

	return true;
}

MessageFrame* msg_frame_advance(MessageHistory* history, MessageFrameState* processed_msg_frame, r32 timestamp_cutoff)
{
	if (processed_msg_frame->starting)
	{
		SequenceID next_sequence = sequence_advance(processed_msg_frame->sequence_id, 1);
		MessageFrame* next_frame = msg_frame_by_sequence(history, next_sequence);
		if (next_frame && next_frame->timestamp <= timestamp_cutoff)
		{
			processed_msg_frame->sequence_id = next_sequence;
			processed_msg_frame->starting = false;
			return next_frame;
		}
	}
	else
	{
		MessageFrame* frame = msg_frame_by_sequence(history, processed_msg_frame->sequence_id);
		if (frame)
		{
			SequenceID next_sequence = sequence_advance(processed_msg_frame->sequence_id, 1);
			SequenceID sequence_cutoff = msg_history_most_recent_sequence_before(*history, timestamp_cutoff);
			if (!sequence_more_recent(next_sequence, sequence_cutoff))
			{
				MessageFrame* next_frame = msg_frame_by_sequence(history, next_sequence);
				if (next_frame)
				{
					processed_msg_frame->sequence_id = next_sequence;
					return next_frame;
				}
			}
		}
	}
	return nullptr;
}

b8 ack_get(const Ack& ack, SequenceID sequence_id)
{
	s32 relative = sequence_relative_to(sequence_id, ack.sequence_id);
	if (relative == 0)
		return true;
	else if (relative > 0)
		return false;
	else if (relative < -NET_ACK_PREVIOUS_SEQUENCES)
		return false;
	else
		return ack.previous_sequences & (u64(1) << (-relative - 1));
}

void sequence_history_add(SequenceHistory* history, SequenceID id, r32 timestamp)
{
	if (history->length == history->capacity())
		history->remove(history->length - 1);
	*history->insert(0) = { timestamp, id };
}

// returns true if the sequence history contains the given ID and it has a timestamp greater than the given cutoff
b8 sequence_history_contains_newer_than(const SequenceHistory& history, SequenceID id, r32 timestamp_cutoff)
{
	for (s32 i = 0; i < history.length; i++)
	{
		const SequenceHistoryEntry& entry = history[i];
		if (entry.id == id && entry.timestamp > timestamp_cutoff)
			return true;
	}
	return false;
}

b8 msgs_write(StreamWrite* p, const MessageHistory& history, const Ack& remote_ack, SequenceHistory* recently_resent, r32 rtt)
{
	using Stream = StreamWrite;

	serialize_align(p);

	s32 bytes = 0;

	if (history.msg_frames.length > 0)
	{
		// resend previous frames
		// if the remote ack sequence is invalid, that means they haven't received anything yet
		// so don't bother resending stuff
		if (remote_ack.sequence_id != NET_SEQUENCE_INVALID)
		{
			// rewind to a bunch of frames previous
			s32 index = history.current_index;
			for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
			{
				s32 next_index = index > 0 ? index - 1 : history.msg_frames.length - 1;
				if (next_index == history.current_index || history.msg_frames[next_index].timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
					break;
				index = next_index;
			}

			// start resending frames starting at that index
			SequenceID current_seq = history.msg_frames[history.current_index].sequence_id;
			r32 timestamp_cutoff = state_common.timestamp - vi_max(tick_rate() * 2.0f, rtt * 0.5f); // don't resend stuff multiple times; wait a certain period before trying to resend it again
			for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
			{
				const MessageFrame& frame = history.msg_frames[index];
				s32 relative_sequence = sequence_relative_to(frame.sequence_id, remote_ack.sequence_id);
				if (relative_sequence >= -NET_ACK_PREVIOUS_SEQUENCES
					&& !ack_get(remote_ack, frame.sequence_id)
					&& !sequence_history_contains_newer_than(*recently_resent, frame.sequence_id, timestamp_cutoff)
					&& 8 + bytes + frame.write.bytes_written() <= NET_MAX_MESSAGES_SIZE)
				{
#if DEBUG_MSG
					vi_debug("Resending seq %d: %d bytes", s32(frame.sequence_id), s32(frame.write.bytes_written()));
#endif
					bytes += frame.write.bytes_written();
					serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
					sequence_history_add(recently_resent, frame.sequence_id, state_common.timestamp);
				}

				index = index < history.msg_frames.length - 1 ? index + 1 : 0;
				if (index == history.current_index)
					break;
			}
		}

		// current frame
		{
			const MessageFrame& frame = history.msg_frames[history.current_index];
			if (8 + bytes + frame.write.bytes_written() <= NET_MAX_MESSAGES_SIZE)
			{
#if DEBUG_MSG
				vi_debug("Sending seq %d: %d bytes", s32(frame.sequence_id), s32(frame.bytes));
#endif
				serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
			}
		}
	}

	bytes = 0;
	serialize_int(p, s32, bytes, 0, NET_MAX_MESSAGES_SIZE); // zero sized frame marks end of message frames

	return true;
}

b8 msgs_read(StreamRead* p, MessageHistory* history, const Ack& remote_ack)
{
	using Stream = StreamRead;

	serialize_align(p);

	b8 first_frame = true;
	while (true)
	{
		s32 bytes;
		serialize_int(p, s32, bytes, 0, NET_MAX_MESSAGES_SIZE);
		if (bytes)
		{
			SequenceID sequence_id;
			serialize_int(p, SequenceID, sequence_id, 0, NET_SEQUENCE_COUNT - 1);
			if (msg_history_contains(*history, sequence_id))
			{
				// already received this frame; discard it
				u8 buffer[NET_MAX_PACKET_SIZE];
				serialize_bytes(p, buffer, bytes);
			}
			else
			{
				MessageFrame* frame = msg_history_add(history, state_common.timestamp, bytes);
				frame->sequence_id = sequence_id;
				frame->remote_sequence_id = remote_ack.sequence_id;
#if DEBUG_MSG
				if (bytes > 1)
					vi_debug("Received seq %d: %d bytes", s32(frame->sequence_id), s32(bytes));
#endif
				frame->read.resize_bytes(bytes);
				serialize_bytes(p, (u8*)frame->read.data.data, bytes);
			}
			first_frame = false;
		}
		else
			break;
	}

	return true;
}

void calculate_rtt(r32 timestamp, const Ack& ack, const MessageHistory& send_history, r32* rtt)
{
	r32 new_rtt = -1.0f;
	if (send_history.msg_frames.length > 0)
	{
		s32 index = send_history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			const MessageFrame& msg = send_history.msg_frames[index];
			if (msg.sequence_id == ack.sequence_id)
			{
				new_rtt = timestamp - msg.timestamp;
				break;
			}
			index = index > 0 ? index - 1 : send_history.msg_frames.length - 1;
			if (index == send_history.current_index || send_history.msg_frames[index].timestamp < state_common.timestamp - NET_TIMEOUT)
				break;
		}
	}
	if (new_rtt == -1.0f || *rtt == -1.0f)
		*rtt = new_rtt;
	else
		*rtt = (*rtt * 0.95f) + (new_rtt * 0.05f);
}

b8 equal_states_quat(const TransformState& a, const TransformState& b)
{
	r32 tolerance_rot = 0.002f;
	if (a.resolution == Resolution::Medium || b.resolution == Resolution::Medium)
		tolerance_rot = 0.001f;
	if (a.resolution == Resolution::High || b.resolution == Resolution::High)
		tolerance_rot = 0.0001f;
	return Quat::angle(a.rot, b.rot) < tolerance_rot;
}

template<typename Stream> b8 serialize_transform(Stream* p, TransformState* transform, const TransformState* base)
{
	serialize_enum(p, Resolution, transform->resolution);
	if (!serialize_position(p, &transform->pos, transform->resolution))
		net_error();

	b8 b;
	if (Stream::IsWriting)
		b = !base || !equal_states_quat(*transform, *base);
	serialize_bool(p, b);
	if (b)
	{
		if (!serialize_quat(p, &transform->rot, transform->resolution))
			net_error();
	}
	return true;
}

template<typename Stream> b8 serialize_minion(Stream* p, MinionState* state, const MinionState* base)
{
	b8 b;

	if (Stream::IsWriting)
		b = !base || state->revision != base->revision;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->revision);

	if (Stream::IsWriting)
		b = !base || state->rotation != base->rotation;
	serialize_bool(p, b);
	if (b)
		serialize_r32_range(p, state->rotation, -PI, PI, 8);

	if (Stream::IsWriting)
		b = !base || state->animation != base->animation;
	serialize_bool(p, b);
	if (b)
		serialize_asset(p, state->animation, Loader::animation_count);

	serialize_r32_range(p, state->animation_time, 0, 20.0f, 11);

	return true;
}

template<typename Stream> b8 serialize_drone(Stream* p, DroneState* state, const DroneState* base)
{
	b8 b;

	if (Stream::IsWriting)
		b = !base || state->revision != base->revision;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->revision);

	serialize_r32_range(p, state->cooldown, 0, DRONE_COOLDOWN_MAX, 8);

	return true;
}

template<typename Stream> b8 serialize_player_manager(Stream* p, PlayerManagerState* state, const PlayerManagerState* base)
{
	b8 b;

	if (Stream::IsReading)
		state->active = true;

	if (Stream::IsWriting)
		b = !base || state->spawn_timer != base->spawn_timer;
	serialize_bool(p, b);
	if (b)
		serialize_r32_range(p, state->spawn_timer, 0, SPAWN_DELAY, 8);

	if (Stream::IsWriting)
		b = !base || state->energy != base->energy;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->energy);

	return true;
}

b8 equal_states_transform(const TransformState& a, const TransformState& b)
{
	r32 tolerance_pos = 0.008f;
	if (a.resolution == Resolution::Medium || b.resolution == Resolution::Medium)
		tolerance_pos = 0.002f;
	if (a.resolution == Resolution::High || b.resolution == Resolution::High)
		tolerance_pos = 0.001f;

	if (a.revision == b.revision
		&& a.resolution == b.resolution
		&& a.parent.equals(b.parent)
		&& equal_states_quat(a, b))
	{
		return s32(a.pos.x / tolerance_pos) == s32(b.pos.x / tolerance_pos)
			&& s32(a.pos.y / tolerance_pos) == s32(b.pos.y / tolerance_pos)
			&& s32(a.pos.z / tolerance_pos) == s32(b.pos.z / tolerance_pos);
	}

	return false;
}

b8 equal_states_transform(const StateFrame* a, const StateFrame* b, s32 index)
{
	if (a && b)
	{
		b8 a_active = a->transforms_active.get(index);
		b8 b_active = b->transforms_active.get(index);
		if (a_active == b_active)
		{
			if (a_active && b_active)
				return equal_states_transform(a->transforms[index], b->transforms[index]);
			else if (!a_active && !b_active)
				return true;
		}
	}
	return false;
}

b8 equal_states_minion(const StateFrame* frame_a, const StateFrame* frame_b, s32 index)
{
	if (frame_a && frame_b)
	{
		b8 a_active = frame_a->minions_active.get(index);
		b8 b_active = frame_b->minions_active.get(index);
		return !a_active && !b_active;
	}
	return false;
}

b8 equal_states_drone(const StateFrame* frame_a, const StateFrame* frame_b, s32 index)
{
	if (frame_a && frame_b)
	{
		b8 a_active = frame_a->drones_active.get(index);
		b8 b_active = frame_b->drones_active.get(index);
		return !a_active && !b_active;
	}
	return false;
}

b8 equal_states_player(const PlayerManagerState& a, const PlayerManagerState& b)
{
	return a.spawn_timer == b.spawn_timer
		&& a.energy == b.energy
		&& a.active == b.active;
}

template<typename Stream> b8 serialize_state_frame(Stream* p, StateFrame* frame, const StateFrame* base)
{
	if (Stream::IsReading)
	{
		if (base)
			memcpy(frame, base, sizeof(*frame));
		else
			new (frame) StateFrame();
		frame->timestamp = state_common.timestamp;
	}

	serialize_int(p, SequenceID, frame->sequence_id, 0, NET_SEQUENCE_COUNT - 1);

	// transforms
	{
		s32 changed_count;
		if (Stream::IsWriting)
		{
			// count changed transforms
			changed_count = 0;
			s32 index = vi_min(s32(frame->transforms_active.start), base ? s32(base->transforms_active.start) : MAX_ENTITIES - 1);
			while (index < vi_max(s32(frame->transforms_active.end), base ? s32(base->transforms_active.end) : 0))
			{
				if (!equal_states_transform(frame, base, index))
					changed_count++;
				index++;
			}
		}
		serialize_int(p, s32, changed_count, 0, MAX_ENTITIES);

		s32 index;
		if (Stream::IsWriting)
			index = vi_min(s32(frame->transforms_active.start), base ? s32(base->transforms_active.start) : MAX_ENTITIES - 1);
		for (s32 i = 0; i < changed_count; i++)
		{
			if (Stream::IsWriting)
			{
				while (equal_states_transform(frame, base, index))
					index++;
			}

			serialize_int(p, s32, index, 0, MAX_ENTITIES - 1);

			b8 active;
			if (Stream::IsWriting)
				active = frame->transforms_active.get(index);
			serialize_bool(p, active);
			if (Stream::IsReading)
				frame->transforms_active.set(index, active);

			if (active)
			{
				b8 revision_changed;
				if (Stream::IsWriting)
					revision_changed = !base || frame->transforms[index].revision != base->transforms[index].revision;
				serialize_bool(p, revision_changed);
				if (revision_changed)
					serialize_s16(p, frame->transforms[index].revision);
				b8 parent_changed;
				if (Stream::IsWriting)
					parent_changed = revision_changed || !base || !frame->transforms[index].parent.equals(base->transforms[index].parent);
				serialize_bool(p, parent_changed);
				if (parent_changed)
					serialize_ref(p, frame->transforms[index].parent);
				if (!serialize_transform(p, &frame->transforms[index], (base && !revision_changed) ? &base->transforms[index] : nullptr))
					net_error();
			}

			if (Stream::IsWriting)
				index++;
		}
#if DEBUG_TRANSFORMS
		vi_debug("Wrote %d transforms", changed_count);
#endif
	}

	// players
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		PlayerManagerState* state = &frame->players[i];
		b8 serialize;
		if (Stream::IsWriting)
			serialize = state->active && (!base || !equal_states_player(*state, base->players[i]));
		serialize_bool(p, serialize);
		if (serialize)
		{
			if (!serialize_player_manager(p, state, base ? &base->players[i] : nullptr))
				net_error();
		}
	}

	// minions
	{
		s32 changed_count;
		if (Stream::IsWriting)
		{
			// count changed minions
			changed_count = 0;
			s32 index = vi_min(s32(frame->minions_active.start), base ? s32(base->minions_active.start) : MAX_ENTITIES - 1);
			while (index < vi_max(s32(frame->minions_active.end), base ? s32(base->minions_active.end) : 0))
			{
				if (!equal_states_minion(frame, base, index))
					changed_count++;
				index++;
			}
		}
		serialize_int(p, s32, changed_count, 0, MAX_ENTITIES);

		s32 index;
		if (Stream::IsWriting)
			index = vi_min(s32(frame->minions_active.start), base ? s32(base->minions_active.start) : MAX_ENTITIES - 1);
		for (s32 i = 0; i < changed_count; i++)
		{
			if (Stream::IsWriting)
			{
				while (equal_states_minion(frame, base, index))
					index++;
			}

			serialize_int(p, s32, index, 0, MAX_ENTITIES - 1);
			b8 active;
			if (Stream::IsWriting)
				active = frame->minions_active.get(index);
			serialize_bool(p, active);
			if (Stream::IsReading)
				frame->minions_active.set(index, active);
			if (active)
			{
				if (!serialize_minion(p, &frame->minions[index], base ? &base->minions[index] : nullptr))
					net_error();
			}
			index++;
		}
	}

	// drones
	{
		s32 changed_count;
		if (Stream::IsWriting)
		{
			// count changed drones
			changed_count = 0;
			s32 index = vi_min(s32(frame->drones_active.start), base ? s32(base->drones_active.start) : MAX_ENTITIES - 1);
			while (index < vi_max(s32(frame->drones_active.end), base ? s32(base->drones_active.end) : 0))
			{
				if (!equal_states_drone(frame, base, index))
					changed_count++;
				index++;
			}
		}
		serialize_int(p, s32, changed_count, 0, MAX_ENTITIES);

		s32 index;
		if (Stream::IsWriting)
			index = vi_min(s32(frame->drones_active.start), base ? s32(base->drones_active.start) : MAX_ENTITIES - 1);
		for (s32 i = 0; i < changed_count; i++)
		{
			if (Stream::IsWriting)
			{
				while (equal_states_drone(frame, base, index))
					index++;
			}

			serialize_int(p, s32, index, 0, MAX_ENTITIES - 1);
			b8 active;
			if (Stream::IsWriting)
				active = frame->drones_active.get(index);
			serialize_bool(p, active);
			if (Stream::IsReading)
				frame->drones_active.set(index, active);
			if (active)
			{
				if (!serialize_drone(p, &frame->drones[index], base ? &base->drones[index] : nullptr))
					net_error();
			}
			index++;
		}
	}

	return true;
}

Resolution transform_resolution(const Transform* t)
{
	if (t->has<Drone>())
		return Resolution::High;
	return Resolution::Medium;
}

void state_frame_build(StateFrame* frame)
{
	frame->sequence_id = state_common.local_sequence_id;

	// transforms
	for (auto i = Transform::list.iterator(); !i.is_last(); i.next())
	{
		if (Game::net_transform_filter(i.item()->entity(), Game::level.mode))
		{
			frame->transforms_active.set(i.index, true);
			TransformState* transform = &frame->transforms[i.index];
			transform->revision = i.item()->revision;
			transform->pos = i.item()->pos;
			transform->rot = i.item()->rot;
			transform->parent = i.item()->parent.ref(); // ID must come out to IDNull if it's null; don't rely on revision to null the reference
			transform->resolution = transform_resolution(i.item());
		}
	}

	// players
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		PlayerManagerState* state = &frame->players[i.index];
		state->spawn_timer = i.item()->spawn_timer;
		Entity* instance = i.item()->instance.ref();
		state->energy = i.item()->energy;
		state->active = true;
	}

	// minions
	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		frame->minions_active.set(i.index, true);
		MinionState* minion = &frame->minions[i.index];
		minion->revision = i.item()->revision;
		u32 quantized_rotation = u32(255.0f * (LMath::angle_range(i.item()->get<Walker>()->rotation) + PI) / (PI * 2.0f));
		minion->rotation = -PI + r32(quantized_rotation) * (PI * 2.0f) / 255.0f;

		const Animator::Layer& layer = i.item()->get<Animator>()->layers[0];
		minion->animation = layer.animation;
		minion->animation_time = layer.time;
	}

	// drones
	for (auto i = Drone::list.iterator(); !i.is_last(); i.next())
	{
		frame->drones_active.set(i.index, true);
		DroneState* drone = &frame->drones[i.index];
		drone->revision = i.item()->revision;
		drone->cooldown = i.item()->cooldown;
	}
}

// get the absolute pos and rot of the given transform
void transform_absolute(const StateFrame& frame, s32 index, Vec3* abs_pos, Quat* abs_rot)
{
	if (abs_rot)
		*abs_rot = Quat::identity;
	*abs_pos = Vec3::zero;
	while (index != IDNull)
	{ 
		if (frame.transforms_active.get(index))
		{
			// this transform is being tracked with the dynamic transform system
			const TransformState* transform = &frame.transforms[index];
			if (abs_rot)
				*abs_rot = transform->rot * *abs_rot;
			*abs_pos = (transform->rot * *abs_pos) + transform->pos;
			index = transform->parent.id;
		}
		else
		{
			// this transform is not being tracked in our system; get its info from the game state
			Transform* transform = &Transform::list[index];
			if (abs_rot)
				*abs_rot = transform->rot * *abs_rot;
			*abs_pos = (transform->rot * *abs_pos) + transform->pos;
			index = transform->parent.ref() ? transform->parent.id : IDNull;
		}
	}
}

// convert the given pos and rot to the local coordinate system of the given transform
void transform_absolute_to_relative(const StateFrame& frame, s32 index, Vec3* pos, Quat* rot)
{
	Quat abs_rot;
	Vec3 abs_pos;
	transform_absolute(frame, index, &abs_pos, &abs_rot);

	Quat abs_rot_inverse = abs_rot.inverse();
	*rot = abs_rot_inverse * *rot;
	*pos = abs_rot_inverse * (*pos - abs_pos);
}

void state_frame_interpolate(const StateFrame& a, const StateFrame& b, StateFrame* result, r32 timestamp)
{
	result->timestamp = timestamp;
	vi_assert(timestamp >= a.timestamp);
	r32 blend = vi_min((timestamp - a.timestamp) / (b.timestamp - a.timestamp), 1.0f);
	result->sequence_id = b.sequence_id;

	// transforms
	{
		result->transforms_active = b.transforms_active;
		s32 index = s32(b.transforms_active.start);
		while (index < b.transforms_active.end)
		{
			TransformState* transform = &result->transforms[index];
			const TransformState& last = a.transforms[index];
			const TransformState& next = b.transforms[index];

			transform->parent = next.parent;
			transform->revision = next.revision;

			if (last.revision == next.revision)
			{
				if (last.parent.id == next.parent.id)
				{
					transform->pos = Vec3::lerp(blend, last.pos, next.pos);
					transform->rot = Quat::slerp(blend, last.rot, next.rot);
				}
				else
				{
					Vec3 last_pos;
					Quat last_rot;
					transform_absolute(a, index, &last_pos, &last_rot);

					if (next.parent.id != IDNull)
						transform_absolute_to_relative(b, next.parent.id, &last_pos, &last_rot);

					transform->pos = Vec3::lerp(blend, last_pos, next.pos);
					transform->rot = Quat::slerp(blend, last_rot, next.rot);
				}
			}
			else
			{
				transform->pos = next.pos;
				transform->rot = next.rot;
			}
			index = b.transforms_active.next(index);
		}
	}

	// players
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		PlayerManagerState* player = &result->players[i];
		const PlayerManagerState& last = a.players[i];
		const PlayerManagerState& next = b.players[i];
		*player = last;
		if (player->active)
		{
			if (fabsf(last.spawn_timer - next.spawn_timer) > tick_rate() * 5.0f)
				player->spawn_timer = next.spawn_timer;
			else
				player->spawn_timer = LMath::lerpf(blend, last.spawn_timer, next.spawn_timer);
		}
	}

	// minions
	{
		result->minions_active = b.minions_active;
		s32 index = s32(b.minions_active.start);
		while (index < b.minions_active.end)
		{
			MinionState* minion = &result->minions[index];
			const MinionState& last = a.minions[index];
			const MinionState& next = b.minions[index];

			minion->revision = next.revision;
			if (last.revision == next.revision)
			{
				minion->rotation = LMath::angle_range(LMath::lerpf(blend, last.rotation, LMath::closest_angle(next.rotation, last.rotation)));
				minion->animation = last.animation;
				if (last.animation == next.animation && fabsf(next.animation_time - last.animation_time) < tick_rate() * 10.0f)
					minion->animation_time = LMath::lerpf(blend, last.animation_time, next.animation_time);
				else
				{
					minion->animation_time = last.animation_time + blend * tick_rate();
					r32 duration = minion->animation == AssetNull ? 0.0f : Loader::animation(minion->animation)->duration;
					if (minion->animation_time > duration)
					{
						minion->animation = next.animation;
						minion->animation_time = next.animation_time + minion->animation_time - duration;
					}
				}
			}
			else
			{
				minion->rotation = next.rotation;
				minion->animation = next.animation;
				minion->animation_time = next.animation_time;
			}

			index = b.minions_active.next(index);
		}
	}

	// drones
	{
		result->drones_active = b.drones_active;
		s32 index = s32(b.drones_active.start);
		while (index < b.drones_active.end)
		{
			DroneState* drone = &result->drones[index];
			const DroneState& last = a.drones[index];
			const DroneState& next = b.drones[index];

			drone->revision = next.revision;
			if (last.revision == next.revision)
				drone->cooldown = LMath::lerpf(blend, last.cooldown, next.cooldown);
			else
				drone->cooldown = next.cooldown;

			index = b.drones_active.next(index);
		}
	}
}

void state_frame_apply(const StateFrame& frame, const StateFrame& frame_last, const StateFrame* frame_next)
{
	// transforms
	{
		s32 index = frame.transforms_active.start;
		while (index < frame.transforms_active.end)
		{
			Transform* t = &Transform::list[index];
			const TransformState& s = frame.transforms[index];
			if (t->revision == s.revision && Transform::list.active(index))
			{
				if (t->has<PlayerControlHuman>() && t->get<PlayerControlHuman>()->player.ref()->local())
				{
					// this is a local player; we don't want to immediately overwrite its position with the server's data
					// let the PlayerControlHuman deal with it
				}
				else
				{
					t->pos = s.pos;
					t->rot = s.rot;
					t->parent = s.parent;

					if (t->has<RigidBody>())
					{
						RigidBody* body = t->get<RigidBody>();
						btRigidBody* btBody = body->btBody;
						if (btBody)
						{
							Vec3 abs_pos;
							Quat abs_rot;
							transform_absolute(frame, index, &abs_pos, &abs_rot);

							btTransform world_transform(abs_rot, abs_pos);
							btTransform old_transform = btBody->getWorldTransform();
							btBody->setWorldTransform(world_transform);
							btBody->setInterpolationWorldTransform(world_transform);
							if ((world_transform.getOrigin() - old_transform.getOrigin()).length2() > 0.01f * 0.01f
								|| Quat::angle(world_transform.getRotation(), old_transform.getRotation()) > 0.1f)
								body->activate_linked();
							else
								body->btBody->setActivationState(ISLAND_SLEEPING);
						}
					}

					if (frame_next)
					{
						// calculate client-side velocity for components that need it
						if (t->has<Target>())
						{
							Vec3 abs_pos_last;
							Quat abs_rot_last;
							transform_absolute(frame_last, index, &abs_pos_last, &abs_rot_last);
							Vec3 abs_pos_next;
							Quat abs_rot_next;
							transform_absolute(*frame_next, index, &abs_pos_next, &abs_rot_next);
							t->get<Target>()->net_velocity = t->get<Target>()->net_velocity * 0.9f + ((abs_pos_next - abs_pos_last) / tick_rate()) * 0.1f;
						}
						else if (t->has<TramRunner>())
						{
							Vec3 abs_pos_last;
							transform_absolute(frame_last, index, &abs_pos_last);
							Vec3 abs_pos_next;
							transform_absolute(*frame_next, index, &abs_pos_next);
							t->get<TramRunner>()->velocity = t->get<TramRunner>()->velocity * 0.9f + ((abs_pos_next - abs_pos_last) / tick_rate()).length() * 0.1f;
						}
						else if (t->has<Bolt>())
						{
							Vec3 abs_pos_last;
							transform_absolute(frame_last, index, &abs_pos_last);
							Vec3 abs_pos_next;
							transform_absolute(*frame_next, index, &abs_pos_next);
							t->get<Bolt>()->velocity = (abs_pos_next - abs_pos_last) / tick_rate();
						}
					}
				}
			}

			index = frame.transforms_active.next(index);
		}
	}

	// players
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const PlayerManagerState& state = frame.players[i];
		if (state.active)
		{
			PlayerManager* s = &PlayerManager::list[i];
			s->spawn_timer = state.spawn_timer;
			s->energy = state.energy;
		}
	}

	// minions
	{
		s32 index = frame.minions_active.start;
		while (index < frame.minions_active.end)
		{
			if (Minion::list.active(index))
			{
				const MinionState& s = frame.minions[index];
				Minion* m = &Minion::list[index];
				m->get<Walker>()->rotation = s.rotation;
				Animator::Layer* layer = &m->get<Animator>()->layers[0];
				layer->animation = s.animation;
				layer->time = s.animation_time;
			}

			index = frame.minions_active.next(index);
		}
	}

	// drones
	{
		s32 index = frame.drones_active.start;
		while (index < frame.drones_active.end)
		{
			if (Drone::list.active(index))
			{
				const DroneState& state = frame.drones[index];
				Drone* drone = &Drone::list[index];
				if (drone->has<PlayerControlHuman>() && drone->get<PlayerControlHuman>()->local())
				{
					// only overwrite cooldown value if it hasn't changed recently
					// to facilitate client-side prediction
					if (Game::real_time.total - drone->cooldown_last_local_change >= rtt(drone->get<PlayerControlHuman>()->player.ref()) + interpolation_delay() + tick_rate() * 2.0f)
						drone->cooldown = state.cooldown;
				}
				else
					drone->cooldown = state.cooldown;
			}

			index = frame.drones_active.next(index);
		}
	}
}

StateFrame* state_frame_add(StateHistory* history)
{
	StateFrame* frame;
	if (history->frames.length < history->frames.capacity())
	{
		frame = history->frames.add();
		history->current_index = history->frames.length - 1;
	}
	else
	{
		history->current_index = (history->current_index + 1) % history->frames.capacity();
		frame = &history->frames[history->current_index];
	}
	new (frame) StateFrame();
	frame->timestamp = state_common.timestamp;
	return frame;
}

const StateFrame* state_frame_by_sequence(const StateHistory& history, SequenceID sequence_id)
{
	if (history.frames.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			const StateFrame* frame = &history.frames[index];
			if (frame->sequence_id == sequence_id)
				return frame;

			// loop backward through most recent frames
			index = index > 0 ? index - 1 : history.frames.length - 1;
			if (index == history.current_index || history.frames[index].timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;
		}
	}
	return nullptr;
}

const StateFrame* state_frame_by_timestamp(const StateHistory& history, r32 timestamp)
{
	if (history.frames.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < NET_PREVIOUS_SEQUENCES_SEARCH; i++)
		{
			const StateFrame* frame = &history.frames[index];

			if (frame->timestamp < timestamp)
				return &history.frames[index];

			// loop backward through most recent frames
			index = index > 0 ? index - 1 : history.frames.length - 1;
			if (index == history.current_index || history.frames[index].timestamp < state_common.timestamp - NET_TIMEOUT) // hit the end
				break;
		}
	}
	return nullptr;
}

const StateFrame* state_frame_next(const StateHistory& history, const StateFrame& frame)
{
	if (history.frames.length > 1)
	{
		s32 index = &frame - history.frames.data;
		index = index < history.frames.length - 1 ? index + 1 : 0;
		const StateFrame& frame_next = history.frames[index];
		if (sequence_more_recent(frame_next.sequence_id, frame.sequence_id))
			return &frame_next;
	}
	return nullptr;
}

void replay_filename_generate(char* filename)
{
	while (true)
	{
		sprintf(filename, "rec/%u.rec", mersenne::rand());

		// check for file's existence
		FILE* f = fopen(filename, "rb");
		if (f)
			fclose(f);
		else
			break;
	}
}

b8 master_user_role_set(u32 server_id, u32 user_id, Master::Role role)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::UserRoleSet);

	b8 is_server;
#if SERVER
	is_server = true;
	serialize_bool(&p, is_server);
	serialize_u64(&p, Settings::secret);
#else
	is_server = false;
	serialize_bool(&p, is_server);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
#endif

	serialize_u32(&p, server_id);
	serialize_u32(&p, user_id);
	serialize_enum(&p, Master::Role, role);

	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

#if SERVER

namespace Server
{

struct Client
{
	Sock::Address address;
	r32 timeout;
	r32 rtt = 0.5f;
	r32 auth_timeout = 8.0f; // we allow a client to connect for a certain amount of time before hearing from the master server that the client is okay
	Master::UserKey user_key;
	Ack ack = { u32(-1), NET_SEQUENCE_INVALID }; // most recent ack we've received from the client
	Ack ack_load = { u32(-1), NET_SEQUENCE_INVALID }; // most recent ack for load messages we've received from the client
	MessageHistory msgs_in_history; // messages we've received from the client
	MessageHistory msgs_out_load_history; // messages we've sent to this client to serialize map data. NOTE: map data must fit within 64 sequences.
	MessageBuffer msgs_out_load; // buffer of messages we need to send to serialize map data
	SequenceHistory recently_resent; // sequences we resent to the client recently
	SequenceHistory recently_resent_load; // sequences we resent to the client recently (load messages)
	StaticArray<Ref<PlayerHuman>, MAX_GAMEPADS> players;
	// most recent sequence ID we've processed from the client
	// starts at NET_SEQUENCE_COUNT - 1 so that the first sequence ID we expect to receive is 0
	MessageFrameState processed_msg_frame = { NET_SEQUENCE_COUNT - 1, true };
	SequenceID first_load_sequence;
	SequenceID acked_state_frame = NET_SEQUENCE_INVALID; // most recent state frame the client has acked
	char username[MAX_USERNAME + 1];
	// when a client connects, we add them to our list, but set connected to false.
	// as soon as they send us an Update packet, we set connected to true.
	b8 connected;
	b8 loading_done;
	b8 is_admin;
};

struct ExpectedClient
{
	struct TeamEntry
	{
		u64 uuid;
		AI::Team team;
	};

	typedef StaticArray<ExpectedClient::TeamEntry, MAX_GAMEPADS> Teams;

	Teams teams;
	r32 timestamp;
	Master::UserKey user_key;
	b8 is_admin;
};

b8 msg_process(StreamRead*, Client*, SequenceID);

struct StateServer
{
	FILE* replay_file;
	Array<Client> clients;
	Array<Ref<Entity>> finalize_children_queue;
	Array<ExpectedClient> expected_clients;
	Mode mode;
	r32 time_sync_timer;
	r32 master_timer;
	r32 idle_timer = NET_SERVER_IDLE_TIME;
	Sock::Address replay_address;
	b8 transitioning_level;
};
StateServer state_server;

struct StateServerPersistent
{
	Sock::Address public_ipv4;
	Sock::Address public_ipv6;
};
StateServerPersistent state_server_persistent;

b8 client_owns(Client* c, Entity* e)
{
	PlayerHuman* player = nullptr;
	if (e->has<PlayerHuman>())
		player = e->get<PlayerHuman>();
	else if (e->has<PlayerControlHuman>())
		player = e->get<PlayerControlHuman>()->player.ref();

	for (s32 i = 0; i < c->players.length; i++)
	{
		if (c->players[i].ref() == player)
			return true;
	}

	return false;
}

Client* client_for_player(const PlayerHuman* player)
{
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* c = &state_server.clients[i];
		for (s32 j = 0; j < c->players.length; j++)
		{
			if (c->players[j].ref() == player)
				return c;
		}
	}
	return nullptr;
}

// for external consumption
ID client_id(const PlayerHuman* player)
{
	Client* client = client_for_player(player);
	if (client)
		return ID(client - &state_server.clients[0]);
	else
		return IDNull;
}

void packet_sent(const StreamWrite& p, const Sock::Address& address)
{
	if (state_server.replay_file && address.equals(state_server.replay_address))
	{
		s16 size = p.bytes_written();
		fwrite(&size, sizeof(s16), 1, state_server.replay_file);
		fwrite(p.data.data, sizeof(s8), s32(size), state_server.replay_file);
	}
}

void server_state(Master::ServerState* s)
{
	s->level = Game::level.id;
	if (s->level == AssetNull)
	{
		s->id = 0;
		s->player_slots = 1;
	}
	else
	{
		s->id = Game::session.config.id;
		s->player_slots = s8(vi_max(0, Game::session.config.max_players - PlayerHuman::list.count()));
	}
	s->story_mode_team = Game::level.story_mode_team;
	s->region = Settings::region;
}

b8 master_send_status_update()
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::ServerStatusUpdate);
	serialize_u64(&p, Settings::secret);
	Master::ServerState s;
	server_state(&s);
	if (!serialize_server_state(&p, &s))
		net_error();

	if (!Sock::Address::serialize(&p, &state_server_persistent.public_ipv4))
		net_error();

	if (!Sock::Address::serialize(&p, &state_server_persistent.public_ipv6))
		net_error();

	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);

	state_server.master_timer = 0.0f;

	return true;
}

void init()
{
	if (Sock::udp_open(&state_persistent.sock, Settings::port))
	{
		fprintf(stderr, "%s\n", Sock::get_error());
		vi_assert(false);
	}

	master_init();
	master_send(Master::Message::Disconnect);
	state_persistent.master.reset();

	if (Settings::public_ipv4[0])
		Sock::Address::get(&state_server_persistent.public_ipv4, Settings::public_ipv4, Settings::port);

	if (Settings::public_ipv6[0])
		Sock::Address::get(&state_server_persistent.public_ipv6, Settings::public_ipv6, Settings::port);
}

// let clients know we're about to switch levels
void transition_level()
{
	msg_finalize(msg_new(MessageType::TransitionLevel));
	state_server.transitioning_level = true;
}

// disable entity messages while we're unloading the level
void level_unloading()
{
	state_server.mode = Mode::Loading;
}

// enter normal operation after loading a level
void level_unloaded()
{
	vi_assert(state_server.mode == Mode::Loading);
	state_server.mode = Mode::Idle;
}

// disable entity spawn messages while we're loading the level
void level_loading()
{
	state_server.mode = Mode::Loading;
}

// enter normal operation after loading a level
void level_loaded()
{
	vi_assert(state_server.mode == Mode::Loading);
	state_server.mode = Mode::Active;
	master_send_status_update();
}

b8 sync_time()
{
	using Stream = StreamWrite;
	StreamWrite* p = msg_new(MessageType::TimeSync);
	serialize_r32_range(p, Team::match_time, 0, Game::session.config.time_limit(), 16);
	serialize_r32_range(p, Game::session.time_scale, 0, 1, 8);
	s32 players = PlayerHuman::list.count();
	serialize_int(p, s32, players, 0, MAX_PLAYERS);
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
	{
		serialize_int(p, ID, i.index, 0, MAX_PLAYERS - 1);
		const Client* client = client_for_player(i.item());
		r32 r = client ? client->rtt : 0.0f;
		serialize_r32_range(p, r, 0, 1.024f, 10);
	}
	msg_finalize(p);

	state_server.time_sync_timer = 0.0f;
	return true;
}

b8 packet_build_init(StreamWrite* p, Client* client)
{
	using Stream = StreamWrite;
	packet_init(p);
	ServerPacket type = ServerPacket::Init;
	serialize_enum(p, ServerPacket, type);
	serialize_int(p, SequenceID, client->first_load_sequence, 0, NET_SEQUENCE_COUNT - 1);
	if (!serialize_init_packet(p))
		net_error();
	packet_finalize(p);
	return true;
}

b8 packet_build_ping_response(StreamWrite* p, u32 token)
{
	using Stream = StreamWrite;
	packet_init(p);
	ServerPacket type = ServerPacket::PingResponse;
	serialize_enum(p, ServerPacket, type);
	serialize_u32(p, token);
	packet_finalize(p);
	return true;
}

b8 packet_build_disconnect(StreamWrite* p, DisconnectReason reason)
{
	using Stream = StreamWrite;
	packet_init(p);
	ServerPacket type = ServerPacket::Disconnect;
	serialize_enum(p, ServerPacket, type);
	serialize_enum(p, DisconnectReason, reason);
	packet_finalize(p);
	return true;
}

b8 packet_build_update(StreamWrite* p, Client* client, StateFrame* frame)
{
	packet_init(p);
	using Stream = StreamWrite;

	{
		ServerPacket type = ServerPacket::Update;
		serialize_enum(p, ServerPacket, type);
	}

	{
		Ack ack = msg_history_ack(client->msgs_in_history);
		serialize_int(p, SequenceID, ack.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
		serialize_u64(p, ack.previous_sequences);
	}

	{
		Ack client_ack;
		if (client->ack.sequence_id == NET_SEQUENCE_INVALID)
		{
			// client hasn't acked anything yet; just assume they don't need us to resend anything yet
			client_ack.sequence_id = sequence_advance(client->first_load_sequence, -1);
			client_ack.previous_sequences = u64(-1);
		}
		else
		{
			client_ack = client->ack;
			if (client->msgs_out_load_history.msg_frames.length > 0)
			{
				// we're somewhere in the initialization process
				// make sure we don't resend sequences from before the client joined
				s32 legitimate_sequences = sequence_relative_to(client_ack.sequence_id, client->first_load_sequence);
				vi_assert(legitimate_sequences >= 0);
				for (s32 i = NET_ACK_PREVIOUS_SEQUENCES; i > legitimate_sequences; i--)
					client_ack.previous_sequences |= u64(1) << (i - 1);
			}
		}
		msgs_write(p, state_common.msgs_out_history, client_ack, &client->recently_resent, client->rtt);
	}

	b8 has_load_msgs = !client->loading_done;
	serialize_bool(p, has_load_msgs);
	if (has_load_msgs)
		msgs_write(p, client->msgs_out_load_history, client->ack_load, &client->recently_resent_load, client->rtt);
	else if (frame)
	{
		if (client->msgs_out_load_history.msg_frames.length > 0
			&& client->ack.sequence_id != NET_SEQUENCE_INVALID
			&& sequence_relative_to(client->ack.sequence_id, client->first_load_sequence) > NET_ACK_PREVIOUS_SEQUENCES)
			client->msgs_out_load_history.msg_frames.length = 0; // it's been long enough, we can stop worrying about this. all frames should have state frames by now

		serialize_int(p, SequenceID, client->acked_state_frame, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because base_sequence_id might be NET_SEQUENCE_INVALID
		const StateFrame* base = state_frame_by_sequence(state_common.state_history, client->acked_state_frame);
		if (!serialize_state_frame(p, frame, base))
			net_error();
	}

	packet_finalize(p);
	return true;
}

void handle_client_disconnect(Client* c)
{
	if (c->address.equals(state_server.replay_address))
	{
		if (state_server.replay_file)
			fclose(state_server.replay_file);

		new (&state_server.replay_address) Sock::Address();
	}

	for (s32 i = 0; i < c->players.length; i++)
	{
		PlayerHuman* player = c->players[i].ref();
		if (player)
		{
			Entity* instance = player->get<PlayerManager>()->instance.ref();
			if (instance)
				World::remove_deferred(instance);
			World::remove_deferred(player->entity());
		}
	}
	state_server.clients.remove(c - &state_server.clients[0]);
	master_send_status_update();
}

void update(const Update& u, r32 dt)
{
	// prune ExpectedClients that have timed out
	for (s32 i = 0; i < state_server.expected_clients.length; i++)
	{
		const ExpectedClient& expected_client = state_server.expected_clients[i];
		if (state_common.timestamp - expected_client.timestamp > NET_EXPECTED_CLIENT_TIMEOUT)
		{
			state_server.expected_clients.remove(i);
			i--;
		}
	}

	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* client = &state_server.clients[i];
		while (MessageFrame* frame = msg_frame_advance(&client->msgs_in_history, &client->processed_msg_frame, state_common.timestamp + 1.0f))
		{
			frame->read.rewind();
			while (frame->read.bytes_read() < frame->bytes)
			{
				b8 success = msg_process(&frame->read, client, frame->sequence_id);
				if (!success)
					break;
			}
		}

		if (client->auth_timeout > 0.0f) // master server hasn't authenticated this client yet
		{
			client->auth_timeout = vi_max(0.0f, client->auth_timeout - dt);
			if (client->auth_timeout == 0.0f)
			{
				// didn't expect this client
				StreamWrite p;
				packet_build_disconnect(&p, DisconnectReason::AuthFailed);
				packet_send(p, client->address);
				handle_client_disconnect(client);
			}
		}
	}
}

void client_force_disconnect(ID id, DisconnectReason reason)
{
	Client* client = &state_server.clients[id];
	StreamWrite p;
	packet_build_disconnect(&p, reason);
	packet_send(p, client->address);
	handle_client_disconnect(client);
}

void admin_set(PlayerHuman* player, b8 value)
{
	Client* client = client_for_player(player);
	if (client)
		client->is_admin = value;
}

void tick(const Update& u, r32 dt)
{
	if (state_server.mode == Mode::Active)
	{
		state_server.time_sync_timer += dt;
		if (state_server.time_sync_timer > 5.0f)
			sync_time();
	}

	if (PlayerHuman::list.count() == 0)
	{
		if (state_server.mode != Mode::Idle)
		{
			state_server.idle_timer -= dt;
			if (state_server.idle_timer < 0.0f)
			{
				state_server.mode = Mode::Idle;
				Game::unload_level();
			}
		}
	}
	else
		state_server.idle_timer = 1.0f; // after players have connected, the server can be empty for one second before it reverts to idle state

	state_server.master_timer += dt;
	if (state_server.master_timer > NET_MASTER_STATUS_INTERVAL)
		master_send_status_update();
	state_persistent.master.update(state_common.timestamp, &state_persistent.sock, 4);

	StateFrame* frame = nullptr;

	msgs_out_consolidate(&state_common.msgs_out, &state_common.msgs_out_history, state_common.local_sequence_id);
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* client = &state_server.clients[i];
		if (!client->loading_done)
			msgs_out_consolidate(&client->msgs_out_load, &client->msgs_out_load_history, state_common.local_sequence_id);
	}
	frame = state_frame_add(&state_common.state_history);
	state_frame_build(frame);

	StreamWrite p;
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* client = &state_server.clients[i];
		client->timeout += dt;
		if (client->timeout > NET_TIMEOUT)
		{
			{
				char addr[NET_MAX_ADDRESS];
				client->address.str(addr);
				vi_debug("Client %s timed out.", addr);
			}
			handle_client_disconnect(client);
			i--;
		}
		else
		{
			p.reset();
			packet_build_update(&p, client, frame);
			packet_send(p, client->address);
		}
	}

	state_common.local_sequence_id = sequence_advance(state_common.local_sequence_id, 1);
}

b8 client_connected(StreamRead* p, Client* client)
{
	using Stream = StreamRead;

	return true;
}

b8 packet_handle_master(StreamRead* p)
{
	using Stream = StreamRead;
	{
		s16 version;
		serialize_s16(p, version);
	}
	SequenceID seq;
	serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
	Master::Message type;
	serialize_enum(p, Master::Message, type);
	state_persistent.master.received(type, seq, state_persistent.master_addr, &state_persistent.sock);

	switch (type)
	{
		case Master::Message::Ack:
		case Master::Message::Keepalive:
		{
			break;
		}
		case Master::Message::Disconnect:
		{
			state_persistent.master.reset();
			break;
		}
		case Master::Message::ServerLoad:
		{
			if (!Master::serialize_server_config(p, &Game::session.config))
				net_error();

			if (Game::session.config.id == 0)
			{
				Game::session.type = SessionType::Story;
				AssetID level;
				serialize_s16(p, level);
				StoryModeTeam story_mode_team;
				serialize_enum(p, StoryModeTeam, story_mode_team);
				Game::load_level(level, Game::Mode::Pvp, story_mode_team);
			}
			else
			{
				Game::session.type = SessionType::Multiplayer;
				vi_assert(Game::session.config.levels.length > 0);
				Game::load_level(Overworld::zone_id_for_uuid(Game::session.config.levels[mersenne::rand() % Game::session.config.levels.length]), Game::Mode::Pvp);
			}
			break;
		}
		case Master::Message::ServerConfig:
		{
			u32 request_id;
			serialize_u32(p, request_id); // not used
			if (!Master::serialize_server_config(p, &Game::level.config_scheduled))
				net_error();
			Game::level.config_scheduled_apply = true;
			break;
		}
		case Master::Message::ExpectClient:
		{
			Master::UserKey key;
			serialize_u32(p, key.id);
			serialize_u32(p, key.token);
			b8 is_admin;
			serialize_bool(p, is_admin);

			// first, check if the client already connected
			b8 already_connected = false;
			for (s32 i = 0; i < state_server.clients.length; i++)
			{
				Client* client = &state_server.clients[i];
				if (key.equals(client->user_key))
				{
					client->auth_timeout = 0.0f;
					client->is_admin = is_admin;
					if (is_admin)
					{
						// let the client's player know they're an admin
						for (s32 i = 0; i < client->players.length; i++)
						{
							if (client->players[i].ref()->gamepad == 0)
								client->players[i].ref()->get<PlayerManager>()->make_admin();
						}
					}
					already_connected = true;
					break;
				}
			}

			if (!already_connected)
			{
				// client hasn't connected yet; save the entry in expected_clients for when they do connect
				ExpectedClient* entry = nullptr;
				for (s32 i = 0; i < state_server.expected_clients.length; i++)
				{
					ExpectedClient* c = &state_server.expected_clients[i];
					if (c->user_key.id == key.id)
					{
						entry = c;
						break;
					}
				}

				if (!entry)
				{
					entry = state_server.expected_clients.add();
					new (entry) ExpectedClient();
				}

				entry->timestamp = state_common.timestamp;
				entry->user_key = key;
				entry->is_admin = is_admin;
			}
			break;
		}
		case Master::Message::WrongVersion:
		{
			// todo: something
			break;
		}
		default:
		{
			net_error();
			break;
		}
	}

	return true;
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
	if (address.equals(state_persistent.master_addr))
		return packet_handle_master(p);

	Client* client = nullptr;
	s32 client_index = -1;
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		if (address.equals(state_server.clients[i].address))
		{
			client = &state_server.clients[i];
			client_index = i;
			break;
		}
	}

	using Stream = StreamRead;

	ClientPacket type;
	serialize_enum(p, ClientPacket, type);

	switch (type)
	{
		case ClientPacket::Connect:
		{
			s16 game_version;
			serialize_s16(p, game_version);
			s32 local_players;
			serialize_int(p, s32, local_players, 1, MAX_GAMEPADS);
			if (game_version == GAME_VERSION)
			{
				if (!client
					&& state_server.mode == Mode::Active
					&& Game::session.config.max_players >= PlayerHuman::list.count() + local_players)
				{
					client = state_server.clients.add();
					client_index = state_server.clients.length - 1;
					new (client) Client();
					client->address = address;
					client->first_load_sequence = state_common.local_sequence_id;
					{
						char str[NET_MAX_ADDRESS];
						address.str(str);
						vi_debug("Client %s starting on sequence %d", str, s32(client->first_load_sequence));
					}

					if (Settings::record && Game::session.type != SessionType::Story && !state_server.replay_file)
					{
						state_server.replay_address = address;

						char filename[MAX_PATH_LENGTH + 1];
						replay_filename_generate(filename);
						vi_debug("Recording gameplay to '%s'.", filename);
						state_server.replay_file = fopen(filename, "wb");
					}

					// serialize out map data
					{
						using Stream = StreamWrite;

						// entity data
						for (auto j = Entity::list.iterator(); !j.is_last(); j.next())
						{
							StreamWrite* p = msg_new(&client->msgs_out_load, MessageType::EntityCreate);
							serialize_int(p, ID, j.index, 0, MAX_ENTITIES - 1);
							if (!serialize_entity(p, j.item()))
								vi_assert(false);
							msg_finalize(p);
						}

						// done
						msg_finalize(msg_new(&client->msgs_out_load, MessageType::InitDone));
					}
				}

				if (client)
				{
					StreamWrite p;
					packet_build_init(&p, client);
					packet_send(p, address);
				}
				else
				{
					// server is full
					StreamWrite p;
					packet_build_disconnect(&p, DisconnectReason::ServerFull);
					packet_send(p, address);
				}
			}
			else
			{
				// wrong version
				StreamWrite p;
				packet_build_disconnect(&p, DisconnectReason::WrongVersion);
				packet_send(p, address);
			}
			break;
		}
		case ClientPacket::Update:
		{
			if (!client) // unknown client; ignore
				return false;

			// read ack
			{
				Ack ack_candidate;
				serialize_int(p, SequenceID, ack_candidate.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
				serialize_u64(p, ack_candidate.previous_sequences);
				if (sequence_more_recent(ack_candidate.sequence_id, client->ack.sequence_id))
					client->ack = ack_candidate;

				if (!msgs_read(p, &client->msgs_in_history, ack_candidate))
					net_error();
			}

			{
				b8 has_ack_load;
				serialize_bool(p, has_ack_load);
				if (has_ack_load)
				{
					Ack ack_candidate;
					serialize_int(p, SequenceID, ack_candidate.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
					serialize_u64(p, ack_candidate.previous_sequences);
					if (sequence_more_recent(ack_candidate.sequence_id, client->ack_load.sequence_id))
						client->ack_load = ack_candidate;
				}
			}

			calculate_rtt(state_common.timestamp, client->ack, state_common.msgs_out_history, &client->rtt);

			client->timeout = 0.0f;
			if (!client->connected)
			{
				char str[NET_MAX_ADDRESS];
				client->address.str(str);
				vi_debug("Client %s connected.", str);
				client->connected = true;
			}

			// client is letting us know what the last state frame they received was
			serialize_int(p, SequenceID, client->acked_state_frame, 0, NET_SEQUENCE_COUNT);

			b8 most_recent;
			{
				// we must serialize the current sequence ID separately from the message system
				// because perhaps msgs_write did not have enough room to serialize the message frame for the current sequence
				SequenceID sequence_id;
				serialize_int(p, SequenceID, sequence_id, 0, NET_SEQUENCE_COUNT - 1);

				if (abs(sequence_relative_to(sequence_id, client->processed_msg_frame.sequence_id)) > NET_MAX_SEQUENCE_GAP)
				{
					// we missed a packet that we'll never be able to recover
					char str[NET_MAX_ADDRESS];
					client->address.str(str);
					vi_debug("Connection with client %s failed due to sequence gap. Last seq: %d Current seq: %d", str, s32(client->processed_msg_frame.sequence_id), s32(sequence_id));
					handle_client_disconnect(client);
					return false;
				}

				// only take player control data if this is the most recent packet we've received
				most_recent = sequence_relative_to(sequence_id, msg_history_most_recent_sequence(client->msgs_in_history)) >= 0;
			}

			s32 count;
			serialize_int(p, ID, count, 0, MAX_GAMEPADS);
			for (s32 i = 0; i < count; i++)
			{
				ID id;
				serialize_int(p, ID, id, 0, MAX_PLAYERS - 1);

				PlayerControlHuman* c = &PlayerControlHuman::list[id];

				PlayerControlHuman::RemoteControl control;

				if (!serialize_player_control(p, &control))
					net_error();

				if (most_recent && PlayerControlHuman::list.active(id) && client_owns(client, c->entity()))
					c->remote_control_handle(control);
			}

			break;
		}
		case ClientPacket::Ping:
		{
			u32 token;
			serialize_u32(p, token);
			{
				StreamWrite p;
				packet_build_ping_response(&p, token);
				packet_send(p, address);
			}
			break;
		}
		case ClientPacket::Disconnect:
		{
			if (!client) // unknown client; ignore
				return false;
			char str[NET_MAX_ADDRESS];
			address.str(str);
			vi_debug("Client %s disconnected.", str);
			handle_client_disconnect(client);

			break;
		}
		default:
		{
			vi_debug("%s", "Discarding packet due to invalid packet type.");
			net_error();
		}
	}

	return true;
}

b8 add_players(Net::StreamRead* p, Client* client, s32 count, const ExpectedClient::Teams* teams = nullptr)
{
	using Stream = Net::StreamRead;

	if (Game::session.type == SessionType::Multiplayer)
		Game::remove_bots_if_necessary(count);

	for (s32 i = 0; i < count; i++)
	{
		s8 gamepad;
		serialize_int(p, s8, gamepad, 0, MAX_GAMEPADS - 1);
		u64 uuid;
		serialize_u64(p, uuid);

		b8 already_added = false;
		for (s32 j = 0; j < client->players.length; j++)
		{
			PlayerHuman* player = client->players[j].ref();
			if (player->gamepad == gamepad)
			{
				already_added = true;
				break;
			}
		}

		if (!already_added && PlayerHuman::list.count() < Game::session.config.max_players)
		{
			Team* team_ref = nullptr;

			switch (Game::session.type)
			{
				case SessionType::Multiplayer:
				{
					if (teams)
					{
						// remember previous teams, and rotate them
						for (s32 j = 0; j < teams->length; j++)
						{
							const ExpectedClient::TeamEntry& entry = (*teams)[j];
							if (entry.uuid == uuid)
							{
								if (entry.team != AI::TeamNone)
									team_ref = &Team::list[(entry.team + 1) % Team::list.count()];
								break;
							}
						}
					}

					if (!team_ref)
					{
						// distribute players evenly
						// assign player randomly to one of the teams with the smallest number of players
						StaticArray<Team*, MAX_TEAMS> smallest_teams;
						s32 least_players;
						Team::with_least_players(&least_players);
						for (auto i = Team::list.iterator(); !i.is_last(); i.next())
						{
							if (i.item()->player_count() == least_players)
								smallest_teams.add(i.item());
						}
						vi_assert(smallest_teams.length > 0);
						team_ref = smallest_teams[mersenne::rand() % smallest_teams.length];
					}
					break;
				}
				case SessionType::Story:
					team_ref = Game::level.story_mode_team == StoryModeTeam::Attack ? &Team::list[1] : &Team::list[0];
					break;
				default:
					vi_assert(false);
					break;
			}

			Entity* e = World::create<ContainerEntity>();
			PlayerManager* manager = e->add<PlayerManager>(team_ref);
			strncpy(manager->username, client->username, MAX_USERNAME);
			if (gamepad > 0)
			{
				char number[5] = {};
				snprintf(number, 5, " [%d]", s32(gamepad + 1));
				Font::truncate(manager->username, MAX_USERNAME, number, Font::EllipsisMode::Always);
			}
			PlayerHuman* player = e->add<PlayerHuman>(false, gamepad);
			player->master_id = client->user_key.id;
			manager->is_admin = client->is_admin && gamepad == 0;
			player->uuid = uuid;
			client->players.add(player);
			finalize(e);
		}
	}
	master_send_status_update();

	return true;
}

// server function for processing messages
b8 msg_process(StreamRead* p, Client* client, SequenceID seq)
{
	using Stream = StreamRead;
	vi_assert(client);
	MessageType type;
	serialize_enum(p, MessageType, type);
#if DEBUG_MSG
	if (type != MessageType::Noop)
		vi_debug("Processing message type %d", type);
#endif
	switch (type)
	{
		case MessageType::Noop:
		{
			break;
		}
		case MessageType::LoadingDone:
		{
			client->loading_done = true;
			{
				char str[NET_MAX_ADDRESS];
				client->address.str(str);
				vi_debug("Client %s finished loading.", str);
			}

			// client is connected; we're no longer expecting them
			for (s32 i = 0; i < state_server.expected_clients.length; i++)
			{
				const Master::UserKey& expected_client = state_server.expected_clients[i].user_key;
				if (expected_client.equals(client->user_key))
				{
					state_server.expected_clients.remove(i);
					i--;
				}
			}
			break;
		}
		case MessageType::PlayerControlHuman:
		{
			Ref<PlayerControlHuman> c;
			serialize_ref(p, c);
			b8 valid = c.ref() && client_owns(client, c.ref()->entity());
			if (!PlayerControlHuman::net_msg(p, c.ref(), valid ? MessageSource::Remote : MessageSource::Invalid, seq))
				net_error();
			break;
		}
		case MessageType::PlayerManager:
		{
			Ref<PlayerManager> m;
			serialize_ref(p, m);

			PlayerManager::Message msg;
			serialize_enum(p, PlayerManager::Message, msg);

			b8 valid = !m.ref()
				|| client_owns(client, m.ref()->entity())
				|| (m.ref()->is_admin && (msg == PlayerManager::Message::TeamSchedule || msg == PlayerManager::Message::CanSpawn));
			if (!PlayerManager::net_msg(p, m.ref(), msg, valid ? MessageSource::Remote : MessageSource::Invalid))
				net_error();
			break;
		}
		case MessageType::UpgradeStation:
		{
			if (!UpgradeStation::net_msg(p, MessageSource::Remote))
				net_error();
			break;
		}
		case MessageType::ClientSetup:
		{
			// create players
			if (client->players.length == 0)
			{
				serialize_u32(p, client->user_key.id);
				serialize_u32(p, client->user_key.token);

				s32 username_length;
				serialize_int(p, s32, username_length, 0, MAX_USERNAME);
				serialize_bytes(p, (u8*)client->username, username_length);
				client->username[username_length] = '\0';

				s32 local_players;
				serialize_int(p, s32, local_players, 1, MAX_GAMEPADS);

				ExpectedClient::Teams teams;

				for (s32 i = 0; i < state_server.expected_clients.length; i++)
				{
					const ExpectedClient& expected_client = state_server.expected_clients[i];
					if (expected_client.user_key.equals(client->user_key))
					{
						client->auth_timeout = 0.0f;
						client->is_admin = expected_client.is_admin;
						teams = expected_client.teams;
						state_server.expected_clients.remove(i);
						break;
					}
				}

				if (local_players <= Game::session.config.max_players - PlayerHuman::list.count())
					add_players(p, client, local_players, &teams);
				else
				{
					// server is full
					StreamWrite p;
					packet_build_disconnect(&p, DisconnectReason::ServerFull);
					packet_send(p, client->address);
					handle_client_disconnect(client);
					return false;
				}
			}
			break;
		}
		case MessageType::AddPlayer:
		{
			if (!add_players(p, client, 1))
				net_error();
			break;
		}
		case MessageType::DebugCommand:
		{
			s32 len;
			serialize_int(p, s32, len, 0, 512);
			char buffer[513];
			serialize_bytes(p, (u8*)buffer, len);
			buffer[len] = '\0';
#if !RELEASE_BUILD
			Game::execute(buffer);
#endif
			break;
		}
		default:
			net_error();
			break;
	}
	serialize_align(p);
	return true;
}

Mode mode()
{
	return state_server.mode;
}

void reset()
{
	StreamWrite p;
	packet_build_disconnect(&p, DisconnectReason::ServerResetting);
	for (s32 i = 0; i < state_server.clients.length; i++)
		packet_send(p, state_server.clients[i].address);

	if (state_server.replay_file)
		fclose(state_server.replay_file);

	Array<ExpectedClient> expected_clients;
	if (state_server.transitioning_level)
	{
		// transition expected_clients over
		for (s32 i = 0; i < state_server.expected_clients.length; i++)
			expected_clients.add(state_server.expected_clients[i]);

		for (s32 i = 0; i < state_server.clients.length; i++)
		{
			const Client& client = state_server.clients[i];
			ExpectedClient expected_client;
			expected_client.timestamp = 0.0f;
			expected_client.user_key = client.user_key;
			expected_client.is_admin = client.is_admin;
			for (s32 j = 0; j < client.players.length; j++)
			{
				const PlayerHuman* player = client.players[j].ref();
				expected_client.teams.add({ player->uuid, player->get<PlayerManager>()->team.ref()->team() });
			}
			expected_clients.add(expected_client);
		}
	}

	state_server.~StateServer();
	new (&state_server) StateServer();

	for (s32 i = 0; i < expected_clients.length; i++)
		state_server.expected_clients.add(expected_clients[i]);
}

void player_deleting(const PlayerHuman* player)
{
	Server::Client* client = Server::client_for_player(player);
	if (client)
	{
		for (s32 i = 0; i < client->players.length; i++)
		{
			if (client->players[i].ref() == player)
			{
				client->players.remove(i);
				i--;
			}
		}
	}
}

// this is meant for external consumption in the game code.
r32 rtt(const PlayerHuman* p, SequenceID client_seq)
{
	if (Game::level.local && p->local())
		return 0.0f;

	Server::Client* client = Server::client_for_player(p);

	vi_assert(client);

	MessageFrame* frame = msg_frame_by_sequence(&client->msgs_in_history, client_seq);
	if (frame)
	{
		SequenceID server_seq = frame->remote_sequence_id;
		const StateFrame* state_frame = state_frame_by_sequence(state_common.state_history, server_seq);
		return state_common.timestamp - state_frame->timestamp;
	}
	else
		return client->rtt;
}

}

#else

namespace Client
{

MasterError master_error;
DisconnectReason disconnect_reason;
r32 master_auth_timer;
Array<std::array<char, MAX_PATH_LENGTH + 1> > replay_files;
s32 replay_file_index;

void replay_file_add(const char* filename)
{
	std::array<char, MAX_PATH_LENGTH + 1>* entry = replay_files.add();
	strncpy(entry->data(), filename, MAX_PATH_LENGTH);
}

s32 replay_file_count()
{
	return replay_files.length;
}

b8 msg_process(StreamRead*);

struct StateClient
{
	FILE* replay_file;
	r32 timeout;
	r32 tick_timer;
	r32 server_rtt = 0.15f;
	r32 rtts[MAX_PLAYERS];
	u32 requested_server_id;
	MessageHistory msgs_in_history; // messages we've received from the server
	MessageHistory msgs_in_load_history; // load messages we've received from the server
	Ack server_ack = { u32(-1), NET_SEQUENCE_INVALID }; // most recent ack we've received from the server
	Sock::Address server_address;
	SequenceHistory server_recently_resent; // sequences we recently resent to the server
	MessageFrameState server_processed_msg_frame = { NET_SEQUENCE_INVALID, false }; // most recent sequence ID we've processed from the server
	MessageFrameState server_processed_load_msg_frame = { NET_SEQUENCE_INVALID, false }; // most recent sequence ID of load messages we've processed from the server
	AssetID requested_level;
	Mode mode;
	ReplayMode replay_mode;
	StoryModeTeam requested_story_mode_team;
	b8 reconnect;
};
StateClient state_client;

b8 master_send_auth()
{
	master_auth_timer = MASTER_AUTH_TIMEOUT;

	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::Auth);

	serialize_enum(&p, Master::AuthType, Game::auth_type);

	if (Game::auth_type == Master::AuthType::GameJolt)
	{
		s32 length = strlen(Settings::gamejolt_username);
		serialize_int(&p, s32, length, 0, MAX_PATH_LENGTH);
		serialize_bytes(&p, (u8*)Settings::gamejolt_username, length);

		length = strlen(Settings::gamejolt_token);
		serialize_int(&p, s32, length, 0, MAX_AUTH_KEY);
		serialize_bytes(&p, (u8*)Settings::gamejolt_token, length);
	}
	else
	{
#if !defined(__ORBIS__)
		if (Game::auth_type == Master::AuthType::Steam)
		{
			s32 length = strlen(Game::steam_username);
			serialize_int(&p, s32, length, 0, MAX_PATH_LENGTH);
			serialize_bytes(&p, (u8*)Game::steam_username, length);
		}
#endif

		serialize_int(&p, s32, Game::auth_key_length, 0, MAX_AUTH_KEY);
		serialize_bytes(&p, (u8*)Game::auth_key, Game::auth_key_length);
	}

	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

b8 master_friendship_request(u32 friend_id, Master::Message msg)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, msg);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
	serialize_u32(&p, friend_id);
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

b8 master_friendship_get(u32 friend_id)
{
	return master_friendship_request(friend_id, Master::Message::FriendshipGet);
}

b8 master_friend_add(u32 friend_id)
{
	return master_friendship_request(friend_id, Master::Message::FriendAdd);
}

b8 master_friend_remove(u32 friend_id)
{
	return master_friendship_request(friend_id, Master::Message::FriendRemove);
}

b8 master_save_server_config(const Master::ServerConfig& config, u32 request_id)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::ClientSaveServerConfig);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
	serialize_u32(&p, request_id);
	b8 creating = config.id == 0;
	serialize_bool(&p, creating);
	Master::ServerConfig c = config;
	if (creating)
		c.id = u32(-1);
	if (!Master::serialize_server_config(&p, &c))
		net_error();
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

b8 master_request_server_details(u32 config_id, u32 request_id)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::ClientRequestServerDetails);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
	serialize_u32(&p, request_id);
	serialize_u32(&p, config_id);
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

void master_keepalive()
{
	if (state_common.timestamp - state_persistent.master.last_sent_timestamp > 2.0f)
		master_send(Master::Message::Keepalive);
}

b8 master_request_server_list(ServerListType type, s32 offset)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::ClientRequestServerList);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
	serialize_enum(&p, Region, Settings::region);
	serialize_enum(&p, ServerListType, type);
	serialize_s32(&p, offset);
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

void master_cancel_outgoing()
{
	state_persistent.master.reset();
}

b8 ping(const Sock::Address& addr, u32 token)
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	{
		ClientPacket type = ClientPacket::Ping;
		serialize_enum(&p, ClientPacket, type);
	}
	serialize_u32(&p, token);
	packet_finalize(&p);
	packet_send(p, addr);
	return true;
}

b8 add_player(s8 gamepad)
{
	using Stream = StreamWrite;
	StreamWrite* p = msg_new(MessageType::AddPlayer);
	serialize_int(p, s8, gamepad, 0, MAX_GAMEPADS - 1);
	serialize_u64(p, Game::session.local_player_uuids[gamepad]);
	msg_finalize(p);
	return true;
}

void init()
{
	if (Sock::udp_open(&state_persistent.sock))
	{
		fprintf(stderr, "%s\n", Sock::get_error());
		vi_assert(false);
	}

	master_init();
}

ReplayMode replay_mode()
{
	return state_client.replay_mode;
}

b8 packet_build_connect(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Connect;
	serialize_enum(p, ClientPacket, type);
	s16 version = GAME_VERSION;
	serialize_s16(p, version);
	s32 local_players = Game::session.local_player_count();;
	serialize_int(p, s32, local_players, 1, MAX_GAMEPADS);

	packet_finalize(p);
	return true;
}

b8 packet_build_disconnect(StreamWrite* p)
{
	using Stream = StreamWrite;
	packet_init(p);
	ClientPacket type = ClientPacket::Disconnect;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 packet_build_update(StreamWrite* p, const Update& u)
{
	using Stream = StreamWrite;
	packet_init(p);
	{
		ClientPacket type = ClientPacket::Update;
		serialize_enum(p, ClientPacket, type);
	}

	// ack received messages
	{
		Ack ack = msg_history_ack(state_client.msgs_in_history);
		serialize_int(p, SequenceID, ack.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
		serialize_u64(p, ack.previous_sequences);
	}

	msgs_write(p, state_common.msgs_out_history, state_client.server_ack, &state_client.server_recently_resent, state_client.server_rtt);

	{
		b8 has_ack_load = state_client.mode == Mode::Loading;
		serialize_bool(p, has_ack_load);
		if (has_ack_load)
		{
			Ack ack = msg_history_ack(state_client.msgs_in_load_history);
			serialize_int(p, SequenceID, ack.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
			serialize_u64(p, ack.previous_sequences);
		}
	}

	// let the server know the last state frame we received
	{
		SequenceID most_recent_state_frame;
		if (state_common.state_history.frames.length > 0)
			most_recent_state_frame = state_common.state_history.frames[state_common.state_history.current_index].sequence_id;
		else
			most_recent_state_frame = NET_SEQUENCE_INVALID;
		serialize_int(p, SequenceID, most_recent_state_frame, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
	}

	// we must serialize the current sequence ID separately from the message system
	// because perhaps msgs_write did not have enough room to serialize the message frame for the current sequence
	serialize_int(p, SequenceID, state_common.local_sequence_id, 0, NET_SEQUENCE_COUNT - 1);

	// player control
	if (state_client.mode == Mode::Connected)
	{
		s32 count = PlayerControlHuman::count_local();
		serialize_int(p, s32, count, 0, MAX_GAMEPADS);
		for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->local())
			{
				serialize_int(p, ID, i.index, 0, MAX_PLAYERS - 1);
				PlayerControlHuman::RemoteControl control = i.item()->remote_control_get(u);
				if (!serialize_player_control(p, &control))
					net_error();
			}
		}
	}
	else
	{
		// still loading
		s32 count = 0; // no player control data
		serialize_int(p, s32, count, 0, MAX_GAMEPADS);
	}

	packet_finalize(p);
	return true;
}

void update(const Update& u, r32 dt)
{
	if (master_auth_timer > 0.0f)
	{
		master_auth_timer = vi_max(0.0f, master_auth_timer - dt);
		if (master_auth_timer == 0.0f)
		{
			if (state_client.mode == Mode::Disconnected)
				state_persistent.master.reset();
			master_error = MasterError::Timeout;
		}
	}

	if (show_stats) // bandwidth counters are updated every half second
	{
		if (state_client.mode == Mode::Disconnected)
			Console::debug("%s", "Disconnected");
		else
			Console::debug("%.0fkbps down | %.0fkbps up | %.0fms", state_common.bandwidth_in * 8.0f / 500.0f, state_common.bandwidth_out * 8.0f / 500.0f, state_client.server_rtt * 1000.0f);
	}

	if (Game::level.local)
		return;

	r32 interpolation_time = state_common.timestamp - interpolation_delay();

	const StateFrame* frame = state_frame_by_timestamp(state_common.state_history, interpolation_time);
	if (frame)
	{
		const StateFrame* frame_next = state_frame_next(state_common.state_history, *frame);
		const StateFrame* frame_final;
		StateFrame interpolated;
		if (frame_next)
		{
			state_frame_interpolate(*frame, *frame_next, &interpolated, interpolation_time);
			frame_final = &interpolated;
		}
		else
			frame_final = frame;

		// apply frame_final to world
		state_frame_apply(*frame_final, *frame, frame_next);
	}

	while (MessageFrame* frame = state_client.mode == Mode::Loading
		? msg_frame_advance(&state_client.msgs_in_load_history, &state_client.server_processed_load_msg_frame, state_common.timestamp)
		: msg_frame_advance(&state_client.msgs_in_history, &state_client.server_processed_msg_frame, interpolation_time))
	{
		frame->read.rewind();
#if DEBUG_MSG
		if (frame->bytes > 1)
			vi_debug("Processing seq %d", frame->sequence_id);
#endif
		while (frame->read.bytes_read() < frame->bytes)
		{
			b8 success = Client::msg_process(&frame->read);
			if (!success)
				break;
		}
	}
}

void handle_server_disconnect(DisconnectReason reason)
{
	disconnect_reason = reason;
	state_client.mode = Mode::Disconnected;
	if (state_client.replay_mode == ReplayMode::Replaying)
	{
		if (replay_files.length > 0)
		{
			Game::unload_level();
			Game::save.reset();
			Game::session.reset(SessionType::Multiplayer);
			replay();
		}
	}
	else if (state_client.reconnect)
	{
		Sock::Address addr = state_client.server_address;
		Game::unload_level();
		connect(addr);
	}
	else
	{
		Game::unload_level();
		if (Game::session.type == SessionType::Story && reason == DisconnectReason::ServerResetting)
			Game::schedule_load_level(Game::save.zone_current, Game::Mode::Parkour);
	}
}

b8 master_send_server_request()
{
	using Stream = StreamWrite;
	StreamWrite p;
	packet_init(&p);
	state_persistent.master.add_header(&p, state_persistent.master_addr, Master::Message::ClientRequestServer);
	serialize_u32(&p, Game::user_key.id);
	serialize_u32(&p, Game::user_key.token);
	serialize_u32(&p, state_client.requested_server_id);
	if (state_client.requested_server_id == 0) // story mode
	{
		serialize_s16(&p, state_client.requested_level);
		serialize_enum(&p, Region, Settings::region);
		serialize_enum(&p, StoryModeTeam, state_client.requested_story_mode_team);
	}
	else
	{
		s8 local_players = Game::session.local_player_count();
		serialize_int(&p, s8, local_players, 1, MAX_GAMEPADS);
	}
	packet_finalize(&p);
	state_persistent.master.send(p, state_common.timestamp, state_persistent.master_addr, &state_persistent.sock);
	return true;
}

b8 master_request_server(u32 id, AssetID level, StoryModeTeam story_mode_team)
{
	vi_assert((id == 0) == (level != AssetNull));

	Game::level.local = false;
	Game::schedule_timer = 0.0f;

	state_client.timeout = 0.0f;
	state_client.mode = Mode::ContactingMaster;
	state_client.requested_server_id = id;
	state_client.requested_level = level;
	state_client.requested_story_mode_team = story_mode_team;

	master_send(Master::Message::Disconnect);
	state_persistent.master.reset();
	master_auth_timer = 0.0f;

	master_send_server_request();

	return true;
}

void tick(const Update& u, r32 dt)
{
	switch (state_client.mode)
	{
		case Mode::Disconnected:
		{
			break;
		}
		case Mode::ContactingMaster:
		{
			state_client.timeout += dt;
			if (state_client.timeout > NET_MASTER_STATUS_INTERVAL)
			{
				state_client.timeout = 0.0f;
				master_send_server_request();
			}
			break;
		}
		case Mode::Connecting:
		{
			state_client.timeout += dt;
			char str[NET_MAX_ADDRESS];
			state_client.server_address.str(str);
			vi_debug("Connecting to %s...", str);
			StreamWrite p;
			packet_build_connect(&p);
			packet_send(p, state_client.server_address);
			break;
		}
		case Mode::Loading:
		case Mode::Connected:
		{
			state_client.timeout += dt;
			if (state_client.timeout > NET_TIMEOUT)
			{
				char str[NET_MAX_ADDRESS];
				state_client.server_address.str(str);
				vi_debug("Connection to %s timed out.", str);
				handle_server_disconnect(DisconnectReason::Timeout);
			}
			else
			{
				msgs_out_consolidate(&state_common.msgs_out, &state_common.msgs_out_history, state_common.local_sequence_id);

				StreamWrite p;
				packet_build_update(&p, u);
				packet_send(p, state_client.server_address);

				state_common.local_sequence_id = sequence_advance(state_common.local_sequence_id, 1);
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
}

void connect(Sock::Address addr)
{
	Game::level.local = false;
	Game::schedule_timer = 0.0f;
	state_client.server_address = addr;
	state_client.timeout = 0.0f;
	state_client.mode = Mode::Connecting;
	if (Settings::record && Game::session.type != SessionType::Story)
	{
		state_client.replay_mode = ReplayMode::Recording;
		if (state_client.replay_file)
			fclose(state_client.replay_file);

		// generate a filename for this replay
		char filename[MAX_PATH_LENGTH + 1];
		replay_filename_generate(filename);
		vi_debug("Recording gameplay to '%s'.", filename);
		replay_file_add(filename);
		state_client.replay_file = fopen(filename, "wb");
		vi_assert(state_client.replay_file);
	}
}

void connect(const char* ip, u16 port)
{
	Sock::Address addr;
	Sock::Address::get(&addr, ip, port);
	connect(addr);
}

void replay(const char* filename)
{
	if (state_client.replay_file)
		fclose(state_client.replay_file);
	if (!filename)
	{
		vi_assert(replay_files.length > 0);
		filename = replay_files[replay_file_index].data();
		replay_file_index = (replay_file_index + 1) % replay_files.length;
	}
	state_client.replay_file = fopen(filename, "rb");
	if (state_client.replay_file)
	{
		Game::level.local = false;
		Game::schedule_timer = 0.0f;
		state_client.timeout = 0.0f;
		state_client.mode = Mode::Connecting;
		Sock::Address::get(&state_client.server_address, "127.0.0.1", 3495);
		state_client.replay_mode = ReplayMode::Replaying;
	}
}

Mode mode()
{
	return state_client.mode;
}

b8 packet_handle_master(StreamRead* p)
{
	using Stream = StreamRead;
	{
		s16 version;
		serialize_s16(p, version);
	}
	SequenceID seq;
	serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
	Master::Message type;
	serialize_enum(p, Master::Message, type);
	state_persistent.master.received(type, seq, state_persistent.master_addr, &state_persistent.sock);
	master_auth_timer = 0.0f;
	master_error = MasterError::None;
	switch (type)
	{
		case Master::Message::Ack:
		{
			break;
		}
		case Master::Message::AuthResponse:
		{
			b8 success;
			serialize_bool(p, success);
			if (success)
			{
				serialize_u32(p, Game::user_key.id);
				serialize_u32(p, Game::user_key.token);
				s32 username_length;
				serialize_int(p, s32, username_length, 0, MAX_USERNAME);
				char username[MAX_USERNAME + 1];
				serialize_bytes(p, (u8*)username, username_length);
				username[username_length] = '\0';
				memset(Settings::username, 0, sizeof(Settings::username));
				strncpy(Settings::username, username, MAX_USERNAME);
				Loader::settings_save();
			}
			else
				Game::auth_failed();
			break;
		}
		case Master::Message::ReauthRequired:
		{
			vi_debug("%s", "Reauthenticating...");
			master_send_auth();
			break;
		}
		case Master::Message::Disconnect:
		{
			state_persistent.master.reset();
			state_client.mode = Mode::Disconnected;
			break;
		}
		case Master::Message::ClientConnect:
		{
			Sock::Address addr;
			if (!Sock::Address::serialize(p, &addr))
				net_error();
			connect(addr);
			master_send(Master::Message::Disconnect);
			state_persistent.master.reset();
			break;
		}
		case Master::Message::WrongVersion:
		{
			master_error = MasterError::WrongVersion;
			state_client.mode = Mode::Disconnected;
			break;
		}
		case Master::Message::ServerConfigSaved:
		{
			u32 id;
			serialize_u32(p, id);
			u32 request_id;
			serialize_u32(p, request_id);
			Overworld::master_server_config_saved(id, request_id);
			break;
		}
		case Master::Message::ServerDetails:
		{
			u32 request_id;
			serialize_u32(p, request_id);

			Master::ServerDetails details;
			if (!Master::serialize_server_details(p, &details))
				net_error();

			Overworld::master_server_details_response(details, request_id);
			break;
		}
		case Master::Message::ServerList:
		{
			ServerListType type;
			serialize_enum(p, ServerListType, type);
			s32 last_index = -1;
			while (true)
			{
				s32 index;
				serialize_s32(p, index);
				if (index < 0)
					break;

				Master::ServerListEntry entry;
				if (!Master::serialize_server_list_entry(p, &entry))
					net_error();

				Overworld::master_server_list_entry(type, index, entry);

				last_index = index;
			}
			b8 done;
			serialize_bool(p, done);
			if (done)
				Overworld::master_server_list_end(type, last_index + 1);
			break;
		}
		case Master::Message::FriendshipState:
		{
			u32 friend_id;
			serialize_u32(p, friend_id);
			b8 state;
			serialize_bool(p, state);
			Menu::friendship_state(friend_id, state);
			break;
		}
		default:
		{
			net_error();
			break;
		}
	}
	return true;
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
	if (address.equals(state_persistent.master_addr))
		return packet_handle_master(p);

	using Stream = StreamRead;

	ServerPacket type;
	serialize_enum(p, ServerPacket, type);

	if (type == ServerPacket::PingResponse)
	{
		u32 token;
		serialize_u32(p, token);
		Overworld::ping_response(address, token);
		return true;
	}
	else if (state_client.mode == Mode::Disconnected
		|| state_client.mode == Mode::ContactingMaster
		|| !address.equals(state_client.server_address))
	{
		// unknown host; ignore
		return false;
	}

	switch (type)
	{
		case ServerPacket::Init:
		{
			if (state_client.mode == Mode::Connecting)
			{
				SequenceID seq;
				serialize_int(p, SequenceID, seq, 0, NET_SEQUENCE_COUNT - 1);
				if (!serialize_init_packet(p))
					net_error();
				state_client.server_processed_msg_frame = state_client.server_processed_load_msg_frame = { sequence_advance(seq, -1), true };
				vi_debug("Starting on sequence %d", s32(seq));
				state_client.mode = Mode::Loading;
				state_client.timeout = 0.0f;

				// send client setup message
				{
					using Stream = StreamWrite;
					StreamWrite* p2 = msg_new(MessageType::ClientSetup);

					serialize_u32(p2, Game::user_key.id);
					serialize_u32(p2, Game::user_key.token);

					s32 username_length = strlen(Settings::username);
					serialize_int(p2, s32, username_length, 0, MAX_USERNAME);
					serialize_bytes(p2, (u8*)Settings::username, username_length);

					s32 local_players = Game::session.local_player_count();
					serialize_int(p2, s32, local_players, 1, MAX_GAMEPADS);
					for (s32 i = 0; i < MAX_GAMEPADS; i++)
					{
						if (Game::session.local_player_mask & (1 << i))
						{
							serialize_int(p2, s32, i, 0, MAX_GAMEPADS - 1); // gamepad
							serialize_u64(p2, Game::session.local_player_uuids[i]); // uuid
						}
					}
					msg_finalize(p2);
				}
			}
			break;
		}
		case ServerPacket::Update:
		{
			if (state_client.mode == Mode::Connecting)
				return false; // need the init packet first

			// read ack
			Ack ack_candidate;
			serialize_int(p, SequenceID, ack_candidate.sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because it might be NET_SEQUENCE_INVALID
			serialize_u64(p, ack_candidate.previous_sequences);
			if (sequence_more_recent(ack_candidate.sequence_id, state_client.server_ack.sequence_id))
				state_client.server_ack = ack_candidate;

			if (!msgs_read(p, &state_client.msgs_in_history, ack_candidate))
				net_error();

			calculate_rtt(state_common.timestamp, state_client.server_ack, state_common.msgs_out_history, &state_client.server_rtt);

			b8 has_load_msgs;
			serialize_bool(p, has_load_msgs);
			if (state_client.mode == Mode::Loading)
				vi_assert(has_load_msgs);
			if (has_load_msgs)
			{
				if (!msgs_read(p, &state_client.msgs_in_load_history, ack_candidate))
					net_error();
			}

			if (p->bytes_read() < p->bytes_total) // server doesn't always send state frames
			{
				SequenceID base_sequence_id;
				serialize_int(p, SequenceID, base_sequence_id, 0, NET_SEQUENCE_COUNT); // not NET_SEQUENCE_COUNT - 1, because base_sequence_id might be NET_SEQUENCE_INVALID
				const StateFrame* base = state_frame_by_sequence(state_common.state_history, base_sequence_id);
				StateFrame frame;
				if (!serialize_state_frame(p, &frame, base))
					net_error();

				// make sure the server says we have a base state frame if and only if we actually have it
				if ((base_sequence_id == NET_SEQUENCE_INVALID) != (base == nullptr))
				{
					char str[NET_MAX_ADDRESS];
					state_client.server_address.str(str);
					vi_debug("Lost connection to %s due to missing state frame delta compression basis seq %d. Current seq: %d", str, s32(base_sequence_id), s32(frame.sequence_id));
					handle_server_disconnect(DisconnectReason::SequenceGap);
					return false;
				}

				// check for large gaps in the sequence
				const MessageFrameState& processed_msg_frame = has_load_msgs ? state_client.server_processed_load_msg_frame : state_client.server_processed_msg_frame;
				if (abs(sequence_relative_to(processed_msg_frame.sequence_id, frame.sequence_id)) > NET_MAX_SEQUENCE_GAP)
				{
					// we missed a packet that we'll never be able to recover
					char str[NET_MAX_ADDRESS];
					state_client.server_address.str(str);
					vi_debug("Lost connection to %s due to sequence gap. Last seq: %d. Current seq: %d.", str, s32(processed_msg_frame.sequence_id), s32(frame.sequence_id));
					handle_server_disconnect(DisconnectReason::SequenceGap);
					return false;
				}

				// only insert the frame into the history if it is more recent
				if (state_common.state_history.frames.length == 0 || sequence_more_recent(frame.sequence_id, state_common.state_history.frames[state_common.state_history.current_index].sequence_id))
				{
					memcpy(state_frame_add(&state_common.state_history), &frame, sizeof(StateFrame));

					// let players know where the server thinks they are immediately, with no interpolation
					for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
					{
						Transform* t = i.item()->get<Transform>();
						const TransformState& transform_state = frame.transforms[t->id()];
						if (transform_state.revision == t->revision)
						{
							PlayerControlHuman::RemoteControl* control = &i.item()->remote_control;
							control->parent = transform_state.parent;
							control->pos = transform_state.pos;
							control->rot = transform_state.rot;
						}
					}
				}
			}

			state_client.timeout = 0.0f; // reset connection timeout
			break;
		}
		case ServerPacket::Disconnect:
		{
			DisconnectReason reason;
			serialize_enum(p, DisconnectReason, reason);
			char str[NET_MAX_ADDRESS];
			state_client.server_address.str(str);
			vi_debug("Connection closed by %s. Reason: %d", str, s32(reason));
			handle_server_disconnect(reason);
			break;
		}
		default:
		{
			vi_debug("%s", "Discarding packet due to invalid packet type.");
			net_error();
		}
	}

	return true;
}

// client function for processing messages
// these will only come from the server; no loopback messages
b8 msg_process(StreamRead* p)
{
	s32 start_pos = p->bits_read;
	using Stream = StreamRead;
	MessageType type;
	serialize_enum(p, MessageType, type);
#if DEBUG_MSG
	if (type != MessageType::Noop)
		vi_debug("Processing message type %d", type);
#endif
	switch (type)
	{
		case MessageType::EntityCreate:
		{
			ID id;
			serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
			Entity* e = World::net_add(id);
			if (!serialize_entity(p, e))
				net_error();
			break;
		}
		case MessageType::EntityRemove:
		{
			ID id;
			serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
#if DEBUG_ENTITY
			vi_debug("Deleting entity ID %d", s32(id));
#endif
			World::net_remove(&Entity::list[id]);
			break;
		}
		case MessageType::InitDone:
		{
			vi_assert(state_client.mode == Mode::Loading);
			Tram::setup(); // HACK to make sure trams are in the right position
			for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
				World::awake(i.item());
			Game::awake_all();

			// let the server know we're done loading
			vi_debug("%s", "Finished loading.");
			msg_finalize(msg_new(MessageType::LoadingDone));
			state_client.mode = Mode::Connected;

			// letterbox effect
			Game::schedule_timer = TRANSITION_TIME * 0.5f;

			break;
		}
		case MessageType::TimeSync:
		{
			serialize_r32_range(p, Team::match_time, 0, Game::session.config.time_limit(), 16);
			serialize_r32_range(p, Game::session.time_scale, 0, 1, 8);
			s32 player_count;
			serialize_int(p, s32, player_count, 0, MAX_PLAYERS);
			for (s32 i = 0; i < player_count; i++)
			{
				ID id;
				serialize_int(p, ID, id, 0, MAX_PLAYERS - 1);
				serialize_r32_range(p, state_client.rtts[id], 0, 1.024f, 10);
			}
			break;
		}
		case MessageType::TransitionLevel:
		{
			// start letterbox animation
			Game::schedule_timer = TRANSITION_TIME;
			// wait for server to disconnect, then reconnect
			state_client.reconnect = true;
			break;
		}
		case MessageType::Noop:
			break;
		default:
		{
			p->rewind(start_pos);
			if (!Net::msg_process(p, MessageSource::Remote))
				net_error();
			break;
		}
	}
	serialize_align(p);
	return true;
}

void reset()
{
	if (state_client.mode == Mode::Connected)
	{
		StreamWrite p;
		packet_build_disconnect(&p);
		packet_send(p, state_client.server_address);
	}

	if (state_client.replay_mode == ReplayMode::Recording)
	{
		s16 size = 0;
		fwrite(&size, sizeof(s16), 1, state_client.replay_file);
	}

	if (state_client.replay_file)
		fclose(state_client.replay_file);

	state_client.~StateClient();
	new (&state_client) StateClient();
}

b8 lagging()
{
	return state_client.mode == Mode::Disconnected
		|| (state_client.msgs_in_history.msg_frames.length > 0 && state_common.timestamp - state_client.msgs_in_history.msg_frames[state_client.msgs_in_history.current_index].timestamp > tick_rate() * 8.0f);
}

Sock::Address server_address()
{
	return state_client.server_address;
}

b8 execute(const char* string)
{
	using Stream = StreamWrite;
	Stream* p = msg_new(MessageType::DebugCommand);
	s32 len = strlen(string);
	serialize_int(p, s32, len, 0, 512);
	serialize_bytes(p, (u8*)string, len);
	msg_finalize(p);
	return true;
}


}

#endif

r32 tick_rate()
{
	return (Game::level.mode == Game::Mode::Pvp && Team::match_state == Team::MatchState::Active) ? (1.0f / 60.0f) : (1.0f / 30.0f);
}

r32 interpolation_delay()
{
	return (tick_rate() * 4.0f) + 0.02f;
}

void init()
{
	Sock::init();

#if SERVER
	Server::init();
#else
	Client::init();
#endif
}

b8 finalize_internal(Entity* e)
{
	using Stream = StreamWrite;
	StreamWrite* p = msg_new(MessageType::EntityCreate);
	ID id = e->id();
	serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
	if (!serialize_entity(p, e))
		vi_assert(false);
	msg_finalize(p);
	return true;
}

void finalize(Entity* e)
{
#if SERVER
	if (Server::state_server.mode != Server::Mode::Loading)
	{
		finalize_internal(e);
		for (s32 i = 0; i < Server::state_server.finalize_children_queue.length; i++)
			finalize_internal(Server::state_server.finalize_children_queue[i].ref());
		Server::state_server.finalize_children_queue.length = 0;
	}
#else
	// client
	vi_assert(Game::level.local); // client can't spawn entities if it is connected to a server
#endif
}

void finalize_child(Entity* e)
{
#if SERVER
	if (Server::state_server.mode != Server::Mode::Loading)
		Server::state_server.finalize_children_queue.add(e);
#else
	finalize(e);
#endif
}

b8 remove(Entity* e)
{
#if SERVER
	if (Server::state_server.mode != Server::Mode::Loading)
	{
		vi_assert(Entity::list.active(e->id()));
		using Stream = StreamWrite;
		StreamWrite* p = msg_new(MessageType::EntityRemove);
		ID id = e->id();
#if DEBUG_ENTITY
		vi_debug("Deleting entity ID %d", s32(id));
#endif
		serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
		msg_finalize(p);
	}
#endif
	return true;
}

#define NET_MAX_FRAME_TIME 0.2f

struct PacketEntry
{
	r32 timestamp;
	StreamRead packet;
	Sock::Address address;
	PacketEntry(r32 t) : timestamp(t), packet(), address() {}
};

#if DEBUG_LAG
Array<PacketEntry> lag_buffer;
#endif

void packet_read(const Update& u, PacketEntry* entry)
{
#if DEBUG_PACKET_LOSS
	if (mersenne::randf_co() < DEBUG_PACKET_LOSS_AMOUNT) // packet loss simulation
		return;
#endif

	state_common.bandwidth_in_counter += entry->packet.bytes_total;

	if (entry->packet.bytes_total > 0 && entry->packet.read_checksum())
	{
		packet_decompress(&entry->packet, entry->packet.bytes_total);
#if SERVER
		Server::packet_handle(u, &entry->packet, entry->address);
#else
		Client::packet_handle(u, &entry->packet, entry->address);
#endif
	}
	else
		vi_debug("%s", "Discarding packet due to invalid checksum.");
}

void update_start(const Update& u)
{
	r32 dt = vi_min(Game::real_time.delta, NET_MAX_FRAME_TIME);
	state_common.timestamp += dt;

#if !SERVER
	if (Client::state_client.replay_mode == Client::ReplayMode::Replaying)
	{
		Client::state_client.tick_timer -= dt;
		if (Client::state_client.tick_timer < 0.0f)
		{
			s16 size;
			b8 packet_successfully_read = false;
			if (fread(&size, sizeof(s16), 1, Client::state_client.replay_file) == 1)
			{
				s32 bytes_received = s32(size);
				if (bytes_received > 0)
				{
					PacketEntry entry(state_common.timestamp);
					entry.address = Client::state_client.server_address;
					if (fread(entry.packet.data.data, sizeof(s8), bytes_received, Client::state_client.replay_file) == bytes_received)
					{
						entry.packet.resize_bytes(bytes_received);
						packet_read(u, &entry);
						packet_successfully_read = true;
					}
				}
			}

			if (!packet_successfully_read)
				Client::handle_server_disconnect(DisconnectReason::SequenceGap);
			Client::state_client.tick_timer = vi_max(0.0f, Client::state_client.tick_timer + tick_rate());
		}
	}
#endif

	while (true)
	{
		PacketEntry entry(state_common.timestamp);
		s32 bytes_received = Sock::udp_receive(&state_persistent.sock, &entry.address, entry.packet.data.data, NET_MAX_PACKET_SIZE);
		entry.packet.resize_bytes(bytes_received);
		if (bytes_received > 0)
		{
#if DEBUG_LAG
			lag_buffer.add(entry); // save for later
#else
#if !SERVER
			if (Client::state_client.replay_mode == Client::ReplayMode::Replaying)
				continue; // ignore all incoming packets while we're replaying
			else if (Client::state_client.replay_mode == Client::ReplayMode::Recording
				&& entry.address.equals(Client::state_client.server_address))
			{
				s16 size = s16(bytes_received);
				fwrite(&size, sizeof(s16), 1, Client::state_client.replay_file);
				fwrite(entry.packet.data.data, sizeof(s8), bytes_received, Client::state_client.replay_file);
			}
#endif
			packet_read(u, &entry); // read packet instantly
#endif
		}
		else
			break;
	}

#if DEBUG_LAG
	// wait DEBUG_LAG_AMOUNT before reading packet
	for (s32 i = 0; i < lag_buffer.length; i++)
	{
		PacketEntry* entry = &lag_buffer[i];
		if (entry->timestamp < state_common.timestamp - DEBUG_LAG_AMOUNT / Game::session.time_scale)
		{
			packet_read(u, entry);
			lag_buffer.remove_ordered(i);
			i--;
		}
	}
#endif

#if SERVER
	Server::update(u, dt);
#else
	Client::update(u, dt);
#endif
	
	// update bandwidth every half second
	if (s32(state_common.timestamp * 2.0f) > s32((state_common.timestamp - dt) * 2.0f))
	{
		state_common.bandwidth_in = state_common.bandwidth_in_counter;
		state_common.bandwidth_out = state_common.bandwidth_out_counter;
		state_common.bandwidth_in_counter = 0;
		state_common.bandwidth_out_counter = 0;
	}
}

void update_end(const Update& u)
{
	r32 dt = vi_min(Game::real_time.delta, NET_MAX_FRAME_TIME);
#if SERVER
	// server always runs at 60 FPS
	Server::tick(u, dt);
#else
	state_persistent.master.update(state_common.timestamp, &state_persistent.sock, 4);

	if (Game::level.local || Client::state_client.replay_mode == Client::ReplayMode::Replaying)
		state_common.msgs_out.length = 0; // clear out message queue because we're never going to send these
	else
	{
		Client::state_client.tick_timer -= dt;
		if (Client::state_client.tick_timer < 0.0f)
		{
			Client::tick(u, vi_max(dt, tick_rate()));
			// we're not going to send more than one packet per frame
			// so make sure the tick timer never gets out of control
			Client::state_client.tick_timer = vi_max(0.0f, Client::state_client.tick_timer + tick_rate());
		}
	}
#endif
}

void term()
{
	reset();
	Sock::close(&state_persistent.sock);
}

// unload net state (for example when switching levels)
void reset()
{
	// send disconnect packet
#if SERVER
	Server::reset();
#else
	Client::reset();
#endif

	state_common.~StateCommon();
	new (&state_common) StateCommon();
}



// MESSAGES



b8 msg_serialize_type(StreamWrite* p, MessageType t)
{
	using Stream = StreamWrite;
	serialize_enum(p, MessageType, t);
#if DEBUG_MSG
	if (t != MessageType::Noop)
		vi_debug("Seq %d: building message type %d", s32(state_common.local_sequence_id), s32(t));
#endif
	return true;
}

StreamWrite* msg_new(MessageBuffer* buffer, MessageType t)
{
	StreamWrite* result = buffer->add();
	result->reset();
	msg_serialize_type(result, t);
	return result;
}

StreamWrite* msg_new(MessageType t)
{
	return msg_new(&state_common.msgs_out, t);
}

StreamWrite packet_local;
StreamWrite* msg_new_local(MessageType t)
{
#if SERVER
	// we're the server; send out this message
	return msg_new(t);
#else
	// we're a client
	// so just process this message locally; don't send it
	packet_local.reset();
	msg_serialize_type(&packet_local, t);
	return &packet_local;
#endif
}

template<typename T> b8 msg_serialize_ref(StreamWrite* p, T* t)
{
	using Stream = StreamWrite;
	Ref<T> r = t;
	serialize_ref(p, r);
	return true;
}

// common message processing on both client and server
// on the server, these will only be loopback messages
b8 msg_process(StreamRead* p, MessageSource src)
{
#if SERVER
	vi_assert(src == MessageSource::Loopback);
#endif
	using Stream = StreamRead;
	MessageType type;
	serialize_enum(p, MessageType, type);
	switch (type)
	{
		case MessageType::Drone:
		{
			if (!Drone::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::PlayerControlHuman:
		{
			vi_assert(src == MessageSource::Loopback || !Game::level.local); // Server::msg_process handles this on the server
			Ref<PlayerControlHuman> c;
			serialize_ref(p, c);
			if (!PlayerControlHuman::net_msg(p, c.ref(), src, 0))
				net_error();
			break;
		}
		case MessageType::Battery:
		{
			if (!Battery::net_msg(p))
				net_error();
			break;
		}
		case MessageType::Grenade:
		{
			if (!Grenade::net_msg(p, MessageSource::Remote))
				net_error();
			break;
		}
		case MessageType::Health:
		{
			if (!Health::net_msg(p))
				net_error();
			break;
		}
		case MessageType::Team:
		{
			if (!Team::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::PlayerManager:
		{
			vi_assert(src == MessageSource::Loopback || !Game::level.local); // Server::msg_process handles this on the server
			Ref<PlayerManager> m;
			serialize_ref(p, m);
			PlayerManager::Message msg;
			serialize_enum(p, PlayerManager::Message, msg);
			if (!PlayerManager::net_msg(p, m.ref(), msg, src))
				net_error();
			break;
		}
		case MessageType::ParticleEffect:
		{
			if (!ParticleEffect::net_msg(p))
				net_error();
			break;
		}
		case MessageType::Bolt:
		{
			if (!Bolt::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::UpgradeStation:
		{
			if (!UpgradeStation::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::Turret:
		{
			if (!Turret::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::Flag:
		{
			if (!Flag::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::AddPlayer:
		{
#if SERVER
			// should never get a loopback AddPlayer message
			vi_assert(false);
#else
			s8 gamepad;
			serialize_int(p, s8, gamepad, 0, MAX_GAMEPADS - 1);
			u64 uuid;
			serialize_u64(p, uuid);
			if (Game::level.local)
				Game::add_local_player(gamepad);
			break;
#endif
		}
		case MessageType::DebugCommand: // handled by Server::msg_process
			break;
		default:
		{
			vi_debug("Unknown message type: %d", s32(type));
			net_error();
			break;
		}
	}
	serialize_align(p);
	return true;
}


// after a server or client builds a message to send out, it also processes it locally
// certain special messages are NOT processed locally, they only apply to the remote
b8 msg_finalize(StreamWrite* p)
{
	using Stream = StreamRead;
	p->align();
	p->flush();
	StreamRead r;
	memcpy(&r, p, sizeof(StreamRead));
	r.rewind();
	MessageType type;
	serialize_enum(&r, MessageType, type);
	if (type != MessageType::Noop
		&& type != MessageType::ClientSetup
		&& type != MessageType::EntityCreate
		&& type != MessageType::EntityRemove
		&& type != MessageType::InitDone
		&& type != MessageType::LoadingDone
		&& type != MessageType::TimeSync
		&& type != MessageType::DebugCommand
		&& type != MessageType::TransitionLevel)
	{
		r.rewind();
		msg_process(&r, MessageSource::Loopback);
	}
	return true;
}

// this is meant for external consumption in the game code.
r32 rtt(const PlayerHuman* p)
{
	if (Game::level.local && p->local())
		return 0.0f;

#if SERVER
	const Server::Client* client = Server::client_for_player(p);

	if (client)
		return client->rtt;
	else
		return 0.0f;
#else
	if (p->local())
		return Client::state_client.server_rtt;
	else
		return Client::state_client.rtts[p->id()];
#endif
}

b8 state_frame_by_timestamp(StateFrame* result, r32 timestamp)
{
	const StateFrame* frame_a = state_frame_by_timestamp(state_common.state_history, timestamp);
	if (frame_a)
	{
		const StateFrame* frame_b = state_frame_next(state_common.state_history, *frame_a);
		if (frame_b)
			state_frame_interpolate(*frame_a, *frame_b, result, timestamp);
		else
			*result = *frame_a;
		return true;
	}
	return false;
}

r32 timestamp()
{
	return state_common.timestamp;
}


}


}
