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
#include "assimp/contrib/zlib/zlib.h"

#define DEBUG_MSG 0
#define DEBUG_ENTITY 0

namespace VI
{


namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

typedef u8 SequenceID;

#define TIMEOUT 2.0f
#define TICK_RATE (1.0f / 60.0f)
#define SEQUENCE_BITS 8
#define SEQUENCE_COUNT (1 << SEQUENCE_BITS)

#define MESSAGE_BUFFER s32(TIMEOUT / TICK_RATE)
#define MAX_MESSAGES_SIZE (MAX_PACKET_SIZE / 2)

enum class ClientPacket
{
	Connect,
	Update,
	AckInit,
	count,
};

enum class ServerPacket
{
	Init,
	Keepalive,
	Update,
	count,
};

enum class MessageType
{
	Noop,
	EntityCreate,
	EntityRemove,
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
	serialize_r32_range(p, v->color.x, -1.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.y, -1.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.z, -1.0f, 1.0f, 8);
	serialize_r32_range(p, v->color.w, -1.0f, 1.0f, 8);
	serialize_u16(p, v->mask);
	serialize_u16(p, v->mesh);
	serialize_asset(p, v->shader, Loader::shader_count);
	serialize_asset(p, v->texture, Loader::static_texture_count);
	serialize_u8(p, v->team);
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
		| SkinnedModel::component_mask
		| Projectile::component_mask
		| EnergyPickup::component_mask
		| Sensor::component_mask
		| Rocket::component_mask
		| ContainmentField::component_mask
		| Teleporter::component_mask;

	serialize_u64(p, e->component_mask);
	serialize_u16(p, e->revision);

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
		if (e->component_mask & (ComponentMask(1) << i) & mask)
		{
			serialize_int(p, ID, e->components[i], 0, MAX_ENTITIES - 1);
			ID component_id = e->components[i];
			Revision r;
			if (Stream::IsWriting)
				r = World::component_pools[i]->revision(component_id);
			serialize_u16(p, r);
			if (Stream::IsReading)
				World::component_pools[i]->net_add(component_id, e->id(), r);
		}
	}

	if (e->has<Transform>())
	{
		Transform* t = e->get<Transform>();
		serialize_r32(p, t->pos.x);
		serialize_r32(p, t->pos.y);
		serialize_r32(p, t->pos.z);
		serialize_r32(p, t->rot.x);
		serialize_r32(p, t->rot.y);
		serialize_r32(p, t->rot.z);
		serialize_r32(p, t->rot.w);
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
		serialize_u8(p, a->team);
		serialize_bool(p, a->stealth);
	}

	if (e->has<Awk>())
	{
	}

	if (e->has<LocalPlayerControl>())
	{
	}

	if (e->has<PlayerCommon>())
	{
	}

	if (e->has<MinionCommon>())
	{
	}

	if (e->has<Health>())
	{
		Health* h = e->get<Health>();
		serialize_r32_range(p, h->regen_timer, 0, 10, 8);
		serialize_u8(p, h->shield);
		serialize_u8(p, h->shield_max);
		serialize_u8(p, h->hp);
		serialize_u8(p, h->hp_max);
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
		serialize_u16(p, l->mask);
		serialize_u8(p, l->team);
	}

	if (e->has<SpotLight>())
	{
		SpotLight* l = e->get<SpotLight>();
		serialize_r32_range(p, l->color.x, 0, 1, 8);
		serialize_r32_range(p, l->color.y, 0, 1, 8);
		serialize_r32_range(p, l->color.z, 0, 1, 8);
		serialize_r32_range(p, l->radius, 0, 50, 8);
		serialize_r32_range(p, l->fov, 0, PI, 8);
		serialize_u16(p, l->mask);
		serialize_u8(p, l->team);
	}

	if (e->has<ControlPoint>())
	{
		ControlPoint* c = e->get<ControlPoint>();
		serialize_u8(p, c->team);
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
		EnergyPickup* h = e->get<EnergyPickup>();
		serialize_u8(p, h->team);
	}

	if (e->has<Sensor>())
	{
		Sensor* s = e->get<Sensor>();
		serialize_ref(p, s->owner);
		serialize_u8(p, s->team);
	}

	if (e->has<Rocket>())
	{
		Rocket* r = e->get<Rocket>();
		serialize_ref(p, r->target);
		serialize_ref(p, r->owner);
		serialize_u8(p, r->team);
	}

	if (e->has<ContainmentField>())
	{
		ContainmentField* c = e->get<ContainmentField>();
		serialize_r32(p, c->remaining_lifetime);
		serialize_ref(p, c->field);
		serialize_ref(p, c->owner);
		serialize_u8(p, c->team);
	}

	if (e->has<Teleporter>())
	{
		Teleporter* t = e->get<Teleporter>();
		serialize_u8(p, t->team);
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
		serialize_u16(p, w->mesh);
		serialize_asset(p, w->texture, Loader::static_texture_count);
	}

	if (e->has<DirectionalLight>())
	{
		DirectionalLight* d = e->get<DirectionalLight>();
		serialize_r32_range(p, d->color.x, -1.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.y, -1.0f, 1.0f, 8);
		serialize_r32_range(p, d->color.z, -1.0f, 1.0f, 8);
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

	if (Stream::IsReading)
		World::awake(e);

	return true;
}

template<typename Stream> b8 serialize_init_packet(Stream* p)
{
	serialize_u16(p, Game::level.id);
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
	serialize_r32(p, Game::level.skybox.sky_decal_fog_start);
	serialize_r32(p, Game::level.skybox.fog_start);
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

MessageHistory msgs_out_history;
StaticArray<StreamWrite, MESSAGE_BUFFER> msgs_out;
r32 tick_timer;
Sock::Handle sock;
SequenceID local_sequence_id = 1;

void packet_init(StreamWrite* p)
{
	p->bits(NET_PROTOCOL_ID, 32); // packet_send() will replace this with the packet checksum
}

void packet_finalize(StreamWrite* p)
{
	vi_assert(p->data[0] == NET_PROTOCOL_ID);
	p->flush();
	u32 checksum = crc32((const u8*)&p->data[0], sizeof(u32));
	checksum = crc32((const u8*)&p->data[1], (p->data.length - 1) * sizeof(u32), checksum);

	p->data[0] = checksum;

	StreamWrite compressed;
	compressed.resize_bytes(MAX_PACKET_SIZE);
	z_stream z;
	z.zalloc = nullptr;
	z.zfree = nullptr;
	z.opaque = nullptr;
	z.next_out = (Bytef*)compressed.data.data;
	z.avail_out = MAX_PACKET_SIZE;
	z.next_in = (Bytef*)p->data.data;
	z.avail_in = p->bytes_written();

	s32 result = deflateInit(&z, Z_DEFAULT_COMPRESSION);
	vi_assert(result == Z_OK);

	result = deflate(&z, Z_FINISH);

	vi_assert(result == Z_STREAM_END && z.avail_in == 0);

	result = deflateEnd(&z);
	vi_assert(result == Z_OK);

	p->reset();
	p->resize_bytes(MAX_PACKET_SIZE - z.avail_out);
	if (p->data.length > 0)
	{
		p->data[p->data.length - 1] = 0; // make sure everything gets zeroed out so the CRC32 comes out right
		memcpy(p->data.data, compressed.data.data, MAX_PACKET_SIZE - z.avail_out);
	}
}

void packet_decompress(StreamRead* p, s32 bytes)
{
	StreamRead decompressed;
	decompressed.resize_bytes(bytes);
	
	z_stream z;
	z.zalloc = nullptr;
	z.zfree = nullptr;
	z.opaque = nullptr;
	z.next_in = (Bytef*)p->data.data;
	z.avail_in = bytes;
	z.next_out = (Bytef*)decompressed.data.data;
	z.avail_out = MAX_PACKET_SIZE;

	s32 result = inflateInit(&z);
	vi_assert(result == Z_OK);
	
	result = inflate(&z, Z_NO_FLUSH);
	vi_assert(result == Z_STREAM_END);

	result = inflateEnd(&z);
	vi_assert(result == Z_OK);

	p->reset();
	p->resize_bytes(MAX_PACKET_SIZE - z.avail_out);
	if (p->data.length > 0)
	{
		p->data[p->data.length - 1] = 0; // make sure everything is zeroed out so the CRC32 comes out right
		memcpy(p->data.data, decompressed.data.data, MAX_PACKET_SIZE - z.avail_out);
	}
}

void packet_send(const StreamWrite& p, const Sock::Address& address)
{
	Sock::udp_send(&sock, address, p.data.data, p.data.length * sizeof(u32));
}

b8 msg_send_noop()
{
	using Stream = StreamWrite;

	StreamWrite* p = msg_new();
	MessageType type = MessageType::Noop;
	serialize_enum(p, MessageType, type);
	return true;
}

// consolidate msgs_out into msgs_out_history
b8 msgs_out_consolidate()
{
	using Stream = StreamWrite;

	if (msgs_out.length == 0)
		msg_send_noop(); // we have to send SOMETHING every sequence

	s32 bytes = 0;
	s32 msgs = 0;
	for (s32 i = 0; i < msgs_out.length; i++)
	{
		s32 msg_bytes = msgs_out[i].bytes_written();
		if (64 + bytes + msg_bytes > MAX_MESSAGES_SIZE)
			break;
		bytes += msg_bytes;
		msgs++;
	}

	MessageFrame* frame = msg_history_add(&msgs_out_history, Game::real_time.total, bytes);

	frame->sequence_id = local_sequence_id;

	serialize_int(&frame->write, s32, bytes, 0, MAX_MESSAGES_SIZE); // message frame size
	if (bytes > 0)
	{
		serialize_int(&frame->write, SequenceID, frame->sequence_id, 0, SEQUENCE_COUNT - 1);
		for (s32 i = 0; i < msgs; i++)
		{
			msgs_out[i].flush();
			serialize_bytes(&frame->write, (u8*)msgs_out[i].data.data, msgs_out[i].bytes_written());
		}
	}
	
	for (s32 i = msgs - 1; i >= 0; i--)
		msgs_out.remove_ordered(i);

	return true;
}

MessageFrame* msg_frame_advance(MessageHistory* history, SequenceID* id)
{
	if (history->msgs.length > 0)
	{
		s32 index = history->current_index;
		SequenceID next_sequence = sequence_advance(*id, 1);
		for (s32 i = 0; i < 64; i++)
		{
			MessageFrame* msg = &history->msgs[index];
			if (msg->sequence_id == next_sequence)
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
				serialize_bytes(p, (u8*)frame.write.data.data, frame.write.bytes_written());
		}
	}

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

	serialize_int(p, SequenceID, ack->sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack->previous_sequences);

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
		}
		else
			break;
	}

	return true;
}

#if SERVER

namespace Server
{

struct LocalPlayer;

struct Client
{
	Sock::Address address;
	r32 timeout;
	r32 rtt = 0.5f;
	Ack ack = { u32(-1), 0 }; // most recent ack we've received from the client
	MessageHistory msgs_in_history; // messages we've received from the client
	SequenceHistory recently_resent; // sequences we resent to the client recently
	SequenceID processed_sequence_id; // most recent sequence ID we've processed from the client
	Ref<LocalPlayer> player;
	b8 connected;
};

enum Mode
{
	Waiting,
	Active,
	count,
};

Array<Client> clients;
r32 tick_timer;
Mode mode;
s32 expected_clients = 1;

s32 connected_clients()
{
	s32 result = 0;
	for (s32 i = 0; i < clients.length; i++)
	{
		if (clients[i].connected)
			result++;
	}
	return result;
}

b8 init()
{
	if (Sock::udp_open(&sock, 3494, true))
	{
		printf("%s\n", Sock::get_error());
		return false;
	}

	Game::session.multiplayer = true;
	Game::schedule_load_level(Asset::Level::Tyche, Game::Mode::Pvp);

	return true;
}

b8 build_packet_init(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Init;
	serialize_enum(p, ServerPacket, type);
	serialize_init_packet(p);
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

b8 build_packet_update(StreamWrite* p, Client* client)
{
	packet_init(p);
	using Stream = StreamWrite;
	ServerPacket type = ServerPacket::Update;
	serialize_enum(p, ServerPacket, type);
	Ack ack = msg_history_ack(client->msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);
	msgs_write(p, msgs_out_history, client->ack, &client->recently_resent, client->rtt);
	packet_finalize(p);
	return true;
}

void update(const Update& u)
{
	for (s32 i = 0; i < clients.length; i++)
	{
		Client* client = &clients[i];
		if (client->connected)
		{
			while (const MessageFrame* frame = msg_frame_advance(&client->msgs_in_history, &client->processed_sequence_id))
			{
				// todo: process messages in frame
			}
		}
	}
#if DEBUG
	for (s32 i = 0; i < World::create_queue.length; i++)
		vi_assert(World::create_queue[i].ref()->finalized);
	World::create_queue.length = 0;
#endif
}

void tick(const Update& u)
{
	if (mode == Mode::Active)
		msgs_out_consolidate();
	StreamWrite p;
	for (s32 i = 0; i < clients.length; i++)
	{
		Client* client = &clients[i];
		client->timeout += Game::real_time.delta;
		if (client->timeout > TIMEOUT)
		{
			vi_debug("Client %s:%hd timed out.", Sock::host_to_str(client->address.host), client->address.port);
			clients.remove(i);
			i--;
		}
		else if (client->connected)
		{
			p.reset();
			build_packet_update(&p, client);
			packet_send(p, clients[i].address);
		}
	}

	if (mode == Mode::Active)
	{
		local_sequence_id++;
		if (local_sequence_id == SEQUENCE_COUNT)
			local_sequence_id = 0;
	}
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
	Client* client = nullptr;
	for (s32 i = 0; i < clients.length; i++)
	{
		if (address.equals(clients[i].address))
		{
			client = &clients[i];
			break;
		}
	}

	using Stream = StreamRead;
	if (!p->read_checksum())
	{
		vi_debug("Discarding packet for invalid checksum.");
		return false;
	}

	ClientPacket type;
	serialize_enum(p, ClientPacket, type);

	switch (type)
	{
		case ClientPacket::Connect:
		{
			if (clients.length < expected_clients)
			{
				Client* client = nullptr;
				for (s32 i = 0; i < clients.length; i++)
				{
					if (clients[i].address.equals(address))
					{
						client = &clients[i];
						break;
					}
				}

				if (!client)
				{
					client = clients.add();
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
			Client* client = nullptr;
			for (s32 i = 0; i < clients.length; i++)
			{
				if (clients[i].address.equals(address))
				{
					client = &clients[i];
					break;
				}
			}

			if (client && !client->connected)
			{
				vi_debug("Client %s:%hd connected.", Sock::host_to_str(address.host), address.port);
				client->connected = true;
			}

			if (connected_clients() == expected_clients)
			{
				mode = Mode::Active;
				using Stream = StreamWrite;
				for (auto i = Entity::list.iterator(); !i.is_last(); i.next())
				{
					StreamWrite* p = msg_new();
					MessageType type = MessageType::EntityCreate;
					serialize_enum(p, MessageType, type);
					serialize_int(p, ID, i.index, 0, MAX_ENTITIES - 1);
					serialize_entity(p, i.item());
				}
			}
			break;
		}
		case ClientPacket::Update:
		{
			if (!client)
			{
				vi_debug("Discarding packet from unknown client.");
				return false;
			}

			if (!msgs_read(p, &client->msgs_in_history, &client->ack))
				return false;
			calculate_rtt(Game::real_time.total, client->ack, msgs_out_history, &client->rtt);

			client->timeout = 0.0f;
			break;
		}
		default:
		{
			vi_debug("Discarding packet due to invalid packet type.");
			return false;
		}
	}

	return true;
}

}

#else

namespace Client
{

enum class Mode
{
	Disconnected,
	Connecting,
	Acking,
	Connected,
};

Sock::Address server_address;
Mode mode;
r32 timeout;
MessageHistory msgs_in_history; // messages we've received from the server
Ack server_ack = { u32(-1), 0 }; // most recent ack we've received from the server
SequenceHistory server_recently_resent; // sequences we recently resent to the server
r32 server_rtt = 0.5f;
SequenceID server_processed_sequence_id; // most recent sequence ID we've processed from the server

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

b8 build_packet_ack_init(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::AckInit;
	serialize_enum(p, ClientPacket, type);
	packet_finalize(p);
	return true;
}

b8 build_packet_update(StreamWrite* p)
{
	packet_init(p);
	using Stream = StreamWrite;
	ClientPacket type = ClientPacket::Update;
	serialize_enum(p, ClientPacket, type);

	// ack received messages
	Ack ack = msg_history_ack(msgs_in_history);
	serialize_int(p, SequenceID, ack.sequence_id, 0, SEQUENCE_COUNT - 1);
	serialize_u32(p, ack.previous_sequences);

	msgs_write(p, msgs_out_history, server_ack, &server_recently_resent, server_rtt);
	packet_finalize(p);
	return true;
}

b8 msg_process(StreamRead* p)
{
	using Stream = StreamRead;
	MessageType type;
	serialize_enum(p, MessageType, type);
#if DEBUG_MSG
	vi_debug("Processing message type %d", type);
#endif
	switch (type)
	{
		case MessageType::Noop:
		{
			break;
		}
		case MessageType::EntityCreate:
		{
			ID id;
			serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
			Entity::list.active(id, true);
			Entity::list.free_list.length--;
			Entity* e = &Entity::list[id];
			if (!serialize_entity(p, e))
				net_error();
			break;
		}
		case MessageType::EntityRemove:
		{
			// todo
			vi_assert(false);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	serialize_align(p);
	return true;
}

void update(const Update& u)
{
	while (MessageFrame* frame = msg_frame_advance(&msgs_in_history, &server_processed_sequence_id))
	{
		frame->read.rewind();
		while (frame->read.bytes_read() < frame->bytes)
		{
			b8 success = msg_process(&frame->read);
#if DEBUG_MSG
			vi_debug("Read %d/%d bytes", frame->read.bytes_read(), frame->bytes);
#endif
			if (!success)
				break;
		}
	}
}

void tick(const Update& u)
{
	timeout += Game::real_time.delta;
	switch (mode)
	{
		case Mode::Disconnected:
		{
			break;
		}
		case Mode::Connecting:
		{
			if (timeout > 0.25f)
			{
				timeout = 0.0f;
				vi_debug("Connecting to %s:%hd...", Sock::host_to_str(server_address.host), server_address.port);
				StreamWrite p;
				build_packet_connect(&p);
				packet_send(p, server_address);
			}
			break;
		}
		case Mode::Acking:
		{
			if (timeout > 0.25f)
			{
				timeout = 0.0f;
				vi_debug("Confirming connection to %s:%hd...", Sock::host_to_str(server_address.host), server_address.port);
				StreamWrite p;
				build_packet_ack_init(&p);
				packet_send(p, server_address);
			}
			break;
		}
		case Mode::Connected:
		{
			if (timeout > TIMEOUT)
			{
				vi_debug("Lost connection to %s:%hd.", Sock::host_to_str(server_address.host), server_address.port);
				mode = Mode::Disconnected;
			}
			else
			{
				msgs_out_consolidate();

				StreamWrite p;
				build_packet_update(&p);
				packet_send(p, server_address);

				local_sequence_id++;
				if (local_sequence_id == SEQUENCE_COUNT)
					local_sequence_id = 0;
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
	Sock::get_address(&server_address, ip, port);
	mode = Mode::Connecting;
}

b8 packet_handle(const Update& u, StreamRead* p, const Sock::Address& address)
{
	using Stream = StreamRead;
	if (!address.equals(server_address))
	{
		vi_debug("%s", "Discarding packet from unexpected host.");
		return false;
	}
	if (!p->read_checksum())
	{
		vi_debug("%s", "Discarding packet due to invalid checksum.");
		return false;
	}
	ServerPacket type;
	serialize_enum(p, ServerPacket, type);
	switch (type)
	{
		case ServerPacket::Init:
		{
			if (mode == Mode::Connecting && serialize_init_packet(p))
			{
				Camera* camera = Camera::add();
				camera->viewport =
				{
					Vec2(0, 0),
					Vec2(u.input->width, u.input->height),
				};
				r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
				camera->perspective((40.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);
				camera->pos = Vec3(0, 30, -60);
				camera->rot = Quat::look(Vec3(0, 0, 1));

				mode = Mode::Acking; // acknowledge the init packet
			}
			break;
		}
		case ServerPacket::Keepalive:
		{
			timeout = 0.0f; // reset connection timeout
			break;
		}
		case ServerPacket::Update:
		{
			if (mode == Mode::Acking)
			{
				vi_debug("Connected to %s:%hd.", Sock::host_to_str(server_address.host), server_address.port);
				mode = Mode::Connected;
			}

			if (!msgs_read(p, &msgs_in_history, &server_ack))
				return false;
			calculate_rtt(Game::real_time.total, server_ack, msgs_out_history, &server_rtt);
			timeout = 0.0f; // reset connection timeout
			break;
		}
		default:
		{
			vi_debug("%s", "Discarding packet due to invalid packet type.");
			return false;
		}
	}

	return true;
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

void finalize(Entity* e)
{
#if SERVER
	if (Server::clients.length == Server::expected_clients)
	{
		using Stream = StreamWrite;
		StreamWrite* p = msg_new();
		MessageType type = MessageType::EntityCreate;
		serialize_enum(p, MessageType, type);
		ID id = e->id();
		serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
		serialize_entity(p, e);
	}
#if DEBUG
	e->finalized = true;
#endif
#endif
}

void remove(Entity* e)
{
#if SERVER
	using Stream = StreamWrite;
	StreamWrite* p = msg_new();
	MessageType type = MessageType::EntityRemove;
	serialize_enum(p, MessageType, type);
	ID id = e->id();
	serialize_int(p, ID, id, 0, MAX_ENTITIES - 1);
#endif
}

void update(const Update& u)
{
	while (true)
	{
		Sock::Address address;
		StreamRead incoming_packet;
		s32 bytes_received = Sock::udp_receive(&sock, &address, incoming_packet.data.data, MAX_PACKET_SIZE);
		if (bytes_received > 0)
		{
			//if (mersenne::randf_co() < 0.75f) // packet loss simulation
			{
				incoming_packet.resize_bytes(bytes_received);

				packet_decompress(&incoming_packet, bytes_received);
#if SERVER
				Server::packet_handle(u, &incoming_packet, address);
#else
				Client::packet_handle(u, &incoming_packet, address);
#endif
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

	tick_timer += Game::real_time.delta;
	if (tick_timer > TICK_RATE)
	{
		tick_timer = 0.0f;

#if SERVER
		Server::tick(u);
#else
		Client::tick(u);
#endif
	}
}

void term()
{
	Sock::close(&sock);
}

StreamWrite* msg_new()
{
	StreamWrite* result = msgs_out.add();
	result->reset();
	return result;
}


}


}