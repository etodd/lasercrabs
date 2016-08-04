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

struct LevelNode
{
	s32 index;
	AssetID id;
	Ref<Transform> pos;
};

enum class SplitscreenState
{
	SelectTeams,
	SelectLevel,
	Deploying,
};

struct Data
{
	r32 deploy_timer;
	AssetID last_level = AssetNull;
	AssetID next_level = AssetNull;
	SplitscreenState splitscreen_state = SplitscreenState::SelectTeams;
	Camera* camera;
	Vec3 camera_offset;
	Array<LevelNode> levels;
	s32 tip_index;
	r32 tip_time;
};

Data data;

const s32 tip_count = 10;
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
};

b8 splitscreen_teams_are_valid()
{
	s32 player_count = 0;
	s32 team_counts[(s32)AI::Team::count] = {};
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team team = Game::state.local_player_config[i];
		if (team != AI::Team::None)
			team_counts[(s32)team]++;
	}
	for (s32 i = 0; i < (s32)AI::Team::count; i++)
	{
		if (team_counts[i] == 0)
			return false;
	}
	return true;
}

void init(const Update& u, const EntityFinder& entities)
{
	if (Game::state.level != Asset::Level::terminal)
		return;

	data = Data();

	data.tip_index = mersenne::rand() % tip_count;

	data.camera = Camera::add();

	{
		Entity* map_view_entity = entities.find("map_view");
		if (map_view_entity)
		{
			Transform* map_view = map_view_entity->get<Transform>();
			data.camera->rot = Quat::look(map_view->absolute_rot() * Vec3(0, -1, 0));
			data.camera->pos = data.camera_offset = map_view->absolute_pos();
		}
	}

	s32 start_level = Game::state.local_multiplayer ? Game::tutorial_levels : 0; // skip tutorial levels if we're in splitscreen mode
	for (s32 i = start_level; i < Asset::Level::count; i++)
	{
		AssetID level_id = Game::levels[i];
		if (level_id == AssetNull)
			break;

		Entity* entity = entities.find(AssetLookup::Level::names[level_id]);
		if (entity)
		{
			data.levels.add({ i, level_id, entity->get<Transform>() });
			if (level_id == data.last_level)
				data.camera->pos = entity->get<Transform>()->absolute_pos() + data.camera_offset;
		}
	}

	data.camera->viewport =
	{
		Vec2(0, 0),
		Vec2(u.input->width, u.input->height),
	};
	r32 aspect = data.camera->viewport.size.y == 0 ? 1 : (r32)data.camera->viewport.size.x / (r32)data.camera->viewport.size.y;
	data.camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);

	if (!Game::state.local_multiplayer)
	{
		// singleplayer
		const char* entry_point_str = Loader::level_name(Game::levels[Game::save.level_index]);
		Penelope::init(strings_get(entry_point_str));
	}
}

void splitscreen_select_teams_update(const Update& u)
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		b8 player_active = u.input->gamepads[i].active || i == 0;

		AI::Team* team = &Game::state.local_player_config[i];
		if (player_active)
		{
			if (i > 0 || Menu::main_menu_state == Menu::State::Hidden) // ignore player 0 input if the main menu is open
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
						*team = AI::Team::None;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
				}
				else if (left)
				{
					if (*team == AI::Team::B)
					{
						if (i == 0) // player 0 must stay in
							*team = AI::Team::A;
						else
							*team = AI::Team::None;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
					else if (*team == AI::Team::None)
					{
						*team = AI::Team::A;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
				}
				else if (right)
				{
					if (*team == AI::Team::A)
					{
						if (i == 0) // player 0 must stay in
							*team = AI::Team::B;
						else
							*team = AI::Team::None;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
					else if (*team == AI::Team::None)
					{
						*team = AI::Team::B;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
				}
			}
		}
		else // controller is gone
			*team = AI::Team::None;
	}

	if (u.last_input->get(Controls::Interact, 0)
		&& !u.input->get(Controls::Interact, 0)
		&& splitscreen_teams_are_valid())
	{
		data.splitscreen_state = SplitscreenState::SelectLevel;
		data.next_level = Game::levels[Game::tutorial_levels];
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
		else if (team == AI::Team::None)
		{
			color = &UI::default_color;
			x_offset = 0.0f;
		}
		else if (team == AI::Team::A)
		{
			color = &UI::accent_color;
			x_offset = -team_offset;
		}
		else if (team == AI::Team::B)
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

	data.~Data();
	memset(&data, 0, sizeof(data));
}

void focus_camera_on_level(const Update& u, const LevelNode& level)
{
	Vec3 target = data.camera_offset + level.pos.ref()->absolute_pos();
	data.camera->pos += (target - data.camera->pos) * vi_min(1.0f, Game::real_time.delta) * 3.0f;
}

void splitscreen_select_level_update(const Update& u)
{
	s32 index = -1;
	for (s32 i = 0; i < data.levels.length; i++)
	{
		const LevelNode& node = data.levels[i];
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
				index = vi_min(index + 1, data.levels.length - 1);
				data.next_level = data.levels[index].id;
			}
			else if ((!u.input->get(Controls::Backward, 0)
				&& u.last_input->get(Controls::Backward, 0))
				|| (Input::dead_zone(u.last_input->gamepads[0].left_y) > 0.0f
					&& Input::dead_zone(u.input->gamepads[0].left_y) <= 0.0f))
			{
				index = vi_max(index - 1, 0);
				data.next_level = data.levels[index].id;
			}

			if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				data.splitscreen_state = SplitscreenState::Deploying;
				data.deploy_timer = DEPLOY_TIME_OFFLINE;
				data.tip_time = Game::real_time.total;
			}
		}

		focus_camera_on_level(u, data.levels[index]);
	}
}

const LevelNode* get_level_node(AssetID id)
{
	for (s32 i = 0; i < data.levels.length; i++)
	{
		if (data.levels[i].id == data.next_level)
			return &data.levels[i];
	}
	return nullptr;
}

// returns current level node
const LevelNode* levels_draw(const RenderParams& params)
{
	// highlight level locations
	const LevelNode* current_level = nullptr;
	for (s32 i = 0; i < data.levels.length; i++)
	{
		const LevelNode& node = data.levels[i];
		Transform* pos = node.pos.ref();
		const Vec4* color;
		if (Game::state.local_multiplayer)
			color = node.id == data.next_level ? &UI::accent_color : &Team::ui_color_friend;
		else
			color = node.index <= Game::save.level_index ? &UI::accent_color : &UI::alert_color;
		UI::indicator(params, pos->absolute_pos(), *color, false);

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

void splitscreen_select_level_draw(const RenderParams& params)
{
	levels_draw(params);

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
	const LevelNode& level_node = *get_level_node(data.next_level);
	focus_camera_on_level(u, level_node);

	data.deploy_timer -= Game::real_time.delta;
	data.camera->active = data.deploy_timer > 0.5f;
	if (data.deploy_timer < 0.0f)
		go(level_node.index);
}

void deploy_draw(const RenderParams& params)
{
	const LevelNode* current_level = levels_draw(params);

	// show "loading..."
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.color = UI::accent_color;
	text.text(_(Game::state.local_multiplayer ? strings::loading_offline : strings::connecting));
	Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

	UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::background_color);

	text.draw(params, pos);

	Vec2 triangle_pos = Vec2
	(
		pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
		pos.y
	);
	UI::triangle_border(params, { triangle_pos, Vec2(20 * UI::scale) }, 9, UI::accent_color, Game::real_time.total * -8.0f);

	if (Game::save.level_index >= Game::tutorial_levels)
	{
		// show a tip
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::accent_color;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.text(_(tips[data.tip_index]));
		UIMenu::text_clip(&text, data.tip_time, 80.0f);

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f) + Vec2(0, 48.0f * UI::scale);

		UI::box(params, text.rect(pos).outset(12 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
}

void update(const Update& u)
{
	if (Console::visible || Game::state.level != Asset::Level::terminal)
		return;

	if (Game::state.local_multiplayer)
	{
		switch (data.splitscreen_state)
		{
			case SplitscreenState::SelectTeams:
			{
				splitscreen_select_teams_update(u);
				break;
			}
			case SplitscreenState::SelectLevel:
			{
				splitscreen_select_level_update(u);
				break;
			}
			case SplitscreenState::Deploying:
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
	else
	{
		// singleplayer
	}
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default || Game::state.level != Asset::Level::terminal)
		return;

	if (Game::state.local_multiplayer)
	{
		switch (data.splitscreen_state)
		{
			case SplitscreenState::SelectTeams:
			{
				splitscreen_select_teams_draw(params);
				break;
			}
			case SplitscreenState::SelectLevel:
			{
				splitscreen_select_level_draw(params);
				break;
			}
			case SplitscreenState::Deploying:
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
	else
	{
		// singleplayer
	}
}

void show()
{
	Game::schedule_load_level(Asset::Level::terminal, Game::Mode::Special);
}

}

}
