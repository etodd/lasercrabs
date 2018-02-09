#include "audio.h"
#include "asset/soundbank.h"
#include "asset/lookup.h"
#include "data/components.h"
#include "load.h"

#include <stdio.h>
#include "asset/Wwise_IDs.h"
#include "settings.h"
#include "game/entities.h"
#include "game/team.h"

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
#include <AK/Plugin/AkSynthOneSourceFactory.h>
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
Audio::Listener Audio::listener[MAX_GAMEPADS];
PinArray<AudioEntry, MAX_ENTITIES> AudioEntry::list;
r32 Audio::volume_scale = 1.0f;

#if SERVER
b8 Audio::init() { return true; }
void Audio::term() {}
void Audio::update_all(const Update&) {}
void Audio::post_global(AkUniqueID) {}
b8 Audio::post_global_dialogue(AkUniqueID) { return false; }
AudioEntry* Audio::post_global(AkUniqueID, const Vec3&, Transform*, s32) { return nullptr; }
void Audio::param_global(AkRtpcID, AkRtpcValue) {}
void Audio::listener_enable(s8, AI::Team) {}
void Audio::listener_disable(s8) {}
void Audio::listener_update(s8, const Vec3&, const Quat&) {}
AkUniqueID Audio::get_id(const char*) { return 0; }
void Audio::clear() {}

void AudioEntry::init(const Vec3&, Transform*, AudioEntry*, s32) {}
void AudioEntry::cleanup() {}
void AudioEntry::update(r32) {}
void AudioEntry::update_spatialization(UpdateType) {}
void AudioEntry::post(AkUniqueID) {}
void AudioEntry::stop(AkUniqueID) {}
void AudioEntry::stop_all() {}
void AudioEntry::pathfind_result(s8, r32, r32) {}
b8 AudioEntry::post_dialogue(AkUniqueID) { return false; }
void AudioEntry::param(AkRtpcID, AkRtpcValue) {}
void AudioEntry::flag(s32, b8) {}

void Audio::awake() {}
Audio::~Audio() {}
void Audio::post(AkUniqueID) {}
AudioEntry* Audio::entry() const { return nullptr; }
AudioEntry* Audio::post_unattached(AkUniqueID, const Vec3&) { return nullptr; }
AudioEntry* Audio::post_offset(AkUniqueID, const Vec3&) { return nullptr; }
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
#ifdef __linux__
		// disable 5.1 surround on linux
		// because for some reason it thinks headphones can do 5.1
		init_settings.settingsMainOutput.channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
#endif

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

	AK::SoundEngine::RegisterBusMeteringCallback(AK::BUSSES::DIALOGUE, AudioEntry::dialogue_volume_callback, AkMeteringFlags(AK_EnableBusMeter_Peak));

	param_global(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER * volume_scale);
	param_global(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER * volume_scale);

	return true;
}

void Audio::volume_multiplier(r32 v)
{
	volume_scale = v;

	param_global(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER * v);
	param_global(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER * v);
}

AkGameObjectID Audio::listener_id(s8 gamepad)
{
	return AUDIO_OFFSET_LISTENERS + gamepad;
}

// Wwise callbacks
void AudioEntry::dialogue_volume_callback(AK::IAkMetering* metering, AkChannelConfig channel_config, AkMeteringFlags flags)
{
	r32 sum = 0.0f;
	AK::SpeakerVolumes::ConstVectorPtr peak = metering->GetPeak();
	for (s32 i = 0; i < s32(channel_config.uNumChannels); i++)
		sum += peak[i];
	Audio::dialogue_volume = sum;
}

void AudioEntry::dialogue_done_callback(AkCallbackType type, AkCallbackInfo* info)
{
	// anyone paying attention to these should be polling them every frame;
	// if they're not, we don't care which ones get dropped
	if (Audio::dialogue_callbacks.length == Audio::dialogue_callbacks.capacity())
		Audio::dialogue_callbacks.length--;

	AudioEntry* entry = AudioEntry::by_ak_id(info->gameObjID);
	Transform* t = entry->parent.ref();
	if (t)
		Audio::dialogue_callbacks.add(ID(t->entity_id));
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

void AudioEntry::init(const Vec3& npos, Transform* nparent, AudioEntry* parent_entry, s32 f)
{
	revision++;
	pos = npos;
	parent = nparent;
	if (nparent)
		abs_pos = nparent->absolute_pos() + npos;
	else
		abs_pos = pos;
	playing = 0;

	AK::SoundEngine::RegisterGameObj(ak_id());

	if (parent_entry)
	{
		flags = parent_entry->flags;
		memcpy(obstruction, parent_entry->obstruction, sizeof(obstruction));
		memcpy(obstruction_target, parent_entry->obstruction_target, sizeof(obstruction_target));
		memcpy(occlusion, parent_entry->occlusion, sizeof(occlusion));
		memcpy(occlusion_target, parent_entry->occlusion_target, sizeof(occlusion_target));
		memcpy(reverb, parent_entry->reverb, sizeof(reverb));
		memcpy(reverb_target, parent_entry->reverb_target, sizeof(reverb_target));
		update();

		spatialization_update_frame = parent_entry->spatialization_update_frame;
	}
	else
	{
		flags = s8(f);
		update_spatialization(UpdateType::All);
		memcpy(obstruction, obstruction_target, sizeof(obstruction));
		memcpy(occlusion, occlusion_target, sizeof(occlusion));
		memcpy(reverb, reverb_target, sizeof(reverb));
		update();

		spatialization_update_frame = Audio::spatialization_update_frame;
	}
}

void AudioEntry::flag(s32 f, b8 value)
{
	if (value)
		flags |= f;
	else
	{
		flags &= ~f;
		if (f & FlagEnableObstructionOcclusion)
		{
			memset(occlusion, 0, sizeof(occlusion));
			memset(occlusion_target, 0, sizeof(occlusion_target));
			memset(obstruction, 0, sizeof(obstruction));
			memset(obstruction_target, 0, sizeof(obstruction_target));
		}
		if (f & FlagEnableReverb)
		{
			memset(reverb, 0, sizeof(reverb));
			memset(reverb_target, 0, sizeof(reverb_target));
		}
	}
}

AkAuxBusID reverb_aux_bus[MAX_REVERBS] =
{
	AK::AUX_BUSSES::REVERB_SMALL,
	AK::AUX_BUSSES::REVERB_DEFAULT,
	AK::AUX_BUSSES::REVERB_HUGE,
};

void AudioEntry::pathfind_result(s8 listener, r32 path_length, r32 straight_distance)
{
	occlusion_target[listener] = vi_max(0.0f, vi_min(1.0f, 0.05f + (path_length - straight_distance) / (DRONE_MAX_DISTANCE * 0.4f)));
}

void AudioEntry::update_spatialization(UpdateType type)
{
	if (flag(FlagEnableObstructionOcclusion))
	{
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (Audio::listener_mask & (1 << i))
			{
				const Audio::Listener& listener = Audio::listener[i];

				Vec3 dir = abs_pos - listener.pos;
				r32 distance = dir.length();
				if (distance > 0.0f)
				{
					if (!flag(FlagEnableForceFieldObstruction) || ForceField::hash(listener.team, abs_pos) == listener.force_field_hash)
					{
						dir /= distance;

						btCollisionWorld::ClosestRayResultCallback ray_callback(listener.pos, listener.pos + dir * vi_max(0.1f, distance - 0.5f));
						Physics::raycast(&ray_callback, CollisionAudio | (CollisionAllTeamsForceField & ~Team::force_field_mask(listener.team)));
						if (ray_callback.hasHit())
						{
							obstruction_target[i] = 1.0f;
							if (distance > 80.0f)
								occlusion_target[i] = 0.0f;
							else
							{
								if (type == UpdateType::All)
									pathfind_result(s8(i), AI::audio_pathfind(listener.pos, abs_pos), distance);
								else
									AI::audio_pathfind(listener.pos, abs_pos, this, s8(i), distance);
							}
						}
						else
						{
							// clear line of sight
							obstruction_target[i] = 0.0f;
							occlusion_target[i] = 0.0f;
						}
					}
					else
					{
						// inside a different force field
						obstruction_target[i] = 1.0f;
						occlusion_target[i] = 0.7f;
					}
				}
				else
				{
					// distance is zero
					obstruction_target[i] = 0.0f;
					occlusion_target[i] = 0.0f;
				}
			}
		}
	}

	if (flag(FlagEnableReverb))
	{
		ReverbCell reverb;
		AI::audio_reverb_calc(abs_pos, &reverb);
		memcpy(reverb_target, reverb.data, sizeof(reverb_target));
	}

	spatialization_update_frame = Audio::spatialization_update_frame;
}

void AudioEntry::update(r32 dt)
{
	if (parent.ref())
		abs_pos = pos + parent.ref()->absolute_pos();

	const r32 delta = dt * (1.0f / 0.4f); // takes X seconds to lerp to the new value

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (Audio::listener_mask & (1 << i))
		{
			if (flag(FlagEnableObstructionOcclusion))
			{
				if (obstruction_target[i] > obstruction[i])
					obstruction[i] = vi_min(obstruction_target[i], obstruction[i] + delta);
				else
					obstruction[i] = vi_max(obstruction_target[i], obstruction[i] - delta);
				if (occlusion_target[i] > occlusion[i])
					occlusion[i] = vi_min(occlusion_target[i], occlusion[i] + delta);
				else
					occlusion[i] = vi_max(occlusion_target[i], occlusion[i] - delta);
			}
			AK::SoundEngine::SetObjectObstructionAndOcclusion(ak_id(), Audio::listener_id(i), obstruction[i], occlusion[i]);
		}
	}

	{
		s32 reverb_count = 0;
		AkAuxSendValue values[MAX_REVERBS] = {};
		if (flag(FlagEnableReverb))
		{
			for (s32 i = 0; i < MAX_REVERBS; i++)
			{
				if (reverb_target[i] > reverb[i])
					reverb[i] = vi_min(reverb_target[i], reverb[i] + delta);
				else
					reverb[i] = vi_max(reverb_target[i], reverb[i] - delta);

				if (reverb[i] > 0.0f)
				{
					values[reverb_count].auxBusID = reverb_aux_bus[i];
					values[reverb_count].fControlValue = reverb[i];
					values[reverb_count].listenerID = AK_INVALID_GAME_OBJECT;
					reverb_count++;
				}
			}
		}
		AK::SoundEngine::SetGameObjectAuxSendValues(ak_id(), values, reverb_count);
	}

	AkSoundPosition sound_position;
	sound_position.SetPosition(-abs_pos.x, abs_pos.y, abs_pos.z);
	sound_position.SetOrientation(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
	AK::SoundEngine::SetPosition(ak_id(), sound_position);
}

AudioEntry* AudioEntry::by_ak_id(AkGameObjectID id)
{
	return &list[s32(id - AUDIO_OFFSET_ENTRIES)];
}

#if !SERVER
void AudioEntry::event_done_callback(AkCallbackType type, AkCallbackInfo* info)
{
	AudioEntry* entry = AudioEntry::by_ak_id(info->gameObjID);
	entry->playing = vi_max(0, entry->playing - 1);
}
#endif

void AudioEntry::post(AkUniqueID event_id)
{
	AkPlayingID i = AK::SoundEngine::PostEvent(event_id, ak_id(), AkCallbackType::AK_EndOfEvent, &event_done_callback);
	if (i != 0)
		playing++;
}

void AudioEntry::stop(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, ak_id());
	playing = vi_max(0, playing - 1);
}

void AudioEntry::stop_all()
{
	AK::SoundEngine::PostEvent(AK::EVENTS::STOP, ak_id());
	playing = 0;
}

b8 AudioEntry::post_dialogue(AkUniqueID event_id)
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

void AudioEntry::param(AkRtpcID id, AkRtpcValue value)
{
	AK::SoundEngine::SetRTPCValue(id, value, ak_id());
}

void AudioEntry::cleanup()
{
	revision++;
	AK::SoundEngine::UnregisterGameObj(ak_id());
}

s16 Audio::spatialization_update_frame;
void Audio::update_all(const Update& u)
{
	// update obstruction and occlusion of the first n entries that we haven't updated yet
	{
		s32 spatialization_updates = 12 / vi_max(1, s32(BitUtility::popcount(listener_mask)));

		for (auto i = AudioEntry::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->flag(AudioEntry::FlagKeepalive) || i.item()->playing > 0) // Audio component is keeping it alive, or something is playing on it
			{
				if (i.item()->flag(AudioEntry::FlagEnableObstructionOcclusion | AudioEntry::FlagEnableReverb)
					&& spatialization_updates > 0
					&& i.item()->spatialization_update_frame != spatialization_update_frame)
				{
					i.item()->update_spatialization(AudioEntry::UpdateType::ReverbObstruction);
					spatialization_updates--;
				}
				i.item()->update(u.real_time.delta);
			}
			else
			{
				i.item()->cleanup();
				AudioEntry::list.remove(i.index);
			}
		}

		if (spatialization_updates > 0) // we updated all entries
		{
			spatialization_update_frame = (spatialization_update_frame + 1) & s16((1 << 15) - 1); // increment to next frame so that all entries are marked as needing updated

			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (listener_mask & (1 << i))
				{
					Listener* l = &listener[i];
					l->force_field_hash = ForceField::hash(l->team, l->pos);
				}
			}

			// update ambience
			r32 ambience_indoor_outdoor = 0.0f;
			s32 count = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (listener_mask & (1 << i))
				{
					ReverbCell reverb;
					AI::audio_reverb_calc(listener[i].pos, &reverb);
					ambience_indoor_outdoor += reverb.outdoor;
					count++;
				}
			}
			if (count > 0)
				param_global(AK::GAME_PARAMETERS::AMBIENCE_INDOOR_OUTDOOR, ambience_indoor_outdoor / r32(count));
			else
				param_global(AK::GAME_PARAMETERS::AMBIENCE_INDOOR_OUTDOOR, 0.0f);
		}
	}

	AK::SoundEngine::RenderAudio();
}

AkUniqueID Audio::get_id(const char* str)
{
	return AK::SoundEngine::GetIDFromString(str);
}

void Audio::post_global(AkUniqueID event_id)
{
	AK::SoundEngine::PostEvent(event_id, AUDIO_OFFSET_GLOBAL_2D);
}

void Audio::dialogue_done_callback(AkCallbackType type, AkCallbackInfo* info)
{
	// anyone paying attention to these should be polling them every frame;
	// if they're not, we don't care which ones get dropped
	if (Audio::dialogue_callbacks.length == Audio::dialogue_callbacks.capacity())
		Audio::dialogue_callbacks.length--;

	Audio::dialogue_callbacks.add(IDNull);
}

AudioEntry* Audio::entry() const
{
	return &AudioEntry::list[entry_id];
}

b8 Audio::post_global_dialogue(AkUniqueID event_id)
{
	AkPlayingID i = AK::SoundEngine::PostEvent(event_id, AUDIO_OFFSET_GLOBAL_2D, AkCallbackType::AK_EndOfEvent, &dialogue_done_callback);
	return i != 0;
}

b8 Audio::post_dialogue(AkUniqueID event_id)
{
	return entry()->post_dialogue(event_id);
}

AudioEntry* Audio::post_global(AkUniqueID event_id, const Vec3& pos, Transform* parent, s32 flags)
{
	AudioEntry* e = AudioEntry::list.add();
	e->init(pos, parent, nullptr, flags);
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
	s32 listener_count = 0;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (listener_mask & (1 << i))
		{
			listener_ids[listener_count] = listener_id(i);
			listener_count++;
		}
	}
	AK::SoundEngine::SetDefaultListeners(listener_ids, listener_count);

	for (auto i = AudioEntry::list.iterator(); !i.is_last(); i.next())
		AK::SoundEngine::SetListeners(i.item()->ak_id(), listener_ids, listener_count);

	if (listener_count > 0)
	{
		AkChannelConfig config = AK::SoundEngine::GetSpeakerConfiguration();

		if (listener_count == 1)
		{
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (listener_mask & (1 << i))
				{
					AK::SoundEngine::SetListenerSpatialization(listener_id(i), true, config);
					break; // should be only one listener
				}
			}
		}
		else
		{
			// more than one listener; spatialize each listener differently

			// the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize is wrong
			AkReal32 volumes_left[18 * 4] = 
			{
				0.0f, // left
				-96.3f, // right
				-6.0f, // center
				0.0f, // rear left
				-96.3f, // rear right
				0.0f, // side left
				-96.3f, // side right
				0.0f, // lfe
			};
			AkReal32 volumes_right[18 * 4] =
			{
				-96.3f, // left
				0.0f, // right
				-6.0f, // center
				-96.3f, // rear left
				0.0f, // rear right
				-96.3f, // side left
				0.0f, // side right
				0.0f, // lfe
			};
			AkReal32 volumes_center[18 * 4] =
			{
				-6.0f, // left
				-6.0f, // right
				0.0f, // center
				-6.0f, // rear left
				-6.0f, // rear right
				-6.0f, // side left
				-6.0f, // side right
				0.0f, // lfe
			};

			AK::SpeakerVolumes::VectorPtr listener_blueprints[MAX_GAMEPADS - 1][MAX_GAMEPADS];
			vi_assert(MAX_GAMEPADS == 4); // update this if this changes
			// two players
			listener_blueprints[0][0] = volumes_left;
			listener_blueprints[0][1] = volumes_right;
			// three players
			listener_blueprints[1][0] = volumes_center;
			listener_blueprints[1][1] = volumes_left;
			listener_blueprints[1][2] = volumes_right;
			// four players
			listener_blueprints[2][0] = volumes_left;
			listener_blueprints[2][1] = volumes_right;
			listener_blueprints[2][2] = volumes_left;
			listener_blueprints[2][3] = volumes_right;

			s32 index = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (listener_mask & (1 << i))
				{
					AK::SoundEngine::SetListenerSpatialization(listener_id(i), false, config, listener_blueprints[listener_count - 2][index]);
					index++;
				}
			}
		}
	}
}

void Audio::listener_disable(s8 gamepad)
{
	s8 mask = 1 << gamepad;
	if (listener_mask & mask)
	{
		listener_mask &= ~mask;
		listener_list_update();
	}
}

void Audio::listener_enable(s8 gamepad, AI::Team team)
{
	s8 mask = 1 << gamepad;
	listener[gamepad].team = team;
	if (!(listener_mask & mask))
	{
		listener_mask |= mask;
		listener_list_update();
	}
}

void Audio::listener_update(s8 gamepad, const Vec3& pos, const Quat& rot)
{
	listener[gamepad].pos = pos;
	AkListenerPosition listener_position;
	listener_position.SetPosition(-pos.x, pos.y, pos.z);
	Vec3 forward = rot * Vec3(0, 0, 1);
	Vec3 up = rot * Vec3(0, 1, 0);
	listener_position.SetOrientation(-forward.x, forward.y, forward.z, -up.x, up.y, up.z);
	AK::SoundEngine::SetPosition(listener_id(gamepad), listener_position);
}

void Audio::awake()
{
	AudioEntry* e = AudioEntry::list.add();
	entry_id = e->id();
	e->init(Vec3::zero, get<Transform>());
	e->flag(AudioEntry::FlagKeepalive, true);
}

Audio::~Audio()
{
	entry()->flag(AudioEntry::FlagKeepalive, false);
}

void Audio::clear()
{
	post_global(AK::EVENTS::STOP_ALL);
	
	dialogue_callbacks.length = 0;
	for (auto i = AudioEntry::list.iterator(); !i.is_last(); i.next())
		i.item()->cleanup();
	AudioEntry::list.clear();
	for (s32 i = 0; i < AudioEntry::list.data.length; i++)
		AudioEntry::list[i].revision = 0;
}

void Audio::post(AkUniqueID event_id)
{
	entry()->post(event_id);
}

void Audio::stop(AkUniqueID event_id)
{
	entry()->stop(event_id);
}

void Audio::stop_all()
{
	entry()->stop_all();
}

AudioEntry* Audio::post_unattached(AkUniqueID event_id, const Vec3& pos)
{
	AudioEntry* e = AudioEntry::list.add();

	e->init(pos + get<Transform>()->absolute_pos(), nullptr, entry());
	e->post(event_id);
	return e;
}

AudioEntry* Audio::post_offset(AkUniqueID event_id, const Vec3& offset)
{
	AudioEntry* e = AudioEntry::list.add();
	e->init(offset, get<Transform>(), entry());
	e->post(event_id);
	return e;
}

void Audio::param(AkRtpcID id, AkRtpcValue value)
{
	entry()->param(id, value);
}

void Audio::offset(const Vec3& offset)
{
	entry()->pos = offset;
}

#endif

}