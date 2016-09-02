#include "terminal.h"
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
#include "strings.h"
#include "load.h"
#include "entities.h"
#include "render/particles.h"
#include "data/import_common.h"
#include "utf8/utf8.h"
#include "sha1/sha1.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "penelope.h"

namespace VI
{


namespace Terminal
{

#define DEPLOY_TIME_MIN 4.0f
#define DEPLOY_TIME_RANGE 3.0f
#define DEPLOY_TIME_OFFLINE 3.0f

struct ZoneNode
{
	s32 index;
	AssetID id;
	Ref<Transform> pos;
};

enum class State
{
	SplitscreenSelectTeams,
	SplitscreenSelectLevel,
	SplitscreenDeploying,
	StoryMode,
	Deploying,
};

enum class Tab
{
	Messages,
	Map,
	Inventory,
	count,
};

struct Data
{
	struct StoryMode
	{
		Tab tab;
		Tab tab_previous;
		r32 tab_timer;
		Ref<Transform> camera_messages;
		Ref<Transform> camera_inventory;
	};

	Camera* camera;
	Array<ZoneNode> zones;
	Ref<Transform> camera_offset;
	AssetID last_level = AssetNull;
	AssetID next_level = AssetNull;
	State state;
	s32 tip_index;
	r32 tip_time;
	r32 deploy_timer;
	StoryMode story;
};

Data data = Data();

const s32 tip_count = 13;
const AssetID tips[tip_count] =
{
	strings::tip_0,
	strings::tip_1,
	strings::tip_2,
	strings::tip_3,
	strings::tip_4,
	strings::tip_5,
	strings::tip_6,
	strings::tip_7,
	strings::tip_8,
	strings::tip_9,
	strings::tip_10,
	strings::tip_11,
	strings::tip_12,
};

b8 splitscreen_teams_are_valid()
{
	s32 player_count = 0;
	s32 team_counts[MAX_PLAYERS] = {};
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team team = Game::state.local_player_config[i];
		if (team != AI::NoTeam)
			team_counts[(s32)team]++;
	}
	s32 teams_with_players = 0;
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		if (team_counts[i] > 0)
			teams_with_players++;
	}
	return teams_with_players > 1;
}

void init(const Update& u, const EntityFinder& entities)
{
	if (Game::state.level != Asset::Level::terminal)
		return;

	data.tip_index = mersenne::rand() % tip_count;
	s32 start_level = Game::state.local_multiplayer ? Game::tutorial_levels : 0; // skip tutorial levels if we're in splitscreen mode
	data.next_level = Game::levels[start_level];
	data.camera = Camera::add();

	{
		Entity* map_view_entity = entities.find("map_view");
		Transform* map_view = map_view_entity->get<Transform>();
		data.camera_offset = map_view;
		data.camera->pos = map_view->absolute_pos();
		data.camera->rot = Quat::look(map_view->absolute_rot() * Vec3(0, -1, 0));

		data.story.camera_messages = entities.find("camera_messages")->get<Transform>();
		data.story.camera_inventory = entities.find("camera_inventory")->get<Transform>();
	}

	for (s32 i = start_level; i < Asset::Level::count; i++)
	{
		AssetID level_id = Game::levels[i];
		if (level_id == AssetNull)
			break;

		Entity* entity = entities.find(AssetLookup::Level::names[level_id]);
		if (entity)
		{
			data.zones.add({ i, level_id, entity->get<Transform>() });
			if (level_id == data.last_level)
				data.camera->pos = entity->get<Transform>()->absolute_pos() + data.camera_offset.ref()->absolute_pos();
		}
	}

	data.camera->viewport =
	{
		Vec2(0, 0),
		Vec2(u.input->width, u.input->height),
	};
	r32 aspect = data.camera->viewport.size.y == 0 ? 1 : (r32)data.camera->viewport.size.x / (r32)data.camera->viewport.size.y;
	data.camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);

	if (Game::state.local_multiplayer)
	{
		if (Game::save.round == 0)
			data.state = State::SplitscreenSelectTeams;
		else
		{
			// if we've already played a round, skip the team select and go straight to level select
			// the player can always go back
			data.state = State::SplitscreenSelectLevel;
		}
	}
	else
	{
		// singleplayer
		const char* entry_point_str = Loader::level_name(Game::levels[Game::save.level_index]);
		Penelope::init(strings_get(entry_point_str));
		data.state = State::StoryMode;
	}
}

void splitscreen_select_teams_update(const Update& u)
{
	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
	{
		Menu::title();
		return;
	}

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		b8 player_active = u.input->gamepads[i].active || i == 0;

		AI::Team* team = &Game::state.local_player_config[i];
		if (player_active)
		{
			// handle D-pad
			b8 left = u.input->get(Controls::Left, i) && !u.last_input->get(Controls::Left, i);
			b8 right = u.input->get(Controls::Right, i) && !u.last_input->get(Controls::Right, i);

			// handle joysticks
			{
				r32 last_x = Input::dead_zone(u.last_input->gamepads[i].left_x, UI_JOYSTICK_DEAD_ZONE);
				if (last_x == 0.0f)
				{
					r32 x = Input::dead_zone(u.input->gamepads[i].left_x, UI_JOYSTICK_DEAD_ZONE);
					if (x < 0.0f)
						left = true;
					else if (x > 0.0f)
						right = true;
				}
			}

			if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
			{
				if (i > 0) // player 0 must stay in
				{
					*team = AI::NoTeam;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
			else if (left)
			{
				if (*team == 1)
				{
					if (i == 0) // player 0 must stay in
						*team = 0;
					else
						*team = AI::NoTeam;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
				else if (*team == AI::NoTeam)
				{
					*team = 0;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
			else if (right)
			{
				if (*team == 0)
				{
					if (i == 0) // player 0 must stay in
						*team = 1;
					else
						*team = AI::NoTeam;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
				else if (*team == AI::NoTeam)
				{
					*team = 1;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
		}
		else // controller is gone
			*team = AI::NoTeam;
	}

	if (u.last_input->get(Controls::Interact, 0)
		&& !u.input->get(Controls::Interact, 0)
		&& splitscreen_teams_are_valid())
	{
		data.state = State::SplitscreenSelectLevel;
	}
}

void splitscreen_select_teams_draw(const RenderParams& params)
{
	const Rect2& viewport = params.camera->viewport;
	const Vec2 box_size(512 * UI::scale, (512 - 64) * UI::scale);
	UI::box(params, { viewport.size * 0.5f - box_size * 0.5f, box_size }, UI::background_color);

	UIText text;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::accent_color;
	text.wrap_width = box_size.x - 48.0f * UI::scale;
	text.text(_(splitscreen_teams_are_valid() ? strings::splitscreen_prompt_ready : strings::splitscreen_prompt));
	Vec2 pos(viewport.size.x * 0.5f, viewport.size.y * 0.5f + box_size.y * 0.5f - (16.0f * UI::scale));
	text.draw(params, pos);
	pos.y -= 64.0f * UI::scale;

	// draw team labels
	const r32 team_offset = 128.0f * UI::scale;
	text.wrap_width = 0;
	text.text(_(strings::team_a));
	text.draw(params, pos + Vec2(-team_offset, 0));
	text.text(_(strings::team_b));
	text.draw(params, pos + Vec2(team_offset, 0));

	// set up text for gamepad number labels
	text.color = UI::background_color;
	text.wrap_width = 0;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		pos.y -= 64.0f * UI::scale;

		AI::Team team = Game::state.local_player_config[i];

		const Vec4* color;
		r32 x_offset;
		if (i > 0 && !params.sync->input.gamepads[i].active)
		{
			color = &UI::disabled_color;
			x_offset = 0.0f;
		}
		else if (team == AI::NoTeam)
		{
			color = &UI::default_color;
			x_offset = 0.0f;
		}
		else if (team == 0)
		{
			color = &UI::accent_color;
			x_offset = -team_offset;
		}
		else if (team == 1)
		{
			color = &UI::accent_color;
			x_offset = team_offset;
		}
		else
			vi_assert(false);

		Vec2 icon_pos = pos + Vec2(x_offset, 0);
		UI::mesh(params, Asset::Mesh::icon_gamepad, icon_pos, Vec2(48.0f * UI::scale), *color);
		text.text("%d", i + 1);
		text.draw(params, icon_pos);
	}
}

void go(s32 index)
{
	Game::save = Game::Save();
	Game::save.level_index = index;
	Game::schedule_load_level(Game::levels[index], Game::Mode::Pvp);
}

void focus_camera(const Update& u, const Vec3& target_pos, const Quat& target_rot)
{
	data.camera->pos += (target_pos - data.camera->pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
	data.camera->rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), data.camera->rot, target_rot);
}

void focus_camera(const Update& u, const Transform* target)
{
	focus_camera(u, target->absolute_pos(), Quat::look(target->absolute_rot() * Vec3(0, -1, 0)));
}

void focus_camera(const Update& u, const ZoneNode& zone)
{
	Vec3 target_pos = zone.pos.ref()->absolute_pos() + data.camera_offset.ref()->absolute_pos();
	Quat target_rot = Quat::look(data.camera_offset.ref()->absolute_rot() * Vec3(0, -1, 0));
	focus_camera(u, target_pos, target_rot);
}

void select_zone_update(const Update& u)
{
	s32 index = -1;
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& node = data.zones[i];
		if (node.id == data.next_level)
		{
			index = i;
			break;
		}
	}

	if (index != -1)
	{
		if (!UIMenu::active[0])
		{
			// select level
			if ((!u.input->get(Controls::Forward, 0)
				&& u.last_input->get(Controls::Forward, 0))
				|| (Input::dead_zone(u.last_input->gamepads[0].left_y) < 0.0f
					&& Input::dead_zone(u.input->gamepads[0].left_y) >= 0.0f))
			{
				index = vi_min(index + 1, data.zones.length - 1);
				data.next_level = data.zones[index].id;
			}
			else if ((!u.input->get(Controls::Backward, 0)
				&& u.last_input->get(Controls::Backward, 0))
				|| (Input::dead_zone(u.last_input->gamepads[0].left_y) > 0.0f
					&& Input::dead_zone(u.input->gamepads[0].left_y) <= 0.0f))
			{
				index = vi_max(index - 1, 0);
				data.next_level = data.zones[index].id;
			}

			if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				data.state = State::Deploying;
				data.deploy_timer = DEPLOY_TIME_OFFLINE;
				data.tip_time = Game::real_time.total;
			}
		}

		focus_camera(u, data.zones[index]);
	}
}

void splitscreen_select_zone_update(const Update& u)
{
	s32 index = -1;
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& node = data.zones[i];
		if (node.id == data.next_level)
		{
			index = i;
			break;
		}
	}

	if (index != -1)
	{
		if (!UIMenu::active[0])
		{
			if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			{
				data.state = State::SplitscreenSelectTeams;
				return;
			}

			// select level
			if ((!u.input->get(Controls::Forward, 0)
				&& u.last_input->get(Controls::Forward, 0))
				|| (Input::dead_zone(u.last_input->gamepads[0].left_y) < 0.0f
					&& Input::dead_zone(u.input->gamepads[0].left_y) >= 0.0f))
			{
				index = vi_min(index + 1, data.zones.length - 1);
				data.next_level = data.zones[index].id;
			}
			else if ((!u.input->get(Controls::Backward, 0)
				&& u.last_input->get(Controls::Backward, 0))
				|| (Input::dead_zone(u.last_input->gamepads[0].left_y) > 0.0f
					&& Input::dead_zone(u.input->gamepads[0].left_y) <= 0.0f))
			{
				index = vi_max(index - 1, 0);
				data.next_level = data.zones[index].id;
			}

			if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				data.state = State::SplitscreenDeploying;
				data.deploy_timer = DEPLOY_TIME_OFFLINE;
				data.tip_time = Game::real_time.total;
			}
		}

		focus_camera(u, data.zones[index]);
	}
}

const ZoneNode* get_level_node(AssetID id)
{
	for (s32 i = 0; i < data.zones.length; i++)
	{
		if (data.zones[i].id == data.next_level)
			return &data.zones[i];
	}
	return nullptr;
}

// returns current zone node
const ZoneNode* zones_draw(const RenderParams& params)
{
	// highlight level locations
	const ZoneNode* current_level = nullptr;
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& node = data.zones[i];
		Transform* pos = node.pos.ref();
		const Vec4& color = node.id == data.next_level ? UI::accent_color : Team::ui_color_friend;
		UI::indicator(params, pos->absolute_pos(), color, false);

		if (node.id == data.next_level)
			current_level = &node;
	}

	// draw current level name
	if (current_level)
	{
		Vec2 p;
		if (UI::project(params, current_level->pos.ref()->absolute_pos(), &p))
		{
			p.y += 32.0f * UI::scale;
			UIText text;
			text.color = UI::accent_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.text_raw(AssetLookup::Level::names[current_level->id]);
			UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
			text.draw(params, p);
		}
	}

	return current_level;
}

void select_zone_draw(const RenderParams& params)
{
	zones_draw(params);

	// press [x] to deploy
	{
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = UI::accent_color;
		text.text(_(strings::deploy_prompt));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}

	// TODO: show alert box saying the previous match was forfeit
	/*
	if (Game::state.forfeit != Game::Forfeit::None)
	{
		// the previous match was forfeit; let the player know
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = UI::accent_color;
		text.text(_(Game::state.forfeit == Game::Forfeit::NetworkError ? strings::forfeit_network_error : strings::forfeit_opponent_quit));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.8f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
	*/
}

void deploy_update(const Update& u)
{
	const ZoneNode& level_node = *get_level_node(data.next_level);
	focus_camera(u, level_node);

	data.deploy_timer -= Game::real_time.delta;
	data.camera->active = data.deploy_timer > 0.5f;
	if (data.deploy_timer < 0.0f)
		go(level_node.index);
}

void progress_draw(const RenderParams& params, const char* label)
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.color = UI::accent_color;
	text.text(label);
	Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

	UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::background_color);

	text.draw(params, pos);

	Vec2 triangle_pos = Vec2
	(
		pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
		pos.y
	);
	UI::triangle_border(params, { triangle_pos, Vec2(20 * UI::scale) }, 9, UI::accent_color, Game::real_time.total * -12.0f);
}

void deploy_draw(const RenderParams& params)
{
	const ZoneNode* current_level = zones_draw(params);

	// show "loading..."
	progress_draw(params, _(Game::state.local_multiplayer ? strings::loading_offline : strings::connecting));

	{
		// show a tip
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::accent_color;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.text(_(strings::tip), _(tips[data.tip_index]));
		UIMenu::text_clip(&text, data.tip_time, 80.0f);

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f) + Vec2(0, 48.0f * UI::scale);

		UI::box(params, text.rect(pos).outset(12 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
}

#define PADDING (16.0f * UI::scale)
#define TAB_SIZE Vec2(160.0f * UI::scale, UI_TEXT_SIZE_DEFAULT * UI::scale + PADDING * 2.0f)
#define MAIN_VIEW_SIZE (Vec2(768.0f, 512.0f) * ((UI::scale < 1.0f ? 0.5f : 1.0f) * UI::scale))
#define BORDER 2.0f
#define TAB_ANIMATION_TIME 0.3f

void tab_messages_update(const Update& u)
{
	focus_camera(u, data.story.camera_messages.ref());
}


void tab_map_update(const Update& u)
{
	select_zone_update(u);
}

void tab_inventory_update(const Update& u)
{
	focus_camera(u, data.story.camera_inventory.ref());
}

#define STORY_MODE_INIT_TIME 2.0f

void story_mode_update(const Update& u)
{
	// pause
	if (!Game::cancel_event_eaten[0]
		&& ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			|| (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))))
	{
		Game::cancel_event_eaten[0] = true;
		Menu::show();
	}

	if (Menu::main_menu_state != Menu::State::Hidden || Game::time.total < STORY_MODE_INIT_TIME)
		return;

	data.story.tab_timer += u.time.delta;
	if (u.last_input->get(Controls::TabLeft, 0) && !u.input->get(Controls::TabLeft, 0))
	{
		data.story.tab_previous = data.story.tab;
		data.story.tab = (Tab)(vi_max(0, (s32)data.story.tab - 1));
		if (data.story.tab != data.story.tab_previous)
			data.story.tab_timer = 0.0f;
	}
	if (u.last_input->get(Controls::TabRight, 0) && !u.input->get(Controls::TabRight, 0))
	{
		data.story.tab_previous = data.story.tab;
		data.story.tab = (Tab)(vi_min((s32)Tab::count - 1, (s32)data.story.tab + 1));
		if (data.story.tab != data.story.tab_previous)
			data.story.tab_timer = 0.0f;
	}

	if (data.story.tab_timer > TAB_ANIMATION_TIME)
	{
		switch (data.story.tab)
		{
			case Tab::Messages:
			{
				tab_messages_update(u);
				break;
			}
			case Tab::Map:
			{
				tab_map_update(u);
				break;
			}
			case Tab::Inventory:
			{
				tab_inventory_update(u);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_draw(const RenderParams& p, const Data::StoryMode& data, Tab tab, const char* label, Vec2* pos)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab)
	{
		// flash the tab when it is selected
		if (data.tab_timer < TAB_ANIMATION_TIME)
		{
			if (UI::flash_function(Game::real_time.total))
				color = &UI::default_color;
			else
				draw = false; // don't draw the tab at all
		}
		else
			color = &UI::accent_color;
	}
	else
		color = &UI::disabled_color;

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	// calculate the width of the tab right now and the width of the tab before the last transition
	r32 current_width = data.tab == tab ? main_view_size.x : tab_size.x;
	r32 previous_width = data.tab_previous == tab ? main_view_size.x : tab_size.x;
	// then blend between the two widths for a nice animation
	r32 blend = Ease::cubic_out(vi_min(1.0f, data.tab_timer / TAB_ANIMATION_TIME), 0.0f, 1.0f);
	Vec2 size(LMath::lerpf(blend, previous_width, current_width), main_view_size.y);

	if (draw)
	{
		// tab body
		{
			if (data.tab != tab) // we're minimized; fill in the background
				UI::box(p, { *pos, size }, Vec4(UI::background_color.xyz(), 0.7f));
			UI::border(p, { *pos, size }, BORDER, *color);
		}

		// actual tab
		{
			Vec2 tab_pos = *pos + Vec2(0, main_view_size.y);
			UI::box(p, { tab_pos, tab_size }, *color);

			UIText text;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::background_color;
			text.text(label);
			text.draw(p, tab_pos + Vec2(PADDING));
		}
	}

	Rect2 result = { *pos, size };

	pos->x += size.x + PADDING;

	return result;
}

void tab_messages_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	if (data.tab == Tab::Map)
		select_zone_draw(p);
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	if (Game::time.total < STORY_MODE_INIT_TIME)
	{
		progress_draw(p, _(strings::connecting));
		return;
	}

	const Rect2& vp = p.camera->viewport;

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	Vec2 center = vp.size * 0.5f;
	Vec2 bottom_left = center + (main_view_size * -0.5f) - tab_size;
	Vec2 total_size = Vec2(main_view_size.x + (tab_size.x + PADDING) * 2.0f, main_view_size.y);

	Vec2 pos = bottom_left;
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Messages, _(strings::tab_messages), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_messages_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Map, _(strings::tab_map), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_map_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Inventory, _(strings::tab_inventory), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_inventory_draw(p, data.story, rect);
	}

	// left/right tab control prompt
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::default_color;
		text.text("[{{TabLeft}}]");

		Vec2 pos = bottom_left + Vec2(tab_size.x * 0.5f, main_view_size.y + tab_size.y * 1.5f);
		UI::box(p, text.rect(pos).outset(PADDING), UI::background_color);
		text.draw(p, pos);

		pos.x += total_size.x - tab_size.x;
		text.text("[{{TabRight}}]");
		UI::box(p, text.rect(pos).outset(PADDING), UI::background_color);
		text.draw(p, pos);
	}
}

void update(const Update& u)
{
	if (data.last_level == Asset::Level::terminal && Game::state.level != Asset::Level::terminal)
	{
		// cleanup
		data.~Data();
		data = Data();
	}
	data.last_level = Game::state.level;

	if (Console::visible || Game::state.level != Asset::Level::terminal)
		return;

	switch (data.state)
	{
		case State::StoryMode:
		{
			story_mode_update(u);
			break;
		}
		case State::SplitscreenSelectTeams:
		{
			splitscreen_select_teams_update(u);
			break;
		}
		case State::SplitscreenSelectLevel:
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
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default || Game::state.level != Asset::Level::terminal)
		return;

	switch (data.state)
	{
		case State::SplitscreenSelectTeams:
		{
			splitscreen_select_teams_draw(params);
			break;
		}
		case State::StoryMode:
		{
			story_mode_draw(params);
			break;
		}
		case State::SplitscreenSelectLevel:
		{
			select_zone_draw(params);
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

void show()
{
	Game::schedule_load_level(Asset::Level::terminal, Game::Mode::Special);
}

}

}
