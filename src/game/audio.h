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
#define AUDIO_OFFSET_GLOBAL_2D_ALL (AUDIO_OFFSET_GLOBAL_2D + MAX_GAMEPADS)

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
		FlagEnableForceFieldObstruction = 1 << 2,
		FlagEnableReverb = 1 << 3,
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
	s8 listener_mask = -1;

	inline b8 flag(s32 f) const
	{
		return flags & f;
	}

	void flag(s32, b8);

	void init(const Vec3&, Transform*, AudioEntry* = nullptr, s32 = FlagEnableObstructionOcclusion | FlagEnableForceFieldObstruction | FlagEnableReverb);
	void cleanup();
	void set_listener_mask(s8);

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
	struct Listener
	{
		Vec3 pos;
		u32 force_field_hash;
		AI::Team team;
		r32 outdoor;
	};

#if !SERVER
	static CAkDefaultIOHookBlocking wwise_io;
	static void dialogue_done_callback(AkCallbackType, AkCallbackInfo*);
#endif

	static r32 dialogue_volume;
	static s8 listener_mask;
	static Listener listener[MAX_GAMEPADS];
	static StaticArray<ID, 32> dialogue_callbacks; // poll this and empty it every frame; ID is entity ID
	static r32 volume_scale;

	static s16 spatialization_update_frame;
	static PinArray<AudioEntry, MAX_ENTITIES> pool_entity;
	static PinArray<AudioEntry, MAX_ENTITIES> pool_global_3d;
	static b8 init();
	static void term();
	static void update_all(const Update&);
	static void post_global(AkUniqueID, s8 = -1);
	static b8 post_global_dialogue(AkUniqueID, s8 = -1);
	static AudioEntry* post_global(AkUniqueID, const Vec3&, Transform* = nullptr, s32 = AudioEntry::FlagEnableObstructionOcclusion | AudioEntry::FlagEnableForceFieldObstruction | AudioEntry::FlagEnableReverb);
	static void param_global(AkRtpcID, AkRtpcValue, s8 = -1);
	static void listener_list_update();
	static void listener_enable(s8, AI::Team);
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