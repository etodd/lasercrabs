#pragma once
#include "types.h"

struct cJSON;

namespace VI
{

struct EntityFinder;
struct RenderParams;
struct Camera;

namespace Net
{
	struct StreamRead;
}

namespace Overworld
{

struct ResourceInfo
{
	AssetID icon;
	AssetID description;
	s16 cost;
};

extern ResourceInfo resource_info[(s32)Resource::count];

b8 net_msg(Net::StreamRead*, Net::MessageSource);
void init(cJSON*);
void update(const Update&);
void draw_opaque(const RenderParams&);
void draw_hollow(const RenderParams&);
void draw_override(const RenderParams&);
void draw_ui(const RenderParams&);
void show(Camera*);
void conversation_finished();
void clear();
void execute(const char*);
void zone_done(AssetID);
void zone_change(AssetID, ZoneState);
b8 active();
void message_add(AssetID, AssetID, r64);
void message_schedule(AssetID, AssetID, r64 = 0.0);
AssetID zone_under_attack();
r32 zone_under_attack_timer();
s32 message_unread_count();
void resource_change(Resource, s16);

}

}
