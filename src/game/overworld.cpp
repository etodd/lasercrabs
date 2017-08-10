#include "overworld.h"
#include "console.h"
#include "game.h"
#include "mersenne/mersenne-twister.h"
#include "audio.h"
#include "menu.h"
#include "asset/level.h"
#include "asset/Wwise_IDs.h"
#include "asset/mesh.h"
#include "asset/font.h"
#include "asset/dialogue.h"
#include "asset/texture.h"
#include "asset/lookup.h"
#include "asset/shader.h"
#include "strings.h"
#include "load.h"
#include "entities.h"
#include "render/particles.h"
#include "data/import_common.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "data/priority_queue.h"
#include "platform/util.h"
#include "noise.h"
#include "team.h"
#include "player.h"
#include "data/components.h"
#include "ease.h"
#include "net.h"
#include "data/json.h"

namespace VI
{


namespace Overworld
{

#define DEPLOY_TIME 1.0f
#define TAB_ANIMATION_TIME 0.3f
#define REFRESH_INTERVAL 5.0f

#define SCALE_MULTIPLIER (UI::scale < 1.0f ? 0.5f : 1.0f)
#define PADDING (16.0f * UI::scale * SCALE_MULTIPLIER)
#define TAB_SIZE Vec2(160.0f * UI::scale, UI_TEXT_SIZE_DEFAULT * UI::scale + PADDING * 2.0f)
#define MAIN_VIEW_SIZE (Vec2(800.0f, 512.0f) * (SCALE_MULTIPLIER * UI::scale))
#define TEXT_SIZE (UI_TEXT_SIZE_DEFAULT * SCALE_MULTIPLIER)
#define MESSAGE_TRUNCATE_LONG (72 * SCALE_MULTIPLIER)
#define MESSAGE_TRUNCATE_SHORT (48 * SCALE_MULTIPLIER)
#define BORDER 2.0f
#define OPACITY 0.8f
#define STRING_BUFFER_SIZE 256
#define BUY_TIME 1.0f
#define EVENT_INTERVAL_PER_ZONE (60.0f * 5.0f)
#define EVENT_ODDS_PER_ZONE (1.0f / EVENT_INTERVAL_PER_ZONE) // an event will happen on average every X minutes per zone you own
#define ZONE_MAX_CHILDREN 12

struct ZoneNode
{
	StaticArray<Vec3, ZONE_MAX_CHILDREN> children;
	Quat rot;
	AssetID id;
	AssetID mesh;
	s16 rewards[s32(Resource::count)];
	s8 size;
	s8 max_teams;

	inline Vec3 pos() const
	{
		return children[children.length - 1];
	}
};

#define DEPLOY_ANIMATION_TIME 1.0f

struct PropEntry
{
	Quat rot;
	Vec3 pos;
	AssetID mesh;
};

struct WaterEntry
{
	Quat rot;
	Vec3 pos;
	Water::Config config;
};

struct DataGlobal
{
	struct Multiplayer
	{
		s8 local_player_mask = 1;
		ServerListType tab;
		ServerListType tab_previous;
	};

	StaticArray<ZoneNode, MAX_ZONES> zones;
	Array<PropEntry> props;
	Array<WaterEntry> waters;
	Vec3 camera_offset_pos;
	Quat camera_offset_rot;

	Multiplayer multiplayer;
};
DataGlobal global;

const AssetID multiplayer_tab_names[s32(ServerListType::count)] = { strings::tab_top, strings::tab_recent, strings::tab_mine, };

struct Data
{
	struct Inventory
	{
		enum class Mode : s8
		{
			Normal,
			Buy,
			count,
		};

		r32 resource_change_time[s32(Resource::count)];
		r32 timer_buy;
		Resource resource_selected;
		s16 buy_quantity;
		Mode mode;
	};

	struct Map
	{
		r32 zones_change_time[MAX_ZONES];
	};

	struct Multiplayer
	{
		enum class State : s8
		{
			Browse,
			EntryView,
			EntryEdit,
			count,
		};

		enum class RequestType : s8
		{
			ConfigGet,
			ConfigSave,
			count,
		};

		enum class EditMode : s8
		{
			Main,
			Name,
			Levels,
			AddLevel,
			AllowedUpgrades,
			StartUpgrades,
			AddStartUpgrade,
			count,
		};

		struct ServerList
		{
			Array<Net::Master::ServerListEntry> entries;
			UIScroll scroll;
			s32 selected;
		};

		struct PingData
		{
			r32 last_sent_time;
			r32 rtt;
			u32 last_sent_token;
		};

		std::unordered_map<u64, PingData> ping;
		ServerList server_lists[s32(ServerListType::count)];
		UIMenu menu[s32(EditMode::count)];
		TextField text_field;
		r32 tab_timer = TAB_ANIMATION_TIME;
		r32 state_transition_time;
		r32 refresh_timer;
		u32 request_id;
		Net::Master::ServerDetails active_server;
		RequestType request_type;
		State state;
		EditMode edit_mode;
		// in State::EntryEdit, this is true if there are unsaved changes.
		// in State::EntryView, this is true if we've received ServerDetails from the master
		b8 active_server_dirty;
	};

	struct StoryMode
	{
		r64 timestamp_last;
		StoryTab tab;
		StoryTab tab_previous;
		r32 tab_timer = TAB_ANIMATION_TIME;
		Inventory inventory;
		Map map;
	};

	Ref<Camera> camera;
	Camera camera_restore_data;
	Quat camera_rot;
	Vec3 camera_pos;
	r32 timer_deploy;
	r32 timer_transition;
	State state;
	State state_next;
	StoryMode story;
	Multiplayer multiplayer;
	AssetID zone_selected = AssetNull;
};

Data data = Data();

void deploy_start()
{
	vi_assert(Game::session.type == SessionType::Story);
	data.state = State::StoryModeDeploying;
	data.timer_deploy = DEPLOY_TIME;
	Audio::post_global_event(AK::EVENTS::PLAY_OVERWORLD_DEPLOY_START);
}

void deploy_done();
const ZoneNode* zone_node_get(AssetID);

b8 multiplayer_can_switch_tab()
{
	return data.multiplayer.state == Data::Multiplayer::State::Browse;
}

void multiplayer_state_transition(Data::Multiplayer::State state)
{
	data.multiplayer.state = state;
	data.multiplayer.state_transition_time = Game::real_time.total;
	data.multiplayer.menu[0].animate();
	data.multiplayer.active_server_dirty = false;
	data.multiplayer.request_id = 0;
	data.multiplayer.refresh_timer = 0.0f;
#if !SERVER
	Net::Client::master_cancel_outgoing();
#endif
}

void multiplayer_edit_mode_transition(Data::Multiplayer::EditMode mode)
{
	data.multiplayer.state_transition_time = Game::real_time.total;
	if (s32(mode) > s32(data.multiplayer.edit_mode))
		data.multiplayer.menu[s32(mode)].animate(); // also resets the menu so the top item is selected
	else
		data.multiplayer.menu[s32(mode)].animation_time = Game::real_time.total; // we're going back to a previous menu; just animate it, but don't go back to the top
	data.multiplayer.edit_mode = mode;
}

void multiplayer_switch_tab(ServerListType type)
{
	global.multiplayer.tab_previous = global.multiplayer.tab;
	global.multiplayer.tab = type;
	if (global.multiplayer.tab != global.multiplayer.tab_previous)
	{
		data.multiplayer.tab_timer = TAB_ANIMATION_TIME;
		data.multiplayer.refresh_timer = 0.0f;
	}
}

void multiplayer_browse_update(const Update& u)
{
	if (u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0))
	{
		new (&data.multiplayer.active_server.config) Net::Master::ServerConfig();
		multiplayer_switch_tab(ServerListType::Mine);
		multiplayer_state_transition(Data::Multiplayer::State::EntryEdit);
		return;
	}

	Data::Multiplayer::ServerList* server_list = &data.multiplayer.server_lists[s32(global.multiplayer.tab)];

	server_list->selected = vi_max(0, vi_min(server_list->entries.length - 1, server_list->selected + UI::input_delta_vertical(u, 0)));
	server_list->scroll.update_menu(server_list->entries.length);
	server_list->scroll.scroll_into_view(server_list->selected);

	if (server_list->selected < server_list->entries.length && u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
	{
		data.multiplayer.active_server.config.id = server_list->entries[server_list->selected].server_state.id;
		multiplayer_state_transition(Data::Multiplayer::State::EntryView);
		return;
	}

	data.multiplayer.refresh_timer -= u.time.delta;
	if (data.multiplayer.refresh_timer < 0.0f)
	{
		data.multiplayer.refresh_timer += REFRESH_INTERVAL;

#if !SERVER
		Net::Client::master_request_server_list(global.multiplayer.tab, server_list->selected);
#endif
	}
}

r32 ping(const Sock::Address& addr)
{
	auto i = data.multiplayer.ping.find(addr.hash());
	if (i == data.multiplayer.ping.end())
		return -1.0f;
	else
		return i->second.rtt;
}

void ping_send(const Sock::Address& addr)
{
	u64 hash = addr.hash();
	Data::Multiplayer::PingData* ping;
	auto i = data.multiplayer.ping.find(hash);
	if (i == data.multiplayer.ping.end())
	{
		auto i = data.multiplayer.ping.insert(std::pair<u64, Data::Multiplayer::PingData>(hash, Data::Multiplayer::PingData()));
		ping = &i.first->second;
		ping->rtt = -1.0f;
	}
	else
		ping = &i->second;
	ping->last_sent_token = u32(mersenne::rand());
	ping->last_sent_time = Game::real_time.total;

#if !SERVER
	Net::Client::ping(addr, ping->last_sent_token);
#endif
}

void ping_response(const Sock::Address& addr, u32 token)
{
	auto i = data.multiplayer.ping.find(addr.hash());
	if (i != data.multiplayer.ping.end())
	{
		Data::Multiplayer::PingData* ping = &i->second;
		if (ping->last_sent_token == token)
			ping->rtt = Game::real_time.total - ping->last_sent_time;
	}
}

void master_server_list_entry(ServerListType type, s32 index, const Net::Master::ServerListEntry& entry)
{
	if (active()
		&& data.state == State::Multiplayer)
	{
		Data::Multiplayer::ServerList* list = &data.multiplayer.server_lists[s32(type)];
		if (index >= list->entries.length)
			list->entries.resize(index + 1);
		list->entries.data[index] = entry;
		ping_send(entry.addr);
	}
}

void multiplayer_entry_edit_cancel(s8 gamepad = 0)
{
	if (data.multiplayer.active_server.config.id) // we were editing a config that actually exists; switch to EntryView mode
		multiplayer_state_transition(Data::Multiplayer::State::EntryView);
	else // we were creating a new entry and we never saved it; go back to Browse
		multiplayer_state_transition(Data::Multiplayer::State::Browse);
	Game::cancel_event_eaten[0] = true;
}

void multiplayer_request_setup(Data::Multiplayer::RequestType type)
{
	data.multiplayer.request_id = vi_max(u32(1), u32(mersenne::rand()));
	data.multiplayer.request_type = type;
}

void multiplayer_entry_edit_update(const Update& u)
{
	b8 cancel = u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0];

	if (data.multiplayer.request_id)
	{
		if (cancel)
		{
			data.multiplayer.request_id = 0; // cancel active request
			Game::cancel_event_eaten[0] = true;
#if !SERVER
			Net::Client::master_cancel_outgoing();
#endif
		}
	}
	else
	{
		Net::Master::ServerConfig* config = &data.multiplayer.active_server.config;
		UIMenu* menu = &data.multiplayer.menu[s32(data.multiplayer.edit_mode)];
		switch (data.multiplayer.edit_mode)
		{
			case Data::Multiplayer::EditMode::Name:
			{
				data.multiplayer.text_field.update(u, 0, MAX_SERVER_CONFIG_NAME);
				if (cancel)
				{
					data.multiplayer.text_field.set("");
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}
				else if (u.last_input->get(Controls::UIAcceptText, 0) && !u.input->get(Controls::UIAcceptText, 0))
				{
					data.multiplayer.active_server_dirty = true;
					strncpy(config->name, data.multiplayer.text_field.value.data, MAX_SERVER_CONFIG_NAME);
					data.multiplayer.text_field.set("");
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					menu->end();
					return;
				}
				break;
			}
			case Data::Multiplayer::EditMode::Main:
			{
				menu->start(u, 0);

				// cancel
				if (cancel || menu->item(u, _(strings::cancel), nullptr, false, Asset::Mesh::icon_close))
				{
					Game::cancel_event_eaten[0] = true;
					if (data.multiplayer.active_server_dirty)
						Menu::dialog(0, &multiplayer_entry_edit_cancel, _(strings::confirm_entry_cancel));
					else
					{
						multiplayer_entry_edit_cancel();
						menu->end();
						return;
					}
				}

				// save
				if (menu->item(u, _(strings::save), nullptr, false, Asset::Mesh::icon_arrow)
					|| (u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0)))
				{
					if (config->levels.length == 0)
						Menu::dialog(0, &Menu::dialog_no_action, _(strings::error_no_levels));
					else if (strlen(config->name) == 0)
						Menu::dialog(0, &Menu::dialog_no_action, _(strings::error_no_name));
					else
					{
						multiplayer_request_setup(Data::Multiplayer::RequestType::ConfigSave);
#if !SERVER
						Net::Client::master_save_server_config(*config, data.multiplayer.request_id);
#endif
					}
				}

				// edit name
				if (menu->item(u, _(strings::edit_name)))
				{
					data.multiplayer.text_field.set(config->name);
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Name);
					menu->end();
					return;
				}

				s32 delta;
				char str[MAX_PATH_LENGTH + 1];

				// private
				{
					b8* is_private = &config->is_private;
					delta = menu->slider_item(u, _(strings::is_private), _(*is_private ? strings::yes : strings::no));
					if (delta != 0)
					{
						*is_private = !(*is_private);
						data.multiplayer.active_server_dirty = true;
					}
				}

				{
					// game type
					AssetID value;
					switch (config->game_type)
					{
						case GameType::Assault:
						{
							value = strings::game_type_assault;
							break;
						}
						case GameType::Deathmatch:
						{
							value = strings::game_type_deathmatch;
							break;
						}
						default:
						{
							vi_assert(false);
							break;
						}
					}
					if (UIMenu::enum_option(&config->game_type, menu->slider_item(u, _(strings::game_type), _(value))))
						data.multiplayer.active_server_dirty = true;
				}

				// levels
				{
					sprintf(str, "%d", s32(config->levels.length));
					if (menu->item(u, _(strings::levels), str))
					{
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Levels);
						menu->end();
						return;
					}
				}

				if (config->game_type == GameType::Assault)
				{
					// respawns
					s16* respawns = &config->respawns;
					sprintf(str, "%hd", *respawns);
					delta = menu->slider_item(u, _(strings::drones), str);
					if (delta < 0)
					{
						*respawns = vi_max(1, s32(*respawns) - 2);
						data.multiplayer.active_server_dirty = true;
					}
					else if (delta > 0)
					{
						*respawns = vi_min(1000, s32(*respawns) + 2);
						data.multiplayer.active_server_dirty = true;
					}
				}
				else
				{
					// kill limit
					s16* kill_limit = &config->kill_limit;
					sprintf(str, "%hd", *kill_limit);
					delta = menu->slider_item(u, _(strings::kill_limit), str);
					if (delta < 0)
					{
						*kill_limit = vi_max(1, s32(*kill_limit) - 2);
						data.multiplayer.active_server_dirty = true;
					}
					else if (delta > 0)
					{
						*kill_limit = vi_min(1000, s32(*kill_limit) + 2);
						data.multiplayer.active_server_dirty = true;
					}
				}

				{
					// time limit
					u8* time_limit = &config->time_limit_minutes;
					sprintf(str, _(strings::timer), s32(*time_limit), 0);
					delta = menu->slider_item(u, _(strings::time_limit), str);
					*time_limit = vi_max(2, vi_min(254, (*time_limit) + (delta * 2)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// max players
					s8* max_players = &config->max_players;
					sprintf(str, "%hhd", *max_players);
					delta = menu->slider_item(u, _(strings::max_players), str);
					*max_players = vi_max(2, vi_min(MAX_PLAYERS, *max_players + delta));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// min players
					s8* min_players = &config->min_players;
					sprintf(str, "%hhd", *min_players);
					delta = menu->slider_item(u, _(strings::min_players), str);
					*min_players = vi_max(1, vi_min(MAX_PLAYERS, *min_players + delta));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// team count
					if (config->game_type == GameType::Assault)
						config->team_count = 2;

					s8* team_count = &config->team_count;
					sprintf(str, "%hhd", *team_count);
					delta = menu->slider_item(u, _(strings::teams), str, config->game_type == GameType::Assault);

					s32 max_teams = vi_min(s32(config->max_players), MAX_TEAMS);
					for (s32 i = 0; i < config->levels.length; i++)
						max_teams = vi_min(max_teams, s32(zone_node_get(config->levels[i])->max_teams));

					*team_count = s8(vi_max(2, vi_min(max_teams, *team_count + delta)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				// bots
				{
					b8* fill_bots = &config->fill_bots;
					delta = menu->slider_item(u, _(strings::fill_bots), _(*fill_bots ? strings::on : strings::off));
					if (delta)
					{
						*fill_bots = !(*fill_bots);
						data.multiplayer.active_server_dirty = true;
					}
				}

				{
					// drone shield
					s8* drone_shield = &config->drone_shield;
					sprintf(str, "%d", s32(*drone_shield));
					delta = menu->slider_item(u, _(strings::drone_shield), str);
					*drone_shield = vi_max(0, vi_min(DRONE_SHIELD, (*drone_shield) + delta));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// start energy
					s16* start_energy = &config->start_energy;
					sprintf(str, "%d", s32(*start_energy));
					delta = menu->slider_item(u, _(strings::start_energy), str);
					*start_energy = vi_max(0, vi_min(MAX_START_ENERGY, (*start_energy) + (delta * 100)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				// enable minions
				{
					b8* enable_minions = &config->enable_minions;
					delta = menu->slider_item(u, _(strings::enable_minions), _(*enable_minions ? strings::on : strings::off));
					if (delta)
					{
						*enable_minions = !(*enable_minions);
						data.multiplayer.active_server_dirty = true;
					}
				}

				// allowed upgrades
				{
					sprintf(str, "%d", s32(Net::popcount(0x0ffff & config->allow_upgrades)));
					if (menu->item(u, _(strings::allow_upgrades), str))
					{
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::AllowedUpgrades);
						menu->end();
						return;
					}
				}

				// start upgrades
				{
					sprintf(str, "%d", s32(config->start_upgrades.length));
					if (menu->item(u, _(strings::start_upgrades), str))
					{
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::StartUpgrades);
						menu->end();
						return;
					}
				}

				menu->end();
				break;
			}
			case Data::Multiplayer::EditMode::Levels:
			{
				menu->start(u, 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}

				for (s32 i = 0; i < config->levels.length; i++)
				{
					if (menu->item(u, Loader::level_name(config->levels[i]), nullptr, false, Asset::Mesh::icon_close))
					{
						config->levels.remove_ordered(i);
						data.multiplayer.active_server_dirty = true;
						i--;
					}
				}

				if (config->levels.length < config->levels.capacity())
				{
					if (menu->item(u, _(strings::add_level)))
					{
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::AddLevel);
						menu->end();
						return;
					}
				}

				menu->end();
				break;
			}
			case Data::Multiplayer::EditMode::AddLevel:
			{
				menu->start(u, 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Levels);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}

				for (s32 i = 0; i < Asset::Level::count; i++)
				{
					if (Overworld::zone_is_pvp(AssetID(i)) && i != Asset::Level::Port_District)
					{
						b8 already_added = false;
						for (s32 j = 0; j < config->levels.length; j++)
						{
							if (config->levels[j] == AssetID(i))
							{
								already_added = true;
								break;
							}
						}

						if (!already_added && menu->item(u, Loader::level_name(i)))
						{
							config->levels.add(i);
							data.multiplayer.active_server_dirty = true;
							multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Levels);
							menu->end();
						}
					}
				}

				menu->end();
				break;
			}
			case Data::Multiplayer::EditMode::StartUpgrades:
			{
				menu->start(u, 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}

				for (s32 i = 0; i < config->start_upgrades.length; i++)
				{
					Upgrade upgrade = config->start_upgrades[i];
					if (menu->item(u, _(UpgradeInfo::list[s32(upgrade)].name), nullptr, false, Asset::Mesh::icon_close))
					{
						config->start_upgrades.remove_ordered(i);
						data.multiplayer.active_server_dirty = true;
						i--;
					}
				}

				if (config->start_upgrades.length < config->start_upgrades.capacity())
				{
					if (menu->item(u, _(strings::add_start_upgrade)))
					{
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::AddStartUpgrade);
						menu->end();
						return;
					}
				}

				menu->end();
				break;
			}
			case Data::Multiplayer::EditMode::AddStartUpgrade:
			{
				menu->start(u, 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::StartUpgrades);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}

				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					b8 already_added = false;
					for (s32 j = 0; j < config->start_upgrades.length; j++)
					{
						if (config->start_upgrades[j] == Upgrade(i))
						{
							already_added = true;
							break;
						}
					}

					if (!already_added && menu->item(u, _(UpgradeInfo::list[i].name)))
					{
						config->start_upgrades.add(Upgrade(i));
						data.multiplayer.active_server_dirty = true;
						multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::StartUpgrades);
						menu->end();
					}
				}

				menu->end();
				break;
			}
			case Data::Multiplayer::EditMode::AllowedUpgrades:
			{
				menu->start(u, 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end();
					return;
				}

				s16* allow_upgrades = &config->allow_upgrades;
				for (s32 i = 0; i < s32(Ability::count); i++)
				{
					b8 value = (*allow_upgrades) & (1 << i);
					if (menu->slider_item(u, _(UpgradeInfo::list[i].name), _(value ? strings::yes : strings::no)))
					{
						data.multiplayer.active_server_dirty = true;
						value = !value;
						if (value)
							*allow_upgrades = (*allow_upgrades) | (1 << i);
						else
							*allow_upgrades = (*allow_upgrades) & ~(1 << i);
					}
				}

				menu->end();
				break;
			}
			default:
				vi_assert(false);
				break;
		}
	}
}

void master_server_config_saved(u32 id, u32 request_id)
{
	if (active()
		&& data.state == State::Multiplayer
		&& data.multiplayer.state == Data::Multiplayer::State::EntryEdit
		&& data.multiplayer.request_id == request_id)
	{
		PlayerHuman::log_add(_(strings::entry_saved));
		data.multiplayer.server_lists[s32(ServerListType::Mine)].selected = 0;
		data.multiplayer.active_server.config.id = id;
		data.multiplayer.active_server_dirty = false;
		data.multiplayer.request_id = 0;
	}
}

void master_server_details_response(const Net::Master::ServerDetails& details, u32 request_id)
{
	if (active()
		&& data.state == State::Multiplayer
		&& data.multiplayer.state == Data::Multiplayer::State::EntryView
		&& data.multiplayer.request_id == request_id)
	{
		data.multiplayer.active_server = details;
		data.multiplayer.request_id = 0;
		data.multiplayer.active_server_dirty = true;
		if (details.state.level != AssetNull) // a server is running this config
			ping_send(details.addr);
	}
}

void multiplayer_entry_view_update(const Update& u)
{
	b8 cancel = u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0];

	if (data.multiplayer.request_id && !data.multiplayer.active_server_dirty) // we don't have any config data yet
	{
		if (cancel)
		{
			data.multiplayer.request_id = 0; // cancel active request
			Game::cancel_event_eaten[0] = true;
#if !SERVER
			Net::Client::master_cancel_outgoing();
#endif
		}
	}
	else
	{
		if (cancel)
		{
			multiplayer_state_transition(Data::Multiplayer::State::Browse);
			Game::cancel_event_eaten[0] = true;
			return;
		}
		else if (data.multiplayer.active_server.is_admin
			&& u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0))
		{
			multiplayer_state_transition(Data::Multiplayer::State::EntryEdit);
			return;
		}
		else if (u.input->get(Controls::Interact, 0) && !u.last_input->get(Controls::Interact, 0))
		{
			u32 id = data.multiplayer.active_server.config.id;
			Game::unload_level();
			Game::session.local_player_mask = global.multiplayer.local_player_mask;
#if !SERVER
			Net::Client::master_request_server(id);
#endif
			return;
		}

		data.multiplayer.refresh_timer -= u.time.delta;
		if (data.multiplayer.refresh_timer < 0.0f)
		{
			data.multiplayer.refresh_timer += REFRESH_INTERVAL;

			multiplayer_request_setup(Data::Multiplayer::RequestType::ConfigGet);
#if !SERVER
			Net::Client::master_request_server_details(data.multiplayer.active_server.config.id, data.multiplayer.request_id);
#endif
		}
	}
}

void multiplayer_update(const Update& u)
{
	if (Menu::main_menu_state != Menu::State::Hidden || Game::scheduled_load_level != AssetNull)
		return;

	if (data.multiplayer.state == Data::Multiplayer::State::Browse
		&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0])
	{
		Menu::title();
		Game::cancel_event_eaten[0] = true;
		return;
	}

	for (s32 i = 1; i < MAX_GAMEPADS; i++)
	{
		if (u.input->gamepads[i].type == Gamepad::Type::None)
			global.multiplayer.local_player_mask &= ~(1 << i);
		else
		{
			b8 player_active = global.multiplayer.local_player_mask & (1 << i);
			if (!player_active
				&& u.last_input->get(Controls::Interact, i) && !u.input->get(Controls::Interact, i))
			{
				global.multiplayer.local_player_mask |= 1 << i;
			}
			else if (player_active
				&& u.last_input->get(Controls::Cancel, i) && !u.input->get(Controls::Cancel, i))
			{
				global.multiplayer.local_player_mask &= ~(1 << i);
			}
		}
	}

#if !SERVER
	Net::Client::master_keepalive();
#endif

	if (multiplayer_can_switch_tab())
	{
		if (u.last_input->get(Controls::TabLeft, 0) && !u.input->get(Controls::TabLeft, 0))
			multiplayer_switch_tab(ServerListType(vi_max(0, s32(global.multiplayer.tab) - 1)));
		if (u.last_input->get(Controls::TabRight, 0) && !u.input->get(Controls::TabRight, 0))
			multiplayer_switch_tab(ServerListType(vi_min(s32(ServerListType::count) - 1, s32(global.multiplayer.tab) + 1)));
	}

	if (data.multiplayer.tab_timer > 0.0f)
	{
		data.multiplayer.tab_timer = vi_max(0.0f, data.multiplayer.tab_timer - u.time.delta);
		if (data.multiplayer.tab_timer == 0.0f)
			data.multiplayer.menu[s32(data.multiplayer.edit_mode)].animate();
	}
	else
	{
		switch (data.multiplayer.state)
		{
			case Data::Multiplayer::State::Browse:
			{
				multiplayer_browse_update(u);
				break;
			}
			case Data::Multiplayer::State::EntryEdit:
			{
				multiplayer_entry_edit_update(u);
				return;
				break;
			}
			case Data::Multiplayer::State::EntryView:
			{
				multiplayer_entry_view_update(u);
				break;
			}
			default:
				vi_assert(false);
				break;
		}
	}
}

void tab_draw_label(const RenderParams& p, const char* label, const Vec2& pos, const Vec4& color, const Vec4& text_color = UI::color_background)
{
	UI::box(p, { pos, TAB_SIZE }, color);

	UIText text;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Min;
	text.color = text_color;
	text.text(0, label);
	text.draw(p, pos + Vec2(PADDING));
}

void tab_draw_body(const RenderParams& p, const Vec2& pos, r32 width, const Vec4& color, const Vec4& background_color)
{
	if (width > 0.0f)
	{
		Vec2 size(width, MAIN_VIEW_SIZE.y);
		if (background_color.w > 0.0f)
			UI::box(p, { pos, size }, background_color);
		UI::border(p, { pos, size }, BORDER, color);
	}
}

void tab_draw(const RenderParams& p, const char* label, const Vec2& pos, r32 width, const Vec4& color, const Vec4& background_color, const Vec4& text_color = UI::color_background)
{
	tab_draw_body(p, pos, width, color, background_color);
	tab_draw_label(p, label, pos + Vec2(0, MAIN_VIEW_SIZE.y), color, text_color);
}

void draw_gamepad_icon(const RenderParams& p, const Vec2& pos, s32 index, const Vec4& color, r32 scale = 1.0f)
{
	UI::mesh(p, Asset::Mesh::icon_gamepad, pos, Vec2(48.0f * scale * UI::scale), color);
	UIText text;
	text.size *= scale;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;
	text.color = UI::color_background;
	text.text(0, "%d", index + 1);
	text.draw(p, pos);
}

void multiplayer_top_bar_draw(const RenderParams& params, const Vec2& pos, const Vec2& top_bar_size)
{
	UI::box(params, { pos, top_bar_size }, UI::color_background);

	UIText text;
	text.size = TEXT_SIZE;
	text.anchor_y = UIText::Anchor::Center;
	text.color = UI::color_accent();
	switch (data.multiplayer.state)
	{
		case Data::Multiplayer::State::Browse:
			text.text(0, "%s    %s    %s", _(strings::prompt_select), _(strings::prompt_entry_create), _(strings::prompt_back));
			break;
		case Data::Multiplayer::State::EntryView:
		{
			if (data.multiplayer.active_server.is_admin)
				text.text(0, "%s    %s    %s", _(strings::prompt_connect), _(strings::prompt_entry_edit), _(strings::prompt_back));
			else
				text.text(0, "%s    %s", _(strings::prompt_connect), _(strings::prompt_back));
			break;
		}
		case Data::Multiplayer::State::EntryEdit:
			text.text(0, "%s    %s", _(strings::prompt_entry_save), _(strings::prompt_back));
			break;
		default:
			vi_assert(false);
			break;
	}
	UIMenu::text_clip(&text, data.multiplayer.state_transition_time, 100.0f);
	if (Game::ui_gamepad_types[0] == Gamepad::Type::None)
	{
		text.anchor_x = UIText::Anchor::Min;
		text.draw(params, pos + Vec2(PADDING, top_bar_size.y * 0.5f));
	}
	else
	{
		text.anchor_x = UIText::Anchor::Max;
		text.draw(params, pos + Vec2(top_bar_size.x - PADDING, top_bar_size.y * 0.5f));
	}
}

void game_type_string(UIText* text, GameType type, s8 team_count, s8 max_players)
{
	AssetID teams_type;
	switch (team_count)
	{
		case 2:
		{
			if (max_players == 2)
				teams_type = strings::teams_type_1v1;
			else
				teams_type = strings::teams_type_team;
			break;
		}
		case 3:
		{
			if (max_players == 3)
				teams_type = strings::teams_type_free_for_all;
			else
				teams_type = strings::teams_type_cutthroat;
			break;
		}
		case 4:
		{
			if (max_players == 4)
				teams_type = strings::teams_type_free_for_all;
			else
				teams_type = strings::teams_type_team;
			break;
		}
		default:
		{
			teams_type = AssetNull;
			vi_assert(false);
			break;
		}
	}
	AssetID game_type;
	switch (type)
	{
		case GameType::Assault:
		{
			game_type = strings::game_type_assault;
			break;
		}
		case GameType::Deathmatch:
		{
			game_type = strings::game_type_deathmatch;
			break;
		}
		default:
		{
			game_type = AssetNull;
			vi_assert(false);
			break;
		}
	}
	text->text(0, "%s %s", _(teams_type), _(game_type));
}

void multiplayer_browse_draw(const RenderParams& params, const Rect2& rect)
{
	Vec2 panel_size(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);
	Vec2 top_bar_size(panel_size.x, panel_size.y * 1.5f);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

	multiplayer_top_bar_draw(params, pos, top_bar_size);

	pos.y -= panel_size.y + PADDING * 1.5f;

	// server list
	{
		UIText text;
		text.size = TEXT_SIZE;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Center;
		text.font = Asset::Font::pt_sans;

		const Data::Multiplayer::ServerList& list = data.multiplayer.server_lists[s32(global.multiplayer.tab)];
		list.scroll.start(params, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
		for (s32 i = list.scroll.top(); i < list.scroll.bottom(list.entries.length); i++)
		{
			const Net::Master::ServerListEntry& entry = list.entries[i];
			b8 selected = i == list.selected;

			UI::box(params, { pos, panel_size }, UI::color_background);

			if (selected)
				UI::border(params, Rect2(pos, panel_size).outset(-2.0f * UI::scale), 2.0f, UI::color_accent());

			text.color = selected ? UI::color_accent() : UI::color_default;
			text.clip = 48;
			text.text_raw(0, entry.name, UITextFlagSingleLine);
			text.draw(params, pos + Vec2(PADDING, panel_size.y * 0.5f));

			text.clip = 18;
			text.text_raw(0, entry.creator_username, UITextFlagSingleLine);
			text.draw(params, pos + Vec2(panel_size.x * 0.4f, panel_size.y * 0.5f));
			text.clip = 0;

			if (entry.server_state.level != AssetNull)
			{
				text.text_raw(0, Loader::level_name(entry.server_state.level));
				text.draw(params, pos + Vec2(panel_size.x * 0.55f, panel_size.y * 0.5f));
			}

			{
				game_type_string(&text, entry.game_type, entry.team_count, entry.max_players);
				text.draw(params, pos + Vec2(panel_size.x * 0.7f, panel_size.y * 0.5f));
			}

			{
				if (!selected)
					text.color = UI::color_default;
				text.text(0, "%d/%d", s32(entry.max_players - entry.server_state.player_slots), s32(entry.max_players));
				text.draw(params, pos + Vec2(panel_size.x * 0.85f, panel_size.y * 0.5f));
			}

			if (entry.server_state.level != AssetNull) // there's a server running this config
			{
				r32 p = ping(entry.addr);
				if (p > 0.0f)
				{
					if (!selected)
						text.color = UI::color_ping(p);
					text.text(0, _(strings::ping), s32(p * 1000.0f));
					text.draw(params, pos + Vec2(panel_size.x * 0.9f, panel_size.y * 0.5f));
				}
			}

			pos.y -= panel_size.y;
		}
		list.scroll.end(params, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
	}
}

void multiplayer_entry_edit_draw(const RenderParams& params, const Rect2& rect)
{
	if (data.multiplayer.request_id)
	{
		AssetID str;
		switch (data.multiplayer.request_type)
		{
			case Data::Multiplayer::RequestType::ConfigSave:
				str = strings::entry_saving;
				break;
			case Data::Multiplayer::RequestType::ConfigGet:
				str = strings::loading;
				break;
			default:
			{
				str = AssetNull;
				vi_assert(false);
				break;
			}
		}
		Menu::progress_infinite(params, _(str), rect.pos + rect.size * 0.5f);
	}
	else
	{
		switch (data.multiplayer.edit_mode)
		{
			case Data::Multiplayer::EditMode::Name:
			{
				Vec2 field_size(rect.size.x + PADDING * -4.0f, MENU_ITEM_HEIGHT);
				Rect2 field_rect =
				{
					rect.pos + (rect.size * 0.5f) + (field_size * -0.5f),
					field_size
				};

				UIText text;
				text.size = TEXT_SIZE;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Min;

				{
					// prompt
					text.color = UI::color_default;
					text.text(0, _(strings::prompt_name));
					UIMenu::text_clip(&text, data.multiplayer.state_transition_time, 100.0f);
					text.draw(params, field_rect.pos + Vec2(0, field_rect.size.y + PADDING));
					text.clip = 0;

					// accept/cancel control prompts

					// accept
					Rect2 controls_rect = field_rect;
					controls_rect.pos.y -= MENU_ITEM_HEIGHT + PADDING;

					text.wrap_width = 0;
					text.anchor_y = UIText::Anchor::Min;
					text.anchor_x = UIText::Anchor::Min;
					text.color = UI::color_accent();
					text.text(0, _(strings::prompt_accept_text));
					Vec2 prompt_pos = controls_rect.pos + Vec2(PADDING);
					text.draw(params, prompt_pos);

					// cancel
					text.anchor_x = UIText::Anchor::Max;
					text.color = UI::color_alert();
					text.clip = 0;
					text.text(0, _(strings::prompt_cancel));
					text.draw(params, prompt_pos + Vec2(controls_rect.size.x + PADDING * -2.0f, 0));
				}

				{
					// text field
					UI::box(params, field_rect, UI::color_background);
					UI::border(params, field_rect, 2.0f, UI::color_accent());

					text.font = Asset::Font::pt_sans;
					text.size = TEXT_SIZE;
					text.anchor_x = UIText::Anchor::Min;
					Loader::font(text.font);
					text.color = UI::color_default;
					Array<char>* value = &data.multiplayer.text_field.value;
					if (value->length > 64 + 1) // truncate
					{
						const char* start = value->data;
						while (start < &value->data[value->length - 64])
							start = Font::codepoint_next(start);
						text.text_raw(0, start, UITextFlagSingleLine);
					}
					else
						text.text_raw(0, value->data, UITextFlagSingleLine);
					text.draw(params, field_rect.pos + Vec2(PADDING * 0.8125f));
				}
				break;
			}
			case Data::Multiplayer::EditMode::Levels:
			case Data::Multiplayer::EditMode::AddLevel:
			case Data::Multiplayer::EditMode::Main:
			case Data::Multiplayer::EditMode::AllowedUpgrades:
			case Data::Multiplayer::EditMode::StartUpgrades:
			case Data::Multiplayer::EditMode::AddStartUpgrade:
			{
				const Rect2& vp = params.camera->viewport;
				
				Vec2 panel_size(((rect.size.x + PADDING * -2.0f) / 3.0f) + PADDING * -2.0f, PADDING + TEXT_SIZE * UI::scale);
				Vec2 top_bar_size(rect.size.x, panel_size.y * 1.5f);
				Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

				multiplayer_top_bar_draw(params, pos, top_bar_size);
				pos.y -= PADDING;
				pos.x = vp.size.x * 0.5f + MENU_ITEM_WIDTH * -0.5f;

				// top header
				UIText text;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.color = UI::color_default;
				text.wrap_width = MENU_ITEM_WIDTH + (PADDING * -2.0f);
				text.size = MENU_ITEM_FONT_SIZE;
				if (data.multiplayer.active_server.config.name[0] == '\0')
					text.text(0, _(strings::entry_create));
				else
				{
					text.font = Asset::Font::pt_sans;
					text.text_raw(0, data.multiplayer.active_server.config.name, UITextFlagSingleLine);
				}
				UIMenu::text_clip(&text, data.multiplayer.state_transition_time, 100.0f, 28);

				{
					Vec2 text_pos = pos + Vec2(PADDING, 0);
					UI::box(params, text.rect(text_pos).outset(PADDING), UI::color_background);
					text.draw(params, text_pos);
					pos.y = text.rect(text_pos).pos.y + PADDING * -3.0f;
				}

				// secondary header
				if (data.multiplayer.edit_mode != Data::Multiplayer::EditMode::Main)
				{
					text.font = Asset::Font::lowpoly;
					switch (data.multiplayer.edit_mode)
					{
						case Data::Multiplayer::EditMode::Levels:
						{
							text.text(0, _(strings::levels));
							break;
						}
						case Data::Multiplayer::EditMode::AddLevel:
						{
							text.text(0, _(strings::add_level));
							break;
						}
						case Data::Multiplayer::EditMode::AllowedUpgrades:
						{
							text.text(0, _(strings::allow_upgrades));
							break;
						}
						case Data::Multiplayer::EditMode::StartUpgrades:
						{
							text.text(0, _(strings::start_upgrades));
							break;
						}
						case Data::Multiplayer::EditMode::AddStartUpgrade:
						{
							text.text(0, _(strings::add_start_upgrade));
							break;
						}
						default:
							vi_assert(false);
							break;
					}
					UIMenu::text_clip(&text, data.multiplayer.state_transition_time + 0.1f, 100.0f);

					{
						Vec2 text_pos = pos + Vec2(PADDING, 0);
						UI::box(params, text.rect(text_pos).outset(PADDING), UI::color_background);
						text.draw(params, text_pos);
						pos.y = text.rect(text_pos).pos.y + PADDING * -3.0f;
					}
				}

				data.multiplayer.menu[s32(data.multiplayer.edit_mode)].draw_ui(params, pos, UIText::Anchor::Min, UIText::Anchor::Max);
				break;
			}
			default:
				vi_assert(false);
				break;
		}
	}
}

void multiplayer_entry_view_draw(const RenderParams& params, const Rect2& rect)
{
	if (data.multiplayer.request_id && !data.multiplayer.active_server_dirty) // don't have any config data yet
	{
		vi_assert(data.multiplayer.request_type == Data::Multiplayer::RequestType::ConfigGet);
		Menu::progress_infinite(params, _(strings::loading), rect.pos + rect.size * 0.5f);
	}
	else
	{
		Vec2 panel_size(((rect.size.x + PADDING * -2.0f) / 3.0f) + PADDING * -2.0f, PADDING + TEXT_SIZE * UI::scale);
		Vec2 top_bar_size(rect.size.x, panel_size.y * 1.5f);
		Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

		multiplayer_top_bar_draw(params, pos, top_bar_size);
		pos += Vec2(PADDING, PADDING * -2.0f);

		const Net::Master::ServerDetails& details = data.multiplayer.active_server;

		UIText text;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Max;
		text.color = UI::color_default;
		text.size = MENU_ITEM_FONT_SIZE;
		text.font = Asset::Font::pt_sans;
		text.wrap(rect.size.x + PADDING * -2.0f);
		{
			char buffer[UI_TEXT_MAX + 1];
			snprintf(buffer, UI_TEXT_MAX, _(strings::map_by), details.config.name, details.creator_username);
			text.text_raw(0, buffer);
		}

		{
			Rect2 r = text.rect(pos).outset(PADDING);
			UI::box(params, r, UI::color_background);
			text.draw(params, pos);
			pos.y = r.pos.y + PADDING * -2.0f;
		}

		text.font = Asset::Font::lowpoly;
		text.size = TEXT_SIZE;
		text.wrap_width = 0;

		Vec2 top = pos;

		UIText value = text;
		value.anchor_x = UIText::Anchor::Max;

		// column 1
		{
			s32 rows = (details.state.level == AssetNull ? 1 : 2) + 7;
			UI::box(params, { pos + Vec2(-PADDING, panel_size.y * -rows), Vec2(panel_size.x + PADDING * 2.0f, panel_size.y * rows + PADDING) }, UI::color_background);

			if (details.state.level == AssetNull)
			{
				// max players
				text.color = UI::color_default;
				text.text(0, _(strings::max_players));
				text.draw(params, pos);
				value.color = UI::color_default;
				value.text(0, "%d", s32(details.config.max_players));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
			else
			{
				// level name
				text.color = UI::color_accent();
				text.text(0, Loader::level_name(details.state.level));
				text.draw(params, pos);
				text.color = UI::color_default;
				pos.y -= panel_size.y;

				// players
				text.text(0, _(strings::player_count), s32(details.config.max_players - details.state.player_slots), s32(details.config.max_players));
				text.draw(params, pos);

				// ping
				r32 p = ping(details.addr);
				if (p > 0.0f)
				{
					value.color = UI::color_ping(p);
					value.text(0, _(strings::ping), s32(p * 1000.0f));
					value.draw(params, pos + Vec2(panel_size.x, 0));
					value.color = UI::color_default;
				}
				pos.y -= panel_size.y;
			}

			// game type
			game_type_string(&text, details.config.game_type, details.config.team_count, details.config.max_players);
			text.draw(params, pos);
			pos.y -= panel_size.y;

			// time limit
			text.text(0, _(strings::time_limit));
			text.draw(params, pos);
			value.text(0, "%d:00", s32(details.config.time_limit_minutes));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// kill limit
			if (details.config.game_type == GameType::Deathmatch)
			{
				text.text(0, _(strings::kill_limit));
				text.draw(params, pos);
				value.text(0, "%d", s32(details.config.kill_limit));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
			else
			{
				// respawns
				text.text(0, _(strings::drones));
				text.draw(params, pos);
				value.text(0, "%d", s32(details.config.respawns));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}

			// drone shield
			text.text(0, _(strings::drone_shield));
			text.draw(params, pos);
			value.text(0, "%d", s32(details.config.drone_shield));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// enable minions
			text.text(0, _(strings::enable_minions));
			text.draw(params, pos);
			value.text(0, _(details.config.enable_minions ? strings::on : strings::off));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// start energy
			text.text(0, _(strings::start_energy));
			text.draw(params, pos);
			value.text(0, "%d", s32(details.config.start_energy));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// bots
			text.text(0, _(strings::fill_bots));
			text.draw(params, pos);
			value.text(0, _(details.config.fill_bots ? strings::on : strings::off));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;
		}

		// column 2
		pos = top + Vec2(panel_size.x + PADDING * 3.0f, 0);
		{
			s32 rows = 1 + details.config.levels.length;
			UI::box(params, { pos + Vec2(-PADDING, panel_size.y * -rows), Vec2(panel_size.x + PADDING * 2.0f, panel_size.y * rows + PADDING) }, UI::color_background);

			// levels
			if (details.config.levels.length > 0)
				text.color = UI::color_accent();
			text.text(0, _(strings::levels));
			text.draw(params, pos);
			text.color = UI::color_default;
			if (details.config.levels.length == 0)
			{
				value.text(0, _(strings::none));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
			else
			{
				pos.y -= panel_size.y;
				for (s32 i = 0; i < details.config.levels.length; i++)
				{
					text.text_raw(0, Loader::level_name(details.config.levels[i]));
					text.draw(params, pos);
					pos.y -= panel_size.y;
				}
			}
		}

		// column 3
		pos = top + Vec2((panel_size.x + PADDING * 3.0f) * 2.0f, 0);
		{
			s32 rows = vi_max(1, s32(details.config.start_upgrades.length)) + 1 + s32(Upgrade::count);
			UI::box(params, { pos + Vec2(-PADDING, panel_size.y * -rows), Vec2(panel_size.x + PADDING * 2.0f, panel_size.y * rows + PADDING) }, UI::color_background);

			// start upgrades
			if (details.config.start_upgrades.length > 0)
				text.color = UI::color_accent();
			text.text(0, _(strings::start_upgrades));
			text.draw(params, pos);
			text.color = UI::color_default;
			if (details.config.start_upgrades.length == 0)
			{
				value.text(0, _(strings::none));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
			else
			{
				pos.y -= panel_size.y;
				for (s32 i = 0; i < details.config.start_upgrades.length; i++)
				{
					text.text(0, _(UpgradeInfo::list[s32(details.config.start_upgrades[i])].name));
					text.draw(params, pos);
					pos.y -= panel_size.y;
				}
			}

			// allow upgrades
			text.color = UI::color_accent();
			text.text(0, _(strings::allow_upgrades));
			text.draw(params, pos);
			pos.y -= panel_size.y;
			text.color = UI::color_default;
			for (s32 i = 0; i < s32(Upgrade::count); i++)
			{
				text.text(0, _(UpgradeInfo::list[i].name));
				text.draw(params, pos);
				value.text(0, _((details.config.allow_upgrades & (1 << i)) ? strings::on : strings::off));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
		}
	}
}

void multiplayer_draw(const RenderParams& params)
{
	if (Menu::main_menu_state == Menu::State::Hidden)
	{
		const Rect2& vp = params.camera->viewport;
		Vec2 center = vp.size * 0.5f;
		Vec2 tab_size = TAB_SIZE;
		Rect2 rect =
		{
			center + MAIN_VIEW_SIZE * -0.5f + Vec2(0, -tab_size.y),
			MAIN_VIEW_SIZE,
		};

		// left/right tab control prompt
		if (multiplayer_can_switch_tab())
		{
			UIText text;
			text.size = TEXT_SIZE;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::color_default;
			text.text(0, "[{{TabLeft}}]");

			Vec2 pos = rect.pos + Vec2(tab_size.x * 0.5f, rect.size.y + tab_size.y * 1.5f);
			UI::box(params, text.rect(pos).outset(PADDING), UI::color_background);
			text.draw(params, pos);

			pos.x += rect.size.x - tab_size.x;
			text.text(0, "[{{TabRight}}]");
			UI::box(params, text.rect(pos).outset(PADDING), UI::color_background);
			text.draw(params, pos);
		}

		// tab labels
		{
			Vec2 pos = rect.pos + Vec2(0, rect.size.y);
			for (s32 i = 0; i < s32(ServerListType::count); i++)
			{
				const Vec4* color;
				if (ServerListType(i) == global.multiplayer.tab)
				{
					if (data.multiplayer.tab_timer > 0.0f)
						color = UI::flash_function(Game::real_time.total) ? &UI::color_default : &UI::color_disabled();
					else
						color = &UI::color_accent();
				}
				else
					color = &UI::color_disabled();

				tab_draw_label(params, _(multiplayer_tab_names[i]), pos, *color);
				pos.x += tab_size.x + PADDING;
			}
		}

		// main tab body
		{
			r32 main_offset;
			r32 main_width;
			const Vec4* main_color;

			// animate new tab in
			if (data.multiplayer.tab_timer > 0.0f)
			{
				main_color = UI::flash_function(Game::real_time.total) ? &UI::color_default : &UI::color_disabled();

				r32 blend = Ease::cubic_out<r32>(1.0f - vi_min(1.0f, data.multiplayer.tab_timer / TAB_ANIMATION_TIME));
				main_width = blend * rect.size.x;

				if (s32(global.multiplayer.tab) > s32(global.multiplayer.tab_previous))
					main_offset = (1.0f - blend) * rect.size.x; // tab comes in from the right
				else
					main_offset = 0.0f; // tab comes in from the left
			}
			else
			{
				// no animation
				main_color = &UI::color_accent();
				main_offset = 0.0f;
				main_width = rect.size.x;
			}
			tab_draw_body(params, rect.pos + Vec2(main_offset, 0), main_width, *main_color, Vec4(UI::color_background.xyz(), OPACITY));
		}

		// content
		if (data.multiplayer.tab_timer == 0.0f)
		{
			Rect2 rect_padded =
			{
				rect.pos + Vec2(PADDING),
				rect.size + Vec2(PADDING * -2.0f),
			};
			switch (data.multiplayer.state)
			{
				case Data::Multiplayer::State::Browse:
				{
					multiplayer_browse_draw(params, rect_padded);
					break;
				}
				case Data::Multiplayer::State::EntryEdit:
				{
					multiplayer_entry_edit_draw(params, rect_padded);
					break;
				}
				case Data::Multiplayer::State::EntryView:
				{
					multiplayer_entry_view_draw(params, rect_padded);
					break;
				}
				default:
					vi_assert(false);
					break;
			}
		}

		// gamepads
		{
			s32 gamepad_count = 1;
			for (s32 i = 1; i < MAX_GAMEPADS; i++)
			{
				if (params.sync->input.gamepads[i].type != Gamepad::Type::None)
					gamepad_count = i + 1;
			}

			if (gamepad_count > 1)
			{
				const r32 gamepad_spacing = 128.0f * UI::scale * SCALE_MULTIPLIER;
				Vec2 pos = vp.size * Vec2(0.5f, 0.1f) + Vec2(gamepad_spacing * (gamepad_count - 1) * -0.5f, 0);
				for (s32 i = 0; i < gamepad_count; i++)
				{
					draw_gamepad_icon(params, pos, i, (global.multiplayer.local_player_mask & (1 << i)) ? UI::color_accent() : UI::color_disabled(), SCALE_MULTIPLIER);
					pos.x += gamepad_spacing;
				}
			}
		}

		// logs
		PlayerHuman::draw_logs(params, AI::TeamNone, 0);
	}
}

void go(AssetID zone)
{
	vi_assert(Game::level.local);
	if (Game::session.type == SessionType::Story)
	{
		PlayerControlHuman* player = PlayerControlHuman::list.iterator().item();
		Game::save.zone_current_restore = true;
		Game::save.zone_current_restore_position = player->get<Transform>()->absolute_pos();
		Game::save.zone_current_restore_rotation = player->get<PlayerCommon>()->angle_horizontal;
	}
	Game::schedule_load_level(zone, Game::Mode::Pvp);
}

void focus_camera(const Update& u, const Vec3& target_pos, const Quat& target_rot)
{
	data.camera_pos += (target_pos - data.camera_pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
	data.camera_rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), data.camera_rot, target_rot);
}

void focus_camera(const Update& u, const ZoneNode& zone)
{
	Vec3 target_pos = zone.pos() + zone.rot * global.camera_offset_pos;
	focus_camera(u, target_pos, zone.rot * global.camera_offset_rot);
}

const ZoneNode* zone_node_get(AssetID id)
{
	for (s32 i = 0; i < global.zones.length; i++)
	{
		if (global.zones[i].id == id)
			return &global.zones[i];
	}
	return nullptr;
}

s16 energy_increment_zone(const ZoneNode& zone)
{
	return zone.size * (zone.max_teams == MAX_TEAMS ? 200 : 10);
}

s16 energy_increment_total()
{
	s16 result = 0;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		ZoneState zone_state = Game::save.zones[zone.id];
		if (zone_state == ZoneState::PvpFriendly)
			result += energy_increment_zone(zone);
	}
	if (Game::save.group != Net::Master::Group::None)
		result = result / 8;
	return result;
}

void select_zone_update(const Update& u, b8 enable_movement)
{
	if (UIMenu::active[0])
		return;

	const ZoneNode* zone = zone_node_get(data.zone_selected);
	if (!zone)
		return;

	// movement
	if (enable_movement)
	{
		Vec2 movement = PlayerHuman::camera_topdown_movement(u, 0, data.camera.ref());
		r32 movement_amount = movement.length();
		if (movement_amount > 0.0f)
		{
			movement /= movement_amount;
			const ZoneNode* closest = nullptr;
			r32 closest_dot = FLT_MAX;

			for (s32 i = 0; i < zone->children.length; i++)
			{
				const Vec3& zone_pos = zone->children[i];
				for (s32 j = 0; j < global.zones.length; j++)
				{
					const ZoneNode& candidate = global.zones[j];
					if (&candidate == zone)
						continue;

					for (s32 k = 0; k < candidate.children.length; k++)
					{
						const Vec3& candidate_pos = candidate.children[k];
						Vec3 to_candidate = candidate_pos - zone_pos;
						if (movement.dot(Vec2::normalize(Vec2(to_candidate.x, to_candidate.z))) > 0.707f)
						{
							r32 dot = movement.dot(Vec2(to_candidate.x, to_candidate.z));
							if (dot < closest_dot)
							{
								closest = &candidate;
								closest_dot = dot;
							}
						}
					}
				}
			}
			if (closest)
			{
				data.zone_selected = closest->id;
				Audio::post_global_event(AK::EVENTS::PLAY_OVERWORLD_MOVE);
			}
		}
	}

	focus_camera(u, *zone);
}

Vec3 zone_color(const ZoneNode& zone)
{
	if (Game::session.type == SessionType::Story)
	{
		ZoneState zone_state = Game::save.zones[zone.id];
		switch (zone_state)
		{
			case ZoneState::Locked:
			{
				return Vec3(0.25f);
			}
			case ZoneState::ParkourUnlocked:
			{
				return Vec3(1.0f);
			}
			case ZoneState::PvpFriendly:
			{
				return Team::color_friend.xyz();
			}
			case ZoneState::PvpHostile:
			{
				return Team::color_enemy.xyz();
			}
			default:
			{
				vi_assert(false);
				return Vec3::zero;
			}
		}
	}
	else
		return Team::color_friend.xyz();
}

const Vec4& zone_ui_color(const ZoneNode& zone)
{
	vi_assert(Game::session.type == SessionType::Story);
	ZoneState zone_state = Game::save.zones[zone.id];
	switch (zone_state)
	{
		case ZoneState::Locked:
		{
			return UI::color_disabled();
		}
		case ZoneState::ParkourUnlocked:
		{
			return UI::color_default;
		}
		case ZoneState::PvpFriendly:
		{
			return Team::ui_color_friend;
		}
		case ZoneState::PvpHostile:
		{
			return Team::ui_color_enemy;
		}
		default:
		{
			vi_assert(false);
			return UI::color_default;
		}
	}
}

void zone_draw_mesh(const RenderParams& params, AssetID mesh, const Vec3& pos, const Vec4& color)
{
	Loader::mesh_permanent(mesh);
	Loader::shader(Asset::Shader::standard_flat);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_flat);
	sync->write(params.technique);

	Mat4 m;
	m.make_translate(pos);
	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(mesh);
}

#define BACKGROUND_COLOR Vec4(0.8f, 0.8f, 0.8f, 1)

// returns current zone node
const ZoneNode* zones_draw(const RenderParams& params)
{
	vi_assert(Game::session.type == SessionType::Story);

	if (data.timer_deploy > 0.0f || Game::scheduled_load_level != AssetNull)
		return nullptr;

	// highlight zone locations
	const ZoneNode* selected_zone = zone_node_get(data.zone_selected);

	// "you are here"
	const ZoneNode* current_zone = zone_node_get(Game::level.id);
	Vec2 p;
	if (current_zone && UI::project(params, current_zone->pos(), &p))
		UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_accent(), PI);

	// highlight selected zone
	if (selected_zone)
	{
		Vec2 p;
		if (UI::project(params, selected_zone->pos(), &p))
			UI::triangle_border(params, { p, Vec2(48.0f * UI::scale) }, BORDER * 2.0f, UI::color_accent(), PI);

		// cooldown timer
		r64 lost_timer = ZONE_LOST_COOLDOWN - (platform::timestamp() - Game::save.zone_lost_times[selected_zone->id]);
		if (lost_timer > 0.0f)
		{
			UIText text;
			text.color = UI::color_alert();
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;

			{
				s32 remaining_minutes = lost_timer / 60.0;
				s32 remaining_seconds = lost_timer - (remaining_minutes * 60.0);
				text.text(0, _(strings::timer), remaining_minutes, remaining_seconds);
			}

			{
				Vec2 text_pos = p;
				text_pos.y += 32.0f * UI::scale;
				UI::box(params, text.rect(text_pos).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, text_pos);
			}
		}
	}

	// zone under attack
	const ZoneNode* under_attack = zone_node_get(zone_under_attack());
	if (under_attack)
	{
		Vec2 p;
		if (UI::is_onscreen(params, under_attack->pos(), &p))
		{
			if (UI::flash_function(Game::real_time.total))
				UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_alert(), PI);
		}
		else
		{
			if (UI::flash_function(Game::real_time.total))
				UI::indicator(params, under_attack->pos(), UI::color_alert(), true, 1.0f, PI);
		}

		UIText text;
		text.color = UI::color_alert();
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		r32 time = zone_under_attack_timer();
		if (time > 0.0f)
			text.text(0, "%d", s32(ceilf(time)));
		else
			text.text(0, _(strings::zone_defense_expired));

		{
			Vec2 text_pos = p;
			text_pos.y += 32.0f * UI::scale;
			UI::box(params, text.rect(text_pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(params, text_pos);
		}
	}

	return selected_zone;
}

b8 zone_can_capture(AssetID zone_id)
{
	if (zone_id == Game::level.id)
		return false;

	switch (Game::save.zones[zone_id])
	{
		case ZoneState::PvpFriendly:
		{
			// tolerate time differences on the server
#if SERVER
			return zone_id == zone_under_attack();
#else
			return zone_under_attack_timer() > 0.0f && zone_id == zone_under_attack();
#endif
		}
		case ZoneState::PvpHostile:
		{
			// tolerate time differences on the server
#if SERVER
			return true;
#else
			return platform::timestamp() - Game::save.zone_lost_times[zone_id] > ZONE_LOST_COOLDOWN;
#endif
		}
		default:
		{
			return false;
		}
	}
}

namespace OverworldNet
{
	enum class Message : s8
	{
		CaptureOrDefend,
		ZoneUnderAttack,
		ZoneChange,
		ResourceChange,
		Buy,
		count,
	};

	b8 capture_or_defend(AssetID zone)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::CaptureOrDefend;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		Net::msg_finalize(p);
		return true;
	}

	b8 zone_under_attack(AssetID zone)
	{
		vi_assert(Game::level.local);
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::ZoneUnderAttack;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		Net::msg_finalize(p);
		return true;
	}

	b8 zone_change(AssetID zone, ZoneState state)
	{
		vi_assert(Game::level.local);
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::ZoneChange;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		serialize_enum(p, ZoneState, state);
		Net::msg_finalize(p);
		return true;
	}

	b8 resource_change(Resource r, s16 delta)
	{
		vi_assert(Game::level.local);
		if (delta != 0)
		{
			using Stream = Net::StreamWrite;
			Stream* p = Net::msg_new(Net::MessageType::Overworld);
			Message m = Message::ResourceChange;
			serialize_enum(p, Message, m);
			serialize_enum(p, Resource, r);
			serialize_s16(p, delta);
			Net::msg_finalize(p);
		}
		return true;
	}

	b8 buy(Resource r, s16 quantity)
	{
		vi_assert(quantity != 0);
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::Buy;
		serialize_enum(p, Message, m);
		serialize_enum(p, Resource, r);
		serialize_s16(p, quantity);
		Net::msg_finalize(p);
		return true;
	}
}

void resource_change(Resource r, s16 delta)
{
	OverworldNet::resource_change(r, delta);
}

AssetID zone_under_attack()
{
	return Game::session.zone_under_attack;
}

r32 zone_under_attack_timer()
{
	return vi_max(0.0f, Game::session.zone_under_attack_timer - (ZONE_UNDER_ATTACK_TIME - ZONE_UNDER_ATTACK_THRESHOLD));
}

b8 net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	OverworldNet::Message type;
	serialize_enum(p, OverworldNet::Message, type);

	switch (type)
	{
		case OverworldNet::Message::CaptureOrDefend:
		{
			AssetID zone;
			serialize_s16(p, zone);

			// only server accepts CaptureOrDefend messages
			if (Game::level.local && Game::session.type == SessionType::Story)
			{
				if (zone_node_get(zone) && zone_can_capture(zone))
				{
					if (Game::save.resources[s32(Resource::Drones)] >= DEFAULT_ASSAULT_DRONES
						&& (Game::save.zones[zone] == ZoneState::PvpFriendly || Game::save.resources[s32(Resource::AccessKeys)] > 0))
					{
						resource_change(Resource::Drones, -DEFAULT_ASSAULT_DRONES);
						if (Game::save.zones[zone] != ZoneState::PvpFriendly)
							resource_change(Resource::AccessKeys, -1);
						go(zone);
					}
				}
			}
			break;
		}
		case OverworldNet::Message::ZoneUnderAttack:
		{
			AssetID zone;
			serialize_s16(p, zone);

			// server does not accept ZoneUnderAttack messages from client
			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				Game::session.zone_under_attack = zone;
				Game::session.zone_under_attack_timer = zone == AssetNull ? 0.0f : ZONE_UNDER_ATTACK_TIME;
			}
			break;
		}
		case OverworldNet::Message::ZoneChange:
		{
			AssetID zone;
			serialize_s16(p, zone);

			ZoneState state;
			serialize_enum(p, ZoneState, state);
			// server does not accept ZoneChange messages from client
			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				Game::save.zones[zone] = state;
				data.story.map.zones_change_time[zone] = Game::real_time.total;
			}
			break;
		}
		case OverworldNet::Message::ResourceChange:
		{
			Resource r;
			serialize_enum(p, Resource, r);
			s16 delta;
			serialize_s16(p, delta);
			// server does not accept ResourceChange messages from client
			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				Game::save.resources[s32(r)] += delta;
				vi_assert(Game::save.resources[s32(r)] >= 0);
				if (r != Resource::Energy)
					data.story.inventory.resource_change_time[s32(r)] = Game::real_time.total;
			}
			break;
		}
		case OverworldNet::Message::Buy:
		{
			Resource r;
			serialize_enum(p, Resource, r);
			s16 amount;
			serialize_s16(p, amount);
			// only server accepts Buy messages
			if (Game::level.local && Game::session.type == SessionType::Story && amount > 0 && Interactable::is_present(Interactable::Type::Shop))
			{
				s32 cost = s32(amount) * s32(resource_info[s32(r)].cost);
				if (cost <= Game::save.resources[s32(Resource::Energy)])
				{
					resource_change(r, amount);
					resource_change(Resource::Energy, -cost);
				}
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	return true;
}

void hide()
{
	if (data.timer_transition == 0.0f)
	{
		data.timer_transition = TRANSITION_TIME;
		data.state_next = State::Hidden;
		Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_OUT);
	}
}

void hide_complete()
{
	Particles::clear();
	if (data.camera.ref())
	{
		memcpy(data.camera.ref(), &data.camera_restore_data, sizeof(Camera));
		data.camera = nullptr;
	}
	data.state = data.state_next = State::Hidden;
	Audio::post_global_event(AK::EVENTS::STOP_AMBIENCE_OVERWORLD);
}

void deploy_done()
{
	vi_assert(Game::session.type == SessionType::Story);
	OverworldNet::capture_or_defend(data.zone_selected);
}

void deploy_update(const Update& u)
{
	const ZoneNode& zone = *zone_node_get(data.zone_selected);
	focus_camera(u, zone);

	// must use Game::real_time because Game::time is paused when overworld is active

	r32 old_timer = data.timer_deploy;
	data.timer_deploy = vi_max(0.0f, data.timer_deploy - Game::real_time.delta);

	if (data.timer_deploy > 0.5f)
	{
		// particles
		r32 t = old_timer;
		const r32 particle_interval = 0.015f;
		const ZoneNode* zone = zone_node_get(data.zone_selected);
		while (s32(t / particle_interval) > s32(data.timer_deploy / particle_interval))
		{
			r32 particle_blend = (t - 0.5f) / 0.5f;
			Particles::tracers.add
			(
				zone->pos() + Vec3(0, -2.0f + particle_blend * 12.0f, 0),
				Vec3::zero,
				0
			);
			t -= particle_interval;
		}
	}
	else
	{
		// screen shake
		r32 shake = (data.timer_deploy / 0.5f) * 0.05f;
		r32 offset = Game::real_time.total * 20.0f;
		data.camera_pos += Vec3(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);

		if (old_timer >= 0.5f)
			Audio::post_global_event(AK::EVENTS::PLAY_OVERWORLD_DEPLOY);
	}

	if (data.timer_deploy == 0.0f && old_timer > 0.0f)
		deploy_done();
}

void deploy_draw(const RenderParams& params)
{
	zones_draw(params);

	// show "loading..."
	Menu::progress_infinite(params, _(strings::deploying), params.camera->viewport.size * Vec2(0.5f, 0.2f));
}

b8 enable_input()
{
	const Data::StoryMode& story = data.story;
	return data.state != State::Hidden
		&& data.timer_transition == 0.0f
		&& data.timer_deploy == 0.0f
		&& story.inventory.timer_buy == 0.0f
		&& !Menu::dialog_active(0);
}

void group_join(Net::Master::Group g)
{
	// todo: redo this whole thing
	Game::save.group = g;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (zone.max_teams > 2)
		{
			if (g == Net::Master::Group::None)
				zone_change(zone.id, ZoneState::Locked);
			else
				zone_change(zone.id, mersenne::randf_cc() > 0.7f ? ZoneState::PvpFriendly : ZoneState::PvpHostile);
		}
	}
}

void capture_start(s8 gamepad)
{
	if (zone_can_capture(data.zone_selected)
		&& Game::save.resources[s32(Resource::Drones)] >= DEFAULT_ASSAULT_DRONES)
	{
		// one access key needed if we're attacking
		if (Game::save.zones[data.zone_selected] == ZoneState::PvpFriendly || Game::save.resources[s32(Resource::AccessKeys)] > 0)
			deploy_start();
	}
}

void zone_done(AssetID zone)
{
	if (Game::save.zones[zone] == ZoneState::PvpFriendly)
	{
		// we won
		if (Game::level.local)
		{
			const ZoneNode* z = zone_node_get(zone);
			for (s32 i = 0; i < s32(Resource::count); i++)
				resource_change(Resource(i), z->rewards[i]);
		}
	}
	else
	{
		// we lost
		Game::save.zone_lost_times[zone] = platform::timestamp();
	}
}

void zone_change(AssetID zone, ZoneState state)
{
	vi_assert(Game::level.local);
	OverworldNet::zone_change(zone, state);
}

b8 zone_is_pvp(AssetID zone_id)
{
	const ZoneNode* z = zone_node_get(zone_id);
	return z && z->max_teams > 0;
}

void zone_rewards(AssetID zone_id, s16* rewards)
{
	const ZoneNode* z = zone_node_get(zone_id);
	for (s32 i = 0; i < s32(Resource::count); i++)
		rewards[i] = z->rewards[i];
}

b8 zone_filter_default(AssetID zone_id)
{
	return true;
}

b8 zone_filter_can_be_attacked(AssetID zone_id)
{
	if (zone_id == Asset::Level::Port_District)
		return false;
	else if (zone_id == Game::level.id || (Game::session.type == SessionType::Story && zone_id == Game::save.zone_current))
		return false;

	return true;
}

b8 zone_filter_captured(AssetID zone_id)
{
	return Game::save.zones[zone_id] == ZoneState::PvpFriendly;
}

void zone_statistics(s32* captured, s32* hostile, s32* locked, b8 (*filter)(AssetID) = &zone_filter_default)
{
	*captured = 0;
	*hostile = 0;
	*locked = 0;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (filter(zone.id))
		{
			ZoneState state = Game::save.zones[zone.id];
			if (state == ZoneState::PvpFriendly)
				(*captured)++;
			else if (state == ZoneState::PvpHostile)
				(*hostile)++;
			else if (state == ZoneState::Locked)
				(*locked)++;
		}
	}
}

void tab_map_update(const Update& u)
{
	if (data.story.tab == StoryTab::Map && data.story.tab_timer == 0.0f)
	{
		select_zone_update(u, enable_input()); // only enable movement if enable_input()

		// capture button
		if (enable_input()
			&& u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0)
			&& zone_can_capture(data.zone_selected))
		{
			if (Game::save.resources[s32(Resource::Drones)] >= DEFAULT_ASSAULT_DRONES)
			{
				if (Game::save.zones[data.zone_selected] == ZoneState::PvpFriendly) // defending
					Menu::dialog(0, &capture_start, _(strings::confirm_defend), DEFAULT_ASSAULT_DRONES);
				else
				{
					// attacking
					if (Game::save.resources[s32(Resource::AccessKeys)] > 0)
						Menu::dialog(0, &capture_start, _(strings::confirm_capture), DEFAULT_ASSAULT_DRONES, 1);
					else
						Menu::dialog(0, &Menu::dialog_no_action, _(strings::insufficient_resource), 1, _(strings::access_keys));
				}
			}
			else
				Menu::dialog(0, &Menu::dialog_no_action, _(strings::insufficient_resource), DEFAULT_ASSAULT_DRONES, _(strings::drones));
		}
	}
}

ResourceInfo resource_info[s32(Resource::count)] =
{
	{
		Asset::Mesh::icon_energy,
		strings::energy,
		0,
	},
	{
		Asset::Mesh::icon_access_key,
		strings::access_keys,
		800,
	},
	{
		Asset::Mesh::icon_drone,
		strings::drones,
		50,
	},
};

StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
Vec3 ambient_color;

void resource_buy(s8 gamepad)
{
	data.story.inventory.timer_buy = BUY_TIME;
}

void tab_inventory_update(const Update& u)
{
	Data::Inventory* inventory = &data.story.inventory;

	if (data.story.inventory.timer_buy > 0.0f)
	{
		data.story.inventory.timer_buy = vi_max(0.0f, data.story.inventory.timer_buy - u.time.delta);
		if (data.story.inventory.timer_buy == 0.0f)
		{
			Resource resource = data.story.inventory.resource_selected;
			const ResourceInfo& info = resource_info[s32(resource)];
			s16 total_cost = info.cost * s16(data.story.inventory.buy_quantity);
			if (Game::save.resources[s32(Resource::Energy)] >= total_cost)
				OverworldNet::buy(resource, data.story.inventory.buy_quantity);
			data.story.inventory.mode = Data::Inventory::Mode::Normal;
			data.story.inventory.buy_quantity = 1;
		}
	}

	if (data.story.tab == StoryTab::Inventory && enable_input())
	{
		// handle input
		switch (inventory->mode)
		{
			case Data::Inventory::Mode::Normal:
			{
				s32 selected = s32(inventory->resource_selected);
				selected = vi_max(0, vi_min(s32(Resource::count) - 1, selected + UI::input_delta_vertical(u, 0)));
				inventory->resource_selected = (Resource)selected;

				if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				{
					const ResourceInfo& info = resource_info[s32(inventory->resource_selected)];
					if (info.cost > 0)
					{
						inventory->mode = Data::Inventory::Mode::Buy;
						inventory->buy_quantity = 1;
					}
				}
				break;
			}
			case Data::Inventory::Mode::Buy:
			{
				inventory->buy_quantity = vi_max(1, vi_min(8, inventory->buy_quantity + UI::input_delta_horizontal(u, 0)));
				if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				{
					const ResourceInfo& info = resource_info[s32(inventory->resource_selected)];
					if (Game::save.resources[s32(Resource::Energy)] >= info.cost * inventory->buy_quantity)
						Menu::dialog(0, &resource_buy, _(strings::prompt_buy), inventory->buy_quantity * info.cost, inventory->buy_quantity, _(info.description));
					else
						Menu::dialog(0, &Menu::dialog_no_action, _(strings::insufficient_resource), info.cost * inventory->buy_quantity, _(strings::energy));
				}
				else if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0])
				{
					inventory->mode = Data::Inventory::Mode::Normal;
					Game::cancel_event_eaten[0] = true;
				}
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}

	if (data.story.tab != StoryTab::Inventory)
	{
		// minimized view; reset
		inventory->mode = Data::Inventory::Mode::Normal;
		inventory->buy_quantity = 1;
	}
}

AssetID zone_random(b8(*filter1)(AssetID), b8(*filter2)(AssetID) = &zone_filter_default)
{
	StaticArray<AssetID, MAX_ZONES> zones;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (filter1(zone.id) && filter2(zone.id))
			zones.add(zone.id);
	}
	if (zones.length > 0)
		return zones[mersenne::randf_co() * zones.length];
	else
		return AssetNull;
}

void zone_random_attack(r32 elapsed_time)
{
	if (Game::level.local
		&& Game::level.mode == Game::Mode::Parkour
		&& zone_under_attack() == AssetNull
		&& (PlayerControlHuman::list.count() == 0 || !Tram::player_inside(PlayerControlHuman::list.iterator().item()->entity())))
	{
		s32 captured;
		s32 hostile;
		s32 locked;
		zone_statistics(&captured, &hostile, &locked, &zone_filter_can_be_attacked);

		r32 event_odds = elapsed_time * EVENT_ODDS_PER_ZONE * captured;

		while (mersenne::randf_co() < event_odds)
		{
			AssetID z = zone_random(&zone_filter_captured, &zone_filter_can_be_attacked); // live incoming attack
			if (z != AssetNull)
				OverworldNet::zone_under_attack(z);
			event_odds -= 1.0f;
		}
	}
}

void story_mode_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	data.story.tab_timer = vi_max(0.0f, data.story.tab_timer - u.time.delta);

	tab_map_update(u);
	tab_inventory_update(u);
}

Vec2 get_panel_size(const Rect2& rect)
{
	return Vec2(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_story_draw(const RenderParams& p, const Data::StoryMode& data, StoryTab tab, const char* label, Vec2* pos, b8 flash = false)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab && !Menu::dialog_callback[0])
	{
		// flash the tab when it is selected
		if (data.tab_timer > 0.0f)
		{
			if (UI::flash_function(Game::real_time.total))
				color = &UI::color_default;
			else
				draw = false; // don't draw the tab at all
		}
		else
			color = &UI::color_accent();
	}
	else
		color = &UI::color_disabled();

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	// calculate the width of the tab right now and the width of the tab before the last transition
	r32 current_width = data.tab == tab ? main_view_size.x : tab_size.x;
	r32 previous_width = data.tab_previous == tab ? main_view_size.x : tab_size.x;
	// then blend between the two widths for a nice animation
	r32 blend = Ease::cubic_out(1.0f - vi_min(1.0f, data.tab_timer / TAB_ANIMATION_TIME), 0.0f, 1.0f);
	r32 width = LMath::lerpf(blend, previous_width, current_width);

	if (draw)
	{
		// if we're minimized, fill in the background
		const Vec4& background_color = data.tab == tab ? Vec4(0, 0, 0, 0) : Vec4(UI::color_background.xyz(), OPACITY);
		tab_draw(p, label, *pos, width, *color, background_color, flash && !UI::flash_function(Game::real_time.total) ? Vec4(0, 0, 0, 0) : UI::color_background);
	}

	Rect2 result = { *pos, { width, main_view_size.y } };

	pos->x += width + PADDING;

	return result;
}

AssetID group_name[s32(Net::Master::Group::count)] =
{
	strings::none,
	strings::wu_gang,
	strings::ephyra,
};

Rect2 zone_stat_draw(const RenderParams& p, const Rect2& rect, UIText::Anchor anchor_x, s32 index, const char* label, const Vec4& color, b8 draw_text = true)
{
	UIText text;
	text.anchor_x = anchor_x;
	text.anchor_y = UIText::Anchor::Max;
	text.size = TEXT_SIZE * SCALE_MULTIPLIER * 0.75f;
	text.color = color;
	text.wrap_width = vi_min(MENU_ITEM_WIDTH * 0.5f * SCALE_MULTIPLIER, rect.size.x) + (PADDING * -2.0f);
	Vec2 pos = rect.pos;
	switch (anchor_x)
	{
		case UIText::Anchor::Min:
		{
			pos.x += PADDING;
			break;
		}
		case UIText::Anchor::Center:
		{
			pos.x += rect.size.x * 0.5f;
			break;
		}
		case UIText::Anchor::Max:
		{
			pos.x += rect.size.x - PADDING;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	pos.y += rect.size.y - PADDING + index * (text.size * -UI::scale - PADDING);
	text.text_raw(0, label);
	Rect2 text_rect = text.rect(pos);
	UI::box(p, text_rect.outset(PADDING), UI::color_background);
	if (draw_text)
		text.draw(p, pos);
	return text_rect;
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& story, const Rect2& rect)
{
	{
		// draw stats

		char buffer[255];

		s32 index = 0;

		// total energy increment
		{
			const char* label;
			if (story.tab == StoryTab::Map)
				label = _(Game::save.group == Net::Master::Group::None ? strings::energy_generation_total : strings::energy_generation_group);
			else
				label = "+%d";
			sprintf(buffer, label, s32(energy_increment_total()));
			Rect2 zone_stat_rect = zone_stat_draw(p, rect, UIText::Anchor::Max, index++, buffer, UI::color_default);

			// energy increment timer
			r32 icon_size = TEXT_SIZE * 1.5f * UI::scale * (story.tab == StoryTab::Map ? 1.0f : 0.75f);
			r64 t = platform::timestamp();
			UI::triangle_percentage
			(
				p,
				{ zone_stat_rect.pos + Vec2(zone_stat_rect.size.x - icon_size * 0.5f, zone_stat_rect.size.y * 0.5f), Vec2(icon_size) },
				r32(fmod(t, ENERGY_INCREMENT_INTERVAL)) / ENERGY_INCREMENT_INTERVAL,
				UI::color_default,
				PI
			);
		}

		// member of group "x"
		sprintf(buffer, _(strings::member_of_group), _(group_name[s32(Game::save.group)]));
		zone_stat_draw(p, rect, UIText::Anchor::Max, index++, buffer, UI::color_accent());

		if (story.tab != StoryTab::Map)
		{
			// statistics
			s32 captured;
			s32 hostile;
			s32 locked;
			zone_statistics(&captured, &hostile, &locked);

			sprintf(buffer, _(strings::zones_captured), captured);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, Game::save.group == Net::Master::Group::None ? Team::ui_color_friend : UI::color_accent());

			sprintf(buffer, _(strings::zones_hostile), hostile);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_alert());

			sprintf(buffer, _(strings::zones_locked), locked);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_default);
		}

		// zones under attack
		if (zone_under_attack() != AssetNull)
		{
			sprintf(buffer, _(strings::zones_under_attack), 1);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_alert(), UI::flash_function_slow(Game::real_time.total)); // flash text
		}
	}

	if (story.tab == StoryTab::Map)
	{
		zones_draw(p);

		// show selected zone info
		ZoneState zone_state = Game::save.zones[data.zone_selected];

		// show stats
		const ZoneNode* zone = zone_node_get(data.zone_selected);
		zone_stat_draw(p, rect, UIText::Anchor::Min, 0, Loader::level_name(data.zone_selected), zone_ui_color(*zone));
		char buffer[255];
		sprintf(buffer, _(strings::energy_generation), s32(energy_increment_zone(*zone)));
		zone_stat_draw(p, rect, UIText::Anchor::Min, 1, buffer, UI::color_default);

		if (zone_state == ZoneState::PvpHostile)
		{
			// show potential rewards
			b8 has_rewards = false;
			for (s32 i = 0; i < s32(Resource::count); i++)
			{
				if (zone->rewards[i] > 0)
				{
					has_rewards = true;
					break;
				}
			}

			if (has_rewards)
			{
				zone_stat_draw(p, rect, UIText::Anchor::Min, 2, _(strings::capture_bonus), UI::color_accent());
				s32 index = 3;
				for (s32 i = 0; i < s32(Resource::count); i++)
				{
					if (zone->rewards[i] > 0)
					{
						sprintf(buffer, "%d %s", zone->rewards[i], _(resource_info[i].description));
						zone_stat_draw(p, rect, UIText::Anchor::Min, index, buffer, UI::color_default);
						index++;
					}
				}
			}
		}

		// capture prompt
		if (enable_input() && zone_can_capture(data.zone_selected))
		{
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.color = UI::color_accent();
			text.text(0, _(Game::save.zones[data.zone_selected] == ZoneState::PvpFriendly ? strings::prompt_defend : strings::prompt_capture));

			Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

			UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::color_background);

			text.draw(p, pos);
		}
	}
}

r32 resource_change_time(Resource r)
{
	return data.story.inventory.resource_change_time[s32(r)];
}

void inventory_items_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	Vec2 panel_size = get_panel_size(rect);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);
	for (s32 i = 0; i < s32(Resource::count); i++)
	{
		b8 selected = data.tab == StoryTab::Inventory && data.inventory.resource_selected == (Resource)i && !Menu::dialog_active(0);

		UI::box(p, { pos, panel_size }, UI::color_background);
		if (selected)
			UI::border(p, Rect2(pos, panel_size).outset(BORDER * -UI::scale), BORDER, UI::color_accent());

		r32 icon_size = 18.0f * SCALE_MULTIPLIER * UI::scale;

		b8 flash = Game::real_time.total - data.inventory.resource_change_time[i] < 0.5f;
		b8 draw = !flash || UI::flash_function(Game::real_time.total);
		
		const ResourceInfo& info = resource_info[i];

		const Vec4* color;
		if (flash)
			color = &UI::color_accent();
		else if (selected && data.inventory.mode == Data::Inventory::Mode::Buy && Game::save.resources[s32(Resource::Energy)] < data.inventory.buy_quantity * info.cost)
			color = &UI::color_alert(); // not enough energy to buy
		else if (selected)
			color = &UI::color_accent();
		else if (Game::save.resources[i] == 0)
			color = &UI::color_alert();
		else
			color = &UI::color_default;

		if (draw)
			UI::mesh(p, info.icon, pos + Vec2(PADDING + icon_size * 0.5f, panel_size.y * 0.5f), Vec2(icon_size), *color);

		UIText text;
		text.anchor_y = UIText::Anchor::Center;
		text.color = *color;
		text.size = TEXT_SIZE * (data.tab == StoryTab::Inventory ? 1.0f : 0.75f);
		if (draw)
		{
			// current amount
			text.anchor_x = UIText::Anchor::Max;
			text.text(0, "%d", Game::save.resources[i]);
			text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));

			if (data.tab == StoryTab::Inventory)
			{
				text.anchor_x = UIText::Anchor::Min;
				text.text(0, _(info.description));
				text.draw(p, pos + Vec2(icon_size * 2.0f + PADDING * 2.0f, panel_size.y * 0.5f));

				if (selected)
				{
					if (data.inventory.mode == Data::Inventory::Mode::Buy)
					{
						// buy interface
						text.anchor_x = UIText::Anchor::Center;
						text.text(0, "+%d", data.inventory.buy_quantity);

						const r32 buy_quantity_spacing = 32.0f * UI::scale * SCALE_MULTIPLIER;
						Vec2 buy_quantity_pos = pos + Vec2(panel_size.x * 0.4f, panel_size.y * 0.5f);
						text.draw(p, buy_quantity_pos);

						UI::triangle(p, { buy_quantity_pos + Vec2(-buy_quantity_spacing, 0), Vec2(text.size * UI::scale) }, *color, PI * 0.5f);
						UI::triangle(p, { buy_quantity_pos + Vec2(buy_quantity_spacing, 0), Vec2(text.size * UI::scale) }, *color, PI * -0.5f);

						// cost
						text.anchor_x = UIText::Anchor::Min;
						text.text(0, _(strings::ability_spawn_cost), s32(info.cost * data.inventory.buy_quantity));
						text.draw(p, pos + Vec2(panel_size.x * 0.6f, panel_size.y * 0.5f));
					}
					else
					{
						// normal mode
						if (info.cost > 0)
						{
							// "buy more!"
							text.anchor_x = UIText::Anchor::Center;
							text.text(0, _(strings::prompt_buy_more));
							text.draw(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y * 0.5f));
						}
					}
				}
			}
		}

		pos.y -= panel_size.y;
	}
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	inventory_items_draw(p, data, rect);

	if (data.tab == StoryTab::Inventory && data.tab_timer == 0.0f)
	{
		if (data.inventory.timer_buy > 0.0f)
			Menu::progress_bar(p, _(strings::buying), 1.0f - (data.inventory.timer_buy / BUY_TIME), p.camera->viewport.size * Vec2(0.5f, 0.2f));
	}
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	const Rect2& vp = p.camera->viewport;

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	Vec2 center = vp.size * 0.5f;
	Vec2 total_size = Vec2(main_view_size.x + (tab_size.x + PADDING), main_view_size.y);
	Vec2 bottom_left = center + total_size * -0.5f + Vec2(0, -tab_size.y);

	Vec2 pos = bottom_left;
	{
		Rect2 rect = tab_story_draw(p, data.story, StoryTab::Map, _(strings::tab_map), &pos).outset(-PADDING);
		if (data.story.tab_timer == 0.0f)
			tab_map_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_story_draw(p, data.story, StoryTab::Inventory, _(strings::tab_inventory), &pos).outset(-PADDING);
		if (data.story.tab_timer == 0.0f)
			tab_inventory_draw(p, data.story, rect);
	}
}

b8 should_draw_zones()
{
	return data.state == State::StoryModeDeploying
		|| (data.state == State::StoryMode && data.story.tab == StoryTab::Map && data.story.tab_timer == 0.0f);
}

void show_complete()
{
	State state_next = data.state_next;
	StoryTab tab_next = data.story.tab;

	Particles::clear();
	{
		Camera* c = data.camera.ref();
		r32 t = data.timer_transition;
		data.~Data();
		new (&data) Data();
		data.camera = c;
		data.timer_transition = t;

		data.camera_restore_data = *data.camera.ref();

		if (state_next != State::StoryModeOverlay) // if we're going to be modal, we need to mess with the camera.
		{
			data.camera.ref()->flag(CameraFlagColors, false);
			data.camera.ref()->mask = 0;
			Audio::post_global_event(AK::EVENTS::PLAY_AMBIENCE_OVERWORLD);
		}
	}

	data.state = state_next;
	data.story.tab = tab_next;
	if (data.story.tab != data.story.tab_previous)
		Audio::post_global_event(AK::EVENTS::PLAY_OVERWORLD_SHOW);

	if (Game::session.type == SessionType::Story)
	{
		if (Game::session.zone_under_attack == AssetNull)
			data.zone_selected = Game::level.id;
		else
			data.zone_selected = Game::session.zone_under_attack;

		data.story.tab_previous = StoryTab((s32(data.story.tab) + 1) % s32(StoryTab::count));
	}
	else
	{
		if (Game::save.zone_last == AssetNull)
			data.zone_selected = Asset::Level::Crossing;
		else
			data.zone_selected = Game::save.zone_last;
	}

	data.camera_pos = global.camera_offset_pos;
	data.camera_rot = global.camera_offset_rot;

	{
		const ZoneNode* zone = zone_node_get(Game::save.zone_overworld);
		if (!zone)
			zone = zone_node_get(Game::save.zone_last);
		if (zone)
			data.camera_pos += zone->pos();
	}
}

r32 particle_accumulator = 0.0f;
void update(const Update& u)
{
	if (Game::level.id == Asset::Level::overworld
		&& data.state == State::Hidden && data.state_next == State::Hidden
#if !SERVER
		&& Net::Client::mode() == Net::Client::Mode::Disconnected
#endif
		&& Game::scheduled_load_level == AssetNull)
	{
		Camera* c = Camera::add(0);
		data.timer_transition = 0.0f;

		if (Game::session.type == SessionType::Story)
			vi_assert(false);
		else
			show(c, State::Multiplayer);

		show_complete();
		data.timer_transition = 0.0f;
	}

	if (Camera* c = data.camera.ref())
	{
		const DisplayMode& display = Settings::display();
		c->viewport =
		{
			Vec2(0, 0),
			Vec2(display.width, display.height),
		};
		c->perspective((80.0f * PI * 0.5f / 180.0f), 0.1f, Game::level.skybox.far_plane);
	}

	if (data.timer_transition > 0.0f)
	{
		r32 old_timer = data.timer_transition;
		data.timer_transition = vi_max(0.0f, data.timer_transition - u.time.delta);
		if (data.timer_transition < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
		{
			Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_IN);
			if (data.state_next == State::Hidden)
				hide_complete();
			else
				show_complete();
		}
	}

	if (Game::session.type == SessionType::Story)
	{
		// random zone attacks
		{
			if (Game::session.zone_under_attack_timer > 0.0f)
			{
				Game::session.zone_under_attack_timer = vi_max(0.0f, Game::session.zone_under_attack_timer - Game::real_time.delta);
				if (Game::session.zone_under_attack_timer == 0.0f)
				{
					if (Game::level.local
						&& Game::level.id != Game::session.zone_under_attack
						&& data.timer_deploy == 0.0f
						&& Game::scheduled_load_level == AssetNull) // if the player is deploying to this zone to contest it, don't set it to hostile; wait for the match outcome
						zone_change(Game::session.zone_under_attack, ZoneState::PvpHostile);
					Game::session.zone_under_attack = AssetNull;
				}
			}
			if (Game::level.local)
				zone_random_attack(Game::real_time.delta);
		}

		// energy increment
		if (Game::level.local)
		{
			r64 t = platform::timestamp();
			if (s32(t / ENERGY_INCREMENT_INTERVAL) > s32(data.story.timestamp_last / ENERGY_INCREMENT_INTERVAL))
				resource_change(Resource::Energy, energy_increment_total());
			data.story.timestamp_last = t;
		}
	}

	if (data.state != State::Hidden && !Console::visible)
	{
		switch (data.state)
		{
			case State::Multiplayer:
			{
				multiplayer_update(u);
				break;
			}
			case State::StoryMode:
			case State::StoryModeOverlay:
			{
				story_mode_update(u);
				break;
			}
			case State::StoryModeDeploying:
			{
				deploy_update(u);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		if (modal())
		{
			data.camera.ref()->pos = data.camera_pos;
			data.camera.ref()->rot = data.camera_rot;
		}

		// pause
		if (!Game::cancel_event_eaten[0])
		{
			if (Game::session.type == SessionType::Story && ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
				|| (u.input->get(Controls::Scoreboard, 0) && !u.last_input->get(Controls::Scoreboard, 0))))
			{
				Game::cancel_event_eaten[0] = true;
				hide();
			}
			else if (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))
			{
				Game::cancel_event_eaten[0] = true;
				Menu::show();
			}
		}
	}
}

void draw_opaque(const RenderParams& params)
{
	if (modal())
	{
		for (s32 i = 0; i < global.props.length; i++)
		{
			const PropEntry& entry = global.props[i];
			Mat4 m;
			m.make_transform(entry.pos, Vec3(1), entry.rot);
			View::draw_mesh(params, entry.mesh, Asset::Shader::standard, AssetNull, m, Loader::mesh(entry.mesh)->color);
		}

		for (s32 i = 0; i < global.waters.length; i++)
		{
			const WaterEntry& entry = global.waters[i];
			Water::draw_opaque(params, entry.config, entry.pos, entry.rot);
		}

		for (s32 i = 0; i < global.zones.length; i++)
		{
			const ZoneNode& zone = global.zones[i];
			// flash if necessary
			const Vec4& color = Game::session.type != SessionType::Story
				|| Game::real_time.total > data.story.map.zones_change_time[zone.id] + 0.5f
				|| UI::flash_function(Game::real_time.total)
				? Vec4(zone_color(zone), 1.0f)
				: BACKGROUND_COLOR;
			zone_draw_mesh(params, zone.mesh, zone.pos(), color);
		}
	}
}

void draw_hollow(const RenderParams& params)
{
	if (modal())
	{
		for (s32 i = 0; i < global.waters.length; i++)
		{
			const WaterEntry& entry = global.waters[i];
			Water::draw_hollow(params, entry.config, entry.pos, entry.rot);
		}
	}
}

void draw_ui(const RenderParams& params)
{
	if (active() && params.camera == data.camera.ref())
	{
		switch (data.state)
		{
			case State::Multiplayer:
			{
				multiplayer_draw(params);
				break;
			}
			case State::StoryMode:
			case State::StoryModeOverlay:
			{
				story_mode_draw(params);
				break;
			}
			case State::StoryModeDeploying:
			{
				deploy_draw(params);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}

	// letterbox transition effect
	if (data.timer_transition > 0.0f)
		Menu::draw_letterbox(params, data.timer_transition, TRANSITION_TIME);
}

void show(Camera* camera, State state, StoryTab tab)
{
	if (data.timer_transition == 0.0f)
	{
		data.camera = camera;
		data.state_next = state;
		data.story.tab = tab;
		if (state == State::StoryModeOverlay) // overlay; no transition
			show_complete();
		else if (state == State::Hidden && data.state == State::StoryModeOverlay) // overlay; no transition
			hide_complete();
		else // start transition
		{
			Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_OUT);
			data.timer_transition = TRANSITION_TIME;
		}
	}
}

b8 active()
{
	return data.state != State::Hidden;
}

b8 modal()
{
	return active() && data.state != State::StoryModeOverlay;
}

void clear()
{
	hide_complete();
	data.timer_transition = 0.0f;
}

void execute(const char* cmd)
{
	if (strcmp(cmd, "attack") == 0)
	{
		AssetID z = zone_random(&zone_filter_captured, &zone_filter_can_be_attacked); // live incoming attack
		if (z != AssetNull)
			OverworldNet::zone_under_attack(z);
	}
	else if (strstr(cmd, "join ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* group_string = delimiter + 1;
		for (s32 i = 0; i < s32(Net::Master::Group::count); i++)
		{
			if (strcmp(group_string, _(group_name[i])) == 0)
			{
				group_join(Net::Master::Group(i));
				break;
			}
		}
	}
}

cJSON* find_entity(cJSON* level, const char* name)
{
	cJSON* element = level->child;
	while (element)
	{
		const char* element_name = Json::get_string(element, "name");
		if (element_name && strcmp(element_name, name) == 0)
			return element;
		element = element->next;
	}
	return nullptr;
}

void init(cJSON* level)
{
	{
		cJSON* t = find_entity(level, "map_view");
		global.camera_offset_pos = Json::get_vec3(t, "pos");
		global.camera_offset_rot = Quat::look(Json::get_quat(t, "rot") * Vec3(0, -1, 0));
	}

	{
		cJSON* element = level->child;
		s32 element_id = 0;
		while (element)
		{
			const char* zone_name = Json::get_string(element, "Zone");
			if (zone_name)
			{
				AssetID level_id = Loader::find_level(zone_name);
				if (level_id != AssetNull)
				{
					ZoneNode* node = global.zones.add();

					Vec3 zone_pos = Json::get_vec3(element, "pos");
					{
						cJSON* element2 = level->child;
						while (element2)
						{
							if (Json::get_s32(element2, "parent") == element_id)
								node->children.add(zone_pos + Json::get_vec3(element2, "pos"));
							element2 = element2->next;
						}
					}
					node->children.add(zone_pos);
					node->rot = Json::get_quat(element, "rot");

					node->id = level_id;

					cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
					cJSON* mesh_json = meshes->child;
					const char* mesh_ref = mesh_json->valuestring;
					node->mesh = Loader::find_mesh(mesh_ref);

					node->rewards[0] = s16(Json::get_s32(element, "energy", 0));
					node->rewards[1] = s16(Json::get_s32(element, "access_keys", 0));
					node->rewards[2] = s16(Json::get_s32(element, "drones", 0));
					node->size = s8(Json::get_s32(element, "size", 1));
					node->max_teams = s8(Json::get_s32(element, "max_teams", 2));
				}
			}
			else if (cJSON_HasObjectItem(element, "World"))
			{
				ambient_color = Json::get_vec3(element, "ambient_color");
			}
			else if (cJSON_HasObjectItem(element, "DirectionalLight"))
			{
				DirectionalLight light;
				light.color = Json::get_vec3(element, "color");
				light.shadowed = b8(Json::get_s32(element, "shadowed"));
				light.rot = Json::get_quat(element, "rot");
				directional_lights.add(light);
			}
			else if (cJSON_HasObjectItem(element, "Prop"))
			{
				cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
				cJSON* json_mesh = meshes->child;

				Vec3 pos = Json::get_vec3(element, "pos");
				Quat rot = Json::get_quat(element, "rot");

				while (json_mesh)
				{
					const char* mesh_ref = json_mesh->valuestring;

					PropEntry* entry = global.props.add();
					entry->mesh = Loader::find_mesh(mesh_ref);
#if !SERVER
					Loader::mesh_permanent(entry->mesh);
#endif
					entry->pos = pos;
					entry->rot = rot;

					json_mesh = json_mesh->next;
				}
			}
			else if (cJSON_HasObjectItem(element, "Water"))
			{
				cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
				cJSON* mesh_json = meshes->child;
				const char* mesh_ref = mesh_json->valuestring;

				WaterEntry* water = global.waters.add();
				new (&water->config) Water::Config();
				water->config.mesh = Loader::find_mesh(mesh_ref);
				vi_assert(water->config.mesh != AssetNull);
#if !SERVER
				Loader::mesh_permanent(water->config.mesh);
#endif
				water->config.texture = Loader::find(Json::get_string(element, "texture", "water_normal"), AssetLookup::Texture::names);
				water->config.displacement_horizontal = Json::get_r32(element, "displacement_horizontal", 2.0f);
				water->config.displacement_vertical = Json::get_r32(element, "displacement_vertical", 0.75f);
				water->config.color = Vec4(0, 0, 0, 1);
				water->pos = Json::get_vec3(element, "pos");
				water->rot = Json::get_quat(element, "rot");
			}

			element = element->next;
			element_id++;
		}
	}
}

}

}
