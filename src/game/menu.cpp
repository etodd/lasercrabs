#include "menu.h"
#include "asset/level.h"
#include "asset/font.h"
#include "asset/mesh.h"
#include "game.h"
#include "player.h"
#include "render/ui.h"
#include "load.h"
#include "entities.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "render/views.h"
#include "console.h"
#include "mersenne/mersenne-twister.h"
#include "strings.h"
#include "settings.h"

namespace VI
{

namespace Menu
{

#define fov_initial (80.0f * PI * 0.5f / 180.0f)

#if DEBUG
#define CONNECT_DELAY_MIN 0.5f
#define CONNECT_DELAY_RANGE 0.0f
#else
#define CONNECT_DELAY_MIN 2.0f
#define CONNECT_DELAY_RANGE 2.0f
#endif

static Camera* cameras[MAX_GAMEPADS] = {};
static UIText player_text[MAX_GAMEPADS];
static s32 gamepad_count = 0;
static AssetID last_level = AssetNull;
static AssetID next_level = AssetNull;
static r32 connect_timer = 0.0f;
static UIMenu main_menu;

static State main_menu_state;

void reset_players()
{
	Game::state.local_multiplayer_offset = 0;
	for (int i = 0; i < MAX_GAMEPADS; i++)
		Game::state.local_player_config[i] = AI::Team::None;
	Game::state.local_player_config[0] = AI::Team::A;
}

void init()
{
	Loader::font_permanent(Asset::Font::lowpoly);
	refresh_variables();

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		player_text[i].font = Asset::Font::lowpoly;
		player_text[i].size = 18.0f;
		player_text[i].anchor_x = UIText::Anchor::Center;
		player_text[i].anchor_y = UIText::Anchor::Min;
		char buffer[255];
		sprintf(buffer, _(strings::player), i + 1);
		player_text[i].text(buffer);
	}

	reset_players();

	title();
}

void clear()
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (cameras[i])
		{
			cameras[i]->remove();
			cameras[i] = nullptr;
		}
	}
	main_menu_state = State::Hidden;
}

void refresh_variables()
{
	b8 is_gamepad = gamepad_count > 0;
	const Settings::Gamepad& gamepad = Settings::gamepads[0];
	UIText::set_variable("Start", gamepad.bindings[(s32)Controls::Start].string(is_gamepad));
	UIText::set_variable("Resume", gamepad.bindings[(s32)Controls::Pause].string(is_gamepad));
	UIText::set_variable("Action", gamepad.bindings[(s32)Controls::Action].string(is_gamepad));
	UIText::set_variable("Cancel", gamepad.bindings[(s32)Controls::Cancel].string(is_gamepad));

	UIText::set_variable("Primary", gamepad.bindings[(s32)Controls::Primary].string(is_gamepad));
	UIText::set_variable("Secondary", gamepad.bindings[(s32)Controls::Secondary].string(is_gamepad));
	if (is_gamepad)
		UIText::set_variable("Movement", _(strings::left_joystick));
	else
	{
		char buffer[512];
		sprintf
		(
			buffer, _(strings::keyboard_movement),
			gamepad.bindings[(s32)Controls::Forward].string(is_gamepad),
			gamepad.bindings[(s32)Controls::Left].string(is_gamepad),
			gamepad.bindings[(s32)Controls::Backward].string(is_gamepad),
			gamepad.bindings[(s32)Controls::Right].string(is_gamepad)
		);
		UIText::set_variable("Movement", buffer);
	}
	UIText::set_variable("Parkour", gamepad.bindings[(s32)Controls::Parkour].string(is_gamepad));
	UIText::set_variable("Jump", gamepad.bindings[(s32)Controls::Jump].string(is_gamepad));
	UIText::set_variable("Slide", gamepad.bindings[(s32)Controls::Slide].string(is_gamepad));
	UIText::set_variable("Ability1", gamepad.bindings[(s32)Controls::Ability1].string(is_gamepad));
	UIText::set_variable("Ability2", gamepad.bindings[(s32)Controls::Ability2].string(is_gamepad));
	UIText::set_variable("Ability3", gamepad.bindings[(s32)Controls::Ability3].string(is_gamepad));
	UIText::set_variable("Interact", gamepad.bindings[(s32)Controls::Interact].string(is_gamepad));
}

#define logo_size (128.0f * UI::scale)
#define logo_padding (46.0f * UI::scale)

void title_menu(const Update& u, u8 gamepad, UIMenu* menu, State* state)
{
	if (*state == State::Hidden)
	{
		*state = State::Visible;
		menu->animate();
	}
	menu->start(u, 0);
	switch (*state)
	{
		case State::Visible:
		{
			Vec2 pos(logo_padding * 2.0f + logo_size, u.input->height * 0.5f + UIMenu::height(4) * 0.5f);
			if (menu->item(u, &pos, _(strings::play)))
			{
				Game::save = Game::Save();
				transition(Game::levels[Game::save.level_index], Game::Mode::Parkour);
				return;
			}
			if (menu->item(u, &pos, _(strings::options)))
			{
				menu->selected = 0;
				*state = State::Options;
				menu->animate();
			}
			if (menu->item(u, &pos, _(strings::splitscreen)))
				splitscreen();
			if (menu->item(u, &pos, _(strings::exit)))
				Game::quit = true;
			break;
		}
		case State::Options:
		{
			Vec2 pos(logo_padding * 2.0f + logo_size, u.input->height * 0.5f + options_height() * 0.5f);
			if (!options(u, 0, menu, &pos))
			{
				menu->selected = 0;
				*state = State::Visible;
				menu->animate();
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	menu->end();
}

void pause_menu(const Update& u, const Rect2& viewport, u8 gamepad, UIMenu* menu, State* state)
{
	if (*state == State::Hidden)
	{
		menu->clear();
		return;
	}

	menu->start(u, gamepad);
	switch (*state)
	{
		case State::Visible:
		{
			Vec2 pos(0, viewport.size.y * 0.5f + UIMenu::height(3) * 0.5f);
			if (menu->item(u, &pos, _(strings::resume)))
				*state = State::Hidden;
			if (menu->item(u, &pos, _(strings::options)))
			{
				menu->selected = 0;
				*state = State::Options;
				menu->animate();
			}
			if (menu->item(u, &pos, _(strings::main_menu)))
				Menu::title();
			break;
		}
		case State::Options:
		{
			Vec2 pos(0, viewport.size.y * 0.5f + options_height() * 0.5f);
			if (!options(u, gamepad, menu, &pos))
			{
				menu->selected = 0;
				*state = State::Visible;
				menu->animate();
			}
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	menu->end();
}

void update(const Update& u)
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		UIMenu::active[i] = nullptr;

	s32 last_gamepad_count = gamepad_count;
	gamepad_count = 0;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (u.input->gamepads[i].active)
			gamepad_count++;
	}

	if ((last_gamepad_count == 0) != (gamepad_count != 0))
		refresh_variables();

	if (Console::visible)
		return;

	switch (Game::state.level)
	{
		case Asset::Level::splitscreen:
		{
			if (Game::state.level != last_level)
			{
				reset_players();
				Game::state.local_multiplayer = true;
			}

			b8 cameras_changed = false;
			s32 screen_count = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				b8 screen_active = u.input->gamepads[i].active || i == 0;

				if (screen_active && !cameras[i])
				{
					cameras[i] = Camera::add();
					cameras_changed = true;
				}
				else if (cameras[i] && !screen_active)
				{
					cameras[i]->remove();
					cameras[i] = nullptr;
					cameras_changed = true;
				}

				if (screen_active)
				{
					screen_count++;
					if (i > 0) // player 0 must stay in
					{
						if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
						{
							Game::state.local_player_config[i] = AI::Team::None;
							Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
						}
						else if (u.input->get(Controls::Action, i) && !u.last_input->get(Controls::Action, i))
						{
							Game::state.local_player_config[i] = i % 2 == 0 ? AI::Team::A : AI::Team::B;
							Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
						}
					}
				}
				else
					Game::state.local_player_config[i] = AI::Team::None;
			}

			if (cameras_changed)
			{
				Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[screen_count - 1];
				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					Camera* camera = cameras[i];
					if (camera)
					{
						Camera::ViewportBlueprint* viewport = &viewports[i];
						camera->viewport =
						{
							Vec2((s32)(viewport->x * (r32)u.input->width), (s32)(viewport->y * (r32)u.input->height)),
							Vec2((s32)(viewport->w * (r32)u.input->width), (s32)(viewport->h * (r32)u.input->height)),
						};
						r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
						camera->perspective(fov_initial, aspect, 0.01f, Game::level.skybox.far_plane);
					}
				}
			}

			b8 start = false;
			s32 player_count = 0;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (Game::state.local_player_config[i] != AI::Team::None)
				{
					player_count++;
					start |= u.input->get(Controls::Start, i) && !u.last_input->get(Controls::Start, i);
				}
			}

			if (player_count > 1 && start)
			{
				Game::save = Game::Save();
				Game::save.level_index = 1; // skip tutorial
				transition(Game::levels[Game::save.level_index], Game::Mode::Pvp);
			}
			break;
		}
		case Asset::Level::title:
		{
			if (Game::state.level != last_level)
			{
				reset_players();
				main_menu_state = State::Visible;
				main_menu.animate();
			}
			break;
		}
		case Asset::Level::connect:
		{
			if (Game::state.level != last_level)
				connect_timer = CONNECT_DELAY_MIN + mersenne::randf_co() * CONNECT_DELAY_RANGE;

			connect_timer -= u.time.delta;
			if (connect_timer < 0.0f)
			{
				clear();
				Game::schedule_load_level(next_level, Game::Mode::Pvp);
			}
			break;
		}
		case AssetNull:
			break;
		default: // just playing normally
			break;
	}

	if (Game::state.level == Asset::Level::title)
		title_menu(u, 0, &main_menu, &main_menu_state);
	else if (is_special_level(Game::state.level, Game::state.mode))
	{
		// toggle the pause menu
		b8 pause_hit;
		if (Game::state.level == Asset::Level::splitscreen || main_menu_state != State::Hidden)
			pause_hit = u.input->get(Controls::Cancel, 0) && !u.last_input->get(Controls::Cancel, 0);
		else
			pause_hit = u.input->get(Controls::Pause, 0) && !u.last_input->get(Controls::Pause, 0);

		if (pause_hit && Game::time.total > 0.0f && (main_menu_state == State::Hidden || main_menu_state == State::Visible))
		{
			main_menu_state = main_menu_state == State::Hidden ? State::Visible : State::Hidden;
			main_menu.animate();
		}

		// do pause menu
		const Rect2& viewport = cameras[0] ? cameras[0]->viewport : Rect2(Vec2(0, 0), Vec2(u.input->width, u.input->height));
		pause_menu(u, viewport, 0, &main_menu, &main_menu_state);
	}

	last_level = Game::state.level;
}

void transition(AssetID level, Game::Mode mode)
{
	clear();
	if (mode == Game::Mode::Pvp)
	{
		next_level = level;
		Game::schedule_load_level(Asset::Level::connect, Game::Mode::Special);
	}
	else
		Game::schedule_load_level(level, mode);
}

void splitscreen()
{
	clear();
	Game::schedule_load_level(Asset::Level::splitscreen, Game::Mode::Special);
}

void title()
{
	clear();
	Game::schedule_load_level(Asset::Level::title, Game::Mode::Special);
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	const Rect2& viewport = params.camera->viewport;
	switch (Game::state.level)
	{
		case Asset::Level::splitscreen:
		{
			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			const Vec2 box_size(256 * UI::scale);
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (params.camera == cameras[i])
				{
					UI::box(params, { viewport.size * 0.5f - box_size * 0.5f, box_size }, UI::background_color);
					player_text[i].draw(params, viewport.size * 0.5f);
					if (i > 0)
					{
						if (Game::state.local_player_config[i] == AI::Team::None)
							text.text(_(strings::join));
						else
							text.text(_(strings::leave));
					}
					else // player 0 must stay in
						text.text(_(strings::begin));
					text.draw(params, viewport.size * 0.5f + Vec2(0, -16.0f * UI::scale));
					break;
				}
			}
			break;
		}
		case Asset::Level::title:
		{
			Vec2 logo_pos(logo_padding + logo_size * 0.5f, viewport.size.y * 0.5f);
			UI::box(params, { Vec2(0, logo_pos.y - logo_size * 0.5f - logo_padding), Vec2(logo_size + logo_padding * 2.0f + MENU_ITEM_WIDTH, logo_size + logo_padding * 2.0f) }, UI::background_color);
			Mesh* m0 = Loader::mesh(Asset::Mesh::logo_mesh);
			UI::mesh(params, Asset::Mesh::logo_mesh, logo_pos, Vec2(logo_size), m0->color);
			Mesh* m1 = Loader::mesh(Asset::Mesh::logo_mesh_1);
			UI::mesh(params, Asset::Mesh::logo_mesh_1, logo_pos, Vec2(logo_size), m1->color);
			break;
		}
		case Asset::Level::connect:
		{
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.color = UI::accent_color;
			text.text(_(strings::connecting));
			Vec2 pos = viewport.size * 0.5f;

			UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::background_color);

			text.draw(params, pos);

			Vec2 triangle_pos = Vec2
			(
				pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
				pos.y
			);
			UI::triangle_border(params, { triangle_pos, Vec2(20 * UI::scale) }, 6, UI::accent_color, Game::time.total * 8.0f);
			break;
		}
		default:
			break;
	}

	if (is_special_level(Game::state.level, Game::state.mode))
	{
		if (!cameras[0] || params.camera == cameras[0]) // if we have cameras active, only draw the main menu on the first one
			main_menu.draw_alpha(params);
	}
}

b8 is_special_level(AssetID level, Game::Mode mode)
{
	return mode == Game::Mode::Special
		|| level == Asset::Level::connect
		|| level == Asset::Level::title
		|| level == Asset::Level::splitscreen;
}

b8 options(const Update& u, u8 gamepad, UIMenu* menu, Vec2* pos)
{
	bool menu_open = true;
	if (menu->item(u, pos, _(strings::back)) || (!u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad)))
		menu_open = false;

	char str[128];
	UIMenu::Delta delta;

	sprintf(str, "%d", Settings::sfx);
	delta = menu->slider_item(u, pos, _(strings::sfx), str);
	if (delta == UIMenu::Delta::Down)
		Settings::sfx = vi_max(0, Settings::sfx - 10);
	else if (delta == UIMenu::Delta::Up)
		Settings::sfx = vi_min(100, Settings::sfx + 10);
	if (delta != UIMenu::Delta::None)
		Audio::global_param(AK::GAME_PARAMETERS::SFXVOL, (r32)Settings::sfx / 100.0f);

	sprintf(str, "%d", Settings::music);
	delta = menu->slider_item(u, pos, _(strings::music), str);
	if (delta == UIMenu::Delta::Down)
		Settings::music = vi_max(0, Settings::music - 10);
	else if (delta == UIMenu::Delta::Up)
		Settings::music = vi_min(100, Settings::music + 10);
	if (delta != UIMenu::Delta::None)
		Audio::global_param(AK::GAME_PARAMETERS::MUSICVOL, (r32)Settings::music / 100.0f);

	if (!menu_open)
		Loader::settings_save();

	return menu_open;
}

r32 options_height()
{
	return UIMenu::height(3);
}

}

UIMenu* UIMenu::active[MAX_GAMEPADS];

Rect2 UIMenu::Item::rect() const
{
	Vec2 bounds = label.bounds();
	Rect2 box;
	box.pos.x = pos.x - MENU_ITEM_PADDING_LEFT;
	box.pos.y = pos.y - bounds.y - MENU_ITEM_PADDING;
	box.size.x = MENU_ITEM_WIDTH;
	box.size.y = bounds.y + MENU_ITEM_PADDING * 2.0f;
	return box;
}

Rect2 UIMenu::Item::down_rect() const
{
	Rect2 r = rect();
	r.pos.x += MENU_ITEM_PADDING_LEFT + MENU_ITEM_WIDTH * 0.5f;
	r.size.x = r.size.y;
	return r;
}

Rect2 UIMenu::Item::up_rect() const
{
	Rect2 r = rect();
	r32 width = r.size.x;
	r.size.x = r.size.y;
	r.pos.x += width - r.size.x;
	return r;
}

UIMenu::UIMenu()
	: selected(),
	items(),
	animation_time()
{
}

void UIMenu::clear()
{
	items.length = 0;
}

void UIMenu::animate()
{
	animation_time = Game::real_time.total;
}

#define JOYSTICK_DEAD_ZONE 0.5f

void UIMenu::start(const Update& u, u8 g, b8 input)
{
	clear();

	gamepad = g;

	if (!input)
		return;

	if (active[g])
	{
		if (active[g] != this)
			return;
	}
	else
		active[g] = this;

	if (gamepad == 0)
		Game::update_cursor(u);

	if (u.input->gamepads[gamepad].active)
	{
		r32 last_y = Input::dead_zone(u.last_input->gamepads[gamepad].left_y, JOYSTICK_DEAD_ZONE);
		if (last_y == 0.0f)
		{
			r32 y = Input::dead_zone(u.input->gamepads[gamepad].left_y, JOYSTICK_DEAD_ZONE);
			if (y < 0.0f)
				selected--;
			else if (y > 0.0f)
				selected++;
		}
	}

	if (u.input->get(Controls::Forward, gamepad)
		&& !u.last_input->get(Controls::Forward, gamepad))
		selected--;

	if (u.input->get(Controls::Backward, gamepad)
		&& !u.last_input->get(Controls::Backward, gamepad))
		selected++;
}

Rect2 UIMenu::add_item(Vec2* pos, b8 slider, const char* string, const char* value, b8 disabled, AssetID icon)
{
	Item* item = items.add();
	item->icon = icon;
	item->slider = slider;
	item->label.size = item->value.size = MENU_ITEM_FONT_SIZE;
	if (value)
		item->label.wrap_width = MENU_ITEM_VALUE_OFFSET - MENU_ITEM_PADDING - MENU_ITEM_PADDING_LEFT;
	else
		item->label.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING - MENU_ITEM_PADDING_LEFT;
	item->label.anchor_x = UIText::Anchor::Min;
	item->label.anchor_y = item->value.anchor_y = UIText::Anchor::Max;

	b8 is_selected = active[gamepad] == this && selected == items.length - 1;
	item->label.color = item->value.color = disabled ? UI::disabled_color : (is_selected ? UI::accent_color : UI::default_color);
	item->label.text(string);
	item->label.clip = 1 + (s32)((Game::real_time.total - animation_time) * (50.0f + vi_min(items.length, 6) * -5.0f));

	item->value.anchor_x = UIText::Anchor::Center;
	item->value.text(value);
	item->value.clip = item->label.clip;

	item->pos = *pos;
	item->pos.x += MENU_ITEM_PADDING_LEFT;

	Rect2 box = item->rect();

	pos->y -= box.size.y;

	return box;
}

// render a single menu item and increment the position for the next item
b8 UIMenu::item(const Update& u, Vec2* menu_pos, const char* string, const char* value, b8 disabled, AssetID icon)
{
	Rect2 box = add_item(menu_pos, false, string, value, disabled, icon);

	if (active[gamepad] != this)
		return false;

	if (gamepad == 0 && box.contains(Game::cursor))
	{
		selected = items.length - 1;

		if (disabled)
			return false;

		if (!u.input->get(Controls::Click, gamepad)
			&& u.last_input->get(Controls::Click, gamepad)
			&& Game::time.total > 0.5f)
		{
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
			return true;
		}
	}

	if (selected == items.length - 1
		&& !u.input->get(Controls::Action, gamepad)
		&& u.last_input->get(Controls::Action, gamepad)
		&& Game::time.total > 0.5f
		&& !Console::visible
		&& !disabled)
	{
		Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
		return true;
	}
	
	return false;
}

UIMenu::Delta UIMenu::slider_item(const Update& u, Vec2* menu_pos, const char* label, const char* value, b8 disabled, AssetID icon)
{
	Rect2 box = add_item(menu_pos, true, label, value, disabled, icon);

	if (active[gamepad] != this)
		return Delta::None;

	if (gamepad == 0 && box.contains(Game::cursor))
		selected = items.length - 1;

	if (disabled)
		return Delta::None;

	if (selected == items.length - 1
		&& Game::time.total > 0.5f)
	{
		if (!u.input->get(Controls::Left, gamepad)
			&& u.last_input->get(Controls::Left, gamepad))
		{
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
			return Delta::Down;
		}

		if (!u.input->get(Controls::Right, gamepad)
			&& u.last_input->get(Controls::Right, gamepad))
		{
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
			return Delta::Up;
		}

		if (u.input->gamepads[gamepad].active)
		{
			r32 last_x = Input::dead_zone(u.last_input->gamepads[gamepad].left_x, JOYSTICK_DEAD_ZONE);
			if (last_x == 0.0f)
			{
				r32 x = Input::dead_zone(u.input->gamepads[gamepad].left_x, JOYSTICK_DEAD_ZONE);
				if (x < 0.0f)
				{
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					return Delta::Down;
				}
				else if (x > 0.0f)
				{
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					return Delta::Up;
				}
			}
		}

		if (gamepad == 0)
		{
			if (!u.input->get(Controls::Click, gamepad)
				&& u.last_input->get(Controls::Click, gamepad)
				&& Game::time.total > 0.5f)
			{
				Item* item = &items[items.length - 1];
				if (item->down_rect().contains(Game::cursor))
				{
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					return Delta::Down;
				}

				if (item->up_rect().contains(Game::cursor))
				{
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					return Delta::Up;
				}
			}
		}
	}
	
	return Delta::None;
}

void UIMenu::end()
{
	if (selected < 0)
		selected = items.length - 1;
	if (selected >= items.length)
		selected = 0;
}

r32 UIMenu::height(s32 items)
{
	return (items * MENU_ITEM_HEIGHT) - MENU_ITEM_PADDING * 2.0f;
}

void UIMenu::draw_alpha(const RenderParams& params) const
{
	if (items.length == 0)
		return;

	for (s32 i = 0; i < items.length; i++)
	{
		const Item* item = &items[i];
		Rect2 rect = item->rect();
		UI::box(params, rect, UI::background_color);
		if (active[gamepad] == this && i == selected)
			UI::box(params, { item->pos + Vec2(-MENU_ITEM_PADDING_LEFT, item->label.size * -UI::scale), Vec2(4 * UI::scale, item->label.size * UI::scale) }, UI::accent_color);

		if (item->icon != AssetNull)
			UI::mesh(params, item->icon, item->pos + Vec2(MENU_ITEM_PADDING_LEFT * -0.5f, MENU_ITEM_FONT_SIZE * -0.5f), Vec2(UI::scale * MENU_ITEM_FONT_SIZE), item->label.color);

		item->label.draw(params, item->pos);
		if (item->value.has_text())
			item->value.draw(params, item->pos + Vec2(MENU_ITEM_VALUE_OFFSET, 0));
		if (item->slider)
		{
			const Rect2& down_rect = item->down_rect();
			UI::triangle(params, { down_rect.pos + down_rect.size * 0.5f, down_rect.size * 0.5f }, item->label.color, PI * 0.5f);

			const Rect2& up_rect = item->up_rect();
			UI::triangle(params, { up_rect.pos + up_rect.size * 0.5f, up_rect.size * 0.5f }, item->label.color, PI * -0.5f);
		}
	}

	if (gamepad == 0 && active[gamepad] == this)
		Game::draw_cursor(params);
}

}
