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
#if DEBUG
	#include <AK/Comm/AkCommunication.h>
#endif

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
Array<AkGameObjectID> Audio::unregister_queue;
r32 Audio::dialogue_volume;
s8 Audio::listener_mask;

#if SERVER
b8 Audio::init() { return true; }
void Audio::term() {}
void Audio::update() {}
void Audio::post_global_event(AkUniqueID) {}
b8 Audio::post_dialogue_event(AkUniqueID) { return true; }
void Audio::post_global_event(AkUniqueID, const Vec3&) {}
void Audio::post_global_event(AkUniqueID, const Vec3&, const Quat&) {}
void Audio::global_param(AkRtpcID, AkRtpcValue) {}
void Audio::listener_enable(s8) {}
void Audio::listener_disable(s8) {}
void Audio::listener_update(s8, const Vec3&, const Quat&) {}
AkUniqueID Audio::get_id(const char*) { return 0; }

void Audio::awake() {}
Audio::~Audio() {}
void Audio::post_event(AkUniqueID event_id) {}
void Audio::param(AkRtpcID id, AkRtpcValue value) {}

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

	// Create the Stream Manager.
	AkStreamMgrSettings stmSettings;
	AK::StreamMgr::GetDefaultSettings(stmSettings);
	AK::StreamMgr::Create(stmSettings);

	// Create a streaming device with blocking low-level I/O handshaking.
	AkDeviceSettings deviceSettings;
	AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
	deviceSettings.uSchedulerTypeFlags = AK_SCHEDULER_BLOCKING;

	// Init registers lowLevelIO as the File Location Resolver if it was not already defined, and creates a streaming device.
	if (!wwise_io.Init(deviceSettings))
	{
		fprintf(stderr, "Failed to create the Wwise streaming device and low-level IO system.\n");
		return false;
	}

#if !_WIN32
	wwise_io.SetBasePath("./");
#endif
	
	AkInitSettings initSettings;
	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultInitSettings(initSettings);
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformInitSettings);

	if (AK::SoundEngine::Init(&initSettings, &platformInitSettings) != AK_Success)
	{
		fprintf(stderr, "Failed to initialize the Wwise sound engine.\n");
		return false;
	}

	AkMusicSettings musicInit;
	AK::MusicEngine::GetDefaultInitSettings(musicInit);
		
	if (AK::MusicEngine::Init(&musicInit) != AK_Success)
	{
		fprintf(stderr, "Failed to initialize the Wwise music engine.\n");
		return false;
	}

#if DEBUG
	AkCommSettings commSettings;
	AK::Comm::GetDefaultInitSettings(commSettings);
	if (AK::Comm::Init(commSettings) != AK_Success)
	{
		fprintf(stderr, "Failed to initialize Wwise communication.\n");
		return false;
	}
#endif

	// game object for global events
	AK::SoundEngine::RegisterGameObj(MAX_ENTITIES);

	// game objects for listeners
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		AK::SoundEngine::RegisterGameObj(listener_id(i));

	AK::SoundEngine::RegisterBusMeteringCallback(AK::BUSSES::DIALOGUE, Audio::dialogue_volume_callback, AkMeteringFlags(AK_EnableBusMeter_Peak));

	global_param(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER);
	global_param(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER);

	return true;
}

AkGameObjectID Audio::listener_id(s8 gamepad)
{
	return MAX_ENTITIES + 2 + gamepad;
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

void Audio::update()
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		Vec3 pos = i.item()->offset;
		Quat rot = Quat::identity;
		i.item()->get<Transform>()->to_world(&pos, &rot);

		AkSoundPosition sound_position;
		sound_position.SetPosition(pos.x, pos.y, pos.z);
		Vec3 forward = rot * Vec3(0, 0, -1.0f);
		Vec3 up = rot * Vec3(0, 1.0f, 0.0f);
		sound_position.SetOrientation(forward.x, forward.y, forward.z, up.x, up.y, up.z);
		AK::SoundEngine::SetPosition(i.item()->entity_id, sound_position);
	}
	for (s32 i = 0; i < unregister_queue.length; i++)
		AK::SoundEngine::UnregisterGameObj(unregister_queue[i]);
	unregister_queue.length = 0;
	AK::SoundEngine::RenderAudio();
}

AkUniqueID Audio::get_id(const char* str)
{
	return AK::SoundEngine::GetIDFromString(str);
}

void Audio::post_global_event(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, MAX_ENTITIES);
}

b8 Audio::post_dialogue_event(AkUniqueID event_id)
{
	AkPlayingID id = AK::SoundEngine::PostEvent(event_id, entity_id, AkCallbackType::AK_EndOfEvent, &dialogue_done_callback);
	return id != 0;
}

void Audio::post_global_event(AkUniqueID event_id, const Vec3& pos)
{
	post_global_event(event_id, pos, Quat::identity);
}

void Audio::post_global_event(AkUniqueID event_id, const Vec3& pos, const Quat& orientation)
{
	AkGameObjectID id = MAX_ENTITIES + 1;
	AK::SoundEngine::RegisterGameObj(id);

	AkSoundPosition sound_position;
	sound_position.SetPosition(pos.x, pos.y, pos.z);
	Vec3 forward = orientation * Vec3(0, 0, -1.0f);
	Vec3 up = orientation * Vec3(0, 1.0f, 0.0f);
	sound_position.SetOrientation(forward.x, forward.y, forward.z, up.x, up.y, up.z);
	AK::SoundEngine::SetPosition(id, sound_position);

	AK::SoundEngine::PostEvent(event_id, id);
	AK::SoundEngine::UnregisterGameObj(id);
}

void Audio::global_param(AkRtpcID id, AkRtpcValue value)
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
	AkListenerPosition listener_position;
	listener_position.SetPosition(pos.x, pos.y, pos.z);
	Vec3 forward = rot * Vec3(0, 0, -1);
	Vec3 up = rot * Vec3(0, 1, 0);
	listener_position.SetOrientation(forward.x, forward.y, forward.z, up.x, up.y, up.z);
	AK::SoundEngine::SetPosition(listener_id(gamepad), listener_position);
}

void Audio::awake()
{
	AK::SoundEngine::RegisterGameObj(entity_id);
}

Audio::~Audio()
{
	unregister_queue.add(entity_id);
}

void Audio::post_event(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, entity_id);
}

void Audio::param(AkRtpcID id, AkRtpcValue value)
{
	AK::SoundEngine::SetRTPCValue(id, value, entity_id);
}

#endif

}
