#include "net.h"
#include "net_serialize.h"
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
#include "physics.h"
#include "game/awk.h"
#include "game/walker.h"
#include "game/audio.h"
#include "game/player.h"
#include "data/ragdoll.h"
#include "game/minion.h"
#include "game/ai_player.h"
#include "game/team.h"
#include "game/player.h"
#include "assimp/contrib/zlib/zlib.h"
#include "console.h"

#define DEBUG_MSG 0
#define DEBUG_ENTITY 0
#define DEBUG_TRANSFORMS 0
#define DEBUG_BANDWIDTH 0

namespace VI
{


namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

b8 show_stats;
Sock::Handle sock;

#define TIMEOUT 1.0f
#define SEQUENCE_BITS 8
#define SEQUENCE_COUNT (1 << SEQUENCE_BITS)

#define MESSAGE_BUFFER 256
#define MAX_MESSAGES_SIZE (NET_MAX_PACKET_SIZE / 2)

enum class ClientPacket
{
	Connect,
	Update,
	AckInit,
	Disconnect,
	count,
};

enum class ServerPacket
{
	Init,
	Keepalive,
	Update,
	Disconnect,
	count,
};

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

	MessageFrame(r32 t, s32 bytes) : read(), sequence_id(), timestamp(t), bytes(bytes) {}
	~MessageFrame() {}
};

struct StateHistory
{
	StaticArray<StateFrame, 256> frames;
	s32 current_index;
};

struct Ack
{
	u32 previous_sequences;
	SequenceID sequence_id;
};

struct MessageHistory
{
	StaticArray<MessageFrame, 256> msgs;
	s32 current_index;
};

#define SEQUENCE_RESEND_BUFFER 6
struct SequenceHistoryEntry
{
	r32 timestamp;
	SequenceID id;
};

typedef StaticArray<SequenceHistoryEntry, SEQUENCE_RESEND_BUFFER> SequenceHistory;

b8 msg_process(StreamRead*, MessageSource);

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
	serialize_s16(p, v->mask);
	serialize_s16(p, v->mesh);
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
	serialize_position(p, &control->pos, Resolution::High);
	serialize_quat(p, &control->rot, Resolution::High);
	return true;
}

template<typename Stream> b8 serialize_entity(Stream* p, Entity* e)
{
	const ComponentMask mask = Transform::component_mask
		| RigidBody::component_mask
		| View::component_mask
		| Animator::component_mask
		| Rope::component_mask
		| DirectionalLight::component_mask
		| SkyDecal::component_mask
		| AIAgent::component_mask
		| Health::component_mask
		| PointLight::component_mask
		| SpotLight::component_mask
		| ControlPoint::component_mask
		| Shockwave::component_mask
		| Walker::component_mask
		| Ragdoll::component_mask
		| Target::component_mask
		| PlayerTrigger::component_mask
		| SkinnedModel::component_mask
		| Projectile::component_mask
		| EnergyPickup::component_mask
		| Sensor::component_mask
		| Rocket::component_mask
		| ContainmentField::component_mask
		| Teleporter::component_mask
		| Awk::component_mask
		| Audio::component_mask
		| Team::component_mask
		| PlayerHuman::component_mask
		| PlayerManager::component_mask
		| PlayerCommon::component_mask
		| PlayerControlHuman::component_mask;
		//| MinionCommon::component_mask

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
		serialize_position(p, &t->pos, Resolution::High);
		serialize_quat(p, &t->rot, Resolution::High);
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
		serialize_r32_range(p, r->mass, 0, 1, 1);
		serialize_int(p, ID, r->linked_entity, 0, MAX_ENTITIES);
		serialize_asset(p, r->mesh_id, Loader::static_mesh_count);
		serialize_int(p, s16, r->collision_group, -32767, 32767);
		serialize_int(p, s16, r->collision_filter, -32767, 32767);
		serialize_bool(p, r->ccd);
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
			serialize_r32_range(p, l->weight, 0, 1, 8);
			serialize_r32_range(p, l->blend, 0, 1, 8);
			serialize_r32_range(p, l->blend_time, 0, 8, 16);
			serialize_r32(p, l->time);
			serialize_r32_range(p, l->speed, 0, 8, 16);
			serialize_asset(p, l->animation, Loader::animation_count);
			serialize_asset(p, l->last_animation, Loader::animation_count);
			serialize_bool(p, l->loop);
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

	if (e->has<Awk>())
	{
		Awk* a = e->get<Awk>();
		serialize_r32_range(p, a->cooldown, 0, AWK_COOLDOWN, 8);
		serialize_int(p, Ability, a->current_ability, 0, s32(Ability::count) + 1);
		serialize_ref(p, a->shield);
		serialize_ref(p, a->overshield);
		serialize_int(p, s8, a->charges, 0, AWK_CHARGES);
	}

	if (e->has<MinionCommon>())
	{
	}

	if (e->has<Health>())
	{
		Health* h = e->get<Health>();
		serialize_r32_range(p, h->regen_timer, 0, 10, 8);
		serialize_s8(p, h->shield);
		serialize_s8(p, h->shield_max);
		serialize_s8(p, h->hp);
		serialize_s8(p, h->hp_max);
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

	if (e->has<ControlPoint>())
	{
		ControlPoint* c = e->get<ControlPoint>();
		serialize_s8(p, c->team);
	}

	if (e->has<Shockwave>())
	{
		Shockwave* s = e->get<Shockwave>();
		serialize_r32_range(p, s->max_radius, 0, 50, 8);
		serialize_r32_range(p, s->duration, 0, 5, 8);
	}

	if (e->has<Walker>())
	{
		Walker* w = e->get<Walker>();
		serialize_r32_range(p, w->height, 0, 10, 16);
		serialize_r32_range(p, w->support_height, 0, 10, 16);
		serialize_r32_range(p, w->radius, 0, 10, 16);
		serialize_r32_range(p, w->mass, 0, 10, 16);
		serialize_r32(p, w->rotation);
	}

	if (e->has<Target>())
	{
		Target* t = e->get<Target>();
		serialize_r32_range(p, t->local_offset.x, -10, 10, 16);
		serialize_r32_range(p, t->local_offset.y, -10, 10, 16);
		serialize_r32_range(p, t->local_offset.z, -10, 10, 16);
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

	if (e->has<Projectile>())
	{
		Projectile* x = e->get<Projectile>();
		serialize_ref(p, x->owner);
		serialize_r32(p, x->velocity.x);
		serialize_r32(p, x->velocity.y);
		serialize_r32(p, x->velocity.z);
		serialize_r32(p, x->lifetime);
	}

	if (e->has<EnergyPickup>())
	{
		EnergyPickup* b = e->get<EnergyPickup>();
		serialize_s8(p, b->team);
		serialize_ref(p, b->light);
	}

	if (e->has<Sensor>())
	{
		Sensor* s = e->get<Sensor>();
		serialize_s8(p, s->team);
	}

	if (e->has<Rocket>())
	{
		Rocket* r = e->get<Rocket>();
		serialize_ref(p, r->target);
		serialize_ref(p, r->owner);
		serialize_s8(p, r->team);
	}

	if (e->has<ContainmentField>())
	{
		ContainmentField* c = e->get<ContainmentField>();
		serialize_r32(p, c->remaining_lifetime);
		serialize_ref(p, c->field);
		serialize_ref(p, c->owner);
		serialize_s8(p, c->team);
	}

	if (e->has<Teleporter>())
	{
		Teleporter* t = e->get<Teleporter>();
		serialize_s8(p, t->team);
	}

	if (e->has<Water>())
	{
		Water* w = e->get<Water>();
		serialize_r32_range(p, w->color.x, 0, 1.0f, 8);
		serialize_r32_range(p, w->color.y, 0, 1.0f, 8);
		serialize_r32_range(p, w->color.z, 0, 1.0f, 8);
		serialize_r32_range(p, w->color.w, 0, 1.0f, 8);
		serialize_r32_range(p, w->displacement_horizontal, 0, 10, 8);
		serialize_r32_range(p, w->displacement_vertical, 0, 10, 8);
		serialize_s16(p, w->mesh);
		serialize_asset(p, w->texture, Loader::static_texture_count);
	}

	if (e->has<DirectionalLight>())
	{
		DirectionalLight* d = e->get<DirectionalLight>();
		serialize_r32_range(p, d->color.x, 0.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.y, 0.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.z, 0.0f, 1.0f, 8);
		serialize_bool(p, d->shadowed);
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

	if (e->has<Team>())
	{
		Team* t = e->get<Team>();
		serialize_ref(p, t->player_spawn);
	}

	if (e->has<PlayerHuman>())
	{
		PlayerHuman* ph = e->get<PlayerHuman>();
		serialize_ref(p, ph->map_view);
		serialize_u64(p, ph->uuid);
		if (Stream::IsReading)
		{
			ph->local = false;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (ph->uuid == Game::session.local_player_uuids[i])
				{
					ph->local = true;
					ph->gamepad = s8(i);
					break;
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
		serialize_ref(p, m->entity);
		serialize_s16(p, m->credits);
		serialize_s16(p, m->kills);
		serialize_s16(p, m->respawns);
		s32 username_length;
		if (Stream::IsWriting)
			username_length = strlen(m->username);
		serialize_int(p, s32, username_length, 0, MAX_USERNAME);
		serialize_bytes(p, (u8*)m->username, username_length);
		if (Stream::IsReading)
			m->username[username_length] = '\0';
	}

	if (e->has<PlayerCommon>())
	{
		PlayerCommon* pc = e->get<PlayerCommon>();
		serialize_quat(p, &pc->attach_quat, Resolution::High);
		serialize_r32_range(p, pc->angle_horizontal, PI * -2.0f, PI * 2.0f, 16);
		serialize_r32_range(p, pc->angle_vertical, -PI, PI, 16);
		serialize_ref(p, pc->manager);
	}

	if (e->has<PlayerControlHuman>())
	{
		PlayerControlHuman* c = e->get<PlayerControlHuman>();
		serialize_ref(p, c->player);
	}

#if !SERVER
	if (Stream::IsReading && Client::state_client.mode == Client::Mode::Connected)
		World::awake(e);
#endif

	return true;
}

template<typename Stream> b8 serialize_init_packet(Stream* p)
{
	serialize_s16(p, Game::level.id);
	serialize_enum(p, Game::FeatureLevel, Game::level.feature_level);
	serialize_r32(p, Game::level.skybox.far_plane);
	serialize_asset(p, Game::level.skybox.texture, Loader::static_texture_count);
	serialize_asset(p, Game::level.skybox.shader, Loader::shader_count);
	serialize_asset(p, Game::level.skybox.mesh, Loader::static_mesh_count);
	serialize_r32_range(p, Game::level.skybox.color.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.color.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.color.z, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.ambient_color.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.ambient_color.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.ambient_color.z, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.player_light.x, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.player_light.y, 0.0f, 1.0f, 8);
	serialize_r32_range(p, Game::level.skybox.player_light.z, 0.0f, 1.0f, 8);
	serialize_enum(p, Game::Mode, Game::level.mode);
	serialize_enum(p, Game::Type, Game::level.type);
	return true;
}

// true if s1 > s2
b8 sequence_more_recent(SequenceID s1, SequenceID s2)
{
	return ((s1 > s2) && (s1 - s2 <= SEQUENCE_COUNT / 2))
		|| ((s2 > s1) && (s2 - s1 > SEQUENCE_COUNT / 2));
}

s32 sequence_relative_to(SequenceID s1, SequenceID s2)
{
	if (sequence_more_recent(s1, s2))
	{
		if (s1 < s2)
			return (s32(s1) + SEQUENCE_COUNT) - s32(s2);
		else
			return s32(s1) - s32(s2);
	}
	else
	{
		if (s1 < s2)
			return s32(s1) - s32(s2);
		else
			return s32(s1) - (s32(s2) + SEQUENCE_COUNT);
	}
}

SequenceID sequence_advance(SequenceID start, s32 delta)
{
	s32 result = s32(start) + delta;
	while (result < 0)
		result += SEQUENCE_COUNT;
	while (result >= SEQUENCE_COUNT)
		result -= SEQUENCE_COUNT;
	return SequenceID(result);
}

#if DEBUG
void ack_debug(const char* caption, const Ack& ack)
{
	char str[33] = {};
	for (s32 i = 0; i < 32; i++)
		str[i] = (ack.previous_sequences & (1 << i)) ? '1' : '0';
	vi_debug("%s %d %s", caption, s32(ack.sequence_id), str);
}

void msg_history_debug(const MessageHistory& history)
{
	if (history.msgs.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			const MessageFrame& msg = history.msgs[index];
			vi_debug("%d", s32(msg.sequence_id));

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;
		}
	}
}
#endif

MessageFrame* msg_history_add(MessageHistory* history, r32 timestamp, s32 bytes)
{
	MessageFrame* frame;
	if (history->msgs.length < history->msgs.capacity())
	{
		frame = history->msgs.add();
		history->current_index = history->msgs.length - 1;
	}
	else
	{
		history->current_index = (history->current_index + 1) % history->msgs.capacity();
		frame = &history->msgs[history->current_index];
	}
	new (frame) MessageFrame(timestamp, bytes);
	return frame;
}

Ack msg_history_ack(const MessageHistory& history)
{
	Ack ack = {};
	if (history.msgs.length > 0)
	{
		s32 index = history.current_index;
		// find most recent sequence ID
		ack.sequence_id = history.msgs[index].sequence_id;
		for (s32 i = 0; i < 64; i++)
		{
			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;

			const MessageFrame& msg = history.msgs[index];
			if (sequence_more_recent(msg.sequence_id, ack.sequence_id))
				ack.sequence_id = msg.sequence_id;
		}

		index = history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history.msgs.length - 1;
			if (index == history.current_index) // we looped all the way around
				break;

			const MessageFrame& msg = history.msgs[index];
			if (msg.sequence_id != ack.sequence_id) // ignore the ack
			{
				s32 sequence_id_relative_to_most_recent = sequence_relative_to(msg.sequence_id, ack.sequence_id);
				vi_assert(sequence_id_relative_to_most_recent < 0);
				if (sequence_id_relative_to_most_recent >= -32)
					ack.previous_sequences |= 1 << (-sequence_id_relative_to_most_recent - 1);
			}
		}
	}
	return ack;
}

// server/client data
struct StateCommon
{
	MessageHistory msgs_out_history;
	StaticArray<StreamWrite, MESSAGE_BUFFER> msgs_out;
	SequenceID local_sequence_id = 1;
	StateHistory state_history;
	StateFrame state_frame_restore;
	s32 bandwidth_in;
	s32 bandwidth_out;
	s32 bandwidth_in_counter;
	s32 bandwidth_out_counter;
};
StateCommon state_common;

void packet_init(StreamWrite* p)
{
	p->bits(NET_PROTOCOL_ID, 32); // packet_send() will replace this with the packet checksum
}

void packet_finalize(StreamWrite* p)
{
	vi_assert(p->data[0] == NET_PROTOCOL_ID);
	p->flush();

	// compress everything but the protocol ID
	StreamWrite compressed;
	compressed.resize_bytes(NET_MAX_PACKET_SIZE);
	z_stream z;
	z.zalloc = nullptr;
	z.zfree = nullptr;
	z.opaque = nullptr;
	z.next_out = (Bytef*)&compressed.data[1];
	z.avail_out = NET_MAX_PACKET_SIZE - sizeof(u32);
	z.next_in = (Bytef*)&p->data[1];
	z.avail_in = p->bytes_written() - sizeof(u32);

	s32 result = deflateInit(&z, Z_DEFAULT_COMPRESSION);
	vi_assert(result == Z_OK);

	result = deflate(&z, Z_FINISH);

	vi_assert(result == Z_STREAM_END && z.avail_in == 0);

	result = deflateEnd(&z);
	vi_assert(result == Z_OK);

	p->reset();
	p->resize_bytes(sizeof(u32) + NET_MAX_PACKET_SIZE - z.avail_out); // include one u32 for the CRC32
	vi_assert(p->data.length > 0);
	p->data[p->data.length - 1] = 0; // make sure everything gets zeroed out so the CRC32 comes out right
	memcpy(&p->data[1], &compressed.data[1], NET_MAX_PACKET_SIZE - z.avail_out);

	// replace protocol ID with CRC32
	u32 checksum = crc32((const u8*)&p->data[0], sizeof(u32));
	checksum = crc32((const u8*)&p->data[1], (p->data.length - 1) * sizeof(u32), checksum);

	p->data[0] = checksum;
}

void packet_decompress(StreamRead* p, s32 bytes)
{
	StreamRead decompressed;
	decompressed.resize_bytes(NET_MAX_PACKET_SIZE);
	
	z_stream z;
	z.zalloc = nullptr;
	z.zfree = nullptr;
	z.opaque = nullptr;
	z.next_in = (Bytef*)&p->data[1];
	z.avail_in = bytes - sizeof(u32);
	z.next_out = (Bytef*)&decompressed.data[1];
	z.avail_out = NET_MAX_PACKET_SIZE - sizeof(u32);

	s32 result = inflateInit(&z);
	vi_assert(result == Z_OK);
	
	result = inflate(&z, Z_NO_FLUSH);
	vi_assert(result == Z_STREAM_END);

	result = inflateEnd(&z);
	vi_assert(result == Z_OK);

	p->reset();
	p->resize_bytes(sizeof(u32) + NET_MAX_PACKET_SIZE - z.avail_out);
	vi_assert(p->data.length > 0);

	p->data[p->data.length - 1] = 0; // make sure everything is zeroed out so the CRC32 comes out right
	memcpy(&p->data[1], &decompressed.data[1], NET_MAX_PACKET_SIZE - z.avail_out);

	p->bits_read = 32; // skip past the CRC32
}

void packet_send(const StreamWrite& p, const Sock::Address& address)
{
#if DEBUG_BANDWIDTH
	vi_debug("Outgoing packet size: %dB", p.bytes_written());
#endif
	Sock::udp_send(&sock, address, p.data.data, p.bytes_written());
	state_common.bandwidth_out_counter += p.bytes_written();
}

namespace Server
{
	b8 all_clients_loaded();
}

b8 allow_large_messages()
{
#if SERVER
	return Server::all_clients_loaded();
#else
	return false;
#endif
}

// consolidate msgs_out into msgs_out_history
b8 msgs_out_consolidate()
{
	using Stream = StreamWrite;

	if (state_common.msgs_out.length == 0)
		msg_finalize(msg_new(MessageType::Noop)); // we have to send SOMETHING every sequence

	s32 bytes = 0;
	s32 msgs = 0;
	for (s32 i = 0; i < state_common.msgs_out.length; i++)
	{
		s32 msg_bytes = state_common.msgs_out[i].bytes_written();

		// during loading, we can use the whole packet to send message
		// because we don't send state frames
		s32 max_messages_size = allow_large_messages() ? NET_MAX_PACKET_SIZE : MAX_MESSAGES_SIZE;

		if (64 + bytes + msg_bytes > max_messages_size)
			break;
		bytes += msg_bytes;
		msgs++;
	}

	MessageFrame* frame = msg_history_add(&state_common.msgs_out_history, Game::real_time.total, bytes);

	frame->sequence_id = state_common.local_sequence_id;

	serialize_int(&frame->write, s32, bytes, 0, MAX_MESSAGES_SIZE); // message frame size
	if (bytes > 0)
	{
		serialize_int(&frame->write, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);
		for (s32 i = 0; i < msgs; i++)
		{
			serialize_bytes(&frame->write, (u8*)state_common.msgs_out[i].data.data, state_common.msgs_out[i].bytes_written());
			serialize_align(&frame->write);
		}
	}

	frame->write.flush();
	
	for (s32 i = msgs - 1; i >= 0; i--)
		state_common.msgs_out.remove_ordered(i);

	return true;
}

MessageFrame* msg_frame_advance(MessageHistory* history, SequenceID* id, r32 timestamp)
{
	if (history->msgs.length > 0)
	{
		s32 index = history->current_index;
		SequenceID next_sequence = sequence_advance(*id, 1);
		for (s32 i = 0; i < 64; i++)
		{
			MessageFrame* msg = &history->msgs[index];
			if (msg->sequence_id == next_sequence && msg->timestamp <= timestamp)
			{
				*id = next_sequence;
				return msg;
			}

			// loop backward through most recently received frames
			index = index > 0 ? index - 1 : history->msgs.length - 1;
			if (index == history->current_index) // we looped all the way around
				break;
		}
	}
	return nullptr;
}

b8 ack_get(const Ack& ack, SequenceID sequence_id)
{
	if (sequence_more_recent(sequence_id, ack.sequence_id))
		return false;
	else if (sequence_id == ack.sequence_id)
		return true;
	else
	{
		s32 relative = sequence_relative_to(sequence_id, ack.sequence_id);
		vi_assert(relative < 0);
		if (relative < -32)
			return false;
		else
			return ack.previous_sequences & (1 << (-relative - 1));
	}
}

void sequence_history_add(SequenceHistory* history, SequenceID id, r32 timestamp)
{
	if (history->length == history->capacity())
		history->remove(history->length - 1);
	*history->insert(0) = { timestamp, id };
}

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
	s32 bytes = 0;

	if (history.msgs.length > 0)
	{
		// resend previous frames
		{
			// rewind to 64 frames previous
			s32 index = history.current_index;
			for (s32 i = 0; i < 64; i++)
			{
				s32 next_index = index > 0 ? index - 1 : history.msgs.length - 1;
				if (next_index == history.current_index)
					break;
				index = next_index;
			}

			// start resending frames starting at that index
			r32 timestamp_cutoff = Game::real_time.total - (rtt * 3.0f); // wait a certain period before trying to resend a sequence
			for (s32 i = 0; i < 64 && index != history.current_index; i++)
			{
				const MessageFrame& frame = history.msgs[index];
				s32 relative_sequence = sequence_relative_to(frame.sequence_id, remote_ack.sequence_id);
				if (relative_sequence < 0
					&& relative_sequence >= -32
					&& !ack_get(remote_ack, frame.sequence_id)
					&& !sequence_history_contains_newer_than(*recently_resent, frame.sequence_id, timestamp_cutoff)
					&& bytes + frame.write.bytes_written() <= MAX_MESSAGES_SIZE)
				{
					vi_debug("Resending sequence %d", s32(frame.sequence_id));
					bytes += frame.write.bytes_written();
					serialize_align(p);
					serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
					sequence_history_add(recently_resent, frame.sequence_id, Game::real_time.total);
				}

				index = index < history.msgs.length - 1 ? index + 1 : 0;
			}
		}

		// current frame
		{
			const MessageFrame& frame = history.msgs[history.current_index];
			if (bytes + frame.write.bytes_written() <= MAX_MESSAGES_SIZE)
			{
				serialize_align(p);
				serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
			}
		}
	}

	serialize_align(p);
	bytes = 0;
	serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE); // zero sized frame marks end of message frames

	return true;
}

void calculate_rtt(r32 timestamp, const Ack& ack, const MessageHistory& send_history, r32* rtt)
{
	r32 new_rtt = -1.0f;
	if (send_history.msgs.length > 0)
	{
		s32 index = send_history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			const MessageFrame& msg = send_history.msgs[index];
			if (msg.sequence_id == ack.sequence_id)
			{
				new_rtt = timestamp - msg.timestamp;
				break;
			}
			index = index > 0 ? index - 1 : send_history.msgs.length - 1;
			if (index == send_history.current_index)
				break;
		}
	}
	if (new_rtt == -1.0f || *rtt == -1.0f)
		*rtt = new_rtt;
	else
		*rtt = (*rtt * 0.9f) + (new_rtt * 0.1f);
}

b8 msgs_read(StreamRead* p, MessageHistory* history, Ack* ack)
{
	using Stream = StreamRead;

	Ack ack_candidate;
	serialize_int(p, SequenceID, ack_candidate.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack_candidate.previous_sequences);
	if (sequence_more_recent(ack_candidate.sequence_id, ack->sequence_id))
		*ack = ack_candidate;

	while (true)
	{
		serialize_align(p);
		s32 bytes;
		serialize_int(p, s32, bytes, 0, MAX_MESSAGES_SIZE);
		if (bytes)
		{
			MessageFrame* frame = msg_history_add(history, Game::real_time.total, bytes);
			serialize_int(p, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);
			frame->read.resize_bytes(bytes);
			serialize_bytes(p, (u8*)frame->read.data.data, bytes);
			serialize_align(p);
		}
		else
			break;
	}

	return true;
}

template<typename Stream> b8 serialize_quat(Stream* p, Quat* rot, Resolution r)
{
	Quat q;
	s32 largest_index;
	if (Stream::IsWriting)
	{
		q = Quat::normalize(*rot);
		largest_index = 0; // w
		if (fabs(q.x) > fabs(q[largest_index]))
			largest_index = 1;
		if (fabs(q.y) > fabs(q[largest_index]))
			largest_index = 2;
		if (fabs(q.z) > fabs(q[largest_index]))
			largest_index = 3;
		if (q[largest_index] < 0.0f)
		{
			q.w *= -1.0f;
			q.x *= -1.0f;
			q.y *= -1.0f;
			q.z *= -1.0f;
		}
	}
	serialize_int(p, s32, largest_index, 0, 3);

	s32 indices[3];
	{
		s32 index = 0;
		for (s32 i = 0; i < 4; i++)
		{
			if (i != largest_index)
			{
				indices[index] = i;
				index++;
			}
		}
	}
	s32 bits = r == Resolution::High ? 16 : 9;
	serialize_r32_range(p, q[indices[0]], -0.707107f, 0.707107f, bits);
	serialize_r32_range(p, q[indices[1]], -0.707107f, 0.707107f, bits);
	serialize_r32_range(p, q[indices[2]], -0.707107f, 0.707107f, bits);

	if (Stream::IsReading)
	{
		r32 a = q[indices[0]];
		r32 b = q[indices[1]];
		r32 c = q[indices[2]];
		q[largest_index] = sqrtf(1.0f - (a * a) - (b * b) - (c * c));
		*rot = q;
	}
	return true;
}

template<typename Stream> b8 serialize_transform(Stream* p, TransformState* transform)
{
	serialize_enum(p, Resolution, transform->resolution);
	if (!serialize_position(p, &transform->pos, transform->resolution))
		net_error();
	if (!serialize_quat(p, &transform->rot, transform->resolution))
		net_error();
	return true;
}

template<typename Stream> b8 serialize_player_manager(Stream* p, PlayerManagerState* state, const PlayerManagerState* old)
{
	b8 b;

	if (Stream::IsReading)
		state->active = true;

	if (Stream::IsWriting)
		b = !old || state->spawn_timer != old->spawn_timer;
	serialize_bool(p, b);
	if (b)
		serialize_r32_range(p, state->spawn_timer, 0, PLAYER_SPAWN_DELAY, 8);

	if (Stream::IsWriting)
		b = !old || state->state_timer != old->state_timer;
	serialize_bool(p, b);
	if (b)
		serialize_r32_range(p, state->state_timer, 0, 10.0f, 10);

	if (Stream::IsWriting)
		b = !old || state->upgrades != old->upgrades;
	serialize_bool(p, b);
	if (b)
		serialize_s32(p, state->upgrades);

	for (s32 i = 0; i < MAX_ABILITIES; i++)
	{
		if (Stream::IsWriting)
			b = !old || state->abilities[i] != old->abilities[i];
		serialize_bool(p, b);
		if (b)
			serialize_int(p, Ability, state->abilities[i], 0, s32(Ability::count) + 1); // necessary because Ability::None = Ability::count
	}

	if (Stream::IsWriting)
		b = !old || state->current_upgrade != old->current_upgrade;
	serialize_bool(p, b);
	if (b)
		serialize_int(p, Upgrade, state->current_upgrade, 0, s32(Upgrade::count) + 1); // necessary because Upgrade::None = Upgrade::count

	if (Stream::IsWriting)
		b = !old || !state->entity.equals(old->entity);
	serialize_bool(p, b);
	if (b)
		serialize_ref(p, state->entity);

	if (Stream::IsWriting)
		b = !old || state->credits != old->credits;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->credits);

	if (Stream::IsWriting)
		b = !old || state->kills != old->kills;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->kills);

	if (Stream::IsWriting)
		b = !old || state->respawns != old->respawns;
	serialize_bool(p, b);
	if (b)
		serialize_s16(p, state->respawns);

	return true;
}

template<typename Stream> b8 serialize_awk(Stream* p, AwkState* state, const AwkState* old)
{
	b8 b;

	if (Stream::IsReading)
		state->active = true;

	if (Stream::IsWriting)
		b = old && state->charges != old->charges;
	serialize_bool(p, b);
	if (b)
		serialize_int(p, s8, state->charges, 0, AWK_CHARGES);
	return true;
}

b8 equal_states_transform(const TransformState& a, const TransformState& b)
{
	Resolution r = Resolution::Low;
	r32 tolerance_pos_sq = 0.005f * 0.005f;
	r32 tolerance_rot = 0.001f;
	if (a.resolution == Resolution::High || b.resolution == Resolution::High)
	{
		tolerance_pos_sq = 0.001f * 0.001f;
		tolerance_rot = 0.0001f;
	}

	return a.revision == b.revision
		&& a.resolution == b.resolution
		&& a.parent.equals(b.parent)
		&& (a.pos - b.pos).length_squared() < tolerance_pos_sq
		&& Quat::angle(a.rot, b.rot) < tolerance_rot;
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

b8 equal_states_player(const PlayerManagerState& a, const PlayerManagerState& b)
{
	if (a.spawn_timer != b.spawn_timer
		|| a.state_timer != b.state_timer
		|| a.upgrades != b.upgrades
		|| a.current_upgrade != b.current_upgrade
		|| !a.entity.equals(b.entity)
		|| a.credits != b.credits
		|| a.kills != b.kills
		|| a.respawns != b.respawns
		|| a.active != b.active)
		return false;

	for (s32 i = 0; i < MAX_ABILITIES; i++)
	{
		if (a.abilities[i] != b.abilities[i])
			return false;
	}

	return true;
}


b8 equal_states_awk(const AwkState& a, const AwkState& b)
{
	return a.active == b.active
		&& a.charges == b.charges;
}

template<typename Stream> b8 serialize_state_frame(Stream* p, StateFrame* frame, const StateFrame* base)
{
	if (Stream::IsReading)
	{
		if (base)
			memcpy(frame, base, sizeof(*frame));
		else
			new (frame) StateFrame();
		frame->timestamp = Game::real_time.total;
	}

	serialize_int(p, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);

	s32 changed_count;
	if (Stream::IsWriting)
	{
		// count changed transforms
		changed_count = 0;
		s32 index = s32(frame->transforms_active.start);
		while (index < frame->transforms_active.end)
		{
			if (!equal_states_transform(frame, base, index))
				changed_count++;
			index++;
		}
	}
	serialize_int(p, s32, changed_count, 0, MAX_ENTITIES - 1);

	s32 index;
	if (Stream::IsWriting)
		index = s32(frame->transforms_active.start);
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
				parent_changed = !base || !frame->transforms[index].parent.equals(base->transforms[index].parent);
			serialize_bool(p, parent_changed);
			if (parent_changed)
				serialize_ref(p, frame->transforms[index].parent);
			if (!serialize_transform(p, &frame->transforms[index]))
				net_error();
		}
		index++;
	}
#if DEBUG_TRANSFORMS
	vi_debug("Wrote %d transforms", changed_count);
#endif

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

	// Awks
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		AwkState* state = &frame->awks[i];
		b8 serialize;
		if (Stream::IsWriting)
			serialize = state->active && base && !equal_states_awk(*state, base->awks[i]);
		serialize_bool(p, serialize);
		if (serialize)
		{
			if (!serialize_awk(p, state, base ? &base->awks[i] : nullptr))
				net_error();
		}
	}

	return true;
}

b8 transform_filter(const Transform* t)
{
	return t->has<Awk>()
		|| t->has<EnergyPickup>()
		|| t->has<Projectile>()
		|| t->has<Rocket>()
		|| t->has<Walker>()
		|| t->has<Sensor>()
		|| t->has<Rope>();
}

void state_frame_build(StateFrame* frame)
{
	frame->sequence_id = state_common.local_sequence_id;
	for (auto i = Transform::list.iterator(); !i.is_last(); i.next())
	{
		if (transform_filter(i.item()))
		{
			frame->transforms_active.set(i.index, true);
			TransformState* transform = &frame->transforms[i.index];
			transform->revision = i.item()->revision;
			transform->pos = i.item()->pos;
			transform->rot = i.item()->rot;
			transform->parent = i.item()->parent.ref(); // ID must come out to IDNull if it's null; don't rely on revision to null the reference
			transform->resolution = i.item()->has<PlayerControlHuman>() ? Resolution::High : Resolution::Low;
		}
	}

	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		PlayerManagerState* state = &frame->players[i.index];
		state->spawn_timer = i.item()->spawn_timer;
		state->state_timer = i.item()->state_timer;
		state->upgrades = i.item()->upgrades;
		memcpy(state->abilities, i.item()->abilities, sizeof(state->abilities));
		state->current_upgrade = i.item()->current_upgrade;
		state->entity = i.item()->entity;
		state->credits = i.item()->credits;
		state->kills = i.item()->kills;
		state->respawns = i.item()->respawns;
		state->active = true;
	}

	for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
	{
		vi_assert(i.index < MAX_PLAYERS);
		AwkState* state = &frame->awks[i.index];
		state->active = true;
		state->charges = i.item()->charges;
	}
}

// get the absolute pos and rot of the given transform
void transform_absolute(const StateFrame& frame, s32 index, Vec3* abs_pos, Quat* abs_rot)
{
	*abs_rot = Quat::identity;
	*abs_pos = Vec3::zero;
	while (index != IDNull)
	{ 
		if (frame.transforms_active.get(index))
		{
			// this transform is being tracked with the dynamic transform system
			const TransformState* transform = &frame.transforms[index];
			*abs_rot = transform->rot * *abs_rot;
			*abs_pos = (transform->rot * *abs_pos) + transform->pos;
			index = transform->parent.id;
		}
		else
		{
			// this transform is not being tracked in our system; get its info from the game state
			Transform* transform = &Transform::list[index];
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
	result->transforms_active = b.transforms_active;

	// transforms
	{
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
			player->spawn_timer = LMath::lerpf(blend, last.spawn_timer, next.spawn_timer);
			player->state_timer = LMath::lerpf(blend, last.state_timer, next.state_timer);
		}
	}

	// awks
	memcpy(result->awks, b.awks, sizeof(result->awks));
}

void state_frame_apply(const StateFrame& frame)
{
	// transforms
	s32 index = frame.transforms_active.start;
	while (index < frame.transforms_active.end)
	{
		Transform* t = &Transform::list[index];
		const TransformState& s = frame.transforms[index];
		if (t->revision == s.revision && Transform::list.active(index))
		{
			if (t->has<PlayerControlHuman>() && t->get<PlayerControlHuman>()->player.ref()->local)
			{
				// this is a local player; we don't want to immediately overwrite its position with the server's data
				// let the PlayerControlHuman deal with it
			}
			else
			{
				t->pos = s.pos;
				t->rot = s.rot;
				t->parent = s.parent;
			}
		}

		index = frame.transforms_active.next(index);
	}

	// players
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const PlayerManagerState& state = frame.players[i];
		if (state.active)
		{
			PlayerManager* s = &PlayerManager::list[i];
			s->spawn_timer = state.spawn_timer;
			s->state_timer = state.state_timer;
			s->upgrades = state.upgrades;
			memcpy(s->abilities, state.abilities, sizeof(s->abilities));
			s->current_upgrade = state.current_upgrade;
			s->entity = state.entity;
			s->credits = state.credits;
			s->kills = state.kills;
			s->respawns = state.respawns;
		}
	}

	// Awks
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		const AwkState& state = frame.awks[i];
		if (state.active)
		{
			Awk* a = &Awk::list[i];
			a->charges = state.charges;
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
	frame->timestamp = Game::real_time.total;
	return frame;
}

const StateFrame* state_frame_by_sequence(const StateHistory& history, SequenceID sequence_id)
{
	if (history.frames.length > 0)
	{
		s32 index = history.current_index;
		for (s32 i = 0; i < 64; i++)
		{
			const StateFrame* frame = &history.frames[index];
			if (frame->sequence_id == sequence_id)
				return frame;

			// loop backward through most recent frames
			index = index > 0 ? index - 1 : history.frames.length - 1;
			if (index == history.current_index) // we looped all the way around
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
		for (s32 i = 0; i < 64; i++)
		{
			const StateFrame* frame = &history.frames[index];

			if (frame->timestamp < timestamp)
				return &history.frames[index];

			// loop backward through most recent frames
			index = index > 0 ? index - 1 : history.frames.length - 1;
			if (index == history.current_index) // we looped all the way around
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

#if SERVER

namespace Server
{

struct Client
{
	Sock::Address address;
	r32 timeout;
	r32 rtt = 0.5f;
	Ack ack = { u32(-1), 0 }; // most recent ack we've received from the client
	MessageHistory msgs_in_history; // messages we've received from the client
	SequenceHistory recently_resent; // sequences we resent to the client recently
	SequenceID processed_sequence_id; // most recent sequence ID we've processed from the client
	StaticArray<Ref<PlayerHuman>, MAX_GAMEPADS> players;
	b8 connected;
	b8 loading_done;
};

b8 msg_process(StreamRead*, Client*);

struct StateServer
{
	Array<Client> clients;
	Mode mode;
	s32 expected_clients = 1;
	SequenceID sequence_completed_loading;
};
StateServer state_server;

s32 connected_clients()
{
	s32 result = 0;
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		if (state_server.clients[i].connected)
			result++;
	}
	return result;
}

b8 all_clients_loaded()
{
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		const Client& client = state_server.clients[i];
		if (!client.loading_done)
			return false;
	}
	return true;
}

b8 client_owns(Client* c, Entity* e)
{
	if (e->has<PlayerControlHuman>())
	{
		PlayerHuman* player = e->get<PlayerControlHuman>()->player.ref();
		for (s32 i = 0; i < c->players.length; i++)
		{
			if (c->players[i].ref() == player)
				return true;
		}
	}
	return false;
}

b8 init()
{
	if (Sock::udp_open(&sock, 3494, true))
	{
		printf("%s\n", Sock::get_error());
		return false;
	}

	// todo: allow both multiplayer / story mode sessions
	Game::session.story_mode = true;
	Game::load_level(Update(), Asset::Level::Ponos, Game::Mode::Pvp);

	return true;
}

b8 build_packet_init(StreamWrite* p)
{
	using Stream = StreamWrite;
	packet_init(p);
	ServerPacket type = ServerPacket::Init;
	serialize_enum(p, ServerPacket, type);
	serialize_init_packet(p);
	packet_finalize(p);
	return true;
}

b8 build_packet_disconnect(StreamWrite* p)
{
	using Stream = StreamWrite;
	packet_init(p);
	ServerPacket type = ServerPacket::Disconnect;
	serialize_enum(p, ServerPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_keepalive(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Keepalive;
	serialize_enum(p, ServerPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_update(StreamWrite* p, Client* client, StateFrame* frame)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Update;
	serialize_enum(p, ServerPacket, type);
	Ack ack = msg_history_ack(client->msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);
	msgs_write(p, state_common.msgs_out_history, client->ack, &client->recently_resent, client->rtt);

	if (frame)
	{
		serialize_int(p, SequenceID, client->ack.sequence_id, 0, SEQUENCE_COUNT - 1);
		const StateFrame* base = state_frame_by_sequence(state_common.state_history, client->ack.sequence_id);
		serialize_state_frame(p, frame, base);
	}

	packet_finalize(p);
	return true;
}

void update(const Update& u)
{
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* client = &state_server.clients[i];
		if (client->connected)
		{
			while (MessageFrame* frame = msg_frame_advance(&client->msgs_in_history, &client->processed_sequence_id, Game::real_time.total))
			{
				frame->read.rewind();
#if DEBUG_MSG
				vi_debug("Processing seq %d", frame->sequence_id);
#endif
				while (frame->read.bytes_read() < frame->bytes)
				{
					b8 success = msg_process(&frame->read, client);
					if (!success)
						break;
				}
			}
		}
	}

	// ensure everything is being finalized properly
#if DEBUG
	for (s32 i = 0; i < World::create_queue.length; i++)
		vi_assert(World::create_queue[i].ref()->finalized);
	World::create_queue.length = 0;
#endif

}

void tick(const Update& u)
{
	// send out packets

	if (state_server.mode == Mode::Active)
		msgs_out_consolidate();

	StateFrame* frame = nullptr;
	if (all_clients_loaded()) // don't send state frames while loading, to make more room for messages
	{
		frame = state_frame_add(&state_common.state_history);
		state_frame_build(frame);
	}

	StreamWrite p;
	for (s32 i = 0; i < state_server.clients.length; i++)
	{
		Client* client = &state_server.clients[i];
		client->timeout += Game::real_time.delta;
		if (client->timeout > TIMEOUT)
		{
			vi_debug("Client %s:%hd timed out.", Sock::host_to_str(client->address.host), client->address.port);
			state_server.clients.remove(i);
			i--;
		}
		else if (client->connected)
		{
			p.reset();
			build_packet_update(&p, client, frame);
			packet_send(p, state_server.clients[i].address);
		}
	}

	if (state_server.mode == Mode::Active)
	{
		state_common.local_sequence_id++;
		if (state_common.local_sequence_id == SEQUENCE_COUNT)
			state_common.local_sequence_id = 0;
	}
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
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
			if (state_server.clients.length < state_server.expected_clients)
			{
				if (!client)
				{
					client = state_server.clients.add();
					client_index = state_server.clients.length - 1;
					new (client) Client();
					client->address = address;
				}

				{
					StreamWrite p;
					build_packet_init(&p);
					packet_send(p, address);
				}
			}
			break;
		}
		case ClientPacket::AckInit:
		{
			if (client && !client->connected)
			{
				vi_debug("Client %s:%hd connected.", Sock::host_to_str(address.host), address.port);
				client->connected = true;

				{
					// initialize players
					char username[MAX_USERNAME + 1];
					s32 username_length;
					serialize_int(p, s32, username_length, 0, MAX_USERNAME);
					serialize_bytes(p, (u8*)username, username_length);
					username[username_length] = '\0';
					s32 local_players;
					serialize_int(p, s32, local_players, 0, MAX_GAMEPADS);
					client->players.length = 0;
					for (s32 i = 0; i < local_players; i++)
					{
						AI::Team team;
						serialize_int(p, AI::Team, team, 0, MAX_PLAYERS - 1);
						s8 gamepad;
						serialize_int(p, s8, gamepad, 0, MAX_GAMEPADS - 1);

						Entity* e = World::create<ContainerEntity>();
						PlayerManager* manager = e->add<PlayerManager>(&Team::list[Game::level.team_lookup[(s32)team]]);
						if (gamepad == 0)
							sprintf(manager->username, "%s", username);
						else
							sprintf(manager->username, "%s [%d]", username, s32(gamepad + 1));
						PlayerHuman* player = e->add<PlayerHuman>(false, gamepad);
						serialize_u64(p, player->uuid);
						player->local = false;
						client->players.add(player);
					}
				}

				if (connected_clients() == state_server.expected_clients)
				{
					state_server.mode = Mode::Active;
					using Stream = StreamWrite;
					for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
					{
						StreamWrite* p = msg_new(MessageType::EntityCreate);
						serialize_int(p, ID, i.index, 0, MAX_ENTITIES - 1);
						serialize_entity(p, i.item());
						msg_finalize(p);
					}
					msg_finalize(msg_new(MessageType::InitDone));
				}
			}
			break;
		}
		case ClientPacket::Update:
		{
			if (!client)
			{
				vi_debug("%s", "Discarding packet from unknown client.");
				return false;
			}

			if (!msgs_read(p, &client->msgs_in_history, &client->ack))
				net_error();

			calculate_rtt(Game::real_time.total, client->ack, state_common.msgs_out_history, &client->rtt);

			client->timeout = 0.0f;

			s32 count;
			serialize_int(p, ID, count, 0, MAX_GAMEPADS);
			for (s32 i = 0; i < count; i++)
			{
				ID id;
				serialize_int(p, ID, id, 0, MAX_PLAYERS - 1);

				PlayerControlHuman* c = &PlayerControlHuman::list[id];

				PlayerControlHuman::RemoteControl control;

				serialize_player_control(p, &control);

				if (client_owns(client, c->entity()))
					c->remote_control_handle(control);
			}

			break;
		}
		case ClientPacket::Disconnect:
		{
			if (!client)
			{
				vi_debug("%s", "Discarding packet from unknown client.");
				return false;
			}
			vi_debug("Client %s:%hd disconnected.", Sock::host_to_str(address.host), address.port);
			state_server.clients.remove(client_index);

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

// server function for processing messages
b8 msg_process(StreamRead* p, Client* client)
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
			break;
		}
		case MessageType::PlayerControlHuman:
		{
			Ref<PlayerControlHuman> c;
			serialize_ref(p, c);
			b8 valid = c.ref() && client_owns(client, c.ref()->entity());
			if (!PlayerControlHuman::net_msg(p, c.ref(), valid ? MessageSource::Remote : MessageSource::Invalid))
				net_error();
			break;
		}
		default:
		{
			net_error();
			break;
		}
	}
	serialize_align(p);
	return true;
}

Mode mode()
{
	return state_server.mode;
}

s32 expected_clients()
{
	return state_server.expected_clients;
}

void reset()
{
	StreamWrite p;
	build_packet_disconnect(&p);
	for (s32 i = 0; i < state_server.clients.length; i++)
		packet_send(p, state_server.clients[i].address);

	new (&Server::state_server) Server::StateServer();
}

}

#else

namespace Client
{

b8 msg_process(StreamRead*);

struct StateClient
{
	Camera* camera_connecting;
	r32 timeout;
	r32 tick_timer;
	r32 server_rtt = 0.5f;
	Mode mode;
	MessageHistory msgs_in_history; // messages we've received from the server
	Ack server_ack = { u32(-1), 0 }; // most recent ack we've received from the server
	Sock::Address server_address;
	SequenceHistory server_recently_resent; // sequences we recently resent to the server
	SequenceID server_processed_sequence_id; // most recent sequence ID we've processed from the server
};
StateClient state_client;

b8 init()
{
	if (Sock::udp_open(&sock, 3495, true))
	{
		if (Sock::udp_open(&sock, 3496, true))
		{
			printf("%s\n", Sock::get_error());
			return false;
		}
	}

	return true;
}

b8 build_packet_connect(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Connect;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_disconnect(StreamWrite* p)
{
	using Stream = StreamWrite;
	packet_init(p);
	ClientPacket type = ClientPacket::Disconnect;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_ack_init(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::AckInit;
	serialize_enum(p, ClientPacket, type);
	s32 local_players = Game::session.local_player_count();
	s32 username_length = strlen(Game::save.username);
	serialize_int(p, s32, username_length, 0, MAX_USERNAME);
	serialize_bytes(p, (u8*)Game::save.username, username_length);
	serialize_int(p, s32, local_players, 0, MAX_GAMEPADS);
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (Game::session.local_player_config[i] != AI::TeamNone)
		{
			serialize_int(p, AI::Team, Game::session.local_player_config[i], 0, MAX_PLAYERS - 1); // team
			serialize_int(p, s32, i, 0, MAX_GAMEPADS - 1); // gamepad
			serialize_u64(p, Game::session.local_player_uuids[i]); // uuid
		}
	}
	packet_finalize(p);
	return true;
}

b8 build_packet_update(StreamWrite* p, const Update& u)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Update;
	serialize_enum(p, ClientPacket, type);

	// ack received messages
	Ack ack = msg_history_ack(state_client.msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);

	msgs_write(p, state_common.msgs_out_history, state_client.server_ack, &state_client.server_recently_resent, state_client.server_rtt);

	// player control
	s32 count = PlayerControlHuman::count_local();
	serialize_int(p, s32, count, 0, MAX_GAMEPADS);
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->local())
		{
			serialize_int(p, ID, i.index, 0, MAX_PLAYERS - 1);
			PlayerControlHuman::RemoteControl control;
			control.movement = i.item()->get_movement(u, i.item()->get<PlayerCommon>()->look());
			Transform* t = i.item()->get<Transform>();
			control.pos = t->pos;
			control.rot = t->rot;
			control.parent = t->parent;
			serialize_player_control(p, &control);
		}
	}

	packet_finalize(p);
	return true;
}

void update(const Update& u)
{
	// "connecting..." camera
	{
		b8 camera_needed = state_client.mode == Mode::Connecting
			|| state_client.mode == Mode::Acking
			|| state_client.mode == Mode::Loading;
		if (camera_needed && !state_client.camera_connecting)
		{
			state_client.camera_connecting = Camera::add();
			state_client.camera_connecting->mask = 0; // don't display anything; entities will be popping in over the network
			state_client.camera_connecting->viewport =
			{
				Vec2::zero,
				Vec2(u.input->width, u.input->height),
			};
			r32 aspect = state_client.camera_connecting->viewport.size.y == 0 ? 1 : (r32)state_client.camera_connecting->viewport.size.x / (r32)state_client.camera_connecting->viewport.size.y;
			state_client.camera_connecting->perspective((60.0f * PI * 0.5f / 180.0f), aspect, 0.1f, 2.0f);
		}
		else if (!camera_needed && state_client.camera_connecting)
		{
			state_client.camera_connecting->remove();
			state_client.camera_connecting = nullptr;
		}
	}

	if (Game::level.local)
		return;

	r32 interpolation_time = Game::real_time.total - NET_INTERPOLATION_DELAY;

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
		state_frame_apply(*frame_final);
	}

	while (MessageFrame* frame = msg_frame_advance(&state_client.msgs_in_history, &state_client.server_processed_sequence_id, interpolation_time))
	{
		frame->read.rewind();
#if DEBUG_MSG
		vi_debug("Processing seq %d", frame->sequence_id);
#endif
		while (frame->read.bytes_read() < frame->bytes)
		{
			b8 success = Client::msg_process(&frame->read);
			if (!success)
				break;
		}
	}

	if (show_stats) // bandwidth counters are updated every half second
		Console::debug("%.0fkbps down | %.0fkbps up | %.0fms", state_common.bandwidth_in * 8.0f / 500.0f, state_common.bandwidth_out * 8.0f / 500.0f, state_client.server_rtt * 1000.0f);
}

void tick(const Update& u)
{
	state_client.timeout += Game::real_time.delta;
	switch (state_client.mode)
	{
		case Mode::Disconnected:
		{
			break;
		}
		case Mode::Connecting:
		{
			if (state_client.timeout > 0.25f)
			{
				state_client.timeout = 0.0f;
				vi_debug("Connecting to %s:%hd...", Sock::host_to_str(state_client.server_address.host), state_client.server_address.port);
				StreamWrite p;
				build_packet_connect(&p);
				packet_send(p, state_client.server_address);
			}
			break;
		}
		case Mode::Acking:
		{
			if (state_client.timeout > 0.25f)
			{
				state_client.timeout = 0.0f;
				vi_debug("Confirming connection to %s:%hd...", Sock::host_to_str(state_client.server_address.host), state_client.server_address.port);
				StreamWrite p;
				build_packet_ack_init(&p);
				packet_send(p, state_client.server_address);
			}
			break;
		}
		case Mode::Loading:
		case Mode::Connected:
		{
			if (state_client.timeout > TIMEOUT)
			{
				vi_debug("Lost connection to %s:%hd.", Sock::host_to_str(state_client.server_address.host), state_client.server_address.port);
				state_client.mode = Mode::Disconnected;
			}
			else
			{
				msgs_out_consolidate();

				StreamWrite p;
				build_packet_update(&p, u);
				packet_send(p, state_client.server_address);

				state_common.local_sequence_id++;
				if (state_common.local_sequence_id == SEQUENCE_COUNT)
					state_common.local_sequence_id = 0;
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

void connect(const char* ip, u16 port)
{
	Game::level.local = false;
	Sock::get_address(&state_client.server_address, ip, port);
	state_client.mode = Mode::Connecting;
}

Mode mode()
{
	return state_client.mode;
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
	using Stream = StreamRead;
	if (!address.equals(state_client.server_address))
	{
		vi_debug("%s", "Discarding packet from unexpected host.");
		return false;
	}
	ServerPacket type;
	serialize_enum(p, ServerPacket, type);
	switch (type)
	{
		case ServerPacket::Init:
		{
			if (state_client.mode == Mode::Connecting && serialize_init_packet(p))
				state_client.mode = Mode::Acking; // acknowledge the init packet
			break;
		}
		case ServerPacket::Keepalive:
		{
			state_client.timeout = 0.0f; // reset connection timeout
			break;
		}
		case ServerPacket::Update:
		{
			if (state_client.mode == Mode::Acking)
			{
				vi_debug("Connected to %s:%hd.", Sock::host_to_str(state_client.server_address.host), state_client.server_address.port);
				state_client.mode = Mode::Loading;
			}

			if (!msgs_read(p, &state_client.msgs_in_history, &state_client.server_ack))
				net_error();

			calculate_rtt(Game::real_time.total, state_client.server_ack, state_common.msgs_out_history, &state_client.server_rtt);

			if (p->bytes_read() < p->data.length * sizeof(u32)) // server doesn't always send state frames
			{
				SequenceID base_sequence_id;
				serialize_int(p, SequenceID, base_sequence_id, 0, SEQUENCE_COUNT - 1);
				const StateFrame* base = state_frame_by_sequence(state_common.state_history, base_sequence_id);
				StateFrame frame;
				serialize_state_frame(p, &frame, base);
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
			vi_debug("Connection closed by %s:%hd.", Sock::host_to_str(state_client.server_address.host), state_client.server_address.port);
			state_client.mode = Mode::Disconnected;
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
			for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
				World::awake(i.item());

			// let the server know we're done loading
			msg_finalize(msg_new(MessageType::LoadingDone));
			state_client.mode = Mode::Connected;
			break;
		}
		case MessageType::Noop:
		{
			break;
		}
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
		build_packet_disconnect(&p);
		packet_send(p, state_client.server_address);
	}

	new (&state_client) StateClient();
}

}

#endif

b8 init()
{
	if (Sock::init())
		return false;

#if SERVER
	return Server::init();
#else
	return Client::init();
#endif
}

b8 finalize(Entity* e)
{
#if SERVER
	if (Server::state_server.mode == Server::Mode::Active)
	{
		using Stream = StreamWrite;
		StreamWrite* p = msg_new(MessageType::EntityCreate);
		ID id = e->id();
		serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
		serialize_entity(p, e);
		msg_finalize(p);
	}
#if DEBUG
	e->finalized = true;
#endif
#else
	// client
	vi_assert(Game::level.local); // client can't spawn entities if it is connected to a server
#endif
	return true;
}

b8 remove(Entity* e)
{
#if SERVER
	vi_assert(Entity::list.active(e->id()));
	using Stream = StreamWrite;
	StreamWrite* p = msg_new(MessageType::EntityRemove);
	ID id = e->id();
#if DEBUG_ENTITY
	vi_debug("Deleting entity ID %d", s32(id));
#endif
	serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
	msg_finalize(p);
#endif
	return true;
}

void update_start(const Update& u)
{
	while (true)
	{
		Sock::Address address;
		StreamRead incoming_packet;
		s32 bytes_received = Sock::udp_receive(&sock, &address, incoming_packet.data.data, NET_MAX_PACKET_SIZE);
		if (bytes_received > 0)
		{
			//if (mersenne::randf_co() < 0.75f) // packet loss simulation
			{
				state_common.bandwidth_in_counter += bytes_received;

				incoming_packet.resize_bytes(bytes_received);

				if (incoming_packet.read_checksum())
				{
					packet_decompress(&incoming_packet, bytes_received);
#if SERVER
					Server::packet_handle(u, &incoming_packet, address);
#else
					Client::packet_handle(u, &incoming_packet, address);
#endif
				}
				else
					vi_debug("%s", "Discarding packet due to invalid checksum.");
			}
		}
		else
			break;
	}
#if SERVER
	Server::update(u);
#else
	Client::update(u);
#endif
	
	// update bandwidth every half second
	if (s32(Game::real_time.total * 2.0f) > s32((Game::real_time.total - Game::real_time.delta) * 2.0f))
	{
		state_common.bandwidth_in = state_common.bandwidth_in_counter;
		state_common.bandwidth_out = state_common.bandwidth_out_counter;
		state_common.bandwidth_in_counter = 0;
		state_common.bandwidth_out_counter = 0;
	}
}

void update_end(const Update& u)
{
#if SERVER
	// server always runs at 60 FPS
	Server::tick(u);
#else
	Client::state_client.tick_timer += Game::real_time.delta;
	if (Client::state_client.tick_timer > NET_TICK_RATE)
	{
		Client::state_client.tick_timer -= NET_TICK_RATE;

		Client::tick(u);
	}
	// we're not going to send more than one packet per frame
	// so make sure the tick timer never gets out of control
	Client::state_client.tick_timer = fmod(Client::state_client.tick_timer, NET_TICK_RATE);
#endif
}

void term()
{
	reset();
	Sock::close(&sock);
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

	new (&state_common) StateCommon();
#if SERVER
#else
#endif
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

StreamWrite* msg_new(MessageType t)
{
	StreamWrite* result = state_common.msgs_out.add();
	result->reset();
	msg_serialize_type(result, t);
	return result;
}

StreamWrite local_packet;
StreamWrite* msg_new_local(MessageType t)
{
#if SERVER
	// we're the server; send out this message
	return msg_new(t);
#else
	// we're a client
	// so just process this message locally; don't send it
	local_packet.reset();
	msg_serialize_type(&local_packet, t);
	return &local_packet;
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
		case MessageType::Awk:
		{
			if (!Awk::net_msg(p, src))
				net_error();
			break;
		}
		case MessageType::PlayerControlHuman:
		{
			vi_assert(src == MessageSource::Loopback); // this is only executed on the client; Server::msg_process handles this on the server
			Ref<PlayerControlHuman> c;
			serialize_ref(p, c);
			if (!PlayerControlHuman::net_msg(p, c.ref(), src))
				net_error();
			break;
		}
		case MessageType::EnergyPickup:
		{
			if (!EnergyPickup::net_msg(p))
				net_error();
			break;
		}
		default:
		{
			vi_debug("Unknown message type: %d", s32(type));
			net_error();
			break;
		}
	}
	return true;
}


// after the server builds a message to send out, it also processes it locally
// Noop, EntityCreate, and EntityRemove messages are NOT processed locally, they
// only apply to the client
b8 msg_finalize(StreamWrite* p)
{
	using Stream = StreamRead;
	p->flush();
	StreamRead r;
	memcpy(&r, p, sizeof(StreamRead));
	r.rewind();
	MessageType type;
	serialize_enum(&r, MessageType, type);
	if (type != MessageType::Noop
		&& type != MessageType::EntityCreate
		&& type != MessageType::EntityRemove
		&& type != MessageType::InitDone
		&& type != MessageType::LoadingDone)
	{
		r.rewind();
		msg_process(&r, MessageSource::Loopback);
	}
	return true;
}

r32 rtt(const PlayerHuman* p)
{
	if (Game::level.local && p->local)
		return 0.0f;

#if SERVER
	const Server::Client* client = nullptr;
	for (s32 i = 0; i < Server::state_server.clients.length; i++)
	{
		const Server::Client& c = Server::state_server.clients[i];
		for (s32 j = 0; j < c.players.length; j++)
		{
			if (c.players[j].ref() == p)
			{
				client = &c;
				break;
			}
		}
		if (client)
			break;
	}

	if (client)
		return client->rtt;
	else
		return 0.0f;
#else
	return Client::state_client.server_rtt;
#endif
}

const StateFrame* state_frame_by_timestamp(r32 timestamp)
{
	return state_frame_by_timestamp(state_common.state_history, timestamp);
}


}


}
