#pragma once
#include "types.h"
#include "render/render.h"
#include "master.h"

struct cJSON;

namespace VI
{

struct EntityFinder;

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

enum class State : s8
{
	Hidden,
	Multiplayer,
	StoryMode,
	StoryModeOverlay,
	StoryModeDeploying,
	count,
};

enum class StoryTab : s8
{
	Map,
	Inventory,
	count,
};

extern ResourceInfo resource_info[s32(Resource::count)];

b8 net_msg(Net::StreamRead*, Net::MessageSource);
void init(cJSON*);
void update(const Update&);
void draw_ui(const RenderParams&);
void show(Camera*, State, StoryTab = StoryTab::Map);
void clear();
void execute(const char*);
void zone_done(AssetID);
void zone_change(AssetID, ZoneState);
AssetID zone_id_for_uuid(AssetID);
b8 active(); // true if the overworld UI is being shown in any way
void title();
void skip_transition();
b8 transitioning();
b8 zone_is_pvp(AssetID);
s32 zone_max_teams(AssetID);
void zone_rewards(AssetID, s16*);
b8 pvp_colors();
AssetID zone_under_attack();
void resource_change(Resource, s16);
r32 resource_change_time(Resource);
void master_server_list_entry(ServerListType, s32, const Net::Master::ServerListEntry&);
void master_server_list_end(ServerListType, s32);
void master_server_config_saved(u32, u32);
void master_server_details_response(const Net::Master::ServerDetails&, u32);
void ping_response(const Sock::Address&, u32);

}

}
