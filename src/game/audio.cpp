#include "audio.h"
#include "asset/soundbank.h"
#include "asset/lookup.h"
#include "data/components.h"
#include "load.h"

#include <stdio.h>
#include "asset/Wwise_IDs.h"
#include "settings.h"

#if !SERVER
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/AkMeterFXFactory.h>
#include <AK/Plugin/AkRoomVerbFXFactory.h>
#include <AK/Plugin/AkDelayFXFactory.h>
#include "physics.h"
#if DEBUG
	#include <AK/Comm/AkCommunication.h>
#endif
#include "ai.h"

namespace AK
{
	void* AllocHook(size_t in_size)
	{
		return calloc(1, in_size);
	}

	void FreeHook(void* in_ptr)
	{
		free(in_ptr);
	}

#if _WIN32
	void * VirtualAllocHook(
		void * in_pMemAddress,
		size_t in_size,
		DWORD in_dwAllocationType,
		DWORD in_dwProtect
		)
	{
		return VirtualAlloc(in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect);
	}
	void VirtualFreeHook(
		void * in_pMemAddress,
		size_t in_size,
		DWORD in_dwFreeType
		)
	{
		VirtualFree(in_pMemAddress, in_size, in_dwFreeType);
	}
#endif
}
#endif

namespace VI
{

StaticArray<ID, 32> Audio::dialogue_callbacks;
r32 Audio::dialogue_volume;
s8 Audio::listener_mask;
Vec3 Audio::listener_pos[MAX_GAMEPADS];
PinArray<Audio::Entry, MAX_ENTITIES> Audio::Entry::list;

#if SERVER
b8 Audio::init() { return true; }
void Audio::term() {}
void Audio::update_all(const Update&) {}
void Audio::post_global(AkUniqueID) {}
Audio::Entry* Audio::post_global(AkUniqueID, const Vec3&) { return nullptr; }
void Audio::param_global(AkRtpcID, AkRtpcValue) {}
void Audio::listener_enable(s8) {}
void Audio::listener_disable(s8) {}
void Audio::listener_update(s8, const Vec3&, const Quat&) {}
AkUniqueID Audio::get_id(const char*) { return 0; }
void Audio::clear() {}

void Audio::Entry::init(const Vec3&, Transform*, Entry*) {}
void Audio::Entry::cleanup() {}
void Audio::Entry::update(r32) {}
void Audio::Entry::update_obstruction_occlusion() {}
void Audio::Entry::post(AkUniqueID) {}
void Audio::Entry::stop(AkUniqueID) {}
void Audio::Entry::stop_all() {}
b8 Audio::Entry::post_dialogue(AkUniqueID) { return false; }
void Audio::Entry::param(AkRtpcID, AkRtpcValue) {}

void Audio::awake() {}
Audio::~Audio() {}
void Audio::post(AkUniqueID) {}
Audio::Entry* Audio::post_unattached(AkUniqueID, const Vec3&) { return nullptr; }
Audio::Entry* Audio::post_offset(AkUniqueID, const Vec3&) { return nullptr; }
b8 Audio::post_dialogue(AkUniqueID) { return false; }
void Audio::stop(AkUniqueID) {}
void Audio::stop_all() {}
void Audio::param(AkRtpcID, AkRtpcValue) {}
void Audio::offset(const Vec3&) {}

#else

CAkDefaultIOHookBlocking Audio::wwise_io;

b8 Audio::init()
{
	AkMemSettings memSettings;
	memSettings.uMaxNumPools = 20;

	if (AK::MemoryMgr::Init(&memSettings) != AK_Success)
	{
		fprintf(stderr, "Failed to create the Wwise memory manager.\n");
		return false;
	}

	// create the Stream Manager.
	{
		AkStreamMgrSettings stream_settings;
		AK::StreamMgr::GetDefaultSettings(stream_settings);
		AK::StreamMgr::Create(stream_settings);
	}

	// create a streaming device with blocking low-level I/O handshaking.
	{
		AkDeviceSettings device_settings;
		AK::StreamMgr::GetDefaultDeviceSettings(device_settings);
		device_settings.uSchedulerTypeFlags = AK_SCHEDULER_BLOCKING;

		// init registers lowLevelIO as the File Location Resolver if it was not already defined, and creates a streaming device.
		if (!wwise_io.Init(device_settings))
		{
			fprintf(stderr, "Failed to create the Wwise streaming device and low-level IO system.\n");
			return false;
		}
	}

#if !_WIN32
	wwise_io.SetBasePath("./");
#endif
	
	{
		AkInitSettings init_settings;
		AkPlatformInitSettings platform_init_settings;
		AK::SoundEngine::GetDefaultInitSettings(init_settings);
		AK::SoundEngine::GetDefaultPlatformInitSettings(platform_init_settings);

		if (AK::SoundEngine::Init(&init_settings, &platform_init_settings) != AK_Success)
		{
			fprintf(stderr, "Failed to initialize the Wwise sound engine.\n");
			return false;
		}
	}

	{
		AkMusicSettings music_settings;
		AK::MusicEngine::GetDefaultInitSettings(music_settings);

		if (AK::MusicEngine::Init(&music_settings) != AK_Success)
		{
			fprintf(stderr, "Failed to initialize the Wwise music engine.\n");
			return false;
		}
	}

#if DEBUG
	{
		AkCommSettings comm_settings;
		AK::Comm::GetDefaultInitSettings(comm_settings);
		if (AK::Comm::Init(comm_settings) != AK_Success)
		{
			fprintf(stderr, "Failed to initialize Wwise communication.\n");
			return false;
		}
	}
#endif

	// game object for global 2D events
	AK::SoundEngine::RegisterGameObj(AUDIO_OFFSET_GLOBAL_2D);

	// game objects for listeners
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		AK::SoundEngine::RegisterGameObj(listener_id(i));

	AK::SoundEngine::RegisterBusMeteringCallback(AK::BUSSES::DIALOGUE, Audio::dialogue_volume_callback, AkMeteringFlags(AK_EnableBusMeter_Peak));

	param_global(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER);
	param_global(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER);

	return true;
}

AkGameObjectID Audio::listener_id(s8 gamepad)
{
	return AUDIO_OFFSET_LISTENERS + gamepad;
}

// Wwise callbacks
void Audio::dialogue_volume_callback(AK::IAkMetering* metering, AkChannelConfig channel_config, AkMeteringFlags flags)
{
	r32 sum = 0.0f;
	AK::SpeakerVolumes::ConstVectorPtr peak = metering->GetPeak();
	for (s32 i = 0; i < channel_config.uNumChannels; i++)
		sum += peak[i];
	dialogue_volume = sum;
}

void Audio::dialogue_done_callback(AkCallbackType type, AkCallbackInfo* info)
{
	// anyone paying attention to these should be polling them every frame;
	// if they're not, we don't care which ones get dropped
	if (dialogue_callbacks.length == dialogue_callbacks.capacity())
		dialogue_callbacks.length--;

	dialogue_callbacks.add(ID(info->gameObjID));
}

void Audio::term()
{
#if DEBUG
	AK::Comm::Term();
#endif
	AK::MusicEngine::Term();
	AK::SoundEngine::Term();
	wwise_io.Term();
	if (AK::IAkStreamMgr::Get())
		AK::IAkStreamMgr::Get()->Destroy();
	AK::MemoryMgr::Term();
}

#define MAX_IMAGE_SOURCES 4

void Audio::Entry::init(const Vec3& npos, Transform* nparent, Entry* parent_entry)
{
	pos = npos;
	parent = nparent;
	if (nparent)
		abs_pos = nparent->absolute_pos() + npos;
	else
		abs_pos = pos;
	playing = 0;
	keepalive = false;

	AK::SoundEngine::RegisterGameObj(ak_id());

	AkAuxSendValue value;
	value.auxBusID = AK::AUX_BUSSES::REVERB_DEFAULT;
	value.fControlValue = 1.0f;
	value.listenerID = AK_InvalidID;
	AK::SoundEngine::SetGameObjectAuxSendValues(ak_id(), &value, 1);

	if (parent_entry)
	{
		memcpy(obstruction, parent_entry->obstruction, sizeof(obstruction));
		memcpy(obstruction_target, parent_entry->obstruction_target, sizeof(obstruction_target));
		memcpy(occlusion, parent_entry->occlusion, sizeof(occlusion));
		memcpy(occlusion_target, parent_entry->occlusion_target, sizeof(occlusion_target));
		update();

		obstruction_occlusion_frame = parent_entry->obstruction_occlusion_frame;
	}
	else
	{
		update_obstruction_occlusion();
		memcpy(obstruction, obstruction_target, sizeof(obstruction));
		memcpy(occlusion, occlusion_target, sizeof(occlusion));
		update();

		obstruction_occlusion_frame = Audio::obstruction_occlusion_frame;
	}
}

void Audio::Entry::cleanup()
{
	AK::SoundEngine::UnregisterGameObj(ak_id());
}

s16 Audio::obstruction_occlusion_frame;
void Audio::update_all(const Update& u)
{
	// update obstruction and occlusion of the first n entries that we haven't updated yet
	{
		s32 occlusion_updates = 12;

		for (auto i = Entry::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->keepalive || i.item()->playing > 0) // Audio component is keeping it alive, or something is playing on it
			{
				if (occlusion_updates > 0
					&& i.item()->obstruction_occlusion_frame != obstruction_occlusion_frame)
				{
					i.item()->update_obstruction_occlusion();
					occlusion_updates--;
				}
				i.item()->update(u.real_time.delta);
			}
			else
			{
				i.item()->cleanup();
				Entry::list.remove(i.index);
			}
		}

		if (occlusion_updates > 0) // we updated all entries
			obstruction_occlusion_frame = (obstruction_occlusion_frame + 1) & s16((1 << 15) - 1); // increment to next frame so that all entries are marked as needing updated
	}

	AK::SoundEngine::RenderAudio();
}

void Audio::Entry::update_obstruction_occlusion()
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (listener_mask & (1 << i))
		{
			const Vec3& listener = listener_pos[i];

			Vec3 dir = abs_pos - listener;
			r32 distance = dir.length();
			if (distance > 0.0f)
			{
				dir /= distance;

				btCollisionWorld::ClosestRayResultCallback ray_callback(listener, listener + dir * vi_max(0.1f, distance - 0.5f));
				Physics::raycast(&ray_callback, CollisionAudio);
				if (ray_callback.hasHit())
				{
					obstruction_target[i] = 1.0f;
					r32 path_distance = AI::audio_pathfind(ray_callback.m_rayFromWorld, ray_callback.m_rayToWorld);
					occlusion_target[i] = vi_max(0.0f, vi_min(1.0f, (path_distance - distance) / (DRONE_MAX_DISTANCE * 0.75f)));
				}
				else
				{
					obstruction_target[i] = 0.0f;
					occlusion_target[i] = 0.0f;
				}
			}
			else
			{
				obstruction_target[i] = 0.0f;
				occlusion_target[i] = 0.0f;
			}
		}
	}
	obstruction_occlusion_frame = Audio::obstruction_occlusion_frame;
}

AkUniqueID Audio::get_id(const char* str)
{
	return AK::SoundEngine::GetIDFromString(str);
}

void Audio::post_global(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, AUDIO_OFFSET_GLOBAL_2D);
}

b8 Audio::post_dialogue(AkUniqueID event_id)
{
	return Entry::list[entry_id].post_dialogue(event_id);
}

Audio::Entry* Audio::post_global(AkUniqueID event_id, const Vec3& pos)
{
	Entry* e = Entry::list.add();
	e->init(pos, nullptr, nullptr);
	e->post(event_id);
	return e;
}

void Audio::param_global(AkRtpcID id, AkRtpcValue value)
{
	AK::SoundEngine::SetRTPCValue(id, value);
}

void Audio::listener_list_update()
{
	AkGameObjectID listener_ids[MAX_GAMEPADS];
	s32 count = 0;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (listener_mask & (1 << i))
		{
			listener_ids[count] = listener_id(i);
			count++;
		}
	}
	AK::SoundEngine::SetDefaultListeners(listener_ids, count);
}

void Audio::listener_disable(s8 gamepad)
{
	listener_mask &= ~(1 << gamepad);
	listener_list_update();
}

void Audio::listener_enable(s8 gamepad)
{
	listener_mask |= (1 << gamepad);
	listener_list_update();
}

void Audio::listener_update(s8 gamepad, const Vec3& pos, const Quat& rot)
{
	listener_pos[gamepad] = pos;
	AkListenerPosition listener_position;
	listener_position.SetPosition(pos.x, pos.y, pos.z);
	Vec3 forward = rot * Vec3(0, 0, -1);
	Vec3 up = rot * Vec3(0, 1, 0);
	listener_position.SetOrientation(forward.x, forward.y, forward.z, up.x, up.y, up.z);
	AK::SoundEngine::SetPosition(listener_id(gamepad), listener_position);
}

void Audio::awake()
{
	Entry* e = Entry::list.add();
	entry_id = e->id();
	e->init(Vec3::zero, get<Transform>());
	e->keepalive = true;
}

Audio::~Audio()
{
	Entry::list[entry_id].keepalive = false;
}

void Audio::clear()
{
	Audio::post_global(AK::EVENTS::STOP_ALL);

	for (auto i = Entry::list.iterator(); !i.is_last(); i.next())
		i.item()->cleanup();
	Entry::list.clear();
}

void Audio::Entry::update(r32 dt)
{
	if (parent.ref())
		abs_pos = pos + parent.ref()->absolute_pos();

	const r32 delta = dt * (1.0f / 0.3f); // takes X seconds to lerp to the new value
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (Audio::listener_mask & (1 << i))
		{
			if (obstruction_target[i] > obstruction[i])
				obstruction[i] = vi_min(obstruction_target[i], obstruction[i] + delta);
			else
				obstruction[i] = vi_max(obstruction_target[i], obstruction[i] - delta);
			if (occlusion_target[i] > occlusion[i])
				occlusion[i] = vi_min(occlusion_target[i], occlusion[i] + delta);
			else
				occlusion[i] = vi_max(occlusion_target[i], occlusion[i] - delta);
			AK::SoundEngine::SetObjectObstructionAndOcclusion(ak_id(), Audio::listener_id(i), obstruction[i], occlusion[i]);
		}
	}

	AkSoundPosition sound_position;
	sound_position.SetPosition(abs_pos.x, abs_pos.y, abs_pos.z);
	sound_position.SetOrientation(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
	AK::SoundEngine::SetPosition(ak_id(), sound_position);
}

Audio::Entry* Audio::Entry::by_ak_id(AkGameObjectID id)
{
	return &list[id - AUDIO_OFFSET_ENTRIES];
}

#if !SERVER
void Audio::event_done_callback(AkCallbackType type, AkCallbackInfo* info)
{
	Entry::by_ak_id(info->gameObjID)->playing--;
}
#endif

void Audio::Entry::post(AkUniqueID event_id)
{
	AkPlayingID i = AK::SoundEngine::PostEvent(event_id, ak_id(), AkCallbackType::AK_EndOfEvent, &event_done_callback);
	if (i != 0)
		playing++;
}

void Audio::Entry::stop(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, ak_id());
	playing--;
}

void Audio::Entry::stop_all()
{
	AK::SoundEngine::PostEvent(AK::EVENTS::STOP, ak_id());
	playing = 0;
}

b8 Audio::Entry::post_dialogue(AkUniqueID event_id)
{
	AkPlayingID i = AK::SoundEngine::PostEvent(event_id, ak_id(), AkCallbackType::AK_EndOfEvent, &dialogue_done_callback);
	if (i != 0)
	{
		playing++;
		return true;
	}
	else
		return false;
}

void Audio::Entry::param(AkRtpcID id, AkRtpcValue value)
{
	AK::SoundEngine::SetRTPCValue(id, value, ak_id());
}

void Audio::post(AkUniqueID event_id)
{
	Entry::list[entry_id].post(event_id);
}

void Audio::stop(AkUniqueID event_id)
{
	Entry::list[entry_id].stop(event_id);
}

void Audio::stop_all()
{
	Entry::list[entry_id].stop_all();
}

Audio::Entry* Audio::post_unattached(AkUniqueID event_id, const Vec3& pos)
{
	Entry* e = Entry::list.add();

	e->init(pos + get<Transform>()->absolute_pos(), nullptr, &Entry::list[entry_id]);
	e->post(event_id);
	return e;
}

Audio::Entry* Audio::post_offset(AkUniqueID event_id, const Vec3& offset)
{
	Entry* e = Entry::list.add();
	e->init(offset, get<Transform>(), &Entry::list[entry_id]);
	e->post(event_id);
	return e;
}

void Audio::param(AkRtpcID id, AkRtpcValue value)
{
	Entry::list[entry_id].param(id, value);
}

void Audio::offset(const Vec3& offset)
{
	Entry::list[entry_id].pos = offset;
}

#endif

}