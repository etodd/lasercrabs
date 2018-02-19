#include "strings.h"
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
#define REFRESH_INTERVAL 4.0f

#define SCALE_MULTIPLIER (UI::scale < 1.0f ? 0.5f : 1.0f)
#define PADDING (16.0f * UI::scale * SCALE_MULTIPLIER)
#define TAB_SIZE Vec2(160.0f * UI::scale, UI_TEXT_SIZE_DEFAULT * UI::scale + PADDING * 2.0f)
#define MAIN_VIEW_SIZE (Vec2(800.0f, 512.0f) * (SCALE_MULTIPLIER * UI::scale))
#define TEXT_SIZE (UI_TEXT_SIZE_DEFAULT * SCALE_MULTIPLIER)
#define BORDER 2.0f
#define OPACITY 0.8f
#define EVENT_INTERVAL_PER_ZONE (60.0f * 10.0f)
#define EVENT_ODDS_PER_ZONE (1.0f / EVENT_INTERVAL_PER_ZONE) // an event will happen on average every X minutes per zone you own
#define ZONE_MAX_CHILDREN 12

struct ZoneNode
{
	StaticArray<Vec3, ZONE_MAX_CHILDREN> children;
	Quat rot;
	AssetID id;
	AssetID uuid;
	AssetID mesh;
	s16 rewards[s32(Resource::count)];
	s8 size;
	s8 max_teams;

	inline Vec3 pos() const
	{
		return children[children.length - 1];
	}
};

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
		UIMenu menu;
		r32 resource_change_time[s32(Resource::count)];
		s32 flags;
		r32 timer_buy;
		s16 buy_quantity;
		Resource resource_selected;
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
			ChangeRegion,
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
			Levels,
			Upgrades,
			StartAbilities,
			count,
		};

		struct ServerList
		{
			Array<Net::Master::ServerListEntry> entries;
			UIScroll scroll;
			s32 selected;

			ServerList()
				: entries(),
				scroll(12),
				selected()
			{
			}
		};

		struct PingData
		{
			r32 last_sent_time;
			r32 rtt;
			u32 last_sent_token;
		};

		struct TopBarLayout
		{
			struct Button
			{
				enum class Type : s8
				{
					Select,
					CreateServer,
					ChooseRegion,
					Back,
					ConnectServer,
					EditServer,
					SaveServer,
					count,
				};

				Rect2 rect;
				char text[UI_TEXT_MAX];
				Type type;

				Rect2 container() const
				{
					return rect.outset(PADDING * 0.5f);
				}
			};

			StaticArray<Button, 4> buttons;
			Rect2 rect;

			b8 button_clicked(Button::Type type, const Update& u)
			{
				for (s32 i = 0; i < buttons.length; i++)
				{
					const Button& button = buttons[i];
					if (button.type == type)
						return button.container().contains(u.input->cursor) && u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft));
				}
				return false;
			}
		};

		std::unordered_map<u64, PingData> ping;
		ServerList server_lists[s32(ServerListType::count)];
		UIMenu menu[s32(EditMode::count)];
		r32 tab_timer = TAB_ANIMATION_TIME;
		r32 state_transition_time;
		r32 refresh_timer;
		u32 request_id;
		Net::Master::ServerDetails active_server;
		RequestType request_type;
		State state;
		EditMode edit_mode;
		ServerListType tab_mouse_try;
		TopBarLayout top_bar;
		// in State::EntryEdit, this is true if there are unsaved changes.
		// in State::EntryView, this is true if we've received ServerDetails from the master
		b8 active_server_dirty;
	};

	struct StoryMode
	{
		StoryTab tab;
		StoryTab tab_previous;
		r32 tab_timer = TAB_ANIMATION_TIME;
		Inventory inventory;
		Map map;
	};

	Ref<Camera> camera;
	Ref<Camera> restore_camera;
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
	Audio::post_global(AK::EVENTS::PLAY_OVERWORLD_DEPLOY_START, 0);
}

void hide();
void deploy_done();
const ZoneNode* zone_node_by_id(AssetID);
const ZoneNode* zone_node_by_uuid(AssetID);

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

Rect2 multiplayer_main_view_rect()
{
	const DisplayMode& display = Settings::display();
	Vec2 center = Vec2(r32(display.width), r32(display.height)) * 0.5f;
	Vec2 tab_size = TAB_SIZE;
	return
	{
		center + MAIN_VIEW_SIZE * -0.5f + Vec2(0, -tab_size.y),
		MAIN_VIEW_SIZE,
	};
}

Rect2 multiplayer_browse_entry_rect(const Data::Multiplayer::TopBarLayout& top_bar, const Data::Multiplayer::ServerList& list, s32 index)
{
	Rect2 rect = multiplayer_main_view_rect().outset(-PADDING);
	Vec2 panel_size(rect.size.x, PADDING + TEXT_SIZE * UI::scale);
	Vec2 pos = top_bar.rect.pos;
	pos.y -= panel_size.y + (PADDING * 1.5f) + ((index - list.scroll.top()) * panel_size.y);
	return { pos, panel_size };
}

void multiplayer_browse_update(const Update& u)
{
	if (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::CreateServer, u)
		|| (u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0)))
	{
		// create server
		Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
		new (&data.multiplayer.active_server.config) Net::Master::ServerConfig();
		data.multiplayer.active_server.config.region = Settings::region;
		multiplayer_switch_tab(ServerListType::Mine);
		multiplayer_state_transition(Data::Multiplayer::State::EntryEdit);
		return;
	}

	if (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::ChooseRegion, u)
		|| (u.last_input->get(Controls::Scoreboard, 0) && !u.input->get(Controls::Scoreboard, 0)))
	{
		Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
		multiplayer_state_transition(Data::Multiplayer::State::ChangeRegion);
		return;
	}

	Data::Multiplayer::ServerList* server_list = &data.multiplayer.server_lists[s32(global.multiplayer.tab)];

	{
		s32 old_selected = server_list->selected;
		s32 old_scroll = server_list->scroll.pos;

		server_list->selected = vi_max(0, vi_min(server_list->entries.length - 1, server_list->selected + UI::input_delta_vertical(u, 0)));
		if (!UIMenu::active[0] && !Menu::dialog_active(0))
		{
			if (u.input->keys.get(s32(KeyCode::MouseWheelUp)))
				server_list->scroll.pos = vi_max(0, server_list->scroll.pos - 1);
			else if (u.input->keys.get(s32(KeyCode::MouseWheelDown)))
				server_list->scroll.pos++;
			server_list->selected = vi_max(0, vi_min(server_list->entries.length - 1, server_list->selected + server_list->scroll.pos - old_scroll));
		}

		if (Game::ui_gamepad_types[0] == Gamepad::Type::None)
		{
			for (s32 i = server_list->scroll.top(); i < server_list->scroll.bottom(server_list->entries.length); i++)
			{
				if (multiplayer_browse_entry_rect(data.multiplayer.top_bar, *server_list, i).contains(u.input->cursor))
				{
					server_list->selected = i;
					if (u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)))
					{
						// select entry
						data.multiplayer.active_server.config.id = server_list->entries[i].server_state.id;
						multiplayer_state_transition(Data::Multiplayer::State::EntryView);
						Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
						return;
					}

					break;
				}
			}
		}

		server_list->scroll.update_menu(server_list->entries.length);
		server_list->scroll.scroll_into_view(server_list->selected);

		if (server_list->scroll.pos != old_scroll
			&& data.multiplayer.refresh_timer < REFRESH_INTERVAL - 0.5f
			&& (server_list->scroll.top() == 0 ||  server_list->scroll.bottom(server_list->entries.length) == server_list->entries.length))
			data.multiplayer.refresh_timer = 0.0f; // refresh list immediately

		if (server_list->selected != old_selected)
			Audio::post_global(AK::EVENTS::PLAY_MENU_MOVE, 0);
	}

	if (server_list->selected < server_list->entries.length
		&& (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::Select, u) || (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))))
	{
		// select entry
		Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
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

void master_server_list_end(ServerListType type, s32 length)
{
	if (active()
		&& data.state == State::Multiplayer)
	{
		Data::Multiplayer::ServerList* list = &data.multiplayer.server_lists[s32(type)];
		list->entries.resize(length);
		list->selected = vi_min(list->selected, list->entries.length - 1);
		list->scroll.update_menu(list->entries.length);
	}
}

void multiplayer_entry_edit_cancel(s8 gamepad = 0)
{
	if (Game::level.mode != Game::Mode::Special) // we are currently in the server we're editing
		hide();
	else
	{
		if (data.multiplayer.active_server.config.id) // we were editing a config that actually exists; switch to EntryView mode
			multiplayer_state_transition(Data::Multiplayer::State::EntryView);
		else // we were creating a new entry and we never saved it; go back to Browse
			multiplayer_state_transition(Data::Multiplayer::State::Browse);
	}
	Game::cancel_event_eaten[0] = true;
}

void multiplayer_request_setup(Data::Multiplayer::RequestType type)
{
	data.multiplayer.request_id = vi_max(u32(1), u32(mersenne::rand()));
	data.multiplayer.request_type = type;
}

void multiplayer_entry_name_edit(const TextField& text_field)
{
	vi_assert(data.multiplayer.state == Data::Multiplayer::State::EntryEdit);
	data.multiplayer.active_server_dirty = true;
	strncpy(data.multiplayer.active_server.config.name, text_field.value.data, MAX_SERVER_CONFIG_NAME);
}

void multiplayer_entry_edit_show_name(s8 = 0)
{
	Menu::dialog_text_with_cancel(&multiplayer_entry_name_edit, &Menu::dialog_text_cancel_no_action, data.multiplayer.active_server.config.name, MAX_SERVER_CONFIG_NAME, _(strings::prompt_name));
}

void multiplayer_entry_edit_switch_to_levels(s8 = 0)
{
	multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Levels);
}

void multiplayer_entry_save()
{
	vi_assert(data.multiplayer.state == Data::Multiplayer::State::EntryEdit);
	const Net::Master::ServerConfig& config = data.multiplayer.active_server.config;
	if (config.levels.length == 0)
		Menu::dialog(0, &multiplayer_entry_edit_switch_to_levels, _(strings::error_no_levels));
	else if (strlen(config.name) == 0)
		Menu::dialog(0, &multiplayer_entry_edit_show_name, _(strings::error_no_name));
	else
	{
		multiplayer_request_setup(Data::Multiplayer::RequestType::ConfigSave);
#if !SERVER
		Net::Client::master_save_server_config(config, data.multiplayer.request_id);
#endif
	}
}

static const AssetID game_type_strings[s32(GameType::count)] =
{
	strings::game_type_assault,
	strings::game_type_deathmatch,
	strings::game_type_capture_the_flag,
};

static const AssetID preset_strings[s32(Net::Master::Ruleset::Preset::count)] =
{
	strings::preset_standard,
	strings::preset_arcade,
	strings::preset_custom,
};

AssetID game_type_string(GameType type)
{
	vi_assert(s32(type) >= 0 && s32(type) < s32(GameType::count));
	return game_type_strings[s32(type)];
}

void game_type_string(UIText* text, Net::Master::Ruleset::Preset preset, GameType type, s8 team_count, s8 max_players)
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
	if (preset == Net::Master::Ruleset::Preset::Standard)
		text->text(0, "%s %s", _(teams_type), _(game_type_string(type)));
	else
		text->text(0, "%s %s %s", _(preset_strings[s32(preset)]), _(teams_type), _(game_type_string(type)));
}

UIMenu::Origin multiplayer_menu_origin()
{
	const DisplayMode& display = Settings::display();
	r32 y =
		data.multiplayer.state == Data::Multiplayer::State::EntryEdit && data.multiplayer.edit_mode != Data::Multiplayer::EditMode::Main
		? (display.height * 0.6f) - MENU_ITEM_HEIGHT * 2.0f
		: display.height * 0.6f;
	return
	{
		Vec2(display.width * 0.5f, y),
		UIText::Anchor::Center,
		UIText::Anchor::Max,
	};
}

void multiplayer_entry_edit_update(const Update& u)
{
	b8 cancel = data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::Back, u)
		|| (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0]);

	if (data.multiplayer.request_id)
	{
		if (cancel)
		{
			Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
			data.multiplayer.request_id = 0; // cancel active request
			Game::cancel_event_eaten[0] = true;
#if !SERVER
			Net::Client::master_cancel_outgoing();
#endif
			if (Game::level.mode != Game::Mode::Special) // we are currently in the server we're editing
				hide();
		}
	}
	else
	{
		if (!Menu::dialog_active(0)
			&& (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::SaveServer, u) || (u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0))))
		{
			multiplayer_entry_save();
			return;
		}

		Net::Master::ServerConfig* config = &data.multiplayer.active_server.config;

		UIMenu* menu = &data.multiplayer.menu[s32(data.multiplayer.edit_mode)];
		switch (data.multiplayer.edit_mode)
		{
			case Data::Multiplayer::EditMode::Main:
			{
				menu->start(u, multiplayer_menu_origin(), 0);

				// cancel
				if (cancel || menu->item(u, _(strings::cancel), nullptr, false, Asset::Mesh::icon_close))
				{
					Game::cancel_event_eaten[0] = true;
					if (data.multiplayer.active_server_dirty)
						Menu::dialog(0, &multiplayer_entry_edit_cancel, _(strings::confirm_entry_cancel));
					else
					{
						Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
						multiplayer_entry_edit_cancel();
						menu->end(u);
						return;
					}
				}

				// save
				if (menu->item(u, _(strings::save), nullptr, false, Asset::Mesh::icon_arrow_main))
				{
					multiplayer_entry_save();
					menu->end(u);
					return;
				}

				// edit name
				if (menu->item(u, _(strings::edit_name)))
					multiplayer_entry_edit_show_name();

				s32 delta;
				char str[MAX_PATH_LENGTH + 1];

				{
					// private
					b8* is_private = &config->is_private;
					delta = menu->slider_item(u, _(strings::is_private), _(*is_private ? strings::yes : strings::no));
					if (delta != 0)
					{
						*is_private = !(*is_private);
						data.multiplayer.active_server_dirty = true;
					}
				}

				{
					// levels
					sprintf(str, "%d", s32(config->levels.length));
					if (menu->item(u, _(strings::levels), str))
					{
						multiplayer_entry_edit_switch_to_levels();
						menu->end(u);
						return;
					}
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
					if (config->game_type == GameType::Assault || config->game_type == GameType::CaptureTheFlag)
						config->team_count = 2;

					s8* team_count = &config->team_count;
					sprintf(str, "%hhd", *team_count);
					delta = menu->slider_item(u, _(strings::teams), str, config->game_type == GameType::Assault || config->game_type == GameType::CaptureTheFlag);

					s32 max_teams = vi_min(s32(config->max_players), MAX_TEAMS);
					for (s32 i = 0; i < config->levels.length; i++)
						max_teams = vi_min(max_teams, s32(zone_node_by_uuid(config->levels[i])->max_teams));

					*team_count = s8(vi_max(2, vi_min(max_teams, *team_count + delta)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// bots
					s8* fill_bots = &config->fill_bots;
					sprintf(str, "%d", s32(*fill_bots));
					delta = menu->slider_item(u, _(strings::fill_bots), str);
					*fill_bots = vi_max(0, vi_min(s32(config->max_players - 1), (*fill_bots) + delta));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// game type
					if (UIMenu::enum_option(&config->game_type, menu->slider_item(u, _(strings::game_type), _(game_type_string(config->game_type)))))
						data.multiplayer.active_server_dirty = true;
				}

				if (config->game_type == GameType::Assault && config->preset != Net::Master::Ruleset::Preset::Custom)
				{
					// read-only time limit
					sprintf(str, _(strings::timer), DEFAULT_ASSAULT_TIME_LIMIT_MINUTES, 0);
					menu->item(u, _(strings::time_limit), str, true);
				}
				else
				{
					// time limit
					u8* time_limit = &config->time_limit_minutes[s32(config->game_type)];
					sprintf(str, _(strings::timer), s32(*time_limit), 0);
					delta = menu->slider_item(u, _(strings::time_limit), str);
					*time_limit = vi_max(1, vi_min(255, s32(*time_limit) + delta * (*time_limit >= (delta > 0 ? 10 : 11) ? 5 : 1)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				switch (config->game_type)
				{
					case GameType::Assault:
						break;
					case GameType::Deathmatch:
					{
						// kill limit
						s16* kill_limit = &config->kill_limit;
						sprintf(str, "%hd", *kill_limit);
						delta = menu->slider_item(u, _(strings::kill_limit), str);
						*kill_limit = s16(vi_max(1, vi_min(MAX_RESPAWNS, s32(*kill_limit) + delta * (*kill_limit >= (delta > 0 ? 10 : 11) ? 5 : 1))));
						if (delta)
							data.multiplayer.active_server_dirty = true;
						break;
					}
					case GameType::CaptureTheFlag:
					{
						// flag limit
						s16* flag_limit = &config->flag_limit;
						sprintf(str, "%hd", *flag_limit);
						delta = menu->slider_item(u, _(strings::flag_limit), str);
						*flag_limit = s16(vi_max(1, vi_min(MAX_RESPAWNS, s32(*flag_limit) + delta)));
						if (delta)
							data.multiplayer.active_server_dirty = true;
						break;
					}
					default:
						vi_assert(false);
						break;
				}

				{
					// parkour ready time limit
					u8* time_limit = &config->time_limit_parkour_ready;
					sprintf(str, _(strings::timer), s32(*time_limit), 0);
					delta = menu->slider_item(u, _(strings::time_limit_parkour_ready), str);
					*time_limit = vi_max(0, vi_min(255, s32(*time_limit) + delta * (*time_limit >= (delta > 0 ? 10 : 11) ? 5 : 1)));
					if (delta)
						data.multiplayer.active_server_dirty = true;
				}

				{
					// region
					if (UIMenu::enum_option(&config->region, menu->slider_item(u, _(strings::region), _(Menu::region_string(config->region)))))
						data.multiplayer.active_server_dirty = true;
				}

				{
					// preset
					if (UIMenu::enum_option(&config->preset, menu->slider_item(u, _(strings::preset), _(preset_strings[s32(config->preset)]))))
						data.multiplayer.active_server_dirty = true;
				}

				if (config->preset == Net::Master::Ruleset::Preset::Custom)
				{
					{
						// drone shield
						s8* drone_shield = &config->ruleset.drone_shield;
						sprintf(str, "%d", s32(*drone_shield));
						delta = menu->slider_item(u, _(strings::drone_shield), str);
						*drone_shield = vi_max(0, vi_min(DRONE_SHIELD_MAX, (*drone_shield) + delta));
						if (delta)
							data.multiplayer.active_server_dirty = true;
					}

					{
						// spawn delay
						s8* spawn_delay = &config->ruleset.spawn_delay;
						sprintf(str, "%d", s32(*spawn_delay));
						delta = menu->slider_item(u, _(strings::spawn_delay), str);
						*spawn_delay = vi_max(1, vi_min(120, (*spawn_delay) + delta * (*spawn_delay >= (delta > 0 ? 10 : 11) ? 5 : 1)));
						if (delta)
							data.multiplayer.active_server_dirty = true;
					}

					{
						// start energy
						s16* start_energy = &config->ruleset.start_energy;
						sprintf(str, "%d", s32(*start_energy));
						delta = menu->slider_item(u, _(strings::start_energy), str);
						*start_energy = vi_max(0, vi_min(MAX_START_ENERGY, (*start_energy) + (delta * 50)));
						if (delta)
							data.multiplayer.active_server_dirty = true;
					}

					{
						// cooldown speed
						u8* cooldown_speed_index = &config->ruleset.cooldown_speed_index;
						sprintf(str, "%d%%%%", s32(config->ruleset.cooldown_speed() * 100.0f));
						delta = menu->slider_item(u, _(strings::cooldown_speed), str);
						*cooldown_speed_index = vi_max(1, vi_min(COOLDOWN_SPEED_MAX_INDEX, s32(*cooldown_speed_index) + delta));
						if (delta)
							data.multiplayer.active_server_dirty = true;
					}

					{
						// enable batteries
						b8* enable_batteries = &config->ruleset.enable_batteries;
						delta = menu->slider_item(u, _(strings::enable_batteries), _(*enable_batteries ? strings::on : strings::off));
						if (delta)
						{
							*enable_batteries = !(*enable_batteries);
							data.multiplayer.active_server_dirty = true;
						}
					}

					{
						// upgrades
						sprintf(str, "%d", s32(BitUtility::popcount(s16((1 << s32(Upgrade::count)) - 1) & (config->ruleset.upgrades_allow | config->ruleset.upgrades_default))));
						if (menu->item(u, _(strings::upgrades), str))
						{
							multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Upgrades);
							menu->end(u);
							return;
						}
					}

					{
						// start abilities
						sprintf(str, "%d", s32(config->ruleset.start_abilities.length));
						if (menu->item(u, _(strings::start_abilities), str))
						{
							multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::StartAbilities);
							menu->end(u);
							return;
						}
					}
				}

				menu->end(u);
				break;
			}
			case Data::Multiplayer::EditMode::Levels:
			{
				menu->start(u, multiplayer_menu_origin(), 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end(u);
					return;
				}

				for (s32 i = 0; i < Asset::Level::count; i++)
				{
					if (zone_is_pvp(AssetID(i)))
					{
						const ZoneNode* node = zone_node_by_id(AssetID(i));

						s32 existing_index = -1;
						for (s32 j = 0; j < config->levels.length; j++)
						{
							if (config->levels[j] == node->uuid)
							{
								existing_index = j;
								break;
							}
						}

						if (menu->item(u, Loader::level_name(AssetID(i)), nullptr, zone_max_teams(AssetID(i)) < config->team_count || !LEVEL_ALLOWED(node->uuid), existing_index == -1 ? AssetNull : Asset::Mesh::icon_checkmark))
						{
							if (existing_index == -1)
								config->levels.add(node->uuid);
							else
								config->levels.remove(existing_index);
							data.multiplayer.active_server_dirty = true;
						}
					}
				}

				menu->end(u);
				break;
			}
			case Data::Multiplayer::EditMode::StartAbilities:
			{
				menu->start(u, multiplayer_menu_origin(), 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end(u);
					return;
				}

				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					const UpgradeInfo& info = UpgradeInfo::list[i];
					if (info.type != UpgradeInfo::Type::Ability)
						continue;

					s32 existing_index = -1;
					for (s32 j = 0; j < config->ruleset.start_abilities.length; j++)
					{
						if (config->ruleset.start_abilities[j] == Ability(i))
						{
							existing_index = j;
							break;
						}
					}

					b8 disabled = existing_index == -1 && config->ruleset.start_abilities.length == config->ruleset.start_abilities.capacity();
					if (menu->item(u, _(info.name), nullptr, disabled, existing_index == -1 ? AssetNull : Asset::Mesh::icon_checkmark))
					{
						if (existing_index == -1)
							config->ruleset.start_abilities.add(Ability(i));
						else
							config->ruleset.start_abilities.remove(existing_index);
						data.multiplayer.active_server_dirty = true;
					}
				}

				menu->end(u);
				break;
			}
			case Data::Multiplayer::EditMode::Upgrades:
			{
				menu->start(u, multiplayer_menu_origin(), 0);

				if (cancel || menu->item(u, _(strings::back)))
				{
					Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
					multiplayer_edit_mode_transition(Data::Multiplayer::EditMode::Main);
					Game::cancel_event_eaten[0] = true;
					menu->end(u);
					return;
				}

				s16* upgrades_allow = &config->ruleset.upgrades_allow;
				s16* upgrades_default = &config->ruleset.upgrades_default;
				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					b8 is_allowed = (*upgrades_allow) & (1 << i);
					b8 is_default = (*upgrades_default) & (1 << i);
					AssetID string;
					if (is_default)
						string = strings::upgrade_default;
					else if (is_allowed)
						string = strings::upgrade_allowed;
					else
						string = strings::upgrade_disabled;
					const UpgradeInfo& info = UpgradeInfo::list[i];
					s32 delta = menu->slider_item(u, _(info.name), _(string), false, info.icon);
					if (delta)
					{
						data.multiplayer.active_server_dirty = true;
						s32 index = (s32(is_allowed) << 0) | (s32(is_default) << 1);
						index += delta;
						if (index < 0)
							index = 2;
						else if (index > 2)
							index = 0;
						is_allowed = index & (1 << 0);
						is_default = index & (1 << 1);

						if (is_allowed)
							*upgrades_allow = (*upgrades_allow) | (1 << i);
						else
							*upgrades_allow = (*upgrades_allow) & ~(1 << i);

						if (is_default)
							*upgrades_default = (*upgrades_default) | (1 << i);
						else
							*upgrades_default = (*upgrades_default) & ~(1 << i);
					}
				}

				menu->end(u);
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
		if (Game::level.mode != Game::Mode::Special) // we're editing a server while playing in it
			PlayerHuman::log_add(_(strings::entry_saved_in_game)); // changes will take effect next round
		else
			PlayerHuman::log_add(_(strings::entry_saved));
		data.multiplayer.server_lists[s32(ServerListType::Mine)].selected = 0;
		data.multiplayer.server_lists[s32(ServerListType::Mine)].scroll.pos = 0;
		data.multiplayer.active_server.config.id = id;
		data.multiplayer.active_server_dirty = false;
		data.multiplayer.request_id = 0;
	}
}

void master_server_details_response(const Net::Master::ServerDetails& details, u32 request_id)
{
	if (active()
		&& data.state == State::Multiplayer
		&& (data.multiplayer.state == Data::Multiplayer::State::EntryView || data.multiplayer.state == Data::Multiplayer::State::EntryEdit)
		&& data.multiplayer.request_id == request_id)
	{
		data.multiplayer.active_server = details;
		data.multiplayer.request_id = 0;
		if (data.multiplayer.state == Data::Multiplayer::State::EntryView)
			data.multiplayer.active_server_dirty = true; // dirty means we have data
		else
		{
			vi_assert(data.multiplayer.state == Data::Multiplayer::State::EntryEdit);
			data.multiplayer.active_server_dirty = false; // dirty means we've made changes
		}
		if (details.state.level != AssetNull) // a server is running this config
			ping_send(details.addr);
	}
}

void multiplayer_entry_view_update(const Update& u)
{
	b8 cancel = data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::Back, u)
		|| (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0]);

	if (data.multiplayer.request_id && !data.multiplayer.active_server_dirty) // we don't have any config data yet
	{
		if (cancel)
		{
			Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
			data.multiplayer.request_id = 0; // cancel active request
			Game::cancel_event_eaten[0] = true;
			multiplayer_state_transition(Data::Multiplayer::State::Browse);
#if !SERVER
			Net::Client::master_cancel_outgoing();
#endif
		}
	}
	else
	{
		if (cancel)
		{
			Game::cancel_event_eaten[0] = true;
			Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, 0);
			if (Game::level.mode != Game::Mode::Special) // we are currently in the server we're viewing
				hide();
			else
				multiplayer_state_transition(Data::Multiplayer::State::Browse);
			return;
		}
		else if (data.multiplayer.active_server.is_admin
			&& (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::EditServer, u) || (u.last_input->get(Controls::UIContextAction, 0) && !u.input->get(Controls::UIContextAction, 0))))
		{
			Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
			multiplayer_state_transition(Data::Multiplayer::State::EntryEdit);
			return;
		}
		else if (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::ConnectServer, u)
			|| (u.input->get(Controls::Interact, 0) && !u.last_input->get(Controls::Interact, 0)))
		{
			u32 id = data.multiplayer.active_server.config.id;
			Game::unload_level();
#if !SERVER
			Net::Client::master_request_server(id);
#endif
			Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
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

void multiplayer_change_region_update(const Update& u)
{
	if (!Menu::choose_region(u, multiplayer_menu_origin(), 0, &data.multiplayer.menu[0], Menu::AllowClose::Yes))
	{
		Data::Multiplayer::ServerList* top = &data.multiplayer.server_lists[s32(ServerListType::Top)];
		top->entries.length = 0;
		top->selected = 0;
		top->scroll.pos = 0;

		multiplayer_state_transition(Data::Multiplayer::State::Browse);
	}
}

void hide();

void title()
{
	Game::save.reset();
	Game::session.reset(SessionType::Story);
	hide();
}

void multiplayer_tab_left(s8)
{
	Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
	multiplayer_switch_tab(ServerListType(vi_max(0, s32(global.multiplayer.tab) - 1)));
	multiplayer_state_transition(Data::Multiplayer::State::Browse);
}

void multiplayer_tab_right(s8)
{
	Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
	multiplayer_switch_tab(ServerListType(vi_min(s32(ServerListType::count) - 1, s32(global.multiplayer.tab) + 1)));
	multiplayer_state_transition(Data::Multiplayer::State::Browse);
}

void multiplayer_tab_switch_mouse(s8)
{
	Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT, 0);
	multiplayer_switch_tab(data.multiplayer.tab_mouse_try);
	multiplayer_state_transition(Data::Multiplayer::State::Browse);
}

b8 multiplayer_tab_switch_enabled()
{
	return Game::level.mode == Game::Mode::Special && !Menu::dialog_active(0); // only in the main menu; not in-game
}

void multiplayer_tab_switch_try(void (*action)(s8))
{
	if (data.multiplayer.state == Data::Multiplayer::State::EntryEdit
		&& !data.multiplayer.request_id
		&& data.multiplayer.active_server_dirty)
		Menu::dialog(0, action, _(strings::confirm_entry_cancel));
	else
		action(0);
}

Rect2 multiplayer_tab_rect(s32 i)
{
	Rect2 rect = multiplayer_main_view_rect();
	Vec2 tab_size = TAB_SIZE;
	return
	{
		rect.pos + Vec2(i * (tab_size.x + PADDING), rect.size.y),
		tab_size,
	};
}

void multiplayer_top_bar_button_add(Data::Multiplayer::TopBarLayout* layout, Vec2* pos, Data::Multiplayer::TopBarLayout::Button::Type type, const char* format, ...)
{
	Data::Multiplayer::TopBarLayout::Button* button = layout->buttons.add();

	UIText text;
	text.size = TEXT_SIZE;
	text.anchor_y = UIText::Anchor::Center;
	text.anchor_x = UIText::Anchor::Min;
	{
		va_list args;
		va_start(args, format);
		text.text(0, format, args);
		va_end(args);
	}
	button->rect = text.rect(*pos);
	pos->x = button->rect.pos.x + button->rect.size.x + PADDING * 3.0f;
	strncpy(button->text, text.rendered_string, UI_TEXT_MAX);
	button->type = type;
}

void multiplayer_top_bar_layout(Data::Multiplayer::TopBarLayout* layout)
{
	using Layout = VI::Overworld::Data::Multiplayer::TopBarLayout;
	Rect2 rect = multiplayer_main_view_rect().outset(-PADDING);
	Vec2 panel_size(rect.size.x, PADDING + TEXT_SIZE * UI::scale);
	Vec2 top_bar_size(panel_size.x, panel_size.y * 1.5f);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

	layout->rect = { pos, top_bar_size };

	pos += Vec2(PADDING, top_bar_size.y * 0.5f);

	layout->buttons.length = 0;
	switch (data.multiplayer.state)
	{
		case Data::Multiplayer::State::Browse:
		{
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::Back, _(strings::prompt_back));
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::ChooseRegion, _(strings::prompt_region), _(Menu::region_string(Settings::region)));
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::CreateServer, _(strings::prompt_entry_create));
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::Select, _(strings::prompt_select));
			break;
		}
		case Data::Multiplayer::State::EntryView:
		{
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::Back, _(strings::prompt_back));
			if (data.multiplayer.active_server.is_admin)
				multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::EditServer, _(strings::prompt_entry_edit));
			if (Game::level.mode == Game::Mode::Special) // if we're not currently connected to the server we're viewing
				multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::ConnectServer, _(strings::prompt_connect));
			break;
		}
		case Data::Multiplayer::State::EntryEdit:
		{
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::Back, _(strings::prompt_back));
			multiplayer_top_bar_button_add(layout, &pos, Layout::Button::Type::SaveServer, _(strings::prompt_entry_save));
			break;
		}
		case Data::Multiplayer::State::ChangeRegion:
			break;
		default:
			vi_assert(false);
			break;
	}
}

void multiplayer_update(const Update& u)
{
	if (Menu::main_menu_state != Menu::State::Hidden || Game::scheduled_load_level != AssetNull)
		return;

	if (data.multiplayer.state == Data::Multiplayer::State::Browse
		&& (data.multiplayer.top_bar.button_clicked(Data::Multiplayer::TopBarLayout::Button::Type::Back, u) || (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)))
		&& !Game::cancel_event_eaten[0])
	{
		if (Game::level.mode == Game::Mode::Special)
			title();
		else
			hide();
		Game::cancel_event_eaten[0] = true;
		return;
	}

	for (s32 i = 1; i < MAX_GAMEPADS; i++)
	{
		if (u.input->gamepads[i].type == Gamepad::Type::None)
			Game::session.local_player_mask &= ~(1 << i);
		else
		{
			b8 player_active = Game::session.local_player_mask & (1 << i);
			if (!player_active
				&& ((u.last_input->get(Controls::Interact, i) && !u.input->get(Controls::Interact, i))
					|| (u.last_input->get(Controls::Start, i) && !u.input->get(Controls::Start, i))))
			{
				Audio::post_global(AK::EVENTS::PLAY_MENU_ALTER, i);
				Game::session.local_player_mask |= 1 << i;
			}
			else if (player_active
				&& u.last_input->get(Controls::Cancel, i) && !u.input->get(Controls::Cancel, i))
			{
				Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, i);
				Game::session.local_player_mask &= ~(1 << i);
			}
		}
	}

#if !SERVER
	Net::Client::master_keepalive();
#endif

	if (multiplayer_tab_switch_enabled())
	{
		if (s32(global.multiplayer.tab) > 0 && u.last_input->get(Controls::TabLeft, 0) && !u.input->get(Controls::TabLeft, 0))
			multiplayer_tab_switch_try(&multiplayer_tab_left);
		else if (s32(global.multiplayer.tab) < s32(ServerListType::count) - 1 && u.last_input->get(Controls::TabRight, 0) && !u.input->get(Controls::TabRight, 0))
			multiplayer_tab_switch_try(&multiplayer_tab_right);
		else if (Game::ui_gamepad_types[0] == Gamepad::Type::None)
		{
			// mouse tab switching
			if (u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)))
			{
				for (s32 i = 0; i < s32(ServerListType::count); i++)
				{
					if (multiplayer_tab_rect(i).contains(u.input->cursor))
					{
						if (ServerListType(i) != global.multiplayer.tab)
						{
							data.multiplayer.tab_mouse_try = ServerListType(i);
							multiplayer_tab_switch_try(&multiplayer_tab_switch_mouse);
						}
						break;
					}
				}
			}
		}
	}

	if (data.multiplayer.tab_timer > 0.0f)
	{
		data.multiplayer.tab_timer = vi_max(0.0f, data.multiplayer.tab_timer - u.time.delta);
		if (data.multiplayer.tab_timer == 0.0f)
			data.multiplayer.menu[s32(data.multiplayer.edit_mode)].animate();
	}
	else
	{
		multiplayer_top_bar_layout(&data.multiplayer.top_bar);
		switch (data.multiplayer.state)
		{
			case Data::Multiplayer::State::Browse:
				multiplayer_browse_update(u);
				break;
			case Data::Multiplayer::State::EntryEdit:
				multiplayer_entry_edit_update(u);
				break;
			case Data::Multiplayer::State::EntryView:
				multiplayer_entry_view_update(u);
				break;
			case Data::Multiplayer::State::ChangeRegion:
				multiplayer_change_region_update(u);
				break;
			default:
				vi_assert(false);
				break;
		}
	}
}

void multiplayer_in_game_update(const Update& u)
{
#if !SERVER
	if (PlayerHuman::list.count() < Game::session.config.max_players)
	{
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (u.input->gamepads[i].type != Gamepad::Type::None
				&& ((u.last_input->get(Controls::Start, i) && !u.input->get(Controls::Start, i))
					|| (u.last_input->get(Controls::Interact, i) && !u.input->get(Controls::Interact, i)))
				&& !PlayerHuman::for_gamepad(i))
			{
				Game::session.local_player_mask |= 1 << i;
				if (Net::Client::mode() == Net::Client::Mode::Connected)
					Net::Client::add_player(i);
				else
					Game::add_local_player(i);
			}
		}
	}
#endif
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

void multiplayer_top_bar_draw(const RenderParams& params, const Data::Multiplayer::TopBarLayout& layout)
{
	using Layout = VI::Overworld::Data::Multiplayer::TopBarLayout;
	UI::box(params, layout.rect, UI::color_background);

	UIText text;
	text.size = TEXT_SIZE;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Min;
	for (s32 i = 0; i < layout.buttons.length; i++)
	{
		const Layout::Button& button = layout.buttons[i];

		{
			Rect2 container = button.container();
			if (Game::ui_gamepad_types[0] == Gamepad::Type::None && container.contains(params.sync->input.cursor))
			{
				UI::box(params, container, params.sync->input.keys.get(s32(KeyCode::MouseLeft)) ? UI::color_alert() : UI::color_accent());
				text.color = UI::color_background;
			}
			else
				text.color = UI::color_accent();
		}

		text.text_raw(0, button.text);
		text.draw(params, button.rect.pos);
	}
}

void multiplayer_browse_draw(const RenderParams& params, const Rect2& rect)
{
	multiplayer_top_bar_draw(params, data.multiplayer.top_bar);
	Vec2 panel_size(rect.size.x, PADDING + TEXT_SIZE * UI::scale);
	Vec2 pos = data.multiplayer.top_bar.rect.pos + Vec2(0, -panel_size.y - PADDING * 1.5f);

	// server list
	{
		UIText text;
		text.size = TEXT_SIZE * 0.85f;
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
			{
				b8 active = (Rect2(pos, panel_size).contains(params.sync->input.cursor) && params.sync->input.keys.get(s32(KeyCode::MouseLeft)))
					|| params.sync->input.get(Controls::Interact, 0); // is the user pressing down on this entry?
				UI::border(params, Rect2(pos, panel_size).outset(-2.0f * UI::scale), 2.0f, active ? UI::color_alert() : UI::color_accent());
			}

			text.color = selected ? UI::color_accent() : UI::color_default;
			text.clip = 40;
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
				game_type_string(&text, entry.preset, entry.game_type, entry.team_count, entry.max_players);
				text.draw(params, pos + Vec2(panel_size.x * 0.69f, panel_size.y * 0.5f));
			}

			{
				if (!selected)
					text.color = UI::color_default;
				s32 max_players = entry.server_state.level == AssetNull ? entry.max_players : entry.server_state.max_players;
				text.text(0, "%d/%d", vi_max(0, max_players - entry.server_state.player_slots), max_players);
				text.draw(params, pos + Vec2(panel_size.x * 0.91f, panel_size.y * 0.5f));
			}

			if (entry.server_state.level != AssetNull) // there's a server running this config
			{
				r32 p = ping(entry.addr);
				if (p > 0.0f)
				{
					if (!selected)
						text.color = UI::color_ping(p);
					text.text(0, _(strings::ping), s32(p * 1000.0f));
					text.draw(params, pos + Vec2(panel_size.x * 0.95f, panel_size.y * 0.5f));
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
			case Data::Multiplayer::EditMode::Levels:
			case Data::Multiplayer::EditMode::Main:
			case Data::Multiplayer::EditMode::Upgrades:
			case Data::Multiplayer::EditMode::StartAbilities:
			{
				const Rect2& vp = params.camera->viewport;
				
				Vec2 panel_size(((rect.size.x + PADDING * -2.0f) / 3.0f) + PADDING * -2.0f, PADDING + TEXT_SIZE * UI::scale);

				multiplayer_top_bar_draw(params, data.multiplayer.top_bar);

				Vec2 pos(vp.size.x * 0.5f + MENU_ITEM_WIDTH * -0.5f, data.multiplayer.top_bar.rect.pos.y - PADDING);

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
				UIMenu::text_clip(&text, 0, data.multiplayer.state_transition_time, 100.0f, 28);

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
							text.text(0, _(strings::levels));
							break;
						case Data::Multiplayer::EditMode::Upgrades:
							text.text(0, _(strings::upgrades));
							break;
						case Data::Multiplayer::EditMode::StartAbilities:
							text.text(0, _(strings::start_abilities));
							break;
						default:
							vi_assert(false);
							break;
					}
					UIMenu::text_clip(&text, 0, data.multiplayer.state_transition_time + 0.1f, 100.0f);

					{
						Vec2 text_pos = pos + Vec2(PADDING, 0);
						UI::box(params, text.rect(text_pos).outset(PADDING), UI::color_background);
						text.draw(params, text_pos);
						pos.y = text.rect(text_pos).pos.y + PADDING * -3.0f;
					}
				}

				data.multiplayer.menu[s32(data.multiplayer.edit_mode)].draw_ui(params);
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
		const r32 text_size = TEXT_SIZE * 0.8f;
		const r32 padding = PADDING * 0.8f;
		Vec2 panel_size(((rect.size.x + padding * -2.0f) / 3.0f) + padding * -2.0f, padding + text_size * UI::scale);

		multiplayer_top_bar_draw(params, data.multiplayer.top_bar);

		Vec2 pos = data.multiplayer.top_bar.rect.pos + Vec2(padding, padding * -2.0f);

		const Net::Master::ServerDetails& details = data.multiplayer.active_server;

		UIText text;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Max;
		text.color = UI::color_default;
		text.size = MENU_ITEM_FONT_SIZE;
		text.font = Asset::Font::pt_sans;
		text.wrap(rect.size.x + padding * -2.0f);
		{
			char buffer[UI_TEXT_MAX + 1];
			snprintf(buffer, UI_TEXT_MAX, _(strings::map_by), details.config.name, details.creator_username);
			text.text_raw(0, buffer);
		}

		{
			Rect2 r = text.rect(pos).outset(padding);
			UI::box(params, r, UI::color_background);
			text.draw(params, pos);
			pos.y = r.pos.y + padding * -2.0f;
		}

		text.font = Asset::Font::lowpoly;
		text.size = text_size;
		text.wrap_width = 0;

		Vec2 top = pos;

		UIText value = text;
		value.anchor_x = UIText::Anchor::Max;

		// column 1
		{
			s32 rows = (details.state.level == AssetNull ? 1 : 2)
				+ 5
				+ (details.config.game_type == GameType::Assault ? 0 : 1)
				+ (details.config.preset == Net::Master::Ruleset::Preset::Custom ? 2 : 0);
			UI::box(params, { pos + Vec2(-padding, panel_size.y * -rows), Vec2(panel_size.x + padding * 2.0f, panel_size.y * rows + padding) }, UI::color_background);

			if (details.state.level != AssetNull)
			{
				// level name
				text.color = UI::color_accent();
				text.text(0, Loader::level_name(details.state.level));
				text.draw(params, pos);
				text.color = UI::color_default;
				pos.y -= panel_size.y;

				// players
				text.text(0, _(strings::player_count), vi_max(0, s32(details.state.max_players - details.state.player_slots)), s32(details.state.max_players));
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
			game_type_string(&text, details.config.preset, details.config.game_type, details.config.team_count, details.config.max_players);
			text.draw(params, pos);
			pos.y -= panel_size.y;

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

			// time limit
			text.text(0, _(strings::time_limit));
			text.draw(params, pos);
			value.text(0, "%d:00", s32(details.config.time_limit_minutes[s32(details.config.game_type)]));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			switch (details.config.game_type)
			{
				case GameType::Assault:
					break;
				case GameType::Deathmatch:
				{
					// kill limit
					text.text(0, _(strings::kill_limit));
					text.draw(params, pos);
					value.text(0, "%d", s32(details.config.kill_limit));
					value.draw(params, pos + Vec2(panel_size.x, 0));
					pos.y -= panel_size.y;
					break;
				}
				case GameType::CaptureTheFlag:
				{
					// flag limit
					text.text(0, _(strings::flag_limit));
					text.draw(params, pos);
					value.text(0, "%d", s32(details.config.flag_limit));
					value.draw(params, pos + Vec2(panel_size.x, 0));
					pos.y -= panel_size.y;
					break;
				}
				default:
					vi_assert(false);
			}

			// bots
			text.text(0, _(strings::fill_bots));
			text.draw(params, pos);
			value.text(0, "%d", s32(details.config.fill_bots));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// min players
			text.text(0, _(strings::min_players));
			text.draw(params, pos);
			value.text(0, "%d", s32(details.config.min_players));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			// parkour ready time limit
			text.text(0, _(strings::time_limit_parkour_ready));
			text.draw(params, pos);
			value.text(0, "%d:00", s32(details.config.time_limit_parkour_ready));
			value.draw(params, pos + Vec2(panel_size.x, 0));
			pos.y -= panel_size.y;

			if (details.config.preset == Net::Master::Ruleset::Preset::Custom)
			{
				// cooldown speed
				text.text(0, _(strings::cooldown_speed));
				text.draw(params, pos);
				value.text(0, "%d%%", s32(details.config.ruleset.cooldown_speed() * 100.0f));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;

				// drone shield
				text.text(0, _(strings::drone_shield));
				text.draw(params, pos);
				value.text(0, "%d", s32(details.config.ruleset.drone_shield));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
		}

		// column 2
		pos = top + Vec2(panel_size.x + padding * 3.0f, 0);
		{
			s32 rows = 1 + details.config.levels.length;
			UI::box(params, { pos + Vec2(-padding, panel_size.y * -rows), Vec2(panel_size.x + padding * 2.0f, panel_size.y * rows + padding) }, UI::color_background);

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
					const ZoneNode* node = zone_node_by_uuid(details.config.levels[i]);
					text.text_raw(0, Loader::level_name(node->id));
					text.draw(params, pos);
					pos.y -= panel_size.y;
				}
			}
		}

		// column 3
		pos = top + Vec2((panel_size.x + padding * 3.0f) * 2.0f, 0);
		if (details.config.preset == Net::Master::Ruleset::Preset::Custom)
		{
			s32 rows = vi_max(1, s32(details.config.ruleset.start_abilities.length)) + 2 + s32(Upgrade::count);
			UI::box(params, { pos + Vec2(-padding, panel_size.y * -rows), Vec2(panel_size.x + padding * 2.0f, panel_size.y * rows + padding) }, UI::color_background);

			// start abilities
			if (details.config.ruleset.start_abilities.length > 0)
				text.color = UI::color_accent();
			text.text(0, _(strings::start_abilities));
			text.draw(params, pos);
			text.color = UI::color_default;
			if (details.config.ruleset.start_abilities.length == 0)
			{
				value.text(0, _(strings::none));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
			else
			{
				pos.y -= panel_size.y;
				for (s32 i = 0; i < details.config.ruleset.start_abilities.length; i++)
				{
					text.text(0, _(UpgradeInfo::list[s32(details.config.ruleset.start_abilities[i])].name));
					text.draw(params, pos);
					pos.y -= panel_size.y;
				}
			}

			// upgrades
			text.color = UI::color_accent();
			text.text(0, _(strings::upgrades));
			text.draw(params, pos);
			pos.y -= panel_size.y;
			text.color = UI::color_default;
			for (s32 i = 0; i < s32(Upgrade::count); i++)
			{
				text.text(0, _(UpgradeInfo::list[i].name));
				text.draw(params, pos);
				if (details.config.ruleset.upgrades_default & (1 << i))
					value.text(0, _(strings::upgrade_default));
				else if (details.config.ruleset.upgrades_allow & (1 << i))
					value.text(0, "");
				else
					value.text(0, _(strings::upgrade_disabled));
				value.draw(params, pos + Vec2(panel_size.x, 0));
				pos.y -= panel_size.y;
			}
		}
	}
}

void multiplayer_change_region_draw(const RenderParams& params, const Rect2& rect)
{
	data.multiplayer.menu[0].draw_ui(params);
}

void multiplayer_draw(const RenderParams& params)
{
	if (Menu::main_menu_state == Menu::State::Hidden)
	{
		Rect2 rect = multiplayer_main_view_rect();

		Vec2 tab_size = TAB_SIZE;

		// left/right tab control prompt
		{
			UIText text;
			text.size = TEXT_SIZE;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = multiplayer_tab_switch_enabled() && s32(global.multiplayer.tab) > 0 ? UI::color_default : UI::color_disabled();
			text.text(0, "[{{TabLeft}}]");

			Vec2 pos = rect.pos + Vec2(tab_size.x * 0.5f, rect.size.y + tab_size.y * 1.5f);
			UI::box(params, text.rect(pos).outset(PADDING), UI::color_background);
			text.draw(params, pos);

			pos.x += rect.size.x - tab_size.x;
			text.text(0, "[{{TabRight}}]");
			UI::box(params, text.rect(pos).outset(PADDING), UI::color_background);
			text.color = multiplayer_tab_switch_enabled() && s32(global.multiplayer.tab) < s32(ServerListType::count) - 1 ? UI::color_default : UI::color_disabled();
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
				{
					color = &UI::color_disabled();
					if (Game::ui_gamepad_types[0] == Gamepad::Type::None
						&& Rect2(pos, tab_size).contains(params.sync->input.cursor))
						color = params.sync->input.keys.get(s32(KeyCode::MouseLeft)) ? &UI::color_alert() : &UI::color_default;
				}

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
			Rect2 rect_padded = rect.outset(-PADDING);
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
				case Data::Multiplayer::State::ChangeRegion:
				{
					multiplayer_change_region_draw(params, rect_padded);
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
				Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.1f) + Vec2(gamepad_spacing * (gamepad_count - 1) * -0.5f, 0);
				for (s32 i = 0; i < gamepad_count; i++)
				{
					draw_gamepad_icon(params, pos, i, (Game::session.local_player_mask & (1 << i)) ? UI::color_accent() : UI::color_disabled(), SCALE_MULTIPLIER);
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
	vi_assert(Game::level.local && Game::session.type == SessionType::Story);
	if (zone == Asset::Level::Isca && !Game::save.tutorial_complete && Game::save.zones[zone] == ZoneState::PvpHostile)
		Game::schedule_load_level(zone, Game::Mode::Pvp); // run tutorial locally
	else
	{
		// connect to a server
#if !SERVER
		Game::unload_level();
		Net::Client::master_request_server(0, nullptr, zone, Game::save.zones[zone] == ZoneState::PvpFriendly ? StoryModeTeam::Defend : StoryModeTeam::Attack); // 0 = story mode
#endif
	}
}

void focus_camera(const Update& u, const Vec3& target_pos, const Quat& target_rot)
{
	data.camera.ref()->pos += (target_pos - data.camera.ref()->pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
	data.camera.ref()->rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), data.camera.ref()->rot, target_rot);
}

void focus_camera(const Update& u, const ZoneNode& zone)
{
	Vec3 target_pos = zone.pos() + zone.rot * global.camera_offset_pos;
	focus_camera(u, target_pos, zone.rot * global.camera_offset_rot);
}

const ZoneNode* zone_node_by_id(AssetID id)
{
	for (s32 i = 0; i < global.zones.length; i++)
	{
		if (global.zones[i].id == id)
			return &global.zones[i];
	}
	return nullptr;
}

const ZoneNode* zone_node_by_uuid(AssetID uuid)
{
	for (s32 i = 0; i < global.zones.length; i++)
	{
		if (global.zones[i].uuid == uuid)
			return &global.zones[i];
	}
	return nullptr;
}

AssetID zone_id_for_uuid(AssetID uuid)
{
	return zone_node_by_uuid(uuid)->id;
}

AssetID zone_uuid_for_id(AssetID id)
{
	return zone_node_by_id(id)->uuid;
}

Vec3 zone_color(const ZoneNode& zone)
{
	ZoneState zone_state = Game::save.zones[zone.id];
	switch (zone_state)
	{
		case ZoneState::Locked:
			return Vec3(0.3f);
		case ZoneState::ParkourUnlocked:
		case ZoneState::PvpUnlocked:
			return Vec3(1.0f);
		case ZoneState::ParkourOwned:
		case ZoneState::PvpFriendly:
			return Team::color_friend.xyz();
		case ZoneState::PvpHostile:
			return Team::color_enemy().xyz();
		default:
		{
			vi_assert(false);
			return Vec3::zero;
		}
	}
}

const Vec4& zone_ui_color(const ZoneNode& zone)
{
	vi_assert(Game::session.type == SessionType::Story);
	ZoneState zone_state = Game::save.zones[zone.id];
	switch (zone_state)
	{
		case ZoneState::Locked:
			return UI::color_disabled();
		case ZoneState::ParkourUnlocked:
		case ZoneState::PvpUnlocked:
			return UI::color_default;
		case ZoneState::ParkourOwned:
			return UI::color_accent();
		case ZoneState::PvpFriendly:
			return Team::ui_color_friend();
		case ZoneState::PvpHostile:
			return Team::ui_color_enemy();
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
void zones_draw_override(const RenderParams& params)
{
	struct Comparator
	{
		r32 priority(const ZoneNode* n)
		{
			// sort farthest zones first
			Vec3 camera_forward = data.camera.ref()->rot * Vec3(0, 0, 1);
			return camera_forward.dot(n->pos() - data.camera.ref()->pos);
		}

		s32 compare(const ZoneNode* a, const ZoneNode* b)
		{
			r32 pa = priority(a);
			r32 pb = priority(b);
			if (pa > pb)
				return 1;
			else if (pa == pb)
				return 0;
			else
				return -1;
		}
	};

	StaticArray<const ZoneNode*, Asset::Level::count> zones;

	Comparator key;

	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode* zone = &global.zones[i];
		// flash if necessary
		if (Game::session.type == SessionType::Multiplayer
			|| Game::real_time.total > data.story.map.zones_change_time[zone->id] + 0.5f
			|| UI::flash_function(Game::real_time.total))
		{
			zones.add(zone);
		}
	}

	Quicksort::sort<const ZoneNode*, Comparator>(zones.data, 0, zones.length, &key);

	RenderSync* sync = params.sync;

	sync->write(RenderOp::DepthTest);
	sync->write<b8>(true);

	for (s32 i = 0; i < zones.length; i++)
	{
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);
		const ZoneNode& zone = *zones[i];
		zone_draw_mesh(params, zone.mesh, zone.pos(), Vec4(zone_color(zone), 1.0f));
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);
		zone_draw_mesh(params, zone.mesh, zone.pos(), BACKGROUND_COLOR);
	}
}

// returns current zone node
const ZoneNode* zones_draw(const RenderParams& params)
{
	vi_assert(Game::session.type == SessionType::Story);

	if (data.timer_deploy > 0.0f || Game::scheduled_load_level != AssetNull)
		return nullptr;

	// highlight zone locations
	const ZoneNode* selected_zone = zone_node_by_id(data.zone_selected);

	// "you are here"
	const ZoneNode* current_zone = zone_node_by_id(Game::level.id);
	Vec2 p;
	if (current_zone && UI::project(params, current_zone->pos(), &p))
		UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_accent(), PI);

	// highlight selected zone
	if (selected_zone)
	{
		Vec2 p;
		if (UI::project(params, selected_zone->pos(), &p))
			UI::triangle_border(params, { p, Vec2(48.0f * UI::scale) }, BORDER * 2.0f, UI::color_accent(), PI);
	}

	// zone under attack
	const ZoneNode* under_attack = zone_node_by_id(zone_under_attack());
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

		r32 time_remaining = Game::session.zone_under_attack_timer - ZONE_UNDER_ATTACK_THRESHOLD;
		if (time_remaining > 0.0f)
		{
			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::color_alert();

			s32 remaining_minutes = s32(time_remaining / 60.0f);
			s32 remaining_seconds = s32(time_remaining - (remaining_minutes * 60.0f));
			text.text(0, _(strings::timer), remaining_minutes, remaining_seconds);

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
			return Game::session.zone_under_attack_timer > 0.0f && zone_id == zone_under_attack();
#endif
		}
		case ZoneState::PvpHostile:
			return true;
		default:
			return false;
	}
}

void resource_change(Resource r, s16 delta)
{
	Game::save.resources[s32(r)] += delta;
	vi_assert(Game::save.resources[s32(r)] >= 0);
	if (r != Resource::Energy)
		data.story.inventory.resource_change_time[s32(r)] = Game::real_time.total;
}

AssetID zone_under_attack()
{
	return Game::session.zone_under_attack;
}

void hide()
{
	if (data.timer_transition == 0.0f)
	{
		data.timer_transition = TRANSITION_TIME;
		data.state_next = State::Hidden;
		Audio::post_global(AK::EVENTS::PLAY_TRANSITION_OUT, 0);
	}
}

void hide_complete()
{
	if (modal())
		Particles::clear();
	if (data.camera.ref())
	{
		data.camera.ref()->remove();
		data.camera = nullptr;
	}
	if (data.restore_camera.ref())
	{
		data.restore_camera.ref()->flag(CameraFlagActive, true);
		data.restore_camera.ref()->flag(CameraFlagColors, true);
		data.restore_camera = nullptr;
	}
	if (data.state == State::StoryMode)
	{
		Game::save.inside_terminal = false;
		Game::save.zone_overworld = Game::level.id;
		TerminalEntity::open();
		PlayerControlHuman::list.iterator().item()->terminal_exit();
	}
	data.state = data.state_next = State::Hidden;
}

void deploy_done()
{
	vi_assert(Game::session.type == SessionType::Story);
	go(data.zone_selected);
}

void deploy_update(const Update& u)
{
	const ZoneNode& zone = *zone_node_by_id(data.zone_selected);
	focus_camera(u, zone);

	// must use Game::real_time because Game::time is paused when overworld is active

	r32 old_timer = data.timer_deploy;
	data.timer_deploy = vi_max(0.0f, data.timer_deploy - Game::real_time.delta);

	if (data.timer_deploy > 0.5f)
	{
		// particles
		r32 t = old_timer;
		const r32 particle_interval = 0.015f;
		const ZoneNode* zone = zone_node_by_id(data.zone_selected);
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
		data.camera.ref()->pos += Vec3(noise::sample2d(Vec2(offset)) * shake, noise::sample2d(Vec2(offset + 67)) * shake, noise::sample2d(Vec2(offset + 137)) * shake);

		if (old_timer >= 0.5f)
			Audio::post_global(AK::EVENTS::PLAY_OVERWORLD_DEPLOY, 0);
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

b8 story_mode_enable_input()
{
	const Data::StoryMode& story = data.story;
	return data.state != State::Hidden
		&& data.timer_transition == 0.0f
		&& data.timer_deploy == 0.0f
		&& story.inventory.timer_buy == 0.0f
		&& !Menu::dialog_active(0)
		&& !UIMenu::active[0];
}

void group_join(Game::Group g)
{
	// todo: redo this whole thing
	Game::save.group = g;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (zone.max_teams > 2)
		{
			if (g == Game::Group::None)
				zone_change(zone.id, ZoneState::Locked);
			else
				zone_change(zone.id, mersenne::randf_cc() > 0.7f ? ZoneState::PvpFriendly : ZoneState::PvpHostile);
		}
	}
}

void capture_start(s8 gamepad = 0)
{
	if (zone_can_capture(data.zone_selected))
		deploy_start();
}

void zone_change(AssetID zone, ZoneState state)
{
	ZoneState state_old = Game::save.zones[zone];
	if (state_old != state)
	{
		if (state == ZoneState::PvpFriendly)
		{
			// we captured the zone; give rewards
			const ZoneNode* z = zone_node_by_id(zone);
			for (s32 i = 0; i < s32(Resource::count); i++)
				resource_change(Resource(i), z->rewards[i]);
		}
		else if (state == ZoneState::PvpHostile && state_old == ZoneState::PvpFriendly)
		{
			char str[UI_TEXT_MAX];
			sprintf(str, _(strings::zone_lost), Loader::level_name(zone));
			PlayerHuman::log_add(str, AI::Team(0));
		}
	}

	Game::save.zones[zone] = state;
	data.story.map.zones_change_time[zone] = Game::real_time.total;
}

s32 zone_max_teams(AssetID zone_id)
{
	const ZoneNode* z = zone_node_by_id(zone_id);
	return z ? z->max_teams : 0;
}

b8 zone_is_pvp(AssetID zone_id)
{
	return zone_max_teams(zone_id) > 0;
}

void zone_rewards(AssetID zone_id, s16* rewards)
{
	const ZoneNode* z = zone_node_by_id(zone_id);
	for (s32 i = 0; i < s32(Resource::count); i++)
		rewards[i] = z->rewards[i];
}

b8 zone_filter_default(AssetID zone_id)
{
	return true;
}

b8 zone_filter_can_be_attacked(AssetID zone_id)
{
	if (zone_id == Game::level.id || (Game::session.type == SessionType::Story && zone_id == Game::save.zone_current))
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

Rect2 story_mode_main_view_rect(s32 index)
{
	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	const DisplayMode& display = Settings::display();
	Vec2 center = Vec2(r32(display.width), r32(display.height)) * 0.5f;
	Vec2 total_size = Vec2(main_view_size.x + (tab_size.x + PADDING), main_view_size.y);
	Vec2 bottom_left = center + total_size * -0.5f + Vec2(0, -tab_size.y);
	bottom_left.x += r32(index) * (tab_size.x + PADDING);

	return { bottom_left, main_view_size };
}

Rect2 tab_map_capture_prompt(const Rect2& rect, const RenderParams* params = nullptr)
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.text(0, _(Game::save.zones[data.zone_selected] == ZoneState::PvpFriendly ? strings::prompt_defend : strings::prompt_capture));

	Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

	Rect2 box = text.rect(pos).outset(8 * UI::scale);
	if (params)
	{
		const Vec4* bg;
		if (params->sync->input.get(Controls::Interact, 0)
			|| (Game::ui_gamepad_types[0] == Gamepad::Type::None && box.contains(params->sync->input.cursor)))
		{
			text.color = UI::color_background;
			if (params->sync->input.keys.get(s32(KeyCode::MouseLeft)))
				bg = &UI::color_alert();
			else
				bg = &UI::color_accent();
		}
		else
		{
			bg = &UI::color_background;
			text.color = UI::color_accent();
		}
		UI::box(*params, box, *bg);
		text.draw(*params, pos);
	}

	return box;
}

void tab_map_update(const Update& u)
{
	if (data.story.tab == StoryTab::Map && data.story.tab_timer == 0.0f)
	{
		const ZoneNode* zone = zone_node_by_id(data.zone_selected);
		if (!zone)
			return;

		if (story_mode_enable_input())
		{
			if (zone_can_capture(data.zone_selected)
				&& ((u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				|| (u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)) && tab_map_capture_prompt(story_mode_main_view_rect(s32(data.story.tab))).contains(u.input->cursor))))
			{
				// capture button
				capture_start();
			}
			else
			{
				// movement

				const ZoneNode* closest = nullptr;

				// keyboard/gamepad
				Vec2 movement = PlayerHuman::camera_topdown_movement(u, 0, data.camera.ref()->rot);
				r32 movement_amount = movement.length();
				if (movement_amount > 0.0f)
				{
					movement /= movement_amount;
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
				}
				else if (u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)))
				{
					// mouse
					
					r32 closest_distance = FLT_MAX;

					Mat4 view_projection = data.camera.ref()->view() * data.camera.ref()->projection;
					const Rect2& viewport = data.camera.ref()->viewport;

					for (s32 i = 0; i < global.zones.length; i++)
					{
						const ZoneNode& candidate = global.zones[i];

						for (s32 j = 0; j < candidate.children.length; j++)
						{
							const Vec3& candidate_pos = candidate.children[j];
							Vec2 p;
							if (UI::project(view_projection, viewport, candidate_pos, &p))
							{
								r32 distance = (p - u.input->cursor).length_squared();
								if (distance < closest_distance)
								{
									closest = &candidate;
									closest_distance = distance;
								}
							}
						}
					}
				}

				if (closest)
				{
					data.zone_selected = closest->id;
					Audio::post_global(AK::EVENTS::PLAY_OVERWORLD_MOVE, 0);
				}
			}
		}

		focus_camera(u, *zone);
	}
}

ResourceInfo resource_info[s32(Resource::count)] =
{
	{ // Energy
		Asset::Mesh::icon_battery,
		strings::energy,
		0,
		true,
	},
	{ // LotteryTickets
		Asset::Mesh::icon_lottery_ticket,
		strings::lottery_tickets,
		10,
		true,
	},
	{ // WallRun
		Asset::Mesh::icon_wall_run,
		strings::wall_run,
		1000,
		false,
	},
	{ // DoubleJump
		Asset::Mesh::icon_double_jump,
		strings::double_jump,
		3000,
		false,
	},
	{ // ExtendedWallRun
		Asset::Mesh::icon_wall_run,
		strings::extended_wall_run,
		3000,
		false,
	},
	{ // Grapple
		Asset::Mesh::icon_drone,
		strings::grapple,
		3000,
		false,
	},
	{ // AudioLog
		AssetNull,
		strings::audio_log,
		0,
		true,
	},
};

StaticArray<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> directional_lights;
Vec3 ambient_color;
r32 far_plane;
r32 fog_start;

void resource_buy(s8 gamepad)
{
	data.story.inventory.timer_buy = UPGRADE_TIME;
	Audio::post_global(AK::EVENTS::PLAY_DRONE_UPGRADE, gamepad);
}

Vec2 get_panel_size(const Rect2& rect)
{
	return Vec2(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);
}

b8 inventory_can_buy(Resource r, s32 count = 1)
{
	const ResourceInfo& info = resource_info[s32(r)];
	s32 energy = Game::save.resources[s32(Resource::Energy)];
	if (energy < (info.cost * count)
		|| Game::save.resources[s32(r)] > 0)
		return false;

	// prevent player from wasting money on lottery tickets if they don't have wall run yet
	if (r == Resource::LotteryTickets
		&& !Game::save.resources[s32(Resource::WallRun)]
		&& (energy - info.cost * count) < resource_info[s32(Resource::WallRun)].cost)
		return false;

	return true;
}

void inventory_dialog_buy(s8 gamepad, const Update* u, const RenderParams* p)
{
	Data::Inventory* inventory = &data.story.inventory;
	const ResourceInfo& info = resource_info[s32(inventory->resource_selected)];

	Vec2 pos = (Camera::for_gamepad(gamepad)->viewport.size * 0.5f) + Vec2(MENU_ITEM_WIDTH * -0.5f, 0);
	Rect2 text_rect = Rect2(pos, Vec2(MENU_ITEM_WIDTH, MENU_ITEM_FONT_SIZE * UI::scale)).outset(PADDING);

	{
		r32 prompt_height = (PADDING + MENU_ITEM_FONT_SIZE * UI::scale) * Ease::cubic_out<r32>(vi_min((Game::real_time.total - Menu::dialog_time[gamepad]) / DIALOG_ANIM_TIME, 1.0f));
		text_rect.pos.y -= prompt_height;
		text_rect.size.y += prompt_height;
	}

	Rect2 rect_decrease = { pos + Vec2(0, -MENU_ITEM_PADDING), Vec2((MENU_ITEM_FONT_SIZE * UI::scale) + MENU_ITEM_PADDING * 2.0f) };
	Rect2 rect_increase = { pos + Vec2(MENU_ITEM_WIDTH * 0.2f, -MENU_ITEM_PADDING), rect_decrease.size };
	b8 mouse_over_decrease = false;
	b8 mouse_over_increase = false;
	if (Game::ui_gamepad_types[0] == Gamepad::Type::None)
	{
		const Vec2& cursor = u ? u->input->cursor : p->sync->input.cursor;
		mouse_over_decrease = rect_decrease.contains(cursor);
		mouse_over_increase = rect_increase.contains(cursor);
	}

	if (u && info.allow_multiple)
	{
		s32 delta = UI::input_delta_horizontal(*u, 0);
		if (mouse_over_decrease && u->last_input->keys.get(s32(KeyCode::MouseLeft)) && !u->input->keys.get(s32(KeyCode::MouseLeft)))
			delta--;
		else if (mouse_over_increase && u->last_input->keys.get(s32(KeyCode::MouseLeft)) && !u->input->keys.get(s32(KeyCode::MouseLeft)))
			delta++;
		if (delta)
			Audio::post_global(AK::EVENTS::PLAY_MENU_ALTER, gamepad);
		inventory->buy_quantity = s16(vi_max(1, vi_min(32000 / s32(info.cost), inventory->buy_quantity + delta)));
	}

	if (p)
	{
		UI::box(*p, text_rect, UI::color_background);
		UI::border(*p, text_rect, 2.0f, UI::color_accent());

		b8 mouse_click = p->sync->input.keys.get(s32(KeyCode::MouseLeft));

		if (info.allow_multiple)
		{
			// decrease
			const Vec4* bg;
			const Vec4* color;
			if (mouse_over_decrease)
			{
				bg = mouse_click ? &UI::color_alert() : &UI::color_accent();
				color = &UI::color_background;
			}
			else
			{
				bg = nullptr;
				color = &UI::color_accent();
			}
			if (bg)
				UI::box(*p, rect_decrease, *bg);
			UI::triangle(*p, { rect_decrease.pos + rect_decrease.size * 0.5f, rect_decrease.size * 0.5f }, *color, PI * 0.5f);

			// increase
			if (mouse_over_increase)
			{
				bg = mouse_click ? &UI::color_alert() : &UI::color_accent();
				color = &UI::color_background;
			}
			else
			{
				bg = nullptr;
				color = &UI::color_accent();
			}
			if (bg)
				UI::box(*p, rect_increase, *bg);
			UI::triangle(*p, { rect_increase.pos + rect_increase.size * 0.5f, rect_increase.size * 0.5f }, *color, PI * -0.5f);
		}

		UIText text;
		text.size = MENU_ITEM_FONT_SIZE;
		text.color = Game::save.resources[s32(Resource::Energy)] >= info.cost * inventory->buy_quantity ? UI::color_default : UI::color_alert();
		text.anchor_y = UIText::Anchor::Min;

		// quantity
		if (info.allow_multiple)
		{
			text.anchor_x = UIText::Anchor::Center;
			text.text(0, "%hd", inventory->buy_quantity);
			text.draw(*p, pos + Vec2(MENU_ITEM_WIDTH * 0.1f + MENU_ITEM_PADDING * 2.0f, 0));
		}

		// description
		text.anchor_x = UIText::Anchor::Min;
		text.text(0, _(info.description));
		text.draw(*p, pos + Vec2(MENU_ITEM_WIDTH * 0.25f + MENU_ITEM_PADDING * 3.0f, 0));

		// cost
		text.anchor_x = UIText::Anchor::Max;
		text.text(0, _(strings::buy_cost), info.cost * inventory->buy_quantity);
		text.draw(*p, pos + Vec2(MENU_ITEM_WIDTH, 0));
	}

	if (Game::real_time.total > Menu::dialog_time[gamepad] + DIALOG_ANIM_TIME)
	{
		// accept
		b8 can_accept = inventory_can_buy(inventory->resource_selected, inventory->buy_quantity);
		UIText text;
		text.anchor_y = UIText::Anchor::Min;
		text.anchor_x = UIText::Anchor::Min;
		text.color = can_accept ? UI::color_accent() : UI::color_disabled();
		text.clip = 0;
		text.text(gamepad, _(strings::prompt_buy));
		Vec2 prompt_pos = text_rect.pos + Vec2(PADDING);
		Rect2 rect_accept = text.rect(prompt_pos).outset(PADDING * 0.5f);
		if (p)
		{
			if (p->sync->input.get(Controls::Interact, gamepad)
				|| (gamepad == 0
				&& Game::ui_gamepad_types[0] == Gamepad::Type::None
				&& rect_accept.contains(p->sync->input.cursor)))
			{
				const Vec4* bg;
				if (can_accept)
					bg = p->sync->input.keys.get(s32(KeyCode::MouseLeft)) ? &UI::color_alert() : &UI::color_accent();
				else
					bg = &UI::color_disabled();
				UI::box(*p, rect_accept, *bg);
				text.color = UI::color_background;
			}
			text.draw(*p, prompt_pos);
		}

		// cancel
		Rect2 rect_cancel = {};
		text.anchor_x = UIText::Anchor::Max;
		text.color = UI::color_alert();
		text.clip = 0;
		text.text(gamepad, _(strings::prompt_cancel));
		Vec2 pos = prompt_pos + Vec2(text_rect.size.x + PADDING * -2.0f, 0);
		rect_cancel = text.rect(pos).outset(PADDING * 0.5f);
		if (p)
		{
			if (p->sync->input.get(Controls::Cancel, gamepad)
				|| (gamepad == 0
				&& Game::ui_gamepad_types[0] == Gamepad::Type::None
				&& rect_cancel.contains(p->sync->input.cursor)))
			{
				UI::box(*p, rect_cancel, p->sync->input.keys.get(s32(KeyCode::MouseLeft)) ? UI::color_accent() : UI::color_alert());
				text.color = UI::color_background;
			}
			text.draw(*p, pos);
		}

		// dialog buttons
		if (u)
		{
			b8 accept_clicked = false;
			b8 cancel_clicked = false;
			if (gamepad == 0 && Game::ui_gamepad_types[gamepad] == Gamepad::Type::None)
			{
				if (u->last_input->keys.get(s32(KeyCode::MouseLeft)) && !u->input->keys.get(s32(KeyCode::MouseLeft)))
				{
					if (rect_accept.contains(u->input->cursor))
						accept_clicked = true;
					else if (rect_cancel.contains(u->input->cursor))
						cancel_clicked = true;
				}
			}

			if (accept_clicked || (u->last_input->get(Controls::Interact, gamepad) && !u->input->get(Controls::Interact, gamepad)))
			{
				// accept
				if (can_accept)
				{
					Audio::post_global(AK::EVENTS::PLAY_DIALOG_ACCEPT, gamepad);
					Menu::dialog_clear(gamepad);
					resource_buy(gamepad);
				}
			}
			else if (cancel_clicked || (!Game::cancel_event_eaten[gamepad] && u->last_input->get(Controls::Cancel, gamepad) && !u->input->get(Controls::Cancel, gamepad)))
			{
				// cancel
				Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL, gamepad);
				Menu::dialog_clear(gamepad);
				Game::cancel_event_eaten[gamepad] = true;
			}
		}
	}
}

void inventory_items(const Update* u, const RenderParams* p, const Rect2& rect)
{
	Data::StoryMode* story = &data.story;
	Data::Inventory* inventory = &story->inventory;
	Vec2 panel_size = get_panel_size(rect);

	if (story->tab_timer > 0.0f)
		return;

	if (story->tab == StoryTab::Inventory)
		panel_size.x -= PADDING * 2.0f + MENU_ITEM_WIDTH;

	if (p)
	{
		Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);

		if (story->tab == StoryTab::Inventory)
		{
			UIText text;
			text.anchor_y = UIText::Anchor::Max;
			text.anchor_x = UIText::Anchor::Min;
			text.color = UI::color_default;
			text.text(0, _(strings::tab_inventory));
			text.draw(*p, pos);
			pos.y -= panel_size.y + PADDING * 2.0f;
		}

		for (s32 i = 0; i < s32(Resource::count); i++)
		{
			if (Game::save.resources[i] > 0)
			{
				const ResourceInfo& info = resource_info[i];

				UI::box(*p, { pos, panel_size }, UI::color_background);

				r32 icon_size = 18.0f * SCALE_MULTIPLIER * UI::scale;

				b8 flash = Game::real_time.total - inventory->resource_change_time[i] < 0.5f;
				b8 draw = !flash || UI::flash_function(Game::real_time.total);

				if (draw)
				{
					const Vec4& color = flash
						? UI::color_accent()
						: (Menu::dialog_active(0) ? UI::color_disabled() : UI::color_default);

					// icon
					if (info.icon != AssetNull)
						UI::mesh(*p, info.icon, pos + Vec2(PADDING + icon_size * 0.5f, panel_size.y * 0.5f), Vec2(icon_size), color);

					UIText text;
					text.anchor_y = UIText::Anchor::Center;
					text.color = color;

					// description
					if (story->tab == StoryTab::Inventory)
					{
						text.anchor_x = UIText::Anchor::Min;
						text.text(0, _(info.description));
						text.draw(*p, pos + Vec2(icon_size * 2.0f + PADDING * 2.0f, panel_size.y * 0.5f));
					}

					if (info.allow_multiple)
					{
						// current amount
						text.size = TEXT_SIZE * (story->tab == StoryTab::Inventory ? 1.0f : 0.75f);
						text.anchor_x = UIText::Anchor::Max;
						text.text(0, "%d", Game::save.resources[i]);
						text.draw(*p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));
					}
				}
				pos.y -= panel_size.y;
			}
		}
	}

	if (story->tab == StoryTab::Inventory)
	{
		Vec2 pos = rect.pos + Vec2(panel_size.x + PADDING, rect.size.y - panel_size.y);

		if (u)
		{
			pos.y -= panel_size.y + PADDING;
			b8 input = story_mode_enable_input() && inventory->timer_buy == 0.0f;
			inventory->menu.start(*u, { pos, UIText::Anchor::Min, UIText::Anchor::Max }, 0, input ? UIMenu::EnableInput::Yes : UIMenu::EnableInput::No);
			for (s32 i = 0; i < s32(Resource::count); i++)
			{
				if (inventory->flags & (1 << i))
				{
					const ResourceInfo& info = resource_info[i];
					char cost_str[UI_TEXT_MAX];
					snprintf(cost_str, UI_TEXT_MAX, "%d", info.cost);
					if (inventory->menu.item(*u, _(info.description), cost_str, !input || !inventory_can_buy(Resource(i)), info.icon))
					{
						Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW, 0);
						data.story.inventory.resource_selected = Resource(i);
						data.story.inventory.buy_quantity = 1;
						Menu::dialog_custom(0, &inventory_dialog_buy);
					}
				}
			}
			inventory->menu.end(*u);
		}
		else if (p)
		{
			// header
			{
				UIText text;
				text.anchor_y = UIText::Anchor::Max;
				text.anchor_x = UIText::Anchor::Min;
				text.color = UI::color_default;
				text.text(0, _(strings::buy));
				text.draw(*p, pos);
			}

			inventory->menu.draw_ui(*p);
		}
	}
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
			{
				resource_change(Resource::Energy, -total_cost);

				if (resource == Resource::LotteryTickets)
				{
					s32 amount = 0;
					for (s32 i = 0; i < data.story.inventory.buy_quantity; i++)
					{
						s32 r = mersenne::rand() % 10000;
						if (r < 8)
							amount += 1000;
						else if (r < 800)
							amount += 100;
					}
					resource_change(Resource::Energy, amount);
					Menu::dialog(0, &Menu::dialog_no_action, _(strings::lottery_ticket_result), amount);
				}
				else
					resource_change(resource, data.story.inventory.buy_quantity);
			}
			data.story.inventory.buy_quantity = 1;
		}
	}

	inventory_items(&u, nullptr, story_mode_main_view_rect(s32(StoryTab::Inventory)));
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
		return zones[s32(mersenne::randf_co() * zones.length)];
	else
		return AssetNull;
}

void zone_random_attack(r32 elapsed_time)
{
#if !RELEASE_BUILD // incoming attacks disabled for now
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
			{
				Game::session.zone_under_attack = z;
				Game::session.zone_under_attack_timer = z == AssetNull ? 0.0f : ZONE_UNDER_ATTACK_TIME;
			}
			event_odds -= 1.0f;
		}
	}
#endif
}

void story_mode_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	if (data.story.tab_timer > 0.0f)
	{
		data.story.tab_timer = vi_max(0.0f, data.story.tab_timer - u.time.delta);

		if (data.story.tab_timer == 0.0f && data.story.tab == StoryTab::Map)
		{
			// animation completely done; reveal unlocked PvP zones
			for (s32 i = 0; i < MAX_ZONES; i++)
			{
				if (Game::save.zones[i] == ZoneState::PvpUnlocked)
					zone_change(AssetID(i), ZoneState::PvpHostile);
			}
		}
	}

	tab_map_update(u);
	tab_inventory_update(u);
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_story_draw(const RenderParams& p, const Data::StoryMode& data, StoryTab tab, const char* label, Vec2* pos, b8 flash = false)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab && !Menu::dialog_active(0))
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

AssetID group_name[s32(Game::Group::count)] =
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
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, Game::save.group == Game::Group::None ? Team::ui_color_friend() : UI::color_accent());

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
		const ZoneNode* zone = zone_node_by_id(data.zone_selected);
		zone_stat_draw(p, rect, UIText::Anchor::Min, 0, Loader::level_name(data.zone_selected), zone_ui_color(*zone));
		char buffer[255];

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
				zone_stat_draw(p, rect, UIText::Anchor::Min, 1, _(strings::capture_bonus), UI::color_accent());
				s32 index = 2;
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
		if (story_mode_enable_input() && zone_can_capture(data.zone_selected))
			tab_map_capture_prompt(rect, &p);
	}
}

r32 resource_change_time(Resource r)
{
	return data.story.inventory.resource_change_time[s32(r)];
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	inventory_items(nullptr, &p, rect);

	if (data.tab == StoryTab::Inventory && data.tab_timer == 0.0f)
	{
		if (data.inventory.timer_buy > 0.0f)
			Menu::progress_bar(p, _(strings::buying), 1.0f - (data.inventory.timer_buy / UPGRADE_TIME), p.camera->viewport.size * Vec2(0.5f, 0.35f));
	}
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	const Rect2& vp = p.camera->viewport;

	Rect2 main_view_rect = story_mode_main_view_rect(0);

	Vec2 pos = main_view_rect.pos;
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

b8 pvp_colors()
{
	return modal() || (Game::level.mode == Game::Mode::Pvp && Settings::pvp_color_scheme == Settings::PvpColorScheme::HighContrast);
}

void show_complete()
{
	{
		Camera* restore_camera = data.restore_camera.ref();
		StoryTab tab_next = data.story.tab;
		State state_next = data.state_next;
		if (data.camera.ref())
			data.camera.ref()->remove();
		r32 t = data.timer_transition;
		data.~Data();
		new (&data) Data();
		data.state = state_next;
		data.story.tab = tab_next;
		data.timer_transition = t;
		data.restore_camera = restore_camera;
	}

	data.story.tab_previous = StoryTab((s32(data.story.tab) + 1) % s32(StoryTab::count));

	Audio::post_global(AK::EVENTS::PLAY_OVERWORLD_SHOW, 0);

	if (data.state == State::Multiplayer && Game::level.mode != Game::Mode::Special)
	{
		// we're editing a server while playing in that server
		vi_assert(PlayerHuman::count_local() == 1);
		multiplayer_switch_tab(ServerListType::Mine);
		if (PlayerHuman::for_gamepad(0)->get<PlayerManager>()->flag(PlayerManager::FlagIsAdmin))
			multiplayer_state_transition(Data::Multiplayer::State::EntryEdit);
		else
			multiplayer_state_transition(Data::Multiplayer::State::EntryView);
		data.multiplayer.active_server.config.id = Game::session.config.id;
		multiplayer_request_setup(Data::Multiplayer::RequestType::ConfigGet);
#if !SERVER
		Net::Client::master_request_server_details(data.multiplayer.active_server.config.id, data.multiplayer.request_id);
#endif
	}

	if (modal())
	{
		Particles::clear();
		data.restore_camera.ref()->flag(CameraFlagActive, false);
		data.camera = Camera::list.add();
		new (data.camera.ref()) Camera();
		data.camera.ref()->mask = 0;
		data.camera.ref()->flag(CameraFlagColors, false);
		data.camera.ref()->pos = global.camera_offset_pos;
		data.camera.ref()->rot = global.camera_offset_rot;

		const ZoneNode* zone = zone_node_by_id(Game::save.zone_overworld);
		if (!zone)
			zone = zone_node_by_id(Game::save.zone_last);
		if (zone)
			data.camera.ref()->pos += zone->pos();

		if (Game::session.zone_under_attack == AssetNull)
			data.zone_selected = Game::level.id;
		else
			data.zone_selected = Game::session.zone_under_attack;

		Game::save.inside_terminal = true;
		Game::save.zone_overworld = Game::level.id;
	}
}

r32 particle_accumulator = 0.0f;
void update(const Update& u)
{
	if (data.camera.ref())
	{
		const DisplayMode& display = Settings::display();
		data.camera.ref()->viewport =
		{
			Vec2::zero,
			Vec2(r32(display.width), r32(display.height)),
		};
		data.camera.ref()->perspective((60.0f * PI * 0.5f / 180.0f), 1.0f, Game::level.far_plane_get());
	}

#if !SERVER
	if (Game::session.type == SessionType::Multiplayer
		&& Game::level.mode != Game::Mode::Special
		&& Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying
		&& (Net::Client::mode() == Net::Client::Mode::Connected || Game::level.local))
		multiplayer_in_game_update(u);
#endif

	if (data.timer_transition > 0.0f)
	{
		r32 old_timer = data.timer_transition;
		data.timer_transition = vi_max(0.0f, data.timer_transition - u.time.delta);
		if (data.timer_transition < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
		{
			Audio::post_global(AK::EVENTS::PLAY_TRANSITION_IN, 0);
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

		// pause
		if (data.state != State::StoryModeDeploying
			&& data.story.inventory.timer_buy == 0.0f
			&& !Game::cancel_event_eaten[0]
			&& !Menu::dialog_active(0))
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
	}
}

void draw_override(const RenderParams& params)
{
	if (modal() && should_draw_zones())
		zones_draw_override(params);
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
	if (active() && (params.camera == data.camera.ref() || params.camera == data.restore_camera.ref()))
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

void show(Camera* cam, State state, StoryTab tab)
{
	if (data.timer_transition == 0.0f)
	{
		data.restore_camera = cam;
		data.state_next = state;
		data.story.tab = tab;
		if (state == State::StoryModeOverlay) // overlay; no transition
			show_complete();
		else if (state == State::Hidden && data.state == State::StoryModeOverlay) // overlay; no transition
			hide_complete();
		else // start transition
		{
			Audio::post_global(AK::EVENTS::PLAY_TRANSITION_OUT, 0);
			data.timer_transition = TRANSITION_TIME;
		}
	}
}

void shop_flags(s32 flags)
{
	data.story.inventory.flags = flags;
}

// show server settings for current server
void server_settings(Camera* cam)
{
	show(cam, State::Multiplayer);
}

void server_settings_readonly(Camera* cam)
{
	show(cam, State::Multiplayer);
}

void skip_transition_full()
{
	data.timer_transition = 0.0f;
	show_complete();
}

void skip_transition_half()
{
	data.timer_transition = TRANSITION_TIME * 0.5f;
}

b8 active()
{
	return data.state != State::Hidden;
}

b8 modal()
{
	return active() && data.state != State::StoryModeOverlay && data.state != State::Multiplayer;
}

b8 transitioning()
{
	return data.timer_transition > 0.0f;
}

void clear()
{
	hide_complete();
	new (&data) Data();
}

void execute(const char* cmd)
{
	if (strstr(cmd, "capture ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* zone_string = delimiter + 1;
		AssetID id = Loader::find(zone_string, AssetLookup::Level::names);
		if (zone_is_pvp(id))
			zone_change(id, ZoneState::PvpFriendly);
	}
	else if (strstr(cmd, "unlock ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* zone_string = delimiter + 1;
		AssetID id = Loader::find(zone_string, AssetLookup::Level::names);
		if (zone_is_pvp(id))
			zone_change(id, ZoneState::PvpHostile);
		else
			zone_change(id, ZoneState::ParkourUnlocked);
	}
	else if (strcmp(cmd, "attack") == 0)
	{
		AssetID z = zone_random(&zone_filter_captured, &zone_filter_can_be_attacked); // live incoming attack
		if (z != AssetNull)
		{
			Game::session.zone_under_attack = z;
			Game::session.zone_under_attack_timer = ZONE_UNDER_ATTACK_TIME;
		}
	}
	else if (strstr(cmd, "join ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* group_string = delimiter + 1;
		for (s32 i = 0; i < s32(Game::Group::count); i++)
		{
			if (strcmp(group_string, _(group_name[i])) == 0)
			{
				group_join(Game::Group(i));
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
					node->uuid = AssetID(Json::get_s32(element, "uuid"));

					cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
					cJSON* mesh_json = meshes->child;
					const char* mesh_ref = mesh_json->valuestring;
					node->mesh = Loader::find_mesh(mesh_ref);

					node->rewards[s32(Resource::Energy)] = s16(Json::get_s32(element, "energy"));
					node->size = s8(Json::get_s32(element, "size", 1));
					node->max_teams = s8(Json::get_s32(element, "max_teams", 2));
				}
			}
			else if (cJSON_HasObjectItem(element, "World"))
			{
				ambient_color = Json::get_vec3(element, "ambient_color");
				far_plane = Json::get_r32(element, "far_plane", 100.0f);
				fog_start = Json::get_r32(element, "fog_start", far_plane * 0.25f);
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
