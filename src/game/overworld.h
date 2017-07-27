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

struct ServerListEntry
{
	u32 id;
	AssetID level;
	char name[MAX_SERVER_CONFIG_NAME + 1];
	s8 max_players;
	s8 open_slots;
	s8 team_count;
	GameType game_type;
};

extern ResourceInfo resource_info[s32(Resource::count)];
extern StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
extern Vec3 ambient_color;

b8 net_msg(Net::StreamRead*, Net::MessageSource);
void init(cJSON*);
void update(const Update&);
void draw_opaque(const RenderParams&);
void draw_hollow(const RenderParams&);
void draw_ui(const RenderParams&);
void show(Camera*, State, StoryTab = StoryTab::Map);
void clear();
void execute(const char*);
void zone_done(AssetID);
void zone_change(AssetID, ZoneState);
b8 active(); // true if the overworld UI is being shown in any way
b8 modal(); // true if the entire overworld scene is being shown
b8 zone_is_pvp(AssetID);
void zone_rewards(AssetID, s16*);
AssetID zone_under_attack();
r32 zone_under_attack_timer();
void resource_change(Resource, s16);
r32 resource_change_time(Resource);
void master_server_list_entry(ServerListType, s32, const ServerListEntry&);
void master_server_config_saved(u32, u32);

}

}
