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
#include "scripts.h"
#include "overworld.h"
#include "ease.h"
#include "data/components.h"
#include "team.h"

namespace VI
{

namespace Menu
{

State main_menu_state;
DialogCallback dialog_callback[MAX_GAMEPADS];
DialogCallback dialog_cancel_callback[MAX_GAMEPADS];
DialogCallback dialog_callback_last[MAX_GAMEPADS];
r32 dialog_time[MAX_GAMEPADS];
char dialog_string[MAX_GAMEPADS][255];
r32 dialog_time_limit[MAX_GAMEPADS];
Ref<Camera> camera_connecting;
Controls currently_editing_control = Controls::count;
b8 currently_editing_control_enable_input; // should we be listening for any and all button presses to apply to the control binding we're currently editing?

#define DIALOG_ANIM_TIME 0.25f

// default callback
void dialog_no_action(s8 gamepad)
{
}

State settings(const Update&, s8, UIMenu*);
b8 settings_controls(const Update&, s8, UIMenu*, Gamepad::Type);
b8 settings_graphics(const Update&, s8, UIMenu*);

#if SERVER

void init(const InputState&) {}
void update(const Update&) {}
void update_end(const Update&) {}
void clear() {}
void draw_ui(const RenderParams&) {}
void title() {}
void show() {}
void refresh_variables(const InputState&) {}
void pause_menu(const Update&, s8, UIMenu*, State*) {}
void draw_letterbox(const RenderParams&, r32, r32) {}
State settings(const Update&, s8, UIMenu*) { return State::Settings; }
b8 settings_controls(const Update&, s8, UIMenu*, Gamepad::Type) { return true; }
b8 settings_graphics(const Update&, s8, UIMenu*) { return true; }
void progress_spinner(const RenderParams&, const Vec2&, r32) {}
void progress_bar(const RenderParams&, const char*, r32, const Vec2&) {}
void progress_infinite(const RenderParams&, const char*, const Vec2&) {}
void dialog(s8, DialogCallback, const char*, ...) {}
void dialog_with_cancel(s8, DialogCallback, DialogCallback, const char*, ...) {}
void dialog_with_time_limit(s8, DialogCallback, r32, const char*, ...) {}
b8 dialog_active(s8) { return false; }

#else

State* quit_menu_state;

void quit_to_overworld(s8 gamepad)
{
	Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
	if (quit_menu_state)
		*quit_menu_state = State::Hidden;
	quit_menu_state = nullptr;
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
	return dialog_callback[gamepad] || dialog_callback_last[gamepad];
}

void dialog(s8 gamepad, DialogCallback callback, const char* format, ...)
{
	Audio::post_global_event(AK::EVENTS::PLAY_DIALOG_ALERT);
	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], 254, format, args);
#else
	vsnprintf(dialog_string[gamepad], 254, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = nullptr;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = 0.0f;
}

void dialog_with_cancel(s8 gamepad, DialogCallback callback, DialogCallback cancel_callback, const char* format, ...)
{
	Audio::post_global_event(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], 254, format, args);
#else
	vsnprintf(dialog_string[gamepad], 254, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = cancel_callback;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = 0.0f;
}

void dialog_with_time_limit(s8 gamepad, DialogCallback callback, r32 limit, const char* format, ...)
{
	Audio::post_global_event(AK::EVENTS::PLAY_DIALOG_SHOW);

	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(dialog_string[gamepad], 254, format, args);
#else
	vsnprintf(dialog_string[gamepad], 254, format, args);
#endif

	va_end(args);

	dialog_callback[gamepad] = callback;
	dialog_cancel_callback[gamepad] = nullptr;
	dialog_time[gamepad] = Game::real_time.total;
	dialog_time_limit[gamepad] = limit;
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
		UIText::variable_add(i, "Start", gamepad.bindings[s32(Controls::Start)].string(type));
		UIText::variable_add(i, "Cancel", gamepad.bindings[s32(Controls::Cancel)].string(type));

		UIText::variable_add(i, "Primary", gamepad.bindings[s32(Controls::Primary)].string(type));
		UIText::variable_add(i, "Zoom", gamepad.bindings[s32(Controls::Zoom)].string(type));
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
		UIText::variable_add(i, "Ability1", gamepad.bindings[s32(Controls::Ability1)].string(type));
		UIText::variable_add(i, "Ability2", gamepad.bindings[s32(Controls::Ability2)].string(type));
		UIText::variable_add(i, "Ability3", gamepad.bindings[s32(Controls::Ability3)].string(type));
		UIText::variable_add(i, "Interact", gamepad.bindings[s32(Controls::Interact)].string(type));
		UIText::variable_add(i, "InteractSecondary", gamepad.bindings[s32(Controls::InteractSecondary)].string(type));
		UIText::variable_add(i, "Scoreboard", gamepad.bindings[s32(Controls::Scoreboard)].string(type));
		UIText::variable_add(i, "Jump", gamepad.bindings[s32(Controls::Jump)].string(type));
		UIText::variable_add(i, "Parkour", gamepad.bindings[s32(Controls::Parkour)].string(type));
		UIText::variable_add(i, "UIContextAction", gamepad.bindings[s32(Controls::UIContextAction)].string(type));
		UIText::variable_add(i, "UIAcceptText", gamepad.bindings[s32(Controls::UIAcceptText)].string(type));
		UIText::variable_add(i, "TabLeft", gamepad.bindings[s32(Controls::TabLeft)].string(type));
		UIText::variable_add(i, "TabRight", gamepad.bindings[s32(Controls::TabRight)].string(type));
	}
}

void init(const InputState& input)
{
	refresh_variables(input);

	title();
}

void clear()
{
	main_menu_state = State::Hidden;
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		dialog_callback[i] = nullptr;
		dialog_cancel_callback[i] = nullptr;
	}
}

void exit(s8 gamepad)
{
	Game::quit = true;
}

void title_menu(const Update& u, s8 gamepad, UIMenu* menu, State* state)
{
	switch (*state)
	{
		case State::Hidden:
		{
			break;
		}
		case State::Visible:
		{
			menu->start(u, 0);
			if (menu->item(u, _(strings::play)))
			{
				Scripts::Docks::play();
				clear();
			}
			if (menu->item(u, _(strings::splitscreen)))
			{
				Game::save.reset();
				Game::session.reset();
				Game::session.type = SessionType::Multiplayer;
				Game::session.config.game_type = GameType::Assault;
				Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
				clear();
			}
			if (menu->item(u, _(strings::settings)))
			{
				if (gamepad == 0)
					*state = State::Settings;
				else
					*state = State::SettingsControlsGamepad; // other players don't have any other settings
				menu->animate();
			}
			if (menu->item(u, _(strings::exit)))
				dialog(0, &exit, _(strings::confirm_quit));
			menu->end();
			break;
		}
		case State::Settings:
		{
			State s = settings(u, 0, menu);
			if (s != *state)
			{
				*state = s;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsKeyboard:
		{
			if (!settings_controls(u, gamepad, menu, Gamepad::Type::None))
			{
				*state = State::Settings;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsGamepad:
		{
			if (!settings_controls(u, gamepad, menu, u.input->gamepads[gamepad].type))
			{
				if (gamepad == 0)
					*state = State::Settings;
				else // other players don't have any other settings
					*state = State::Visible;
				menu->animate();
			}
			break;
		}
		case State::SettingsGraphics:
		{
			if (!settings_graphics(u, gamepad, menu))
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

void pause_menu(const Update& u, s8 gamepad, UIMenu* menu, State* state)
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
			menu->start(u, gamepad);
			if (menu->item(u, _(strings::close)))
				*state = State::Hidden;
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
				if (Game::session.type == SessionType::Story || Game::level.id == Asset::Level::overworld)
					dialog(gamepad, &quit_to_title, _(strings::confirm_quit));
				else
					dialog(gamepad, &quit_to_overworld, _(strings::confirm_quit));
			}
			menu->end();
			break;
		}
		case State::Settings:
		{
			State s = settings(u, gamepad, menu);
			if (s != *state)
			{
				*state = s;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsKeyboard:
		{
			if (!settings_controls(u, gamepad, menu, Gamepad::Type::None))
			{
				*state = State::Settings;
				menu->animate();
			}
			break;
		}
		case State::SettingsControlsGamepad:
		{
			if (!settings_controls(u, gamepad, menu, u.input->gamepads[gamepad].type))
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
			if (!settings_graphics(u, gamepad, menu))
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
			title();
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
					{
						Game::scheduled_dialog = strings::connection_failed;
						break;
					}
					case Net::DisconnectReason::ServerFull:
					{
						Game::scheduled_dialog = strings::server_full;
						break;
					}
					case Net::DisconnectReason::ServerResetting:
					{
						Game::scheduled_dialog = strings::server_resetting;
						break;
					}
					case Net::DisconnectReason::WrongVersion:
					{
						Game::scheduled_dialog = strings::need_upgrade;
						break;
					}
					case Net::DisconnectReason::AuthFailed:
					{
						Game::scheduled_dialog = strings::auth_failed;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
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

	// dialog
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		if (dialog_time_limit[i] > 0.0f)
		{
			dialog_time_limit[i] = vi_max(0.0f, dialog_time_limit[i] - u.time.delta);
			if (dialog_time_limit[i] == 0.0f)
				dialog_callback[i] = nullptr; // cancel
		}

		// dialog buttons
		if (dialog_callback[i] && dialog_callback_last[i]) // make sure we don't trigger the button on the first frame the dialog is shown
		{
			if (u.last_input->get(Controls::Interact, i) && !u.input->get(Controls::Interact, i))
			{
				// accept
				Audio::post_global_event(AK::EVENTS::PLAY_DIALOG_ACCEPT);
				DialogCallback callback = dialog_callback[i];
				dialog_callback[i] = nullptr;
				dialog_cancel_callback[i] = nullptr;
				callback(s8(i));
			}
			else if (!Game::cancel_event_eaten[i] && u.last_input->get(Controls::Cancel, i) && !u.input->get(Controls::Cancel, i))
			{
				// cancel
				Audio::post_global_event(AK::EVENTS::PLAY_DIALOG_CANCEL);
				DialogCallback cancel_callback = dialog_cancel_callback[i];
				dialog_callback[i] = nullptr;
				dialog_cancel_callback[i] = nullptr;
				Game::cancel_event_eaten[i] = true;
				if (cancel_callback)
					cancel_callback(s8(i));
			}
		}
	}

	if (Game::level.id == Asset::Level::Docks && Game::level.mode == Game::Mode::Special)
		title_menu(u, 0, &main_menu, &main_menu_state);
	else if (Overworld::active())
	{
		// do pause menu
		if (main_menu_state == State::Visible
			&& !Game::cancel_event_eaten[0]
			&& Game::time.total > 0.0f
			&& ((u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))
				|| (u.input->get(Controls::Cancel, 0) && !u.last_input->get(Controls::Cancel, 0))))
		{
			Game::cancel_event_eaten[0] = true;
			main_menu_state = State::Hidden;
			main_menu.clear();
		}
		else
			pause_menu(u, 0, &main_menu, &main_menu_state);
	}
}

void update_end(const Update& u)
{
	// reset cancel event eaten flags
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

			camera_connecting.ref()->viewport =
			{
				Vec2::zero,
				Vec2(u.input->width, u.input->height),
			};
			r32 aspect = camera_connecting.ref()->viewport.size.y == 0 ? 1 : camera_connecting.ref()->viewport.size.x / camera_connecting.ref()->viewport.size.y;
			camera_connecting.ref()->perspective((60.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);
			camera_connecting.ref()->mask = 0; // don't display anything; entities will be popping in over the network
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
	Game::session.reset();
	Game::save.reset();
	Game::schedule_load_level(Asset::Level::Docks, Game::Mode::Special);
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
	{
		if (Game::level.id == Asset::Level::Docks && Game::level.mode == Game::Mode::Special)
		{
			main_menu.draw_ui(params, Vec2(viewport.size.x * 0.5f, viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f), UIText::Anchor::Center, UIText::Anchor::Max);
#if !SERVER
			AssetID error_string = AssetNull;
			switch (Net::Client::master_error)
			{
				case Net::Client::MasterError::None:
				{
					break;
				}
				case Net::Client::MasterError::WrongVersion:
				{
					error_string = strings::need_upgrade;
					break;
				}
				case Net::Client::MasterError::Timeout:
				{
					error_string = strings::master_timeout;
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
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
#endif
		}
		else
			main_menu.draw_ui(params, Vec2(0, viewport.size.y * 0.5f), UIText::Anchor::Min, UIText::Anchor::Center);
	}

	// draw dialog box
	s32 gamepad = 0;
	{
		PlayerHuman* player = PlayerHuman::player_for_camera(params.camera);
		if (player)
			gamepad = player->gamepad;
	}
	if (dialog_callback[gamepad])
	{
		const r32 padding = 16.0f * UI::scale;
		UIText text;
		text.color = UI::color_default;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.text(gamepad, dialog_string[gamepad]);
		UIMenu::text_clip(&text, dialog_time[gamepad], 150.0f);
		Vec2 pos = params.camera->viewport.size * 0.5f;
		Rect2 text_rect = text.rect(pos).outset(padding);

		{
			r32 prompt_height = (padding + UI_TEXT_SIZE_DEFAULT * UI::scale) * Ease::cubic_out<r32>(vi_min((Game::real_time.total - dialog_time[gamepad]) / DIALOG_ANIM_TIME, 1.0f));
			text_rect.pos.y -= prompt_height;
			text_rect.size.y += prompt_height;
		}

		UI::box(params, text_rect, UI::color_background);
		UI::border(params, text_rect, 2.0f, UI::color_accent());

		text.draw(params, pos);

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
			text.draw(params, prompt_pos);

			if (dialog_callback[gamepad] != &dialog_no_action)
			{
				// cancel
				text.anchor_x = UIText::Anchor::Max;
				text.color = UI::color_alert();
				text.clip = 0;
				text.text(gamepad, _(strings::prompt_cancel));
				text.draw(params, prompt_pos + Vec2(text_rect.size.x + padding * -2.0f, 0));
			}
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

// returns next state the menu should be in
State settings(const Update& u, s8 gamepad, UIMenu* menu)
{
	menu->start(u, gamepad);
	b8 exit = menu->item(u, _(strings::back)) || (!Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	s32 delta;

	if ((gamepad > 0 || u.input->gamepads[0].type != Gamepad::Type::None) && menu->item(u, _(strings::settings_controls_gamepad)))
	{
		menu->end();
		return State::SettingsControlsGamepad;
	}

	if (gamepad == 0)
	{
#if !defined(__ORBIS__)
		if (menu->item(u, _(strings::settings_controls_keyboard)))
		{
			menu->end();
			return State::SettingsControlsKeyboard;
		}
#endif

		if (menu->item(u, _(strings::settings_graphics)))
		{
			menu->end();
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
				Audio::global_param(AK::GAME_PARAMETERS::VOLUME_SFX, r32(Settings::sfx) * VOLUME_MULTIPLIER);
		}

		{
			sprintf(str, "%d", Settings::music);
			delta = menu->slider_item(u, _(strings::music), str);
			if (delta < 0)
				Settings::music = vi_max(0, Settings::music - 10);
			else if (delta > 0)
				Settings::music = vi_min(100, Settings::music + 10);
			if (delta != 0)
				Audio::global_param(AK::GAME_PARAMETERS::VOLUME_MUSIC, r32(Settings::music) * VOLUME_MULTIPLIER);
		}
	}

	menu->end();

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
		menu->end();
		Loader::settings_save();
		return State::Visible;
	}

	return State::Settings;
}

// returns true if this menu should remain open
b8 settings_controls(const Update& u, s8 gamepad, UIMenu* menu, Gamepad::Type gamepad_type)
{
	menu->start(u, gamepad, currently_editing_control == Controls::count);
	b8 exit = menu->item(u, _(strings::back)) || (currently_editing_control == Controls::count && !Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	s32 delta;

	{
		u8* sensitivity = &Settings::gamepads[gamepad].sensitivity;
		sprintf(str, "%u", *sensitivity);
		delta = menu->slider_item(u, _(strings::sensitivity), str);
		if (delta < 0)
			*sensitivity = vi_max(10, (s32)(*sensitivity) - 10);
		else if (delta > 0)
			*sensitivity = vi_min(250, (s32)(*sensitivity) + 10);
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

	menu->end();

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

// returns true if this menu should remain open
b8 settings_graphics(const Update& u, s8 gamepad, UIMenu* menu)
{
	menu->start(u, gamepad);
	b8 exit = menu->item(u, _(strings::back)) || (!Game::cancel_event_eaten[gamepad] && !u.input->get(Controls::Cancel, gamepad) && u.last_input->get(Controls::Cancel, gamepad));

	char str[128];
	s32 delta;

	{
		b8* waypoints = &Settings::waypoints;
		sprintf(str, "%s", _(*waypoints ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::waypoints), str);
		if (delta != 0)
			*waypoints = !(*waypoints);
	}

	{
		b8* subtitles = &Settings::subtitles;
		sprintf(str, "%s", _(*subtitles ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::subtitles), str);
		if (delta != 0)
			*subtitles = !(*subtitles);
	}

	{
		s32* fps = &Settings::framerate_limit;
		sprintf(str, "%d", *fps);
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
		sprintf(str, "%s", _(*volumetric_lighting ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::volumetric_lighting), str);
		if (delta != 0)
			*volumetric_lighting = !(*volumetric_lighting);
	}

	{
		b8* antialiasing = &Settings::antialiasing;
		sprintf(str, "%s", _(*antialiasing ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::antialiasing), str);
		if (delta != 0)
			*antialiasing = !(*antialiasing);
	}

	{
		b8* ssao = &Settings::ssao;
		sprintf(str, "%s", _(*ssao ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::ssao), str);
		if (delta != 0)
			*ssao = !(*ssao);
	}

	{
		b8* scan_lines = &Settings::scan_lines;
		sprintf(str, "%s", _(*scan_lines ? strings::on : strings::off));
		delta = menu->slider_item(u, _(strings::scan_lines), str);
		if (delta != 0)
			*scan_lines = !(*scan_lines);
	}

	menu->end();

	if (exit)
	{
		Game::cancel_event_eaten[gamepad] = true;
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

void UIMenu::start(const Update& u, s8 g, b8 input)
{
	clear();

	gamepad = g;

	if (Console::visible || !input || Menu::dialog_active(gamepad))
		return;

	if (active[g])
	{
		if (active[g] != this)
			return;
	}
	else
		active[g] = this;

	s32 delta = UI::input_delta_vertical(u, gamepad);
	if (delta != 0)
	{
		Audio::post_global_event(AK::EVENTS::PLAY_MENU_MOVE);
		selected += delta;
	}
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
	item->label.color = item->value.color = disabled ? UI::color_disabled() : (is_selected ? UI::color_accent() : UI::color_default);
	item->label.text(gamepad, string);
	text_clip(&item->label, animation_time + (items.length - 1 - scroll.pos) * 0.1f, 100.0f);

	item->value.anchor_x = UIText::Anchor::Center;
	item->value.text(gamepad, value);

	if (!scroll.item(items.length - 1)) // this item is not visible
		return false;

	return true;
}

// render a single menu item and increment the position for the next item
b8 UIMenu::item(const Update& u, const char* string, const char* value, b8 disabled, AssetID icon)
{
	if (!add_item(false, string, value, disabled, icon))
		return false;

	if (Console::visible || active[gamepad] != this || Menu::dialog_active(gamepad))
		return false;

	if (selected == items.length - 1
		&& !u.input->get(Controls::Interact, gamepad)
		&& u.last_input->get(Controls::Interact, gamepad)
		&& Game::time.total > 0.35f
		&& !Console::visible
		&& !disabled)
	{
		Audio::post_global_event(AK::EVENTS::PLAY_MENU_SELECT);
		return true;
	}
	
	return false;
}

s32 UIMenu::slider_item(const Update& u, const char* label, const char* value, b8 disabled, AssetID icon)
{
	if (!add_item(true, label, value, disabled, icon))
		return 0;

	if (Console::visible || active[gamepad] != this)
		return 0;

	if (disabled)
		return 0;

	if (selected == items.length - 1
		&& Game::time.total > 0.5f)
	{
		s32 delta = UI::input_delta_horizontal(u, gamepad);
		if (delta < 0)
			Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
		else if (delta > 0)
			Audio::post_global_event(AK::EVENTS::PLAY_MENU_ALTER);
		return delta;
	}
	
	return 0;
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
	return (vi_min(items.length, UI_SCROLL_MAX) * MENU_ITEM_HEIGHT) - MENU_ITEM_PADDING * 2.0f;
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
		Audio::post_global_event(AK::EVENTS::PLAY_CONSOLE_KEY);
	}
}

void UIMenu::text_clip(UIText* text, r32 start_time, r32 speed, s32 max)
{
	text_clip_timer(text, Game::real_time.total - start_time, speed, max);
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

void UIMenu::draw_ui(const RenderParams& params, const Vec2& origin, UIText::Anchor anchor_x, UIText::Anchor anchor_y) const
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

	if (items.length > 0)
	{
		// draw background
		r32 h = height() * Ease::cubic_out<r32>(vi_min((Game::real_time.total - animation_time) / 0.25f, 1.0f));
		Rect2 rect =
		{
			Vec2(pos.x - MENU_ITEM_PADDING_LEFT, pos.y - h - MENU_ITEM_PADDING * 1.5f),
			Vec2(MENU_ITEM_WIDTH, h + MENU_ITEM_PADDING * 3.0f)
		};

		UI::box(params, rect, UI::color_background);
	}

	Rect2 rect;
	for (s32 i = 0; i < items.length; i++)
	{
		if (!scroll.item(i))
			continue;

		const Item& item = items[i];

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

		if (active[gamepad] == this && i == selected)
			UI::box(params, { pos + Vec2(-MENU_ITEM_PADDING_LEFT, item.label.size * -UI::scale), Vec2(4 * UI::scale, item.label.size * UI::scale) }, UI::color_accent());

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

	if (scroll_started)
		scroll.end(params, rect.pos + Vec2(rect.size.x * 0.5f, 0));
}


}
