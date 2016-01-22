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

namespace VI
{

namespace Menu
{

#define fov_initial (80.0f * PI * 0.5f / 180.0f)
	static Camera* cameras[MAX_GAMEPADS] = {};
	static UIText player_text[MAX_GAMEPADS];
	static UIText join_text = UIText();
	static UIText leave_text = UIText();
	static UIText game_over_text = UIText();
	static UIText connecting_text = UIText();
	static UIText start_text = UIText();
	static UIText end_text = UIText();
	static s32 gamepad_count = 0;
	static AssetID last_level = AssetNull;
	static AssetID next_level = AssetNull;
	static r32 connect_timer = 0.0f;

	static b8 player_active[MAX_GAMEPADS];

#if DEBUG
#define CONNECT_DELAY_MIN 0.5f
#define CONNECT_DELAY_RANGE 0.0f
#else
#define CONNECT_DELAY_MIN 2.0f
#define CONNECT_DELAY_RANGE 2.0f
#endif

void init()
{
	Loader::font_permanent(Asset::Font::lowpoly);
	refresh_variables();

	join_text.font =
	leave_text.font =
	game_over_text.font =
	connecting_text.font =
	start_text.font =
	end_text.font =
	Asset::Font::lowpoly;

	join_text.size =
	leave_text.size =
	game_over_text.size =
	connecting_text.size =
	start_text.size =
	end_text.size =
	18.0f;


	join_text.anchor_x =
	leave_text.anchor_x =
	game_over_text.anchor_x =
	connecting_text.anchor_x =
	start_text.anchor_x =
	end_text.anchor_x =
	UIText::Anchor::Center;

	join_text.anchor_y =
	leave_text.anchor_y =
	game_over_text.anchor_y =
	connecting_text.anchor_y =
	start_text.anchor_y =
	end_text.anchor_y =
	UIText::Anchor::Center;

	join_text.text("[{{Action}}] to join");
	leave_text.text("[{{Cancel}}] to leave\n[{{Start}}] to begin");
	game_over_text.text("Account balance reached zero.\nSimulation end.\n[{{Start}}]");
	connecting_text.text("Connecting...");
	start_text.text("[{{Start}}]");
	end_text.text("Demo simulation complete. Thanks for playing!\n[{{Start}}]");

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		player_text[i].font = Asset::Font::lowpoly;
		player_text[i].size = 18.0f;
		player_text[i].anchor_x = UIText::Anchor::Center;
		player_text[i].anchor_y = UIText::Anchor::Min;
		char buffer[255];
		sprintf(buffer, "Player %d", i + 1);
		player_text[i].text(buffer);
	}

	player_active[0] = true;

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
}

void refresh_variables()
{
	if (gamepad_count > 0)
	{
		UIText::set_variable("Start", "Start");
		UIText::set_variable("Resume", "Start");
		UIText::set_variable("Action", "A");
		UIText::set_variable("Cancel", "B");
		UIText::set_variable("Primary", "Right trigger");
		UIText::set_variable("Secondary", "Left trigger");
		UIText::set_variable("Quit", "B");
		UIText::set_variable("Up", "Right bumper");
		UIText::set_variable("Down", "Left bumper");
	}
	else
	{
		UIText::set_variable("Start", "Enter");
		UIText::set_variable("Resume", "Esc");
		UIText::set_variable("Action", "Space");
		UIText::set_variable("Cancel", "Esc");
		UIText::set_variable("Primary", "Mouse1");
		UIText::set_variable("Secondary", "Mouse2");
		UIText::set_variable("Quit", "Enter");
		UIText::set_variable("Up", "Space");
		UIText::set_variable("Down", "Ctrl");
	}
}

void update(const Update& u)
{
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

	switch (Game::data.level)
	{
		case Asset::Level::menu:
		{
			if (Game::data.level != last_level)
			{
				Game::data = Game::Data();
				Game::data.previous_level = AssetNull;
				player_active[0] = true;
				for (s32 i = 1; i < MAX_GAMEPADS; i++)
					player_active[i] = false;
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
					if (u.input->get(Game::bindings.cancel, i) && !u.last_input->get(Game::bindings.cancel, i))
					{
						player_active[i] = false;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
					else if (u.input->get(Game::bindings.action, i) && !u.last_input->get(Game::bindings.action, i))
					{
						player_active[i] = true;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
				}
				else
					player_active[i] = false;
			}

			if (cameras_changed)
			{
				Camera::ViewportBlueprs32* viewports = Camera::viewport_blueprs32s[screen_count - 1];
				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					Camera* camera = cameras[i];
					if (camera)
					{
						Camera::ViewportBlueprs32* viewport = &viewports[i];
						camera->viewport =
						{
							Vec2((s32)(viewport->x * (r32)u.input->width), (s32)(viewport->y * (r32)u.input->height)),
							Vec2((s32)(viewport->w * (r32)u.input->width), (s32)(viewport->h * (r32)u.input->height)),
						};
						r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
						camera->perspective(fov_initial, aspect, 0.01f, Skybox::far_plane);
					}
				}
			}

			b8 start = false;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (player_active[i])
					start |= u.input->get(Game::bindings.start, i) && !u.last_input->get(Game::bindings.start, i);
			}

			if (Game::data.level != last_level && gamepad_count <= 1)
				start = true;

			if (start)
			{
				clear();
				for (s32 i = 0; i < MAX_GAMEPADS; i++)
				{
					if (player_active[i])
					{
						LocalPlayer* p = LocalPlayer::list.add();
						new (p) LocalPlayer(AI::Team::A, i);
					}
				}
				Game::schedule_load_level(Asset::Level::start);
				return;
			}
			break;
		}
		case Asset::Level::title:
		case Asset::Level::end:
		case Asset::Level::game_over:
		{
			if (Game::data.level != last_level)
			{
				AssetID level = Game::data.level;
				Game::data = Game::Data();
				Game::data.level = level;
			}

			b8 go = false;
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				go |= ((u.input->get(Game::bindings.action, i) && !u.last_input->get(Game::bindings.action, i)))
					| ((u.input->get(Game::bindings.start, i) && !u.last_input->get(Game::bindings.start, i)));
			}
			
			if (Game::data.level == Asset::Level::title)
			{
				if (go)
					return menu();
				else
				{
					for (s32 i = 0; i < MAX_GAMEPADS; i++)
						Game::quit |= u.input->get(Game::bindings.cancel, i) && !u.last_input->get(Game::bindings.cancel, i);
				}
			}
			else
			{
				if (go)
					return title();
			}

			break;
		}
		case Asset::Level::connect:
		{
			if (Game::data.level != last_level)
				connect_timer = CONNECT_DELAY_MIN + mersenne::randf_co() * CONNECT_DELAY_RANGE;

			connect_timer -= u.time.delta;
			if (connect_timer < 0.0f)
			{
				clear();
				Game::schedule_load_level(next_level);
				next_level = AssetNull;
			}
			break;
		}
		case AssetNull:
			break;
		default:
		{
			b8 credits_out = false;
			for (s32 i = 0; i < (s32)AI::Team::count; i++)
			{
				if (Game::data.credits[i] <= 0)
				{
					credits_out = true;
					break;
				}
			}
			if (credits_out && LocalPlayerControl::list.count() == 0)
				Game::schedule_load_level(Asset::Level::game_over);
			break;
		}
	}
	last_level = Game::data.level;
}

void transition(AssetID level)
{
	clear();
	next_level = level;
	Game::schedule_load_level(Asset::Level::connect);
}

void menu()
{
	clear();
	Game::schedule_load_level(Asset::Level::menu);
}

void title()
{
	clear();
	Game::schedule_load_level(Asset::Level::title);
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	const Rect2& viewport = params.camera->viewport;
	switch (Game::data.level)
	{
		case Asset::Level::menu:
		{
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				if (params.camera == cameras[i])
				{
					player_text[i].draw(params, viewport.size * 0.5f);
					(player_active[i] ? leave_text : join_text).draw(params, viewport.size * 0.5f);
					return;
				}
			}
			break;
		}
		case Asset::Level::title:
		{
			UI::centered_box(params, { viewport.size * Vec2(0.5f), Vec2(185.0f * UI::scale, 100.0f * UI::scale) }, Vec4(0, 0, 0, 1));
			UI::mesh(params, Asset::Mesh::logo, viewport.size * Vec2(0.5f), Vec2(128.0f * UI::scale, 128.0f * UI::scale));
			start_text.draw(params, viewport.size * Vec2(0.5f, 0.25f));
			break;
		}
		case Asset::Level::game_over:
		{
			game_over_text.draw(params, params.camera->viewport.size * 0.5f);
			break;
		}
		case Asset::Level::end:
		{
			end_text.draw(params, params.camera->viewport.size * 0.5f);
			break;
		}
		case Asset::Level::connect:
		{
			Vec2 pos = params.camera->viewport.size * 0.5f;
			connecting_text.draw(params, pos);

			Vec2 triangle_pos = Vec2
			(
				pos.x - connecting_text.bounds().x * 0.5f - 32.0f * UI::scale,
				pos.y
			);
			UI::triangle(params, { triangle_pos, Vec2(32 * UI::scale) }, Vec4(1), Game::time.total * 8.0f);
			break;
		}
		default:
			break;
	}
}

b8 is_special_level(AssetID level)
{
	return level == Asset::Level::connect
		|| level == Asset::Level::title
		|| level == Asset::Level::menu
		|| level == Asset::Level::game_over
		|| level == Asset::Level::end
		|| level == Asset::Level::start;
}

}

UIMenu::UIMenu()
	: selected(),
	items()
{
}

void UIMenu::clear()
{
	items.length = 0;
}

void UIMenu::start(const Update& u, u8 gamepad)
{
	clear();

	if (gamepad == 0)
		Game::update_cursor(u);

	const Settings& settings = Loader::settings();
	if (u.input->get(settings.bindings.forward, gamepad)
		&& !u.last_input->get(settings.bindings.forward, gamepad))
		selected--;

	if (u.input->get(settings.bindings.backward, gamepad)
		&& !u.last_input->get(settings.bindings.backward, gamepad))
		selected++;
}

// render a single menu item and increment the position for the next item
b8 UIMenu::item(const Update& u, u8 gamepad, Vec2* menu_pos, const char* string)
{
	Item* item = items.add();
	item->label.size = 24.0f;
	item->label.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
	item->label.anchor_x = UIText::Anchor::Min;
	item->label.anchor_y = UIText::Anchor::Max;
	item->label.text(string);
	item->label.color = UI::default_color;

	item->pos = *menu_pos;
	item->pos.x += MENU_ITEM_PADDING;
	Rect2 box = item->label.rect(item->pos).outset(MENU_ITEM_PADDING);

	menu_pos->y -= box.size.y;

	if (box.contains(Game::cursor))
	{
		selected = items.length - 1;
		if (!u.input->get({ KeyCode::MouseLeft }, gamepad)
			&& u.last_input->get({ KeyCode::MouseLeft }, gamepad)
			&& Game::time.total > 0.5f)
			return true;
	}

	if (selected == items.length - 1
		&& !u.input->get(Game::bindings.start, gamepad)
		&& u.last_input->get(Game::bindings.start, gamepad)
		&& Game::time.total > 0.5f)
		return true;
	
	return false;
}

void UIMenu::end()
{
	if (selected < 0)
		selected = items.length - 1;
	if (selected >= items.length)
		selected = 0;
}

void UIMenu::draw_alpha(const RenderParams& params) const
{
	for (s32 i = 0; i < items.length; i++)
	{
		const Item* item = &items[i];
		Rect2 rect = item->label.rect(item->pos).outset(MENU_ITEM_PADDING);
		UI::box(params, rect, i == selected ? UI::subtle_color : Vec4(0, 0, 0, 1));
		item->label.draw(params, item->pos);
	}
}

}
