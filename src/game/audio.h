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
// 0 -> (MAX_ENTITIES - 1): entities
// MAX_ENTITIES: transient global 2D positioned events
// MAX_ENTITIES + 1: transient global 2D positioned events
// MAX_ENTITIES + 2 -> (MAX_ENTITIES + 2 + MAX_GAMEPADS - 1): listeners
// MAX_ENTITIES + 2 + MAX_GAMEPADS -> beyond: attached events

#define ATTACHMENT_ID_OFFSET (MAX_ENTITIES + 2 + MAX_GAMEPADS)

namespace VI
{

struct Vec3;
struct Quat;
struct Transform;

struct Audio : ComponentType<Audio>
{
	struct Attachment
	{
		static PinArray<Attachment, MAX_ENTITIES> list;

		Vec3 offset;
		r32 lifetime;
		Ref<Transform> parent;

		inline ID id()
		{
			return ID(this - &list[0]);
		}

		inline AkGameObjectID ak_id()
		{
			return ATTACHMENT_ID_OFFSET + id();
		}

		void update(r32 = 0.0f);
	};

#if !SERVER
	static CAkDefaultIOHookBlocking wwise_io;
	static void dialogue_volume_callback(AK::IAkMetering*, AkChannelConfig, AkMeteringFlags);
	static void dialogue_done_callback(AkCallbackType, AkCallbackInfo*);
#endif

	static r32 dialogue_volume;
	static s8 listener_mask;
	static StaticArray<ID, 32> dialogue_callbacks; // poll this and empty it every frame; ID is entity ID

	static Array<AkGameObjectID> unregister_queue;
	static b8 init();
	static void term();
	static void update_all(const Update&);
	static void post_event_global(AkUniqueID);
	static void post_event_global(AkUniqueID, const Vec3&);
	static void post_event_global(AkUniqueID, const Vec3&, const Quat&);
	static void param_global(AkRtpcID, AkRtpcValue);
	static void listener_list_update();
	static void listener_enable(s8);
	static void listener_disable(s8);
	static void listener_update(s8, const Vec3&, const Quat&);
	static AkUniqueID get_id(const char*);
	static AkGameObjectID listener_id(s8);
	static void clear();

	Vec3 offset;

	void awake();
	~Audio();
	void post_event(AkUniqueID);
	void post_event_attached(AkUniqueID, const Vec3&, r32);
	b8 post_dialogue_event(AkUniqueID);
	void param(AkRtpcID, AkRtpcValue);
};

}