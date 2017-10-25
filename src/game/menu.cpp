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
#include "asset/font.h"
#include "render/views.h"
#include "console.h"
#include "mersenne/mersenne-twister.h"
#include "strings.h"
#include "settings.h"
#include "audio.h"
#include "net.h"
#include "scripts.h"
#include "overworld.h"
#include "ease.h"
#include "data/components.h"
#include "team.h"

namespace VI
{

namespace Menu
{

enum class FriendshipState
{
	None,
	NotFriends,
	Friends,
	count,
};

#define DIALOG_MAX 255

State main_menu_state;
DialogCallback dialog_callback[MAX_GAMEPADS];
DialogCallback dialog_cancel_callback[MAX_GAMEPADS];
DialogCallback dialog_callback_last[MAX_GAMEPADS];
DialogTextCallback dialog_text_callback;
DialogTextCallback dialog_text_callback_last;
DialogTextCancelCallback dialog_text_cancel_callback;
s32 dialog_text_truncate;
r32 dialog_time[MAX_GAMEPADS];
char dialog_string[MAX_GAMEPADS][DIALOG_MAX + 1];
r32 dialog_time_limit[MAX_GAMEPADS];
TextField dialog_text_field;
Ref<Camera> camera_connecting;
Controls currently_editing_control = Controls::count;
b8 currently_editing_control_enable_input; // should we be listening for any and all button presses to apply to the control binding we're currently editing?
s32 display_mode_index;
b8 display_mode_fullscreen;
b8 display_mode_vsync;
s32 display_mode_index_last;
b8 display_mode_fullscreen_last;
b8 display_mode_vsync_last;
Ref<PlayerManager> teams_selected_player[MAX_GAMEPADS] = {};
FriendshipState teams_selected_player_friendship[MAX_GAMEPADS] = {};
AssetID maps_selected_map = AssetNull;

#define DIALOG_ANIM_TIME 0.25f

// default callbacks
void dialog_no_action(s8 gamepad) { }
void dialog_text_cancel_no_action() { }

State settings(const Update&, const UIMenu::Origin&, s8, UIMenu*);
b8 settings_controls(const Update&, const UIMenu::Origin&, s8, UIMenu*, Gamepad::Type);
b8 settings_graphics(const Update&, const UIMenu::Origin&, s8, UIMenu*);
b8 maps(const Update&, const UIMenu::Origin&, s8, UIMenu*);

static const AssetID region_strings[] =
{
	strings::region_useast,
	strings::region_uswest,
	strings::region_europe,
};

AssetID region_string(Region region)
{
	vi_assert(s32(region) >= 0 && s32(region) < s32(Region::count));
	return region_strings[s32(region)];
}

#if SERVER

void init(const InputState&) {}
void exit(s8) {}
void update(const Update&) {}
void update_end(const Update&) {}
void clear() {}
void draw_ui(const RenderParams&) {}
void title() {}
void title_multiplayer() {}
void splash() {}
void title_menu(const Update& u, Camera* camera) {};
void show() {}
void refresh_variables(const InputState&) {}
void pause_menu(const Update&, const UIMenu::Origin&, s8, UIMenu*, State*) {}
void draw_letterbox(const RenderParams&, r32, r32) {}
State settings(const Update&, const UIMenu::Origin&, s8, UIMenu*) { return State::Visible; }
b8 maps(const Update&, s8, UIMenu*) { return false; }
void teams_select_match_start_init(PlayerHuman*) {}
State teams(const Update&, const UIMenu::Origin&, s8, UIMenu*, TeamSelectMode, EnableInput) { return State::Visible; }
void friendship_state(u32, b8) {}
b8 choose_region(const Update&, const UIMenu::Origin&, s8, UIMenu*, AllowClose) { return false; }
b8 settings_controls(const Update&, const UIMenu::Origin&, s8, UIMenu*, Gamepad::Type) { return false; }
b8 settings_graphics(const Update&, const UIMenu::Origin&, s8, UIMenu*) { return false; }
void progress_spinner(const RenderParams&, const Vec2&, r32) {}
void progress_bar(const RenderParams&, const char*, r32, const Vec2&) {}
void progress_infinite(const RenderParams&, const char*, const Vec2&) {}
void dialog(s8, DialogCallback, const char*, ...) {}
void dialog_with_cancel(s8, DialogCallback, DialogCallback, const char*, ...) {}
void dialog_with_time_limit(s8, DialogCallback, DialogCallback, r32, const char*, ...) {}
void dialog_text(DialogTextCallback, const char*, s32, const char*, ...) {}
void dialog_text_with_cancel(DialogTextCallback, DialogTextCancelCallback, const char*, s32, const char*, ...) {}
b8 dialog_active(s8) { return false; }

#else

State* quit_menu_state;

void quit_multiplayer(s8 gamepad)
{
	if (quit_menu_state)
		*quit_menu_state = State::Hidden;
	quit_menu_state = nullptr;

	if (Game::level.id == Asset::Level::Docks)
		Overworld::title();
	else
	{
		PlayerHuman* player = PlayerHuman::player_for_gamepad(gamepad);
		if (player)
			player->get<PlayerManager>()->leave();
		else
			Menu::title_multiplayer();
	}
}

void quit_to_title(s8 gamepad)
{
	title();
	if (quit_menu_state)
		*quit_menu_state = State::Hidden;
	quit_menu_state = nullptr;
}

b8 dialog_active(s8 gamepad)
{
	return dialog_callback[gamepad]
		|| dialog_callback_last[gamepad]
		|| (gamepad == 0
			&& (dialog_text_callback || dialog_text_callback_last));
}

void dialog(s8 gamepad, DialogCallback callback, const char* format, ...)
{
	if (callback == &dialog_no_action)
		Audio::post_global(AK::EVENTS::PLAY_DIALOG_ALERT);
	else
		Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], DIALOG_MAX, format, args);
#else
	vsnprintf(dialog_string[gamepad], DIALOG_MAX, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = nullptr;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = 0.0f;
}

void dialog_with_cancel(s8 gamepad, DialogCallback callback, DialogCallback cancel_callback, const char* format, ...)
{
	Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], DIALOG_MAX, format, args);
#else
	vsnprintf(dialog_string[gamepad], DIALOG_MAX, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = cancel_callback;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = 0.0f;
}

void dialog_with_time_limit(s8 gamepad, DialogCallback callback, DialogCallback callback_cancel, r32 limit, const char* format, ...)
{
	Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], DIALOG_MAX, format, args);
#else
	vsnprintf(dialog_string[gamepad], DIALOG_MAX, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = callback_cancel;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = limit;
}

void dialog_text(DialogTextCallback callback, const char* initial_value, s32 truncate, const char* format, ...)
{
	Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[0], DIALOG_MAX, format, args);
#else
	vsnprintf(dialog_string[0], DIALOG_MAX, format, args);
#endif

	va_end(args);

	dialog_text_callback = callback;
	dialog_text_cancel_callback = nullptr;
	dialog_text_truncate = truncate;
	dialog_text_field.set(initial_value);
	dialog_time[0] = Game::real_time.total;
	dialog_time_limit[0] = 0.0f;
}

void dialog_text_with_cancel(DialogTextCallback callback, DialogTextCancelCallback cancel_callback, const char* initial_value, s32 truncate, const char* format, ...)
{
	Audio::post_global(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[0], DIALOG_MAX, format, args);
#else
	vsnprintf(dialog_string[0], DIALOG_MAX, format, args);
#endif

	va_end(args);

	dialog_text_callback = callback;
	dialog_text_cancel_callback = cancel_callback;
	dialog_text_truncate = truncate;
	dialog_text_field.set(initial_value);
	dialog_time[0] = Game::real_time.total;
	dialog_time_limit[0] = 0.0f;
}

void progress_spinner(const RenderParams& params, const Vec2& pos, r32 size)
{
	UI::triangle_border(params, { pos, Vec2(size * UI::scale) }, 9, UI::color_accent(), Game::real_time.total * -12.0f);
}

void progress_bar(const RenderParams& params, const char* label, r32 percentage, const Vec2& pos)
{
	UIText text;
	text.color = UI::color_background;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;
	text.text(0, label);

	Rect2 bar = text.rect(pos).outset(16.0f * UI::scale);

	UI::box(params, bar, UI::color_background);
	UI::border(params, bar, 2, UI::color_accent());
	UI::box(params, { bar.pos, Vec2(bar.size.x * percentage, bar.size.y) }, UI::color_accent());

	text.draw(params, bar.pos + bar.size * 0.5f);

	Vec2 triangle_pos = Vec2
	(
		pos.x - text.bounds().x * 0.5f - 48.0f * UI::scale,
		pos.y
	);
	progress_spinner(params, triangle_pos);
}

void progress_infinite(const RenderParams& params, const char* label, const Vec2& pos_overall)
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.color = UI::color_accent();
	text.text(0, label);

	Vec2 pos = pos_overall + Vec2(24 * UI::scale, 0);

	UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::color_background);

	text.draw(params, pos);

	Vec2 triangle_pos = Vec2
	(
		pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
		pos.y
	);
	progress_spinner(params, triangle_pos);
}

Game::Mode next_mode;
UIMenu main_menu;
b8 gamepad_active[MAX_GAMEPADS] = {};

void refresh_variables(const InputState& input)
{
	UIText::variables_clear();
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		Gamepad::Type type = Game::ui_gamepad_types[i];
		const Settings::Gamepad& gamepad = Settings::gamepads[i];
		if (type == Gamepad::Type::None)
		{
			char buffer[512];
			sprintf
			(
				buffer, "[%s %s %s %s]",
				gamepad.bindings[s32(Controls::Forward)].string(Gamepad::Type::None),
				gamepad.bindings[s32(Controls::Left)].string(Gamepad::Type::None),
				gamepad.bindings[s32(Controls::Backward)].string(Gamepad::Type::None),
				gamepad.bindings[s32(Controls::Right)].string(Gamepad::Type::None)
			);
			UIText::variable_add(i, "Movement", buffer);

			sprintf
			(
				buffer, "[%s] + [%s/%s]",
				gamepad.bindings[s32(Controls::Parkour)].string(Gamepad::Type::None),
				gamepad.bindings[s32(Controls::Forward)].string(Gamepad::Type::None),
				gamepad.bindings[s32(Controls::Backward)].string(Gamepad::Type::None)
			);
			UIText::variable_add(i, "ClimbingMovement", buffer);
		}
		else
		{
			UIText::variable_add(i, "Movement", _(strings::left_joystick));

			char buffer[512];
			sprintf
			(
				buffer, "[%s] + [%s]",
				gamepad.bindings[s32(Controls::Parkour)].string(type),
				_(strings::left_joystick)
			);
			UIText::variable_add(i, "ClimbingMovement", buffer);
		}
		for (s32 j = 0; j < s32(Controls::count); j++)
		{
			const char* ui_variable_name = Input::control_ui_variable_name(Controls(j));
			if (ui_variable_name)
				UIText::variable_add(i, ui_variable_name, gamepad.bindings[j].string(type));
		}
	}
}

void init(const InputState& input)
{
	refresh_variables(input);

	display_mode_index = Settings::display_mode_index;
}

void clear()
{
	main_menu_state = State::Hidden;
	dialog_text_callback = nullptr;
	dialog_text_cancel_callback = nullptr;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		dialog_callback[i] = nullptr;
		dialog_cancel_callback[i] = nullptr;
	}
}

void dialog_clear(s8 gamepad)
{
	if (gamepad == 0)
	{
		dialog_text_callback = nullptr;
		dialog_text_cancel_callback = nullptr;
	}
	dialog_callback[gamepad] = nullptr;
	dialog_cancel_callback[gamepad] = nullptr;
}

void exit(s8 gamepad)
{
	Game::quit = true;
}

void title_menu(const Update& u, Camera* camera)
{
	UIMenu::Origin origin;
	{
		const DisplayMode& display = Settings::display();
		origin =
		{
			Vec2(display.width * 0.5f, display.height * 0.65f + MENU_ITEM_HEIGHT * -1.5f),
			UIText::Anchor::Center,
			UIText::Anchor::Max,
		};
	}

	switch (main_menu_state)
	{
		case State::Hidden:
			break;
		case State::Visible:
		{
			if (Settings::region == Region::Invalid)
			{
				// must choose a region first
				if (!choose_region(u, origin, 0, &main_menu, AllowClose::No))
				{
					// done choosing a region
					main_menu.clear();
					main_menu.animate();
				}
			}
			else
			{
				main_menu.start(u, origin, 0);
				if (main_menu.item(u, _(strings::story)))
				{
					Scripts::Docks::play();
					clear();
				}
				if (main_menu.item(u, _(strings::multiplayer)))
				{
					Game::save.reset();
					Game::session.reset(SessionType::Multiplayer);
					Game::session.config.game_type = GameType::Assault;
					Overworld::show(camera, Overworld::State::Multiplayer);
					clear();
				}
				if (main_menu.item(u, _(strings::settings)))
				{
					main_menu_state = State::Settings;
					main_menu.animate();
				}
				if (main_menu.item(u, _(strings::discord)))
					open_url("https://discord.gg/rHkXXhR");
				if (main_menu.item(u, _(strings::twitter)))
					open_url("https://twitter.com/DeceiverGame");
				if (main_menu.item(u, _(strings::bug_report)))
					open_url("https://github.com/etodd/deceiver/issues");
				if (main_menu.item(u, _(strings::exit)))
					dialog(0, &exit, _(strings::confirm_quit));
				main_menu.end(u);
			}
			break;
		}
		case State::Settings:
		{
			State s = settings(u, origin, 0, &main_menu);
			if (s != main_menu_state)
			{
				main_menu_state = s;
				main_menu.animate();
			}
			break;
		}
		case State::SettingsControlsKeyboard:
		{
			if (!settings_controls(u, origin, 0, &main_menu, Gamepad::Type::None))
			{
				main_menu_state = State::Settings;
				main_menu.animate();
			}
			break;
		}
		case State::SettingsControlsGamepad:
		{
			if (!settings_controls(u, origin, 0, &main_menu, u.input->gamepads[0].type))
			{
				main_menu_state = State::Settings;
				main_menu.animate();
			}
			break;
		}
		case State::SettingsGraphics:
		{
			if (!settings_graphics(u, origin, 0, &main_menu))
			{
				main_menu_state = State::Settings;
				main_menu.animate();
			}
			break;
		}
		default:
			vi_assert(false);
			break;
	}
}

void open_url(const char* url)
{
	vi_debug("Opening URL: %s", url);
#if _WIN32
	// Windows
	ShellExecute(0, 0, url, 0, 0 , SW_SHOW);
#elif defined(__APPLE__)
	// Mac
	char buffer[MAX_PATH_LENGTH + 1] = {};
	snprintf(buffer, MAX_PATH_LENGTH, "open \"%s\"", url);
	system(buffer);
#elif defined(__ORBIS__)
	// PS4
	// todo
#else
	// Linux
	char buffer[MAX_PATH_LENGTH + 1] = {};
	snprintf(buffer, MAX_PATH_LENGTH, "xdg-open \"%s\"", url);
	system(buffer);
#endif
}

b8 player(const Update&, const UIMenu::Origin&, s8, UIMenu*);

void pause_menu(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu, State* state)
{
	if (*state == State::Hidden)
	{
		menu->clear();
		quit_menu_state = nullptr;
		return;
	}

	quit_menu_state = state;

	switch (*state)
	{
		case State::Visible:
		{
			menu->start(u, origin, gamepad);
			if (menu->item(u, _(strings::resume)))
				*state = State::Hidden;
			if (Game::session.type == SessionType::Multiplayer && Game::level.mode == Game::Mode::Pvp)
			{
				PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
				if ((me->is_admin || (Game::session.config.game_type == GameType::Assault || Game::session.config.max_players > Game::session.config.team_count))
					&& menu->item(u, _(strings::teams)))
				{
					*state = State::Teams;
					teams_selected_player[gamepad] = nullptr;
					teams_selected_player_friendship[gamepad] = FriendshipState::None;
					menu->animate();
				}
				if (me->is_admin)
				{
					if (menu->item(u, _(strings::levels)))
					{
						*state = State::Maps;
						maps_selected_map = AssetNull;
						menu->animate();
					}

					if (PlayerHuman::count_local() <= 1 && menu->item(u, _(strings::server_settings)))
					{
						*state = State::Hidden;
						Overworld::server_settings(me->get<PlayerHuman>()->camera.ref());
					}
				}
			}
			if (menu->item(u, _(strings::settings)))
			{
				if (gamepad == 0)
					*state = State::Settings;
				else
					*state = State::SettingsControlsGamepad; // other players don't have any other settings
				menu->animate();
			}
			if (menu->item(u, _(strings::quit)))
			{
				if (Game::session.type == SessionType::Story)
					dialog(gamepad, &quit_to_title, _(strings::confirm_quit));
				else
					dialog(gamepad, &quit_multiplayer, _(strings::confirm_quit));
			}
			menu->end(u);
			break;
		}
		case State::Maps:
		{
			if (!maps(u, origin, gamepad, menu))
			{
				*state = State::Visible;
				menu->animate();
			}
			break;
		}
		case State::Teams:
		{
			State s = teams(u, origin, gamepad, menu, TeamSelectMode::Normal);
			if (s != *state)
			{
				*state = s;
				menu->animate();
			}
			break;
		}
		case State::Player:
		{
			if (!player(u, origin, gamepad, menu))
			{
				*state = State::Teams;
				menu->animate();
			}
			break;
		}
		case State::Settings:
		{
			State s = settings(u, origin, gamepad, menu);
			if (s != *state)
			{
				*state = s;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsKeyboard:
		{
			if (!settings_controls(u, origin, gamepad, menu, Gamepad::Type::None))
			{
				*state = State::Settings;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsGamepad:
		{
			if (!settings_controls(u, origin, gamepad, menu, u.input->gamepads[gamepad].type))
			{
				if (gamepad == 0)
					*state = State::Settings;
				else
					*state = State::Visible; // other players don't have any other settings
				menu->animate();
			}
			break;
		}
		case State::SettingsGraphics:
		{
			if (!settings_graphics(u, origin, gamepad, menu))
			{
				*state = State::Settings;
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

struct DialogLayout
{
	Rect2 container;
	Rect2 accept;
	Rect2 cancel;
};

DialogLayout dialog_layout(s8 gamepad, const RenderParams* params = nullptr)
{
	const r32 padding = 16.0f * UI::scale;
	UIText text;
	text.color = UI::color_default;
	text.wrap_width = MENU_ITEM_WIDTH;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.text(gamepad, dialog_string[gamepad]);
	Vec2 pos = Camera::for_gamepad(gamepad)->viewport.size * 0.5f;
	Rect2 text_rect = text.rect(pos).outset(padding);

	{
		r32 prompt_height = (padding + UI_TEXT_SIZE_DEFAULT * UI::scale) * Ease::cubic_out<r32>(vi_min((Game::real_time.total - dialog_time[gamepad]) / DIALOG_ANIM_TIME, 1.0f));
		text_rect.pos.y -= prompt_height;
		text_rect.size.y += prompt_height;
	}

	DialogLayout layout = {};
	layout.container = text_rect;

	if (params)
	{
		UI::box(*params, text_rect, UI::color_background);
		UI::border(*params, text_rect, 2.0f, UI::color_accent());

		UIMenu::text_clip(&text, dialog_time[gamepad], 150.0f);
		text.draw(*params, pos);
	}

	if (Game::real_time.total > dialog_time[gamepad] + DIALOG_ANIM_TIME)
	{
		// accept
		text.wrap_width = 0;
		text.anchor_y = UIText::Anchor::Min;
		text.anchor_x = UIText::Anchor::Min;
		text.color = UI::color_accent();
		text.clip = 0;
		text.text(gamepad, dialog_time_limit[gamepad] > 0.0f ? "%s (%d)" : "%s", _(strings::prompt_accept), s32(dialog_time_limit[gamepad]) + 1);
		Vec2 prompt_pos = text_rect.pos + Vec2(padding);
		layout.accept = text.rect(prompt_pos).outset(padding * 0.5f);
		if (params)
		{
			if (params->sync->input.get(Controls::Interact, gamepad)
				|| (gamepad == 0
				&& Game::ui_gamepad_types[0] == Gamepad::Type::None
				&& layout.accept.contains(UI::cursor_pos)))
			{
				UI::box(*params, layout.accept, params->sync->input.keys.get(s32(KeyCode::MouseLeft)) ? UI::color_alert() : UI::color_accent());
				text.color = UI::color_background;
			}
			text.draw(*params, prompt_pos);
		}

		if (dialog_callback[gamepad] != &dialog_no_action)
		{
			// cancel
			text.anchor_x = UIText::Anchor::Max;
			text.color = UI::color_alert();
			text.clip = 0;
			text.text(gamepad, _(strings::prompt_cancel));
			Vec2 p = prompt_pos + Vec2(text_rect.size.x + padding * -2.0f, 0);
			layout.cancel = text.rect(p).outset(padding * 0.5f);
			if (params)
			{
				if (params->sync->input.get(Controls::Cancel, gamepad)
					|| (gamepad == 0
					&& Game::ui_gamepad_types[0] == Gamepad::Type::None
					&& layout.cancel.contains(UI::cursor_pos)))
				{
					UI::box(*params, layout.cancel, params->sync->input.keys.get(s32(KeyCode::MouseLeft)) ? UI::color_accent() : UI::color_alert());
					text.color = UI::color_background;
				}
				text.draw(*params, p);
			}
		}
	}

	return layout;
}

void update(const Update& u)
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
		UIMenu::active[i] = nullptr;

	if (Console::visible)
		return;

#if !SERVER
	if (!Game::level.local && Net::Client::mode() != Net::Client::Mode::Connected)
	{
		// we're connecting; return to main menu if the user cancels
		if ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			|| (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0)))
		{
			if (Game::session.type == SessionType::Story)
				title();
			else
				title_multiplayer();
		}
	}

	if (Net::Client::mode() == Net::Client::Mode::Disconnected
		&& Game::level.id == AssetNull
		&& Game::scheduled_load_level == AssetNull)
	{
		// connection process failed
		title();
		switch (Net::Client::master_error)
		{
			case Net::Client::MasterError::None:
			{
				switch (Net::Client::disconnect_reason)
				{
					case Net::DisconnectReason::Timeout:
					case Net::DisconnectReason::SequenceGap:
						Game::scheduled_dialog = strings::connection_failed;
						break;
					case Net::DisconnectReason::ServerFull:
						Game::scheduled_dialog = strings::server_full;
						break;
					case Net::DisconnectReason::ServerResetting:
						Game::scheduled_dialog = strings::server_resetting;
						break;
					case Net::DisconnectReason::WrongVersion:
						Game::scheduled_dialog = strings::need_upgrade;
						break;
					case Net::DisconnectReason::AuthFailed:
						Game::scheduled_dialog = strings::auth_failed;
						break;
					case Net::DisconnectReason::Kicked:
						Game::scheduled_dialog = strings::kicked;
						break;
					default:
						vi_assert(false);
						break;
				}
				break;
			}
			case Net::Client::MasterError::WrongVersion:
			{
				Game::scheduled_dialog = strings::need_upgrade;
				break;
			}
			case Net::Client::MasterError::Timeout:
			{
				Game::scheduled_dialog = strings::connection_failed;
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}
#endif

	// dialogs
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (dialog_time_limit[i] > 0.0f)
		{
			dialog_time_limit[i] = vi_max(0.0f, dialog_time_limit[i] - u.time.delta);
			if (dialog_time_limit[i] == 0.0f)
			{
				// cancel
				Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL);
				DialogCallback c = dialog_cancel_callback[i];
				dialog_callback[i] = nullptr;
				dialog_cancel_callback[i] = nullptr;
				if (c)
					c(i);
			}
		}

		// dialog buttons
		if (dialog_callback[i] && dialog_callback_last[i]) // make sure we don't trigger the button on the first frame the dialog is shown
		{
			b8 accept_clicked = false;
			b8 cancel_clicked = false;
			if (i == 0 && Game::ui_gamepad_types[i] == Gamepad::Type::None)
			{
				DialogLayout layout = dialog_layout(i);
				if (u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)))
				{
					if (layout.accept.contains(UI::cursor_pos))
						accept_clicked = true;
					else if (layout.cancel.contains(UI::cursor_pos))
						cancel_clicked = true;
				}
			}

			if (accept_clicked || (u.last_input->get(Controls::Interact, i) && !u.input->get(Controls::Interact, i)))
			{
				// accept
				Audio::post_global(AK::EVENTS::PLAY_DIALOG_ACCEPT);
				DialogCallback callback = dialog_callback[i];
				dialog_callback[i] = nullptr;
				dialog_cancel_callback[i] = nullptr;
				dialog_time_limit[i] = 0.0f;
				callback(s8(i));
			}
			else if (cancel_clicked || (!Game::cancel_event_eaten[i] && u.last_input->get(Controls::Cancel, i) && !u.input->get(Controls::Cancel, i)))
			{
				// cancel
				Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL);
				DialogCallback cancel_callback = dialog_cancel_callback[i];
				dialog_callback[i] = nullptr;
				dialog_cancel_callback[i] = nullptr;
				dialog_time_limit[i] = 0.0f;
				Game::cancel_event_eaten[i] = true;
				if (cancel_callback)
					cancel_callback(s8(i));
			}
		}
	}

	// text dialog
	if (dialog_text_callback && dialog_text_callback_last) // make sure we don't trigger the button on the first frame the dialog is shown
	{
		dialog_text_field.update(u, 0, dialog_text_truncate);
		if (u.last_input->get(Controls::UIAcceptText, 0) && !u.input->get(Controls::UIAcceptText, 0))
		{
			// accept
			Audio::post_global(AK::EVENTS::PLAY_DIALOG_ACCEPT);
			DialogTextCallback callback = dialog_text_callback;
			dialog_text_callback = nullptr;
			dialog_text_cancel_callback = nullptr;
			dialog_time_limit[0] = 0.0f;
			callback(dialog_text_field);
		}
		else if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
		{
			// cancel
			Audio::post_global(AK::EVENTS::PLAY_DIALOG_CANCEL);
			DialogTextCancelCallback cancel_callback = dialog_text_cancel_callback;
			dialog_text_callback = nullptr;
			dialog_text_cancel_callback = nullptr;
			dialog_time_limit[0] = 0.0f;
			Game::cancel_event_eaten[0] = true;
			if (cancel_callback)
				cancel_callback();
		}
	}

	if (Overworld::active())
	{
		// do pause menu
		if (main_menu_state == State::Visible
			&& !Game::cancel_event_eaten[0]
			&& Game::time.total > 0.0f
			&& !Menu::dialog_active(0)
			&& ((u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))
				|| (u.input->get(Controls::Cancel, 0) && !u.last_input->get(Controls::Cancel, 0))))
		{
			Game::cancel_event_eaten[0] = true;
			main_menu_state = State::Hidden;
			main_menu.clear();
		}
		else
		{
			UIMenu::Origin origin =
			{
				Vec2(0, Settings::display().height * 0.5f),
				UIText::Anchor::Min,
				UIText::Anchor::Center
			};
			pause_menu(u, origin, 0, &main_menu, &main_menu_state);
		}
	}
}

void update_end(const Update& u)
{
	// reset cancel event eaten flags
	dialog_text_callback_last = dialog_text_callback;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		dialog_callback_last[i] = dialog_callback[i];
		if (!u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
			Game::cancel_event_eaten[i] = false;
	}
	
	// "connecting..." camera
	{
		b8 camera_needed = Net::Client::mode() == Net::Client::Mode::ContactingMaster
			|| Net::Client::mode() == Net::Client::Mode::Connecting
			|| Net::Client::mode() == Net::Client::Mode::Loading;
		if (camera_needed && !camera_connecting.ref())
		{
			camera_connecting = Camera::add(0);

			const DisplayMode& display = Settings::display();
			camera_connecting.ref()->viewport =
			{
				Vec2::zero,
				Vec2(display.width, display.height),
			};
			camera_connecting.ref()->perspective((60.0f * PI * 0.5f / 180.0f), 0.1f, Game::level.far_plane_get());
			camera_connecting.ref()->mask = 0; // don't display anything; entities will be popping in over the network
			camera_connecting.ref()->flag(CameraFlagColors, false);
		}
		else if (!camera_needed && camera_connecting.ref())
		{
			camera_connecting.ref()->remove();
			camera_connecting = nullptr;
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
	Game::session.reset(SessionType::Story);
	Game::save.reset();
	Game::schedule_load_level(Asset::Level::Docks, Game::Mode::Special);
}

void title_multiplayer()
{
	clear();
	Game::session.reset(SessionType::Multiplayer);
	Game::save.reset();
	Game::schedule_load_level(Asset::Level::Docks, Game::Mode::Special);
}

void splash()
{
	clear();
	Game::session.reset(SessionType::Story);
	Game::save.reset();
	Game::schedule_load_level(Asset::Level::splash, Game::Mode::Special);
}

void draw_ui(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	const Rect2& viewport = params.camera->viewport;

#if !SERVER
	if (!Game::level.local)
	{
		// "connecting..."
		AssetID str;
		switch (Net::Client::mode())
		{
			case Net::Client::Mode::ContactingMaster:
			{
				str = strings::contacting_master;
				break;
			}
			case Net::Client::Mode::Connecting:
			{
				str = strings::connecting;
				break;
			}
			case Net::Client::Mode::Loading:
			{
				str = strings::loading;
				break;
			}
			default:
			{
				str = AssetNull;
				break;
			}
		}
		if (str != AssetNull)
			progress_infinite(params, _(str), viewport.size * 0.5f);
	}
#endif

	if (main_menu_state != State::Hidden)
		main_menu.draw_ui(params);

#if !SERVER
	if (Game::level.mode == Game::Mode::Special)
	{
		AssetID error_string = AssetNull;
		switch (Net::Client::master_error)
		{
			case Net::Client::MasterError::None:
				break;
			case Net::Client::MasterError::WrongVersion:
				error_string = strings::need_upgrade;
				break;
			case Net::Client::MasterError::Timeout:
				error_string = strings::master_timeout;
				break;
			default:
				vi_assert(false);
				break;
		}

		if (error_string != AssetNull)
		{
			UIText text;
			text.color = UI::color_alert();
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.wrap_width = MENU_ITEM_WIDTH;
			text.text(0, _(error_string));
			Vec2 pos = params.camera->viewport.size * Vec2(0.1f, 0.1f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(params, pos);
		}
	}
#endif

	// draw dialog box
	s32 gamepad = 0;
	{
		PlayerHuman* player = PlayerHuman::player_for_camera(params.camera);
		if (player)
			gamepad = player->gamepad;
	}
	if (dialog_callback[gamepad])
		dialog_layout(gamepad, &params);

	// text dialog box
	if (dialog_text_callback)
	{
		Vec2 field_size(MENU_ITEM_WIDTH * 1.5f, MENU_ITEM_HEIGHT);
		Rect2 field_rect =
		{
			(viewport.size * 0.5f) + (field_size * -0.5f),
			field_size
		};

		// container
		{
			r32 prompt_height = (MENU_ITEM_HEIGHT + MENU_ITEM_PADDING) * Ease::cubic_out<r32>(vi_min((Game::real_time.total - dialog_time[0]) / DIALOG_ANIM_TIME, 1.0f));
			Rect2 r =
			{
				field_rect.pos + Vec2(-MENU_ITEM_PADDING, -prompt_height),
				field_rect.size + Vec2(MENU_ITEM_PADDING * 2.0f, prompt_height + MENU_ITEM_HEIGHT),
			};
			UI::box(params, r.outset(MENU_ITEM_PADDING), UI::color_background);
			UI::border(params, r.outset(MENU_ITEM_PADDING), 2.0f, UI::color_accent());
		}

		UIText text;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Min;

		// prompt
		text.color = UI::color_default;
		text.text(0, dialog_string[0]);
		UIMenu::text_clip(&text, dialog_time[0], 100.0f);
		text.draw(params, field_rect.pos + Vec2(0, field_rect.size.y + MENU_ITEM_PADDING));
		text.clip = 0;

		if ((Game::real_time.total - dialog_time[0]) > DIALOG_ANIM_TIME)
		{
			// accept/cancel control prompts

			// accept
			Rect2 controls_rect = field_rect;
			controls_rect.pos.y -= MENU_ITEM_HEIGHT + MENU_ITEM_PADDING;

			text.wrap_width = 0;
			text.anchor_y = UIText::Anchor::Min;
			text.anchor_x = UIText::Anchor::Min;
			text.color = UI::color_accent();
			text.text(0, _(strings::prompt_accept_text));
			Vec2 prompt_pos = controls_rect.pos + Vec2(MENU_ITEM_PADDING);
			text.draw(params, prompt_pos);

			// cancel
			text.anchor_x = UIText::Anchor::Max;
			text.color = UI::color_alert();
			text.clip = 0;
			text.text(0, _(strings::prompt_cancel));
			text.draw(params, prompt_pos + Vec2(controls_rect.size.x + MENU_ITEM_PADDING * -2.0f, 0));
		}

		{
			// text field
			UI::border(params, field_rect, 2.0f, UI::color_accent());

			text.font = Asset::Font::pt_sans;
			text.anchor_x = UIText::Anchor::Min;
			text.color = UI::color_default;
			dialog_text_field.get(&text, 64);
			text.draw(params, field_rect.pos + Vec2(MENU_ITEM_PADDING * 0.8125f));
		}
	}
}

void draw_letterbox(const RenderParams& params, r32 t, r32 total)
{
	const Rect2& vp = params.camera->viewport;
	r32 blend = t > total * 0.5f
		? Ease::cubic_out<r32>(1.0f - (t - (total * 0.5f)) / (total * 0.5f))
		: Ease::cubic_in<r32>(t / (total * 0.5f));
	r32 size = vp.size.y * 0.5f * blend;
	UI::box(params, { Vec2::zero, Vec2(vp.size.x, size) }, UI::color_background);
	UI::box(params, { Vec2(0, vp.size.y - size), Vec2(vp.size.x, size) }, UI::color_background);
}

void settings_graphics_init();

// returns next state the menu should be in
State settings(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu)
{
	menu->start(u, origin, gamepad);
	b8 exit = menu->item(u, _(strings::back)) || (!Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	s32 delta;

	if ((gamepad > 0 || u.input->gamepads[0].type != Gamepad::Type::None) && menu->item(u, _(strings::settings_controls_gamepad)))
	{
		menu->end(u);
		return State::SettingsControlsGamepad;
	}

	if (gamepad == 0)
	{
#if !defined(__ORBIS__)
		if (menu->item(u, _(strings::settings_controls_keyboard)))
		{
			menu->end(u);
			return State::SettingsControlsKeyboard;
		}
#endif

		if (menu->item(u, _(strings::settings_graphics)))
		{
			menu->end(u);
			settings_graphics_init();
			return State::SettingsGraphics;
		}

		{
			sprintf(str, "%d", Settings::sfx);
			delta = menu->slider_item(u, _(strings::sfx), str);
			if (delta < 0)
				Settings::sfx = vi_max(0, Settings::sfx - 10);
			else if (delta > 0)
				Settings::sfx = vi_min(100, Settings::sfx + 10);
			if (delta != 0)
				Audio::param_global(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER * Audio::volume_scale);
		}

		{
			sprintf(str, "%d", Settings::music);
			delta = menu->slider_item(u, _(strings::music), str);
			if (delta < 0)
				Settings::music = vi_max(0, Settings::music - 10);
			else if (delta > 0)
				Settings::music = vi_min(100, Settings::music + 10);
			if (delta != 0)
				Audio::param_global(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER * Audio::volume_scale);
		}

		UIMenu::enum_option(&Settings::region, menu->slider_item(u, _(strings::region), _(region_string(Settings::region))));
	}

	menu->end(u);

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		menu->end(u);
		Loader::settings_save();
		return State::Visible;
	}

	return State::Settings;
}

// returns true if this menu should remain open
b8 settings_controls(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu, Gamepad::Type gamepad_type)
{
	menu->start(u, origin, gamepad, currently_editing_control == Controls::count);
	b8 exit = menu->item(u, _(strings::back)) || (currently_editing_control == Controls::count && !Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	s32 delta;

	{
		u16* sensitivity = gamepad_type == Gamepad::Type::None ? &Settings::gamepads[gamepad].sensitivity_mouse : &Settings::gamepads[gamepad].sensitivity_gamepad;
		sprintf(str, "%d", s32(*sensitivity));
		delta = menu->slider_item(u, _(strings::sensitivity), str);
		*sensitivity = vi_max(10, vi_min(1000, s32(*sensitivity) + delta * (*sensitivity >= (delta > 0 ? 150 : 151) ? 25 : 10)));
	}

	{
		b8* invert_y = &Settings::gamepads[gamepad].invert_y;
		sprintf(str, "%s", _(*invert_y ? strings::yes : strings::no));
		delta = menu->slider_item(u, _(strings::invert_y), str);
		if (delta != 0)
			*invert_y = !(*invert_y);
	}

	{
		b8* zoom_toggle = &Settings::gamepads[gamepad].zoom_toggle;
		sprintf(str, "%s", _(*zoom_toggle ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::zoom_toggle), str);
		if (delta != 0)
			*zoom_toggle = !(*zoom_toggle);
	}

	if (gamepad_type != Gamepad::Type::None)
	{
		b8* rumble = &Settings::gamepads[gamepad].rumble;
		sprintf(str, "%s", _(*rumble ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::rumble), str);
		if (delta != 0)
			*rumble = !(*rumble);
	}

	for (s32 i = 0; i < s32(Controls::count); i++)
	{
		if (Input::control_customizable(Controls(i), gamepad_type))
		{
			InputBinding* binding = &Settings::gamepads[gamepad].bindings[i];

			const char* string;
			b8 disabled;
			if (currently_editing_control == Controls::count)
			{
				disabled = false;
				string = binding->string(gamepad_type);
			}
			else
			{
				// currently editing one of the bindings
				if (currently_editing_control == Controls(i))
				{
					disabled = false;
					string = nullptr;
				}
				else
				{
					disabled = true;
					string = binding->string(gamepad_type);
				}
			}

			b8 selected = menu->item(u, Input::control_string(Controls(i)), string, disabled);

			if (currently_editing_control == Controls::count)
			{
				if (selected)
				{
					currently_editing_control = Controls(i);
					currently_editing_control_enable_input = false; // wait for the player to release keys
				}
			}
			else if (currently_editing_control == Controls(i))
			{
				if (gamepad_type == Gamepad::Type::None)
				{
					if (currently_editing_control_enable_input)
					{
						if (u.last_input->keys.any() && !u.input->keys.any())
						{
							binding->key1 = KeyCode(u.last_input->keys.start);
							currently_editing_control = Controls::count;
							currently_editing_control_enable_input = false;
						}
					}
					else if (!u.last_input->keys.any())
						currently_editing_control_enable_input = true;
				}
				else
				{
					if (currently_editing_control_enable_input)
					{
						if (u.last_input->gamepads[gamepad].btns && !u.input->gamepads[gamepad].btns)
						{
							s32 btns = u.last_input->gamepads[gamepad].btns;
							for (s32 j = 0; j < s32(Gamepad::Btn::count); j++)
							{
								if (btns & (1 << j))
								{
									binding->btn = Gamepad::Btn(j);
									break;
								}
							}
							currently_editing_control = Controls::count;
							currently_editing_control_enable_input = false;
							Game::cancel_event_eaten[gamepad] = true;
						}
					}
					else if (!u.last_input->gamepads[gamepad].btns)
						currently_editing_control_enable_input = true;
				}
			}
		}
	}

	menu->end(u);

	if (exit)
	{
		refresh_variables(*u.input);
		currently_editing_control = Controls::count;
		currently_editing_control_enable_input = false;
		Game::cancel_event_eaten[gamepad] = true;
		return false;
	}
	else
		return true;
}

void settings_graphics_cancel(s8)
{
	Settings::display_mode_index = display_mode_index = display_mode_index_last;
	Settings::fullscreen = display_mode_fullscreen = display_mode_fullscreen_last;
	Settings::vsync = display_mode_vsync = display_mode_vsync_last;
}

void settings_graphics_init()
{
	display_mode_index = display_mode_index_last = Settings::display_mode_index;
	display_mode_fullscreen = display_mode_fullscreen_last = Settings::fullscreen;
	display_mode_vsync = display_mode_vsync_last = Settings::vsync;
}

void settings_graphics_apply(s8)
{
	display_mode_index_last = display_mode_index;
	display_mode_fullscreen_last = display_mode_fullscreen;
	display_mode_vsync_last = display_mode_vsync;
}

// returns true if this menu should remain open
b8 settings_graphics(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu)
{
	menu->start(u, origin, gamepad);
	b8 exit = menu->item(u, _(strings::back)) || (!Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	const s32 MAX_ITEM = 128;
	char str[MAX_ITEM + 1];
	str[MAX_ITEM] = '\0';
	s32 delta;

#if !defined(__ORBIS__)
	{
		const DisplayMode& mode = Loader::display_modes[display_mode_index];
		snprintf(str, MAX_ITEM, "%dx%d", mode.width, mode.height);
		delta = menu->slider_item(u, _(strings::resolution), str);
		display_mode_index += delta;
		if (display_mode_index < 0)
			display_mode_index = Loader::display_modes.length - 1;
		else if (display_mode_index >= Loader::display_modes.length)
			display_mode_index = 0;
	}

	{
		delta = menu->slider_item(u, _(strings::fullscreen), _(display_mode_fullscreen ? strings::on : strings::off));
		if (delta != 0)
			display_mode_fullscreen = !display_mode_fullscreen;
	}

	{
		delta = menu->slider_item(u, _(strings::vsync), _(display_mode_vsync ? strings::on : strings::off));
		if (delta != 0)
			display_mode_vsync = !display_mode_vsync;
	}
#endif

	{
		u8* fov = &Settings::fov;
		snprintf(str, MAX_ITEM, "%d deg", s32(*fov));
		delta = menu->slider_item(u, _(strings::fov), str);
		*fov = u8(vi_max(40, vi_min(150, s32(*fov) + delta * 5)));
	}

	{
		b8* waypoints = &Settings::waypoints;
		delta = menu->slider_item(u, _(strings::waypoints), _(*waypoints ? strings::on : strings::off));
		if (delta != 0)
			*waypoints = !(*waypoints);
	}

	{
		b8* subtitles = &Settings::subtitles;
		delta = menu->slider_item(u, _(strings::subtitles), _(*subtitles ? strings::on : strings::off));
		if (delta != 0)
			*subtitles = !(*subtitles);
	}

	{
		s32* fps = &Settings::framerate_limit;
		snprintf(str, MAX_ITEM, "%d", *fps);
		delta = menu->slider_item(u, _(strings::framerate_limit), str);
		if (delta < 0)
			*fps = vi_max(30, (*fps) - 10);
		else if (delta > 0)
			*fps = vi_min(500, (*fps) + 10);
	}

	{
		AssetID value;
		switch (Settings::shadow_quality)
		{
			case Settings::ShadowQuality::Off:
			{
				value = strings::off;
				break;
			}
			case Settings::ShadowQuality::Medium:
			{
				value = strings::medium;
				break;
			}
			case Settings::ShadowQuality::High:
			{
				value = strings::high;
				break;
			}
			default:
			{
				value = AssetNull;
				vi_assert(false);
				break;
			}
		}
		UIMenu::enum_option(&Settings::shadow_quality, menu->slider_item(u, _(strings::shadow_quality), _(value)));
	}

	{
		b8* volumetric_lighting = &Settings::volumetric_lighting;
		delta = menu->slider_item(u, _(strings::volumetric_lighting), _(*volumetric_lighting ? strings::on : strings::off));
		if (delta != 0)
			*volumetric_lighting = !(*volumetric_lighting);
	}

	{
		b8* antialiasing = &Settings::antialiasing;
		delta = menu->slider_item(u, _(strings::antialiasing), _(*antialiasing ? strings::on : strings::off));
		if (delta != 0)
			*antialiasing = !(*antialiasing);
	}

	{
		b8* ssao = &Settings::ssao;
		delta = menu->slider_item(u, _(strings::ssao), _(*ssao ? strings::on : strings::off));
		if (delta != 0)
			*ssao = !(*ssao);
	}

	{
		b8* scan_lines = &Settings::scan_lines;
		delta = menu->slider_item(u, _(strings::scan_lines), _(*scan_lines ? strings::on : strings::off));
		if (delta != 0)
			*scan_lines = !(*scan_lines);
	}

	{
		b8* shell_casings = &Settings::shell_casings;
		delta = menu->slider_item(u, _(strings::shell_casings), _(*shell_casings ? strings::on : strings::off));
		if (delta != 0)
			*shell_casings = !(*shell_casings);
	}

	menu->end(u);

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		if (display_mode_index != Settings::display_mode_index
			|| display_mode_fullscreen != Settings::fullscreen
			|| display_mode_vsync != Settings::vsync)
		{
			Settings::display_mode_index = display_mode_index;
			Settings::fullscreen = display_mode_fullscreen;
			Settings::vsync = display_mode_vsync;
			dialog_with_time_limit(gamepad, settings_graphics_apply, settings_graphics_cancel, 10.0f, _(strings::prompt_resolution_apply));
		}
		return false;
	}

	return true;
}

void maps_skip_map_cancel(s8 gamepad)
{
	maps_selected_map = AssetNull;
}

void maps_skip_map(s8 gamepad)
{
	PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
	me->map_skip(maps_selected_map);
	maps_selected_map = AssetNull;
}

// returns true if map menu should stay open
b8 maps(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu)
{
	menu->start(u, origin, gamepad);

	b8 exit = menu->item(u, _(strings::back)) || (!Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	menu->text(u, _(strings::prompt_skip_map));

	PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();

	for (AssetID level_id = 0; level_id < AssetID(Asset::Level::count); level_id++)
	{
		if (Overworld::zone_max_teams(level_id) < Game::session.config.team_count)
			continue;

		b8 in_rotation = false;
		for (s32 j = 0; j < Game::session.config.levels.length; j++)
		{
			if (level_id == Overworld::zone_id_for_uuid(Game::session.config.levels[j]))
			{
				in_rotation = true;
				break;
			}
		}

		if (menu->selected == menu->items.length
			&& !u.input->get(Controls::UIContextAction, gamepad) && u.last_input->get(Controls::UIContextAction, gamepad))
		{
			// prompt to switch instantly
			maps_selected_map = level_id;
			Menu::dialog_with_cancel(gamepad, &maps_skip_map, &maps_skip_map_cancel, _(strings::confirm_skip_map), Loader::level_name(level_id));
		}

		if (menu->item(u, Loader::level_name(level_id), nullptr, level_id == Game::level.multiplayer_level_scheduled, in_rotation ? Asset::Mesh::icon_checkmark : AssetNull))
			me->map_schedule(level_id);
	}

	menu->end(u);

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		return false;
	}

	return true;
}

void teams_select_match_start_init(PlayerHuman* player)
{
	PlayerManager* manager = player->get<PlayerManager>();
	teams_selected_player[player->gamepad] = manager;
	manager->team_schedule(manager->team.ref()->team());
}

void teams_kick_player(s8 gamepad)
{
	PlayerManager* player = teams_selected_player[gamepad].ref();
	if (player)
	{
		PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
		me->kick(player);
	}
}

void teams_ban_player(s8 gamepad)
{
	PlayerManager* player = teams_selected_player[gamepad].ref();
	if (player)
	{
		PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
		me->ban(player);
	}
}

void teams_friend_add(s8 gamepad)
{
#if !SERVER
	PlayerManager* selected = teams_selected_player[gamepad].ref();
	if (selected)
		Net::Client::master_friend_add(selected->get<PlayerHuman>()->master_id);
#endif
}

void teams_friend_remove(s8 gamepad)
{
#if !SERVER
	PlayerManager* selected = teams_selected_player[gamepad].ref();
	if (selected)
		Net::Client::master_friend_remove(selected->get<PlayerHuman>()->master_id);
#endif
}

void teams_admin_set(s8 gamepad, b8 value)
{
#if !SERVER
	PlayerManager* selected = teams_selected_player[gamepad].ref();
	if (selected)
	{
		PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
		me->make_admin(selected, value);
	}
#endif
}

void teams_admin_make(s8 gamepad)
{
	teams_admin_set(gamepad, true);
}

void teams_admin_remove(s8 gamepad)
{
	teams_admin_set(gamepad, false);
}

void friendship_state(u32 friend_id, b8 state)
{
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		PlayerManager* player = teams_selected_player[i].ref();
		if (player && player->has<PlayerHuman>() && player->get<PlayerHuman>()->master_id == friend_id)
			teams_selected_player_friendship[i] = state ? FriendshipState::Friends : FriendshipState::NotFriends;
	}
}

b8 player(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu)
{
	PlayerManager* selected = teams_selected_player[gamepad].ref();
	if (!selected)
		return false;

	vi_assert(gamepad == 0 && selected->has<PlayerHuman>());

	PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();

	b8 exit = !Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad);

	menu->start(u, origin, gamepad);

	menu->text(u, selected->username);

	if (menu->item(u, _(strings::back)))
		exit = true;
	else
	{
		if (!Game::level.local && selected->get<PlayerHuman>()->gamepad == 0)
		{
			// add/remove friend
			FriendshipState friendship = teams_selected_player_friendship[gamepad];
			if (friendship == FriendshipState::None)
				menu->item(u, _(strings::loading), nullptr, true);
			else if (menu->item(u, _(friendship == FriendshipState::NotFriends ? strings::friend_add : strings::friend_remove)))
			{
				if (friendship == FriendshipState::NotFriends)
					Menu::dialog_with_cancel(gamepad, &teams_friend_add, nullptr, _(strings::confirm_friend_add), selected->username);
				else
					Menu::dialog_with_cancel(gamepad, &teams_friend_remove, nullptr, _(strings::confirm_friend_remove), selected->username);
			}

			// add/remove admin
			if (me->is_admin)
			{
				if (menu->item(u, _(selected->is_admin ? strings::admin_remove : strings::admin_make)))
				{
					if (selected->is_admin)
						Menu::dialog_with_cancel(gamepad, &teams_admin_remove, nullptr, _(strings::confirm_admin_remove), selected->username);
					else
						Menu::dialog_with_cancel(gamepad, &teams_admin_make, nullptr, _(strings::confirm_admin_make), selected->username);
				}
			}
		}

		// kick
		if (me->is_admin && menu->item(u, _(strings::kick)))
			Menu::dialog_with_cancel(gamepad, &teams_kick_player, nullptr, _(strings::confirm_kick), selected->username);

		// ban
		if (me->is_admin && menu->item(u, _(strings::ban)))
			Menu::dialog_with_cancel(gamepad, &teams_ban_player, nullptr, _(strings::confirm_ban), selected->username);
	}

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		teams_selected_player[gamepad] = nullptr;
	}

	return true;
}

// returns true if the team menu should stay open
State teams(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu, TeamSelectMode mode, EnableInput input)
{
	PlayerManager* me = PlayerHuman::player_for_gamepad(gamepad)->get<PlayerManager>();
	PlayerManager* selected = teams_selected_player[gamepad].ref();

	b8 exit = !Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad);

	menu->start(u, origin, gamepad, mode == TeamSelectMode::Normal && !selected && input == EnableInput::Yes); // can select different players in Normal mode when no player is selected
	
	if (mode == TeamSelectMode::Normal)
	{
		if (menu->item(u, _(strings::back)))
			exit = true;

		if (me->get<PlayerHuman>()->gamepad == 0)
			menu->text(u, _(strings::prompt_options));
	}

	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		const char* value = _(Team::name_selector(i.item()->team_scheduled == AI::TeamNone ? i.item()->team.ref()->team() : i.item()->team_scheduled));
		b8 disabled = input == EnableInput::No || (selected && i.item() != selected) || (i.item() != me && mode == TeamSelectMode::MatchStart);
		AssetID icon = i.item()->can_spawn ? Asset::Mesh::icon_checkmark : AssetNull;

		if (mode == TeamSelectMode::Normal
			&& input == EnableInput::Yes
			&& teams_selected_player[gamepad].ref() == nullptr
			&& menu->selected == menu->items.length && me != i.item() && i.item()->has<PlayerHuman>()
			&& !u.input->get(Controls::UIContextAction, gamepad) && u.last_input->get(Controls::UIContextAction, gamepad))
		{
			// open context menu
			teams_selected_player[gamepad] = i.item();
			teams_selected_player_friendship[gamepad] = FriendshipState::None;
#if !SERVER
			Net::Client::master_friendship_get(i.item()->get<PlayerHuman>()->master_id);
#endif
			menu->end(u);
			return State::Player;
		}

		if (i.item() == selected)
		{
			// player selected; we can switch their team
			menu->selected = menu->items.length; // make sure the menu knows which player we have selected, in case players change
			s32 delta = menu->slider_item(u, i.item()->username, value, disabled, icon, UIMenu::SliderItemAllowSelect::Yes);
			if (i.item()->team_scheduled != AI::TeamNone)
			{
				if (delta == INT_MIN // slider item was clicked
					|| (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad)))
				{
					i.item()->set_can_spawn(true);
					teams_selected_player[gamepad] = nullptr;
				}
				else if (delta != 0)
				{
					AI::Team team_new = i.item()->team_scheduled + delta;
					if (team_new < 0)
						team_new = Team::list.count() - 1;
					else if (team_new >= Team::list.count())
						team_new = 0;
					i.item()->team_schedule(team_new);
				}
			}
		}
		else
		{
			if (mode == TeamSelectMode::MatchStart && i.item() == me)
				menu->selected = menu->items.length; // always have the cursor on me in MatchStart mode

			if (menu->item(u, i.item()->username, value, disabled, icon))
			{
				if (Game::session.type == SessionType::Multiplayer
					&& (i.item() == me || mode == TeamSelectMode::Normal)
					&& (Game::session.config.game_type == GameType::Assault || Game::session.config.max_players > Game::session.config.team_count)) // disallow team switching in FFA
				{
					// we are selecting this player
					teams_selected_player[gamepad] = i.item();
					i.item()->team_schedule(i.item()->team.ref()->team());
					if (Team::match_state == Team::MatchState::TeamSelect)
						i.item()->set_can_spawn(false);
				}
			}
		}
	}

	menu->end(u);

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		if (selected)
		{
			selected->team_schedule(AI::TeamNone);
			teams_selected_player[gamepad] = nullptr;
		}
		else
		{
			if (mode == TeamSelectMode::MatchStart && me->can_spawn)
				me->set_can_spawn(false);
			else
				return State::Visible;
		}
	}

	return State::Teams; // stay open
}

// returns true if menu should stay open
b8 choose_region(const Update& u, const UIMenu::Origin& origin, s8 gamepad, UIMenu* menu, AllowClose allow_close)
{
	menu->start(u, origin, 0);

	menu->text(u, _(strings::choose_region));

	b8 cancel = u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0];
	if (allow_close == AllowClose::Yes && (cancel || menu->item(u, _(strings::back))))
	{
		Game::cancel_event_eaten[0] = true;
		menu->end(u);
		return false;
	}

	for (s32 i = 0; i < s32(Region::count); i++)
	{
		Region region = Region(i);
		if (menu->item(u, _(region_string(region)), nullptr, region == Settings::region, region == Settings::region ? Asset::Mesh::icon_checkmark : AssetNull))
		{
			Settings::region = region;
			Loader::settings_save();
			menu->end(u);
			return false;
		}
	}

	menu->end(u);
	return true;
}
#endif

}

UIMenu* UIMenu::active[MAX_GAMEPADS];

UIMenu::UIMenu()
	: selected(),
	items(),
	animation_time(),
	scroll(),
	allow_select(true)
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

void UIMenu::start(const Update& u, const Origin& o, s8 g, b8 input)
{
	clear();

	origin = o;

	gamepad = g;

	if (Console::visible || Menu::dialog_active(gamepad))
		return;

	if (active[g])
	{
		if (active[g] != this)
			return;
	}
	else
		active[g] = this;

	allow_select = input;
	if (input)
	{
		s32 delta = UI::input_delta_vertical(u, gamepad);
		if (delta != 0)
		{
			Audio::post_global(AK::EVENTS::PLAY_MENU_MOVE);
			selected += delta;
		}
	}
}

Vec2 screen_origin(const UIMenu& menu)
{
	Vec2 pos = menu.origin.pos + Vec2(MENU_ITEM_PADDING_LEFT, 0);
	switch (menu.origin.anchor_x)
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

	switch (menu.origin.anchor_y)
	{
		case UIText::Anchor::Center:
		{
			pos.y += menu.cached_height * 0.5f;
			break;
		}
		case UIText::Anchor::Min:
		{
			pos.y += menu.cached_height;
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

	return pos;
}

Rect2 item_rect(const UIMenu& menu, s32 i)
{
	Vec2 pos = screen_origin(menu);
	return
	{
		Vec2(pos.x - MENU_ITEM_PADDING_LEFT, pos.y - (MENU_ITEM_FONT_SIZE * UI::scale) - MENU_ITEM_PADDING - (i - menu.scroll.top()) * MENU_ITEM_HEIGHT),
		Vec2(MENU_ITEM_WIDTH, (MENU_ITEM_FONT_SIZE * UI::scale) + MENU_ITEM_PADDING * 2.0f),
	};
}

Rect2 item_slider_down_rect(const UIMenu& menu, s32 i)
{
	Rect2 r = item_rect(menu, i);
	r.pos.x += MENU_ITEM_PADDING_LEFT + MENU_ITEM_WIDTH * 0.5f;
	r.size.x = r.size.y;
	return r;
}

Rect2 item_slider_up_rect(const UIMenu& menu, s32 i)
{
	Rect2 r = item_rect(menu, i);
	r.pos.x += r.size.x - r.size.y;
	r.size.x = r.size.y;
	return r;
}

b8 UIMenu::add_item(Item::Type type, const char* string, const char* value, b8 disabled, AssetID icon)
{
	Item* item = items.add();
	item->icon = icon;
	item->type = type;
	item->label.size = item->value.size = MENU_ITEM_FONT_SIZE;
	if (value)
		item->label.wrap_width = MENU_ITEM_VALUE_OFFSET - MENU_ITEM_PADDING - MENU_ITEM_PADDING_LEFT;
	else
		item->label.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING - MENU_ITEM_PADDING_LEFT;
	item->label.anchor_x = UIText::Anchor::Min;
	item->label.anchor_y = item->value.anchor_y = UIText::Anchor::Min;

	b8 is_selected = active[gamepad] == this && selected == items.length - 1;
	item->label.color = item->value.color = (disabled || active[gamepad] != this) ? UI::color_disabled() : (is_selected ? UI::color_accent() : UI::color_default);
	item->label.text(gamepad, string);
	text_clip(&item->label, animation_time + (items.length - 1 - scroll.pos) * 0.1f, 100.0f);

	item->value.anchor_x = UIText::Anchor::Center;
	item->value.text(gamepad, value);

	if (!scroll.visible(items.length - 1))
		return false;

	return true;
}

b8 check_mouse_select(UIMenu* menu, const Update& u, s32 item)
{
	if (item_rect(*menu, item).contains(UI::cursor_pos))
	{
		if (menu->allow_select)
			menu->selected = item;
		return menu->selected == item;
	}
	return false;
}

b8 UIMenu::text(const Update& u, const char* string, const char* value, b8 disabled, AssetID icon)
{
	if (selected == items.length)
	{
		// this item can never be selected
		s32 delta = UI::input_delta_vertical(u, gamepad);
		if (delta == 0)
			delta = items.length > 0 ? -1 : 1;
		selected += delta;
	}

	if (!add_item(Item::Type::Text, string, value, disabled, icon))
		return false;
	return true;
}

b8 UIMenu::item(const Update& u, const char* string, const char* value, b8 disabled, AssetID icon)
{
	if (!add_item(Item::Type::Button, string, value, disabled, icon))
		return false;

	if (Console::visible || active[gamepad] != this || Menu::dialog_active(gamepad))
		return false;

	b8 mouse_selected = false;
	if (gamepad == 0 && Game::ui_gamepad_types[0] == Gamepad::Type::None)
		mouse_selected = check_mouse_select(this, u, items.length - 1);

	if (selected == items.length - 1
		&& Game::time.total > 0.35f
		&& !Console::visible
		&& !disabled)
	{
		if ((mouse_selected && u.last_input->keys.get(s32(KeyCode::MouseLeft)))
			|| u.input->get(Controls::Interact, gamepad))
		{
			// show that the user is getting ready to activate this item
			items[selected].label.color = UI::color_alert();
		}

		if ((mouse_selected && u.last_input->keys.get(s32(KeyCode::MouseLeft)) && !u.input->keys.get(s32(KeyCode::MouseLeft)))
			|| (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad)))
		{
			Audio::post_global(AK::EVENTS::PLAY_MENU_SELECT);
			return true;
		}
	}
	
	return false;
}

s32 UIMenu::slider_item(const Update& u, const char* label, const char* value, b8 disabled, AssetID icon, SliderItemAllowSelect allow_select)
{
	if (!add_item(Item::Type::Slider, label, value, disabled, icon))
		return 0;

	if (Console::visible || active[gamepad] != this)
		return 0;

	if (disabled)
		return 0;

	b8 mouse_selected = false;
	if (gamepad == 0 && Game::ui_gamepad_types[0] == Gamepad::Type::None)
		mouse_selected = check_mouse_select(this, u, items.length - 1);

	if (selected == items.length - 1
		&& Game::time.total > 0.5f)
	{
		s32 delta = UI::input_delta_horizontal(u, gamepad);

		if (mouse_selected)
		{
			if (item_slider_down_rect(*this, items.length - 1).contains(UI::cursor_pos))
			{
				if (u.input->keys.get(s32(KeyCode::MouseLeft))) // show that the user is getting ready to alter this slider
					items[items.length - 1].value.color = UI::color_alert();
				else if (u.last_input->keys.get(s32(KeyCode::MouseLeft)))
					delta = -1;
			}
			else if (item_slider_up_rect(*this, items.length - 1).contains(UI::cursor_pos))
			{
				if (u.input->keys.get(s32(KeyCode::MouseLeft))) // show that the user is getting ready to alter this slider
					items[items.length - 1].value.color = UI::color_alert();
				else if (u.last_input->keys.get(s32(KeyCode::MouseLeft)))
					delta = 1;
			}
			else if (allow_select == SliderItemAllowSelect::Yes)
			{
				if (u.input->keys.get(s32(KeyCode::MouseLeft))) // show that the user is getting ready to select this slider
					items[items.length - 1].label.color = UI::color_alert();
				else if (u.last_input->keys.get(s32(KeyCode::MouseLeft)))
					delta = INT_MIN;
			}
		}

		if (delta != 0)
			Audio::post_global(AK::EVENTS::PLAY_MENU_ALTER);
		return delta;
	}
	
	return 0;
}

void UIMenu::end(const Update& u)
{
	if (allow_select && gamepad == 0)
	{
		if (u.input->keys.get(s32(KeyCode::MouseWheelUp)))
			scroll.pos = vi_max(0, scroll.pos - 1);
		else if (u.input->keys.get(s32(KeyCode::MouseWheelDown)))
			scroll.pos++;
	}

	scroll.update_menu(items.length);

	if (selected < 0)
		selected = items.length - 1;
	if (selected >= items.length)
		selected = 0;

	// make sure we don't have a text item selected
	if (items.length > 0)
	{
		while (selected > 0 && items[selected].type == Item::Type::Text)
			selected--;
	}

	scroll.scroll_into_view(selected);

	cached_height = height();
}

r32 UIMenu::height() const
{
	return (vi_min(items.length, scroll.size) * MENU_ITEM_HEIGHT) - MENU_ITEM_PADDING * 2.0f;
}

void UIMenu::text_clip_timer(UIText* text, r32 timer, r32 speed, s32 max)
{
	r32 clip = timer * speed;
	text->clip = vi_max(1, s32(clip));
	if (max > 0)
		text->clip = vi_min(text->clip, max);
		
	s32 mod = speed < 40.0f ? 1 : (speed < 100.0f ? 2 : 3);
		
	if ((text->clip < max || max == 0)
		&& text->clip % mod == 0
		&& s32(clip - Game::real_time.delta * speed) < s32(clip)
		&& text->clipped()
		&& text->rendered_string[text->clip] != ' '
		&& text->rendered_string[text->clip] != '\t'
		&& text->rendered_string[text->clip] != '\n')
	{
		Audio::post_global(AK::EVENTS::PLAY_CONSOLE_KEY);
	}
}

void UIMenu::text_clip(UIText* text, r32 start_time, r32 speed, s32 max)
{
	text_clip_timer(text, Game::real_time.total - start_time, speed, max);
}

const UIMenu::Item* UIMenu::last_visible_item() const
{
	if (items.length > 0)
		return &items[scroll.bottom(items.length) - 1];
	else
		return nullptr;
}

Rect2 UIMenu::rect(r32 height_scale) const
{
	r32 h = cached_height * height_scale;
	Vec2 pos = screen_origin(*this);
	return
	{
		Vec2(pos.x - MENU_ITEM_PADDING_LEFT, pos.y - h - MENU_ITEM_PADDING * 1.5f),
		Vec2(MENU_ITEM_WIDTH, h + MENU_ITEM_PADDING * 3.0f)
	};
}

void UIMenu::draw_ui(const RenderParams& params) const
{
	if (items.length == 0)
		return;

	b8 scroll_started = false;

	Rect2 last_item_rect;

	if (items.length > 0) // draw background
		UI::box(params, rect(Ease::cubic_out<r32>(vi_min((Game::real_time.total - animation_time) / 0.25f, 1.0f))), UI::color_background);

	Rect2 rect;
	for (s32 i = scroll.top(); i < scroll.bottom(items.length); i++)
	{
		const Item& item = items[i];

		rect = item_rect(*this, i);

		if (!scroll_started)
		{
			scroll.start(params, rect.pos + Vec2(rect.size.x * 0.5f, rect.size.y));
			scroll_started = true;
		}

		if (active[gamepad] == this && i == selected)
			UI::box(params, { rect.pos + Vec2(0, MENU_ITEM_PADDING), Vec2(4 * UI::scale, item.label.size * UI::scale) }, item.label.color);

		if (item.icon != AssetNull)
			UI::mesh(params, item.icon, rect.pos + Vec2(MENU_ITEM_PADDING_LEFT * 0.5f, MENU_ITEM_PADDING + MENU_ITEM_FONT_SIZE * 0.5f), Vec2(UI::scale * MENU_ITEM_FONT_SIZE), item.label.color);

		item.label.draw(params, rect.pos + Vec2(MENU_ITEM_PADDING_LEFT, MENU_ITEM_PADDING));

		r32 value_offset_time = (2 + vi_min(i, 6)) * 0.06f;
		if (Game::real_time.total - animation_time > value_offset_time)
		{
			if (item.value.has_text())
				item.value.draw(params, rect.pos + Vec2(MENU_ITEM_PADDING_LEFT + MENU_ITEM_VALUE_OFFSET, MENU_ITEM_PADDING));
			if (item.type == Item::Type::Slider)
			{
				{
					Rect2 down_rect = item_slider_down_rect(*this, i);
					UI::triangle(params, { down_rect.pos + down_rect.size * 0.5f, down_rect.size * 0.5f }, item.value.color, PI * 0.5f);
				}

				{
					Rect2 up_rect = item_slider_up_rect(*this, i);
					UI::triangle(params, { up_rect.pos + up_rect.size * 0.5f, up_rect.size * 0.5f }, item.value.color, PI * -0.5f);
				}
			}
		}
	}

	if (scroll_started)
		scroll.end(params, rect.pos + Vec2(rect.size.x * 0.5f, 0));
}


}
