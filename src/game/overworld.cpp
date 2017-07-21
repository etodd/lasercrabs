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

namespace VI
{


namespace Overworld
{

#define DEPLOY_TIME 1.0f

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

struct SplitscreenConfig
{
	enum class Mode : s8
	{
		Custom,
		Local,
		Public,
		count,
	};

	Mode mode = Mode::Custom;
	GameType game_type = GameType::Deathmatch;
	r32 time_limit = 8.0f * 60.0f;
	s16 respawns = DEFAULT_ASSAULT_DRONES;
	s16 kill_limit = DEFAULT_ASSAULT_DRONES;
	AI::Team local_player_config[MAX_GAMEPADS] = { 0, AI::TeamNone, AI::TeamNone, AI::TeamNone, };
	b8 allow_abilities = true;

	SessionType session_type() const
	{
		switch (mode)
		{
			case Mode::Public:
				return SessionType::Public;
			case Mode::Custom:
				return SessionType::Custom;
			case Mode::Local:
				return SessionType::Custom;
			default:
			{
				vi_assert(false);
				return SessionType::count;
			}
		}
	}

	void team_counts(s32* team_counts) const
	{
		for (s32 i = 0; i < MAX_TEAMS; i++)
			team_counts[i] = 0;
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			AI::Team team = local_player_config[i];
			if (team != AI::TeamNone)
				team_counts[s32(team)]++;
		}
	}

	s32 team_count() const
	{
		s32 player_count = 0;
		s32 counts[MAX_TEAMS];
		team_counts(counts);
		s32 teams_with_players = 0;
		for (s32 i = 0; i < MAX_TEAMS; i++)
		{
			if (counts[i] > 0)
				teams_with_players++;
		}
		return teams_with_players;
	}

	b8 teams_are_valid() const
	{
		return session_type() == SessionType::Public || team_count() > 1;
	}

	s32 local_player_count() const
	{
		s32 count = 0;
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			if (local_player_config[i] != AI::TeamNone)
				count++;
		}
		return count;
	}

	void apply()
	{
		Game::session.type = session_type();
		Game::session.game_type = game_type;
		Game::session.time_limit = time_limit;
		if (game_type == GameType::Assault)
		{
			Game::session.respawns = respawns;
			Game::session.kill_limit = 0;
		}
		else
		{
			Game::session.respawns = -1;
			Game::session.kill_limit = kill_limit;
		}
		Game::session.allow_abilities = allow_abilities;
		Game::session.team_count = vi_max(2, team_count());
		memcpy(&Game::session.local_player_config, local_player_config, sizeof(local_player_config));
	}

	void consolidate_teams()
	{
		s32 team_count[MAX_TEAMS];
		team_counts(team_count);

		AI::Team team_lookup[MAX_TEAMS];
		AI::Team current_team = 0;
		for (s32 i = 0; i < MAX_TEAMS; i++)
		{
			if (team_count[i] > 0)
			{
				team_lookup[i] = current_team;
				current_team++;
			}
		}
		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			AI::Team team = local_player_config[i];
			if (team != AI::TeamNone)
				local_player_config[i] = team_lookup[team];
		}
	}
};

struct DataGlobal
{
	StaticArray<ZoneNode, MAX_ZONES> zones;
	Array<PropEntry> props;
	Array<WaterEntry> waters;
	Vec3 camera_offset_pos;
	Quat camera_offset_rot;
	SplitscreenConfig splitscreen;
};
DataGlobal global;

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

	struct StoryMode
	{
		r64 timestamp_last;
		Tab tab;
		Tab tab_previous;
		r32 tab_timer;
		r32 mode_transition_time;
		Inventory inventory;
		Map map;
	};

	struct Splitscreen
	{
		UIMenu menu;
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
	Splitscreen splitscreen;
	AssetID zone_selected = AssetNull;
};

Data data = Data();

void deploy_start()
{
	if (Game::session.type == SessionType::Story)
		Game::session.team_count = 2;
	else
		global.splitscreen.consolidate_teams();

	data.state = Game::session.type == SessionType::Story ? State::Deploying : State::SplitscreenDeploying;
	data.timer_deploy = DEPLOY_TIME;
	Audio::post_global_event(AK::EVENTS::PLAY_OVERWORLD_DEPLOY_START);
}

void deploy_done();

void splitscreen_select_options_update(const Update& u)
{
	if (Menu::main_menu_state != Menu::State::Hidden || Game::scheduled_load_level != AssetNull)
		return;

	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0])
	{
		Menu::title();
		Game::cancel_event_eaten[0] = true;
		return;
	}

	data.splitscreen.menu.start(u, 0);
	if (data.splitscreen.menu.item(u, _(strings::back)))
	{
		Menu::title();
		return;
	}

	if (data.splitscreen.menu.item(u, _(strings::_continue)))
		data.state = State::SplitscreenSelectTeams;

	// multiplayer type
	{
		AssetID value;
		switch (global.splitscreen.mode)
		{
			case SplitscreenConfig::Mode::Public:
			{
				value = strings::multiplayer_public;
				break;
			}
			case SplitscreenConfig::Mode::Custom:
			{
				value = strings::multiplayer_custom;
				break;
			}
			case SplitscreenConfig::Mode::Local:
			{
				value = strings::multiplayer_local;
				break;
			}
			default:
			{
				value = AssetNull;
				vi_assert(false);
				break;
			}
		}
		UIMenu::enum_option(&global.splitscreen.mode, data.splitscreen.menu.slider_item(u, _(strings::multiplayer), _(value)));
	}

	if (global.splitscreen.mode != SplitscreenConfig::Mode::Public)
	{
		{
			// game type
			AssetID value;
			switch (global.splitscreen.game_type)
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
			UIMenu::enum_option(&global.splitscreen.game_type, data.splitscreen.menu.slider_item(u, _(strings::game_type), _(value)));
		}

		s32 delta;
		char str[MAX_PATH_LENGTH + 1];

		{
			// time limit
			r32* time_limit = &global.splitscreen.time_limit;
			sprintf(str, _(strings::timer), s32(*time_limit / 60.0f), 0);
			delta = data.splitscreen.menu.slider_item(u, _(strings::time_limit), str);
			if (delta < 0)
				*time_limit = vi_max(120.0f, *time_limit - 120.0f);
			else if (delta > 0)
				*time_limit = vi_min(254.0f * 60.0f, *time_limit + 120.0f);
		}

		if (global.splitscreen.game_type == GameType::Assault)
		{
			// respawns
			s16* respawns = &global.splitscreen.respawns;
			sprintf(str, "%hd", *respawns);
			delta = data.splitscreen.menu.slider_item(u, _(strings::drones), str);
			if (delta < 0)
				*respawns = vi_max(1, s32(*respawns) - 1);
			else if (delta > 0)
				*respawns = vi_min(100, s32(*respawns) + 1);
		}
		else
		{
			// kill limit
			s16* kill_limit = &global.splitscreen.kill_limit;
			sprintf(str, "%hd", *kill_limit);
			delta = data.splitscreen.menu.slider_item(u, _(strings::kill_limit), str);
			if (delta < 0)
				*kill_limit = vi_max(2, s32(*kill_limit) - 2);
			else if (delta > 0)
				*kill_limit = vi_min(200, s32(*kill_limit) + 2);
		}

		// allow abilities
		{
			b8* allow_abilities = &global.splitscreen.allow_abilities;
			delta = data.splitscreen.menu.slider_item(u, _(strings::allow_abilities), _(*allow_abilities ? strings::yes : strings::no));
			if (delta != 0)
				*allow_abilities = !(*allow_abilities);
		}
	}

	data.splitscreen.menu.end();
}

void splitscreen_select_teams_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0] && Game::scheduled_load_level == AssetNull)
	{
		data.state = State::SplitscreenSelectOptions;
		Game::cancel_event_eaten[0] = true;
		return;
	}

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team* team = &global.splitscreen.local_player_config[i];
		if (u.input->gamepads[i].type != Gamepad::Type::None || i == 0) // player is active
		{
			// handle D-pad
			s32 delta = UI::input_delta_horizontal(u, i);

			if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i) && !Game::cancel_event_eaten[i])
			{
				if (i > 0) // player 0 must stay in
				{
					*team = AI::TeamNone;
					Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
					Game::cancel_event_eaten[i] = true;
				}
			}
			else if (delta < 0)
			{
				if (*team == AI::TeamNone)
				{
					// we're already all the way to the left
				}
				else if (*team == 0)
				{
					if (i > 0) // player 0 must stay in
					{
						*team = AI::TeamNone;
						Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
					}
				}
				else
				{
					(*team) -= 1;
					Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
				}
			}
			else if (delta > 0)
			{
				if (*team == AI::TeamNone)
				{
					*team = 0;
					Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
				}
				else if (*team < MAX_TEAMS - 1)
				{
					*team += 1;
					Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
				}
			}
		}
		else // controller is gone
			*team = AI::TeamNone;
	}

	if (u.last_input->get(Controls::Interact, 0)
		&& !u.input->get(Controls::Interact, 0)
		&& global.splitscreen.teams_are_valid())
	{
		if (global.splitscreen.session_type() == SessionType::Public)
		{
			deploy_start();
			deploy_done();
		}
		else
			data.state = State::SplitscreenSelectZone;
	}
}

void tab_draw_common(const RenderParams& p, const char* label, const Vec2& pos, r32 width, const Vec4& color, const Vec4& background_color, const Vec4& text_color = UI::color_background)
{
	// body
	Vec2 size(width, MAIN_VIEW_SIZE.y);
	{
		if (background_color.w > 0.0f)
			UI::box(p, { pos, size }, background_color);
		UI::border(p, { pos, size }, BORDER, color);
	}

	// label
	{
		Vec2 tab_pos = pos + Vec2(0, size.y);
		UI::box(p, { tab_pos, TAB_SIZE }, color);

		UIText text;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Min;
		text.color = text_color;
		text.text(0, label);
		text.draw(p, tab_pos + Vec2(PADDING));
	}
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

void splitscreen_select_options_draw(const RenderParams& params)
{
	if (Menu::main_menu_state == Menu::State::Hidden)
	{
		const Rect2& viewport = params.camera->viewport;
		data.splitscreen.menu.draw_ui(params, Vec2(viewport.size.x * 0.5f, viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f), UIText::Anchor::Center, UIText::Anchor::Max);
	}
}

void splitscreen_select_teams_draw(const RenderParams& params)
{
	const Rect2& vp = params.camera->viewport;
	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	Vec2 center = vp.size * 0.5f;
	{
		Vec2 bottom_left = center + (main_view_size * -0.5f);
		Vec4 background_color = Vec4(UI::color_background.xyz(), OPACITY);
		tab_draw_common(params, _(strings::prompt_splitscreen), bottom_left, main_view_size.x, UI::color_accent(), background_color);
	}

	UIText text;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::color_accent();
	text.wrap_width = main_view_size.x - 48.0f * UI::scale * SCALE_MULTIPLIER;
	Vec2 pos = center + Vec2(0, main_view_size.y * 0.5f - (48.0f * UI::scale * SCALE_MULTIPLIER));

	// prompt
	if (global.splitscreen.teams_are_valid())
	{
		text.text(0, _(strings::prompt_splitscreen_ready));
		text.draw(params, pos);
	}

	pos.y -= 48.0f * UI::scale * SCALE_MULTIPLIER;

	// draw team labels
	const r32 team_offset = 128.0f * UI::scale * SCALE_MULTIPLIER;
	text.wrap_width = 0;
	text.text(0, _(strings::team_a));
	text.draw(params, pos + Vec2(team_offset * -1.0f, 0));
	text.text(0, _(strings::team_b));
	text.draw(params, pos + Vec2(0, 0));
	text.text(0, _(strings::team_c));
	text.draw(params, pos + Vec2(team_offset * 1.0f, 0));
	text.text(0, _(strings::team_d));
	text.draw(params, pos + Vec2(team_offset * 2.0f, 0));

	// set up text for gamepad number labels
	text.color = UI::color_background;
	text.wrap_width = 0;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		pos.y -= 64.0f * UI::scale * SCALE_MULTIPLIER;

		AI::Team team = global.splitscreen.local_player_config[i];

		const Vec4* color;
		r32 x_offset;
		if (i > 0 && params.sync->input.gamepads[i].type == Gamepad::Type::None)
		{
			color = &UI::color_disabled();
			x_offset = team_offset * -2.0f;
		}
		else if (team == AI::TeamNone)
		{
			color = &UI::color_default;
			x_offset = team_offset * -2.0f;
		}
		else
		{
			color = &UI::color_accent();
			x_offset = ((r32)team - 1.0f) * team_offset;
		}

		draw_gamepad_icon(params, pos + Vec2(x_offset, 0), i, *color, SCALE_MULTIPLIER);
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

b8 zone_splitscreen_can_deploy(AssetID z)
{
	const ZoneNode* zone = zone_node_get(z);
	return zone && global.splitscreen.team_count() <= zone->max_teams;
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
					if (&candidate == zone
						|| (data.state == State::SplitscreenSelectZone && !zone_splitscreen_can_deploy(candidate.id)))
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
	{
		if (global.splitscreen.team_count() <= zone.max_teams)
			return Team::color_friend.xyz();
		else
			return Vec3(0.25f);
	}
}

const Vec4& zone_ui_color(const ZoneNode& zone)
{
	if (Game::session.type == SessionType::Story)
	{
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
	else
	{
		if (zone_splitscreen_can_deploy(zone.id))
			return Team::ui_color_friend;
		else
			return UI::color_disabled();
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
	if (data.timer_deploy > 0.0f || Game::scheduled_load_level != AssetNull)
		return nullptr;

	// highlight zone locations
	const ZoneNode* selected_zone = zone_node_get(data.zone_selected);

	if (Game::session.type == SessionType::Story)
	{
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
	}
	else
	{
		// not story mode

		// draw selected zone name
		if (selected_zone)
		{
			Vec2 p;
			if (UI::project(params, selected_zone->pos(), &p))
			{
				UIText text;
				text.color = zone_ui_color(*selected_zone);
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.text_raw(0, AssetLookup::Level::names[selected_zone->id]);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, p);
			}
		}
	}

	return selected_zone;
}

void splitscreen_select_zone_draw(const RenderParams& params)
{
	const ZoneNode* zone = zones_draw(params);

	// press [x] to deploy
	{
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = zone_ui_color(*zone);
		text.text(0, "%s\n%s", _(strings::prompt_deploy), _(strings::prompt_back));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.25f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::color_background);

		text.draw(params, pos);
	}

	// draw teams
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::color_accent();
		text.size *= SCALE_MULTIPLIER;

		s32 player_count = global.splitscreen.local_player_count();
		const r32 gamepad_spacing = 128.0f * UI::scale * SCALE_MULTIPLIER;
		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.1f) + Vec2(gamepad_spacing * (player_count - 1) * -0.5f, 0);

		AssetID team_labels[MAX_TEAMS] =
		{
			strings::team_a,
			strings::team_b,
			strings::team_c,
			strings::team_d,
		};

		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			AI::Team team = global.splitscreen.local_player_config[i];
			if (team != AI::TeamNone)
			{
				text.text(0, _(team_labels[s32(team)]));
				text.draw(params, pos + Vec2(0, 32.0f * UI::scale * SCALE_MULTIPLIER));
				draw_gamepad_icon(params, pos, i, UI::color_accent(), SCALE_MULTIPLIER);
				pos.x += gamepad_spacing;
			}
		}
	}
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
	if (Game::session.type == SessionType::Story)
		OverworldNet::capture_or_defend(data.zone_selected);
	else
	{
		// multiplayer
#if SERVER
		vi_assert(false);
#else
		global.splitscreen.apply();
		if (global.splitscreen.mode == SplitscreenConfig::Mode::Local)
			go(data.zone_selected);
		else
		{
			Game::unload_level();
			Game::save.reset();

			Net::Client::request_server(1); // todo: redesign this whole thing

			clear();
		}
#endif
	}
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

#define TAB_ANIMATION_TIME 0.3f

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
	return zone_node_get(zone_id)->max_teams > 0;
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
	if (data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME)
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

	if (data.story.tab == Tab::Inventory && enable_input())
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

	if (data.story.tab != Tab::Inventory)
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

	data.story.tab_timer += u.time.delta;

	// start the mode transition animation when we first open any tab
	if (data.story.tab_timer > TAB_ANIMATION_TIME && data.story.tab_timer - Game::real_time.delta <= TAB_ANIMATION_TIME)
		data.story.mode_transition_time = Game::real_time.total;

	tab_map_update(u);
	tab_inventory_update(u);
}

Vec2 get_panel_size(const Rect2& rect)
{
	return Vec2(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_draw(const RenderParams& p, const Data::StoryMode& data, Tab tab, const char* label, Vec2* pos, b8 flash = false)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab && !Menu::dialog_callback[0])
	{
		// flash the tab when it is selected
		if (data.tab_timer < TAB_ANIMATION_TIME)
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
	r32 blend = Ease::cubic_out(vi_min(1.0f, data.tab_timer / TAB_ANIMATION_TIME), 0.0f, 1.0f);
	r32 width = LMath::lerpf(blend, previous_width, current_width);

	if (draw)
	{
		// if we're minimized, fill in the background
		const Vec4& background_color = data.tab == tab ? Vec4(0, 0, 0, 0) : Vec4(UI::color_background.xyz(), OPACITY);
		tab_draw_common(p, label, *pos, width, *color, background_color, flash && !UI::flash_function(Game::real_time.total) ? Vec4(0, 0, 0, 0) : UI::color_background);
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
			if (story.tab == Tab::Map)
				label = _(Game::save.group == Net::Master::Group::None ? strings::energy_generation_total : strings::energy_generation_group);
			else
				label = "+%d";
			sprintf(buffer, label, s32(energy_increment_total()));
			Rect2 zone_stat_rect = zone_stat_draw(p, rect, UIText::Anchor::Max, index++, buffer, UI::color_default);

			// energy increment timer
			r32 icon_size = TEXT_SIZE * 1.5f * UI::scale * (story.tab == Tab::Map ? 1.0f : 0.75f);
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

		if (story.tab != Tab::Map)
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

	if (story.tab == Tab::Map)
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
		b8 selected = data.tab == Tab::Inventory && data.inventory.resource_selected == (Resource)i && !Menu::dialog_active(0);

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
		text.size = TEXT_SIZE * (data.tab == Tab::Inventory ? 1.0f : 0.75f);
		if (draw)
		{
			// current amount
			text.anchor_x = UIText::Anchor::Max;
			text.text(0, "%d", Game::save.resources[i]);
			text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));

			if (data.tab == Tab::Inventory)
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

	if (data.tab == Tab::Inventory && data.tab_timer > TAB_ANIMATION_TIME)
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
		Rect2 rect = tab_draw(p, data.story, Tab::Map, _(strings::tab_map), &pos).outset(-PADDING);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_map_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Inventory, _(strings::tab_inventory), &pos).outset(-PADDING);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_inventory_draw(p, data.story, rect);
	}
}

b8 should_draw_zones()
{
	return data.state == State::SplitscreenSelectZone
		|| data.state == State::SplitscreenDeploying
		|| data.state == State::Deploying
		|| (data.state == State::StoryMode && data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME);
}

void splitscreen_select_zone_update(const Update& u)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	// cancel
	if (Game::session.type != SessionType::Story
		&& !Game::cancel_event_eaten[0]
		&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
	{
		data.state = State::SplitscreenSelectTeams;
		Game::cancel_event_eaten[0] = true;
		return;
	}

	select_zone_update(u, true);

	// deploy button
	if (zone_splitscreen_can_deploy(data.zone_selected)
		&& u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
		deploy_start();
}

void show_complete()
{
	State state_next = data.state_next;
	Tab tab_next = data.story.tab;

	Particles::clear();
	{
		Camera* c = data.camera.ref();
		r32 t = data.timer_transition;
		data.~Data();
		new (&data) Data();
		data.camera = c;
		data.timer_transition = t;
		data.splitscreen.menu.animation_time = Game::real_time.total;

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

		data.story.tab_previous = Tab((s32(data.story.tab) + 1) % s32(Tab::count));
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
		c->viewport =
		{
			Vec2(0, 0),
			Vec2(u.input->width, u.input->height),
		};
		r32 aspect = c->viewport.size.y == 0 ? 1 : (r32)c->viewport.size.x / (r32)c->viewport.size.y;
		c->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);
		data.timer_transition = 0.0f;

		if (Game::session.type == SessionType::Story)
			vi_assert(false);
		else if (global.splitscreen.local_player_count() > 1)
			show(c, State::SplitscreenSelectZone);
		else
			show(c, State::SplitscreenSelectOptions);

		show_complete();
		data.timer_transition = 0.0f;
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
			case State::StoryMode:
			case State::StoryModeOverlay:
			{
				story_mode_update(u);
				break;
			}
			case State::SplitscreenSelectOptions:
			{
				splitscreen_select_options_update(u);
				break;
			}
			case State::SplitscreenSelectTeams:
			{
				splitscreen_select_teams_update(u);
				break;
			}
			case State::SplitscreenSelectZone:
			{
				splitscreen_select_zone_update(u);
				break;
			}
			case State::Deploying:
			case State::SplitscreenDeploying:
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
			case State::SplitscreenSelectOptions:
			{
				splitscreen_select_options_draw(params);
				break;
			}
			case State::SplitscreenSelectTeams:
			{
				splitscreen_select_teams_draw(params);
				break;
			}
			case State::StoryMode:
			case State::StoryModeOverlay:
			{
				story_mode_draw(params);
				break;
			}
			case State::SplitscreenSelectZone:
			{
				splitscreen_select_zone_draw(params);
				break;
			}
			case State::Deploying:
			case State::SplitscreenDeploying:
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

void show(Camera* camera, State state, Tab tab)
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