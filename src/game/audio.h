#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "constants.h"

#if SERVER
#include <AK/SoundEngine/Common/AkTypes.h>
#else

#if _WIN32
#include <AK/StreamManager/Win32/AkDefaultIOHookBlocking.h>
#elif defined(__APPLE__)
#include <AK/StreamManager/POSIX/AkDefaultIOHookBlocking.h>
#elif defined(__ORBIS__)
#include <AkDefaultIOHookBlocking.h>
#else // linux
#include <AK/StreamManager/POSIX/AkDefaultIOHookBlocking.h>
#endif
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkCallback.h>

#endif

// ID layout
// 2 -> 2 + MAX_GAMEPADS - 1: listeners
// 2 + MAX_GAMEPADS -> 2 + MAX_GAMEPADS + MAX_ENTITIES - 1: entries
// 2 + MAX_GAMEPADS + MAX_ENTITIES: transient global 2D positioned events

#define AUDIO_OFFSET_LISTENERS 2
#define AUDIO_OFFSET_ENTRIES (2 + MAX_GAMEPADS)
#define AUDIO_OFFSET_GLOBAL_2D (2 + MAX_GAMEPADS + MAX_ENTITIES)
#define MAX_REVERBS 3

namespace VI
{

struct Vec3;
struct Transform;

// 3D positioned sound source
struct AudioEntry
{
	enum class UpdateType : s8
	{
		ReverbObstruction,
		All,
		count,
	};

	enum Flag : s8
	{
		FlagKeepalive = 1 << 0,
		FlagEnableObstructionOcclusion = 1 << 1,
		FlagEnableReverb = 1 << 2,
	};

	static PinArray<AudioEntry, MAX_ENTITIES> list;

	static AudioEntry* by_ak_id(AkGameObjectID);

#if !SERVER
	static void dialogue_volume_callback(AK::IAkMetering*, AkChannelConfig, AkMeteringFlags);
	static void dialogue_done_callback(AkCallbackType, AkCallbackInfo*);
	static void event_done_callback(AkCallbackType, AkCallbackInfo*);
#endif

	Vec3 abs_pos;
	Vec3 pos;
	r32 obstruction[MAX_GAMEPADS];
	r32 obstruction_target[MAX_GAMEPADS];
	r32 occlusion[MAX_GAMEPADS];
	r32 occlusion_target[MAX_GAMEPADS];
	r32 reverb[MAX_REVERBS];
	r32 reverb_target[MAX_REVERBS];
	Ref<Transform> parent;
	s16 spatialization_update_frame;
	Revision revision;
	s8 playing;
	s8 flags;

	inline b8 flag(Flag f) const
	{
		return flags & f;
	}

	inline void flag(Flag f, b8 value)
	{
		if (value)
			flags |= f;
		else
			flags &= ~f;
	}

	void init(const Vec3&, Transform*, AudioEntry* = nullptr);
	void cleanup();

	inline ID id()
	{
		return ID(this - &list[0]);
	}

	inline AkGameObjectID ak_id()
	{
		return AUDIO_OFFSET_ENTRIES + id();
	}

	void update(r32 = 0.0f);
	void update_spatialization(UpdateType);
	void pathfind_result(s8, r32, r32);
	void post(AkUniqueID);
	void stop(AkUniqueID);
	void stop_all();
	b8 post_dialogue(AkUniqueID);
	void param(AkRtpcID, AkRtpcValue);
};

struct Audio : ComponentType<Audio>
{
#if !SERVER
	static CAkDefaultIOHookBlocking wwise_io;
	static void dialogue_done_callback(AkCallbackType, AkCallbackInfo*);
#endif

	static r32 dialogue_volume;
	static s8 listener_mask;
	static Vec3 listener_pos[MAX_GAMEPADS];
	static StaticArray<ID, 32> dialogue_callbacks; // poll this and empty it every frame; ID is entity ID
	static r32 volume_scale;

	static s16 spatialization_update_frame;
	static PinArray<AudioEntry, MAX_ENTITIES> pool_entity;
	static PinArray<AudioEntry, MAX_ENTITIES> pool_global_3d;
	static b8 init();
	static void term();
	static void update_all(const Update&);
	static void post_global(AkUniqueID);
	static b8 post_global_dialogue(AkUniqueID);
	static AudioEntry* post_global(AkUniqueID, const Vec3&);
	static void param_global(AkRtpcID, AkRtpcValue);
	static void listener_list_update();
	static void listener_enable(s8);
	static void listener_disable(s8);
	static void listener_update(s8, const Vec3&, const Quat&);
	static AkUniqueID get_id(const char*);
	static AkGameObjectID listener_id(s8);
	static void clear();
	static void volume_multiplier(r32);

	ID entry_id;

	void awake();
	~Audio();
	void post(AkUniqueID);
	void stop(AkUniqueID);
	void stop_all();
	AudioEntry* entry() const;
	AudioEntry* post_unattached(AkUniqueID, const Vec3& = Vec3::zero);
	AudioEntry* post_offset(AkUniqueID, const Vec3&);
	b8 post_dialogue(AkUniqueID);
	void param(AkRtpcID, AkRtpcValue);
	void offset(const Vec3&);
};

}
