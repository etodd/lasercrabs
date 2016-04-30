#pragma once

#include "data/entity.h"
#include <AK/StreamManager/AkDefaultIOHookBlocking.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkCallback.h>

namespace VI
{

struct Vec3;
struct Quat;

struct Audio : ComponentType<Audio>
{
	static CAkDefaultIOHookBlocking wwise_io;
	static r32 dialogue_volume;
	static void dialogue_volume_callback(AK::IAkMetering*, AkChannelConfig, AkMeteringFlags);
	static b8 dialogue_done;
	static b8 init();
	static void term();
	static void update();
	static void post_global_event(AkUniqueID);
	static b8 post_dialogue_event(AkUniqueID);
	static void dialogue_done_callback(AkCallbackType, AkCallbackInfo*);
	static void post_global_event(AkUniqueID, const Vec3&);
	static void post_global_event(AkUniqueID, const Vec3&, const Vec3&);
	static void global_param(AkRtpcID, const AkRtpcValue);
	static void listener_enable(u32);
	static void listener_disable(u32);
	static void listener_update(u32, const Vec3&, const Quat&);
	static AkUniqueID get_id(const char*);

	b8 registered;

	void awake();
	~Audio();
	void post_event(AkUniqueID);
	void param(AkRtpcID, AkRtpcValue);
};

}
