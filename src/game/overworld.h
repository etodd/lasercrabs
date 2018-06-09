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
	b8 allow_multiple;
};

enum class State : s8
{
	Hidden,
	MultiplayerOnline,
	MultiplayerOffline,
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
extern StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
extern Vec3 ambient_color;
extern r32 far_plane;
extern r32 fog_start;

void init(cJSON*);
void update(const Update&);
void draw_ui(const RenderParams&);
void draw_hollow(const RenderParams&);
void draw_opaque(const RenderParams&);
void draw_override(const RenderParams&);
b8 needs_override();
void show(Camera*, State, StoryTab = StoryTab::Map);
void shop_flags(s32);
void server_settings(Camera*);
void server_settings_readonly(Camera*);
void game_type_string(UIText*, Net::Master::Ruleset::Preset, GameType, s8, s8);
void clear();
void execute(const char*);
void zone_change(AssetID, ZoneState);
AssetID zone_id_for_uuid(AssetID);
AssetID zone_uuid_for_id(AssetID);
b8 active(); // true if the overworld UI is being shown in any way
void title();
void skip_transition_full();
void skip_transition_half();
b8 modal();
b8 transitioning();
b8 zone_is_pvp(AssetID);
s32 zone_max_teams(AssetID, GameType);
void zone_rewards(AssetID, s16*);
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
