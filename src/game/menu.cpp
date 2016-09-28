#include "menu.h"
#include "asset/level.h"
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
#include "audio.h"
#include "net.h"

namespace VI
{

namespace Menu
{

State main_menu_state;

#if SERVER

void init() {}
void update(const Update&) {}
void clear() {}
void draw(const RenderParams&) {}
void title() {}
void show() {}
void refresh_variables() {}
void pause_menu(const Update&, u8, UIMenu*, State*) {}
b8 options(const Update&, u8, UIMenu*) { return true; }

#else

Game::Mode next_mode;
UIMenu main_menu;
b8 gamepad_active[MAX_GAMEPADS] = {};

void refresh_variables()
{
	const Settings::Gamepad& gamepad = Settings::gamepads[0];
	UIText::set_variable("Start", gamepad.bindings[(s32)Controls::Start].string(Game::is_gamepad));
	UIText::set_variable("Cancel", gamepad.bindings[(s32)Controls::Cancel].string(Game::is_gamepad));

	UIText::set_variable("Primary", gamepad.bindings[(s32)Controls::Primary].string(Game::is_gamepad));
	UIText::set_variable("Zoom", gamepad.bindings[(s32)Controls::Zoom].string(Game::is_gamepad));
	if (Game::is_gamepad)
		UIText::set_variable("Movement", _(strings::left_joystick));
	else
	{
		char buffer[512];
		sprintf
		(
			buffer, _(strings::keyboard_movement),
			gamepad.bindings[(s32)Controls::Forward].string(Game::is_gamepad),
			gamepad.bindings[(s32)Controls::Left].string(Game::is_gamepad),
			gamepad.bindings[(s32)Controls::Backward].string(Game::is_gamepad),
			gamepad.bindings[(s32)Controls::Right].string(Game::is_gamepad)
		);
		UIText::set_variable("Movement", buffer);
	}
	UIText::set_variable("Ability1", gamepad.bindings[(s32)Controls::Ability1].string(Game::is_gamepad));
	UIText::set_variable("Ability2", gamepad.bindings[(s32)Controls::Ability2].string(Game::is_gamepad));
	UIText::set_variable("Ability3", gamepad.bindings[(s32)Controls::Ability3].string(Game::is_gamepad));
	UIText::set_variable("Interact", gamepad.bindings[(s32)Controls::Interact].string(Game::is_gamepad));
	UIText::set_variable("InteractSecondary", gamepad.bindings[(s32)Controls::InteractSecondary].string(Game::is_gamepad));
	UIText::set_variable("TabLeft", gamepad.bindings[(s32)Controls::TabLeft].string(Game::is_gamepad));
	UIText::set_variable("TabRight", gamepad.bindings[(s32)Controls::TabRight].string(Game::is_gamepad));
}

void init()
{
	refresh_variables();

	title();
}

void clear()
{
	main_menu_state = State::Hidden;
}


void title_menu(const Update& u, u8 gamepad, UIMenu* menu, State* state)
{
	if (*state == State::Hidden)
	{
		*state = State::Visible;
		menu->animate();
	}
	switch (*state)
	{
		case State::Visible:
		{
			menu->start(u, 0);
			if (menu->item(u, _(strings::play)))
			{
				Game::save = Game::Save();
				Game::session.reset();
				Terminal::show();
				return;
			}
			if (menu->item(u, _(strings::splitscreen)))
			{
				Game::save = Game::Save();
				Game::session.reset();
				Game::session.multiplayer = true;
				Terminal::show();
			}
			if (menu->item(u, _(strings::online)))
			{
				Game::save = Game::Save();
				Game::session.reset();
				Game::session.multiplayer = true;
				Game::unload_level();
				Net::Client::connect("127.0.0.1", 3494);

				Camera* camera = Camera::add();
				camera->viewport =
				{
					Vec2(0, 0),
					Vec2(u.input->width, u.input->height),
				};
				r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
				camera->perspective((40.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);
				camera->pos = Vec3(0, 0, 0);
				camera->rot = Quat::look(Vec3(0, 0, 1));
			}
			if (menu->item(u, _(strings::options)))
			{
				*state = State::Options;
				menu->animate();
			}
			if (menu->item(u, _(strings::exit)))
				Game::quit = true;
			menu->end();
			break;
		}
		case State::Options:
		{
			if (!options(u, 0, menu))
			{
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
}

void pause_menu(const Update& u, u8 gamepad, UIMenu* menu, State* state)
{
	if (*state == State::Hidden)
	{
		menu->clear();
		return;
	}

	switch (*state)
	{
		case State::Visible:
		{
			menu->start(u, gamepad);
			if (menu->item(u, _(strings::close)))
				*state = State::Hidden;
			if (menu->item(u, _(strings::options)))
			{
				*state = State::Options;
				menu->animate();
			}
			if (menu->item(u, _(strings::quit)))
			{
				if (Game::session.level == Asset::Level::terminal)
					Menu::title();
				else
					Terminal::show();
			}
			menu->end();
			break;
		}
		case State::Options:
		{
			if (!options(u, gamepad, menu))
			{
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
}


void update(const Update& u)
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		UIMenu::active[i] = nullptr;

	if (Console::visible)
		return;

	if (Game::session.level == Asset::Level::title)
		title_menu(u, 0, &main_menu, &main_menu_state);
	else if (Game::session.mode == Game::Mode::Special)
	{
		// do pause menu
		if (main_menu_state == State::Visible
			&& !Game::cancel_event_eaten[0]
			&& ((u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))
				|| (u.input->get(Controls::Cancel, 0) && !u.last_input->get(Controls::Cancel, 0)))
			&& Game::time.total > 0.0f)
		{
			Game::cancel_event_eaten[0] = true;
			main_menu_state = State::Hidden;
			main_menu.clear();
		}
		else
		{
			pause_menu(u, 0, &main_menu, &main_menu_state);
		}
	}
}

void show()
{
	main_menu_state = State::Visible;
	main_menu.animate();
}

void title()
{
	clear();
	Game::session.reset();
	main_menu_state = State::Visible;
	main_menu.animate();
	Game::schedule_load_level(Asset::Level::title, Game::Mode::Special);
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	const Rect2& viewport = params.camera->viewport;
	if (Game::session.level == Asset::Level::title)
	{
		Vec2 logo_pos(viewport.size.x * 0.5f, viewport.size.y * 0.65f);
		Vec2 logo_size(MENU_ITEM_WIDTH);
		UI::mesh(params, Asset::Mesh::logo_mesh_3, logo_pos, Vec2(logo_size), UI::color_background);
		UI::mesh(params, Asset::Mesh::logo_mesh_2, logo_pos, Vec2(logo_size), UI::color_default);
		UI::mesh(params, Asset::Mesh::logo_mesh_1, logo_pos, Vec2(logo_size), UI::color_accent);
		UI::mesh(params, Asset::Mesh::logo_mesh, logo_pos, Vec2(logo_size), UI::color_background);
	}

	if (main_menu_state != State::Hidden)
	{
		if (Game::session.level == Asset::Level::title)
			main_menu.draw_alpha(params, Vec2(viewport.size.x * 0.5f, viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f), UIText::Anchor::Center, UIText::Anchor::Max);
		else
			main_menu.draw_alpha(params, Vec2(0, viewport.size.y * 0.5f), UIText::Anchor::Min, UIText::Anchor::Center);
	}
}

// returns true if options menu is still open
b8 options(const Update& u, u8 gamepad, UIMenu* menu)
{
	menu->start(u, gamepad);
	b8 exit = menu->item(u, _(strings::back)) || (!u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	UIMenu::Delta delta;

	{
		u8* sensitivity = &Settings::gamepads[gamepad].sensitivity;
		sprintf(str, "%u", *sensitivity);
		delta = menu->slider_item(u, _(strings::sensitivity), str);
		if (delta == UIMenu::Delta::Down)
			*sensitivity = vi_max(10, (s32)(*sensitivity) - 10);
		else if (delta == UIMenu::Delta::Up)
			*sensitivity = vi_min(250, (s32)(*sensitivity) + 10);
	}

	{
		b8* invert_y = &Settings::gamepads[gamepad].invert_y;
		sprintf(str, "%s", _(*invert_y ? strings::yes : strings::no));
		delta = menu->slider_item(u, _(strings::invert_y), str);
		if (delta != UIMenu::Delta::None)
			*invert_y = !(*invert_y);
	}

	{
		sprintf(str, "%d", Settings::sfx);
		delta = menu->slider_item(u, _(strings::sfx), str);
		if (delta == UIMenu::Delta::Down)
			Settings::sfx = vi_max(0, Settings::sfx - 10);
		else if (delta == UIMenu::Delta::Up)
			Settings::sfx = vi_min(100, Settings::sfx + 10);
		if (delta != UIMenu::Delta::None)
			Audio::global_param(AK::GAME_PARAMETERS::SFXVOL, (r32)Settings::sfx / 100.0f);
	}

	{
		sprintf(str, "%d", Settings::music);
		delta = menu->slider_item(u, _(strings::music), str);
		if (delta == UIMenu::Delta::Down)
			Settings::music = vi_max(0, Settings::music - 10);
		else if (delta == UIMenu::Delta::Up)
			Settings::music = vi_min(100, Settings::music + 10);
		if (delta != UIMenu::Delta::None)
			Audio::global_param(AK::GAME_PARAMETERS::MUSICVOL, (r32)Settings::music / 100.0f);
	}

	menu->end();

	if (exit)
	{
		menu->end();
		Loader::settings_save();
		return false;
	}

	return true;
}
#endif

}

UIMenu* UIMenu::active[MAX_GAMEPADS];

UIMenu::UIMenu()
	: selected(),
	items(),
	animation_time(),
	scroll()
{
}

void UIMenu::clear()
{
	items.length = 0;
}

void UIMenu::animate()
{
	selected = 0;
	scroll.pos = 0;
	animation_time = Game::real_time.total;
}

void UIMenu::start(const Update& u, u8 g, b8 input)
{
	clear();

	gamepad = g;

	if (Console::visible || !input)
		return;

	if (active[g])
	{
		if (active[g] != this)
			return;
	}
	else
		active[g] = this;

	selected += UI::input_delta_vertical(u, gamepad);
}

b8 UIMenu::add_item(b8 slider, const char* string, const char* value, b8 disabled, AssetID icon)
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
	item->label.color = item->value.color = disabled ? UI::color_disabled : (is_selected ? UI::color_accent : UI::color_default);
	item->label.text(string);
	text_clip(&item->label, animation_time + (items.length - 1 - scroll.pos) * 0.1f, 100.0f);

	item->value.anchor_x = UIText::Anchor::Center;
	item->value.text(value);

	if (!scroll.item(items.length - 1)) // this item is not visible
		return false;

	return true;
}

// render a single menu item and increment the position for the next item
b8 UIMenu::item(const Update& u, const char* string, const char* value, b8 disabled, AssetID icon)
{
	if (!add_item(false, string, value, disabled, icon))
		return false;

	if (Console::visible || active[gamepad] != this)
		return false;

	if (selected == items.length - 1
		&& !u.input->get(Controls::Interact, gamepad)
		&& u.last_input->get(Controls::Interact, gamepad)
		&& Game::time.total > 0.5f
		&& !Console::visible
		&& !disabled)
	{
		Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
		return true;
	}
	
	return false;
}

UIMenu::Delta UIMenu::slider_item(const Update& u, const char* label, const char* value, b8 disabled, AssetID icon)
{
	if (!add_item(true, label, value, disabled, icon))
		return Delta::None;

	if (Console::visible || active[gamepad] != this)
		return Delta::None;

	if (disabled)
		return Delta::None;

	if (selected == items.length - 1
		&& Game::time.total > 0.5f)
	{
		s32 delta = UI::input_delta_horizontal(u, gamepad);
		if (delta < 0)
		{
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
			return Delta::Down;
		}
		else if (delta > 0)
		{
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
			return Delta::Up;
		}
	}
	
	return Delta::None;
}

void UIMenu::end()
{
	scroll.update_menu(items.length);

	if (selected < 0)
		selected = items.length - 1;
	if (selected >= items.length)
		selected = 0;

	scroll.scroll_into_view(selected);
}

r32 UIMenu::height() const
{
	return (vi_min(items.length, (u16)UI_SCROLL_MAX) * MENU_ITEM_HEIGHT) - MENU_ITEM_PADDING * 2.0f;
}

void UIMenu::text_clip(UIText* text, r32 start_time, r32 speed)
{
	r32 clip = (Game::real_time.total - start_time) * speed;
	text->clip = vi_max(1, (s32)clip);
		
	s32 mod = speed < 40.0f ? 1 : (speed < 100.0f ? 2 : 3);
	if (text->clip % mod == 0
		&& (s32)(clip - Game::real_time.delta * speed) < (s32)clip
		&& text->clipped()
		&& text->rendered_string[text->clip] != ' '
		&& text->rendered_string[text->clip] != '\t'
		&& text->rendered_string[text->clip] != '\n')
	{
		Audio::post_global_event(AK::EVENTS::PLAY_CONSOLE_KEY);
	}
}

const UIMenu::Item* UIMenu::last_visible_item() const
{
	for (s32 i = items.length - 1; i >= 0; i--)
	{
		if (scroll.item(i))
			return &items[i];
	}
	return nullptr;
}

void UIMenu::draw_alpha(const RenderParams& params, const Vec2& origin, UIText::Anchor anchor_x, UIText::Anchor anchor_y) const
{
	if (items.length == 0)
		return;

	b8 scroll_started = false;

	Rect2 last_item_rect;

	Vec2 pos = origin + Vec2(MENU_ITEM_PADDING_LEFT, 0);
	switch (anchor_x)
	{
		case UIText::Anchor::Center:
		{
			pos.x += MENU_ITEM_WIDTH * -0.5f;
			break;
		}
		case UIText::Anchor::Min:
		{
			break;
		}
		case UIText::Anchor::Max:
		{
			pos.x -= MENU_ITEM_WIDTH;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	switch (anchor_y)
	{
		case UIText::Anchor::Center:
		{
			pos.y += height() * 0.5f;
			break;
		}
		case UIText::Anchor::Min:
		{
			pos.y += height();
			break;
		}
		case UIText::Anchor::Max:
		{
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	for (s32 i = 0; i < items.length; i++)
	{
		if (!scroll.item(i))
			continue;

		const Item& item = items[i];

		Rect2 rect;
		{
			Vec2 bounds = item.label.bounds();
			rect.pos.x = pos.x - MENU_ITEM_PADDING_LEFT;
			rect.pos.y = pos.y - bounds.y - MENU_ITEM_PADDING;
			rect.size.x = MENU_ITEM_WIDTH;
			rect.size.y = bounds.y + MENU_ITEM_PADDING * 2.0f;
		}

		if (!scroll_started)
		{
			scroll.start(params, rect.pos + Vec2(rect.size.x * 0.5f, rect.size.y));
			scroll_started = true;
		}

		UI::box(params, rect, UI::color_background);
		if (active[gamepad] == this && i == selected)
			UI::box(params, { pos + Vec2(-MENU_ITEM_PADDING_LEFT, item.label.size * -UI::scale), Vec2(4 * UI::scale, item.label.size * UI::scale) }, UI::color_accent);

		if (item.icon != AssetNull)
			UI::mesh(params, item.icon, pos + Vec2(MENU_ITEM_PADDING_LEFT * -0.5f, MENU_ITEM_FONT_SIZE * -0.5f), Vec2(UI::scale * MENU_ITEM_FONT_SIZE), item.label.color);

		item.label.draw(params, pos);

		r32 value_offset_time = (2 + vi_min(i, 6)) * 0.06f;
		if (Game::real_time.total - animation_time > value_offset_time)
		{
			if (item.value.has_text())
				item.value.draw(params, pos + Vec2(MENU_ITEM_VALUE_OFFSET, 0));
			if (item.slider)
			{
				Rect2 down_rect = rect;
				{
					down_rect.pos.x += MENU_ITEM_PADDING_LEFT + MENU_ITEM_WIDTH * 0.5f;
					down_rect.size.x = down_rect.size.y;
				}

				UI::triangle(params, { down_rect.pos + down_rect.size * 0.5f, down_rect.size * 0.5f }, item.label.color, PI * 0.5f);

				Rect2 up_rect = rect;
				{
					up_rect.size.x = up_rect.size.y;
					up_rect.pos.x += rect.size.x - up_rect.size.x;
				}
				UI::triangle(params, { up_rect.pos + up_rect.size * 0.5f, up_rect.size * 0.5f }, item.label.color, PI * -0.5f);
			}
		}
		pos.y -= MENU_ITEM_HEIGHT;
	}

	scroll.end(params, pos + Vec2(MENU_ITEM_WIDTH * 0.5f, MENU_ITEM_HEIGHT));
}


}
