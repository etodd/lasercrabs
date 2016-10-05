#include "player.h"
#include "awk.h"
#include "data/components.h"
#include "entities.h"
#include "render/render.h"
#include "input.h"
#include "common.h"
#include "console.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "physics.h"
#include "asset/lookup.h"
#include "asset/mesh.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "asset/animation.h"
#include "asset/armature.h"
#include "strings.h"
#include "ease.h"
#include "render/skinned_model.h"
#include "render/views.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "game.h"
#include "minion.h"
#include "walker.h"
#include "data/animator.h"
#include "mersenne/mersenne-twister.h"
#include "noise.h"
#include "settings.h"
#if DEBUG_AI_CONTROL
#include "ai_player.h"
#endif
#include "scripts.h"
#include "cora.h"

namespace VI
{


#define fov_map_view (60.0f * PI * 0.5f / 180.0f)
#define fov_default (70.0f * PI * 0.5f / 180.0f)
#define fov_zoom (fov_default * 0.5f)
#define fov_sniper (fov_default * 0.25f)
#define zoom_speed_multiplier 0.25f
#define zoom_speed_multiplier_sniper 0.15f
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse 0.1f
#define speed_joystick 5.0f
#define gamepad_rotation_acceleration (1.0f / 0.2f)
#define attach_speed 5.0f
#define rotation_speed 20.0f
#define msg_time 0.75f
#define text_size 16.0f
#define damage_shake_time 0.7f
#define score_summary_delay 2.0f
#define score_summary_accept_delay 3.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u8 hp, u8 shield, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return scale * ((shield + (hp - 1)) * (box_size.x + HP_BOX_SPACING) - HP_BOX_SPACING);
}

void camera_setup(Entity* e, Camera* camera, r32 offset)
{
	Vec3 abs_wall_normal = (e->get<Transform>()->absolute_rot() * e->get<Awk>()->lerped_rotation) * Vec3(0, 0, 1);
	camera->wall_normal = camera->rot.inverse() * abs_wall_normal;
	camera->pos = e->get<Awk>()->center_lerped() + camera->rot * Vec3(0, 0, -offset);
	if (e->get<Transform>()->parent.ref())
	{
		camera->pos += abs_wall_normal * 0.5f;
		camera->pos.y += 0.5f - vi_min((r32)fabs(abs_wall_normal.y), 0.5f);
	}

	camera->range = e->get<Awk>()->range();
	camera->range_center = camera->rot.inverse() * (e->get<Awk>()->center_lerped() - camera->pos);
	camera->cull_range = camera->range_center.length();
	camera->cull_behind_wall = abs_wall_normal.dot(camera->pos - e->get<Awk>()->attach_point()) < 0.0f;
}

PinArray<LocalPlayer, MAX_PLAYERS> LocalPlayer::list;

LocalPlayer::LocalPlayer(PlayerManager* m, u8 g)
	: gamepad(g),
	manager(m),
	camera(),
	msg_text(),
	msg_timer(msg_time),
	menu(),
	revision(),
	angle_horizontal(),
	angle_vertical(),
	menu_state(),
	upgrade_menu_open(),
	upgrade_animation_time(),
	score_summary_scroll(),
	spectate_index()
{
	if (gamepad == 0)
		sprintf(manager.ref()->username, "%s", Game::save.username);
	else
		sprintf(manager.ref()->username, "%s[%d]", Game::save.username, gamepad);

	msg_text.size = text_size;
	msg_text.anchor_x = UIText::Anchor::Center;
	msg_text.anchor_y = UIText::Anchor::Center;

	m->spawn.link<LocalPlayer, &LocalPlayer::spawn>(this);
}

LocalPlayer::UIMode LocalPlayer::ui_mode() const
{
	if (menu_state != Menu::State::Hidden)
		return UIMode::Pause;
	else if (Team::game_over)
		return UIMode::GameOver;
	else if (manager.ref()->entity.ref())
	{
		if (upgrade_menu_open)
			return UIMode::Upgrading;
		else
			return UIMode::Default;
	}
	else
		return UIMode::Dead;
}

void LocalPlayer::msg(const char* msg, b8 good)
{
	msg_text.text(msg);
	msg_text.color = good ? UI::color_accent : UI::color_alert;
	msg_timer = 0.0f;
	msg_good = good;
}

void LocalPlayer::awake(const Update& u)
{
	Audio::listener_enable(gamepad);

	camera = Camera::add();
	camera->fog = false;
	camera->team = (u8)manager.ref()->team.ref()->team();
	camera->mask = 1 << camera->team;
	s32 player_count;
#if DEBUG_AI_CONTROL
	player_count = list.count() + AIPlayer::list.count();
#else
	player_count = list.count();
#endif
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[id()];

	camera->viewport =
	{
		Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
		Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective(fov_map_view, aspect, 1.0f, Game::level.skybox.far_plane);
	Quat rot;
	map_view.ref()->absolute(&camera->pos, &rot);
	camera->rot = Quat::look(rot * Vec3(0, -1, 0));
}

#define DANGER_RAMP_UP_TIME 2.0f
#define DANGER_LINGER_TIME 3.0f
#define DANGER_RAMP_DOWN_TIME 4.0f
r32 LocalPlayer::danger;
void LocalPlayer::update_all(const Update& u)
{
	for (auto i = LocalPlayer::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	// update audio danger parameter

	b8 visible_enemy = false;
	for (auto i = LocalPlayerControl::list.iterator(); !i.is_last(); i.next())
	{
		PlayerCommon* local_common = i.item()->get<PlayerCommon>();
		for (auto j = PlayerCommon::list.iterator(); !j.is_last(); j.next())
		{
			if (PlayerCommon::visibility.get(PlayerCommon::visibility_hash(local_common, j.item())))
			{
				visible_enemy = true;
				break;
			}
		}
		if (visible_enemy)
			break;
	}

	if (visible_enemy)
		danger = vi_min(1.0f + DANGER_LINGER_TIME / DANGER_RAMP_UP_TIME, danger + Game::real_time.delta / DANGER_RAMP_UP_TIME);
	else
		danger = vi_max(0.0f, danger - Game::real_time.delta / DANGER_RAMP_DOWN_TIME);

	Audio::global_param(AK::GAME_PARAMETERS::DANGER, vi_min(danger, 1.0f));
}

#define TUTORIAL_TIME 2.0f

void LocalPlayer::update_camera_rotation(const Update& u)
{
	r32 s = speed_mouse * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta;
	angle_horizontal -= (r32)u.input->cursor_x * s;
	angle_vertical += (r32)u.input->cursor_y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);

	if (u.input->gamepads[gamepad].active)
	{
		r32 s = speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta;
		Vec2 rotation(u.input->gamepads[gamepad].right_x, u.input->gamepads[gamepad].right_y);
		Input::dead_zone(&rotation.x, &rotation.y);
		angle_horizontal -= rotation.x * s;
		angle_vertical += rotation.y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
	}

	if (angle_vertical < PI * -0.495f)
		angle_vertical = PI * -0.495f;
	if (angle_vertical > PI * 0.495f)
		angle_vertical = PI * 0.495f;

	camera->rot = Quat::euler(0, angle_horizontal, angle_vertical);
}

Entity* live_player_get(s32 index)
{
	s32 count = 0;
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		if (count == index)
			return i.item()->entity();
		count++;
	}
	return nullptr;
}

void LocalPlayer::update(const Update& u)
{
	if (Console::visible)
		return;

	// flash message when the buy period expires
	if (!Team::game_over
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& Game::time.total > GAME_BUY_PERIOD
		&& Game::time.total - Game::time.delta <= GAME_BUY_PERIOD)
	{
		msg(_(strings::buy_period_expired), true);
	}

	if (msg_timer < msg_time)
		msg_timer += Game::real_time.delta;

	// close/open pause menu if needed
	{
		if (Game::time.total > 0.5f
			&& u.last_input->get(Controls::Pause, gamepad)
			&& !u.input->get(Controls::Pause, gamepad)
			&& (!manager.ref()->entity.ref() || manager.ref()->entity.ref()->get<Awk>()->current_ability == Ability::None) // HACK because cancel and pause are on the same dang key
			&& !Game::cancel_event_eaten[gamepad]
			&& !upgrade_menu_open
			&& (menu_state == Menu::State::Hidden || menu_state == Menu::State::Visible)
			&& !Cora::has_focus())
		{
			Game::cancel_event_eaten[gamepad] = true;
			menu_state = (menu_state == Menu::State::Hidden) ? Menu::State::Visible : Menu::State::Hidden;
			menu.animate();
		}
		else if (menu_state == Menu::State::Visible
			&& u.last_input->get(Controls::Cancel, gamepad)
			&& !u.input->get(Controls::Cancel, gamepad)
			&& !Game::cancel_event_eaten[gamepad])
		{
			Game::cancel_event_eaten[gamepad] = true;
			menu_state = Menu::State::Hidden;
		}
	}

	// reset camera range after the player dies
	if (!manager.ref()->entity.ref())
		camera->range = 0;

	switch (ui_mode())
	{
		case UIMode::Default:
		{
			if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
			{
				if (manager.ref()->at_control_point()
					&& manager.ref()->can_transition_state()
					&& !manager.ref()->friendly_control_point(manager.ref()->at_control_point()))
				{
					// enemy control point; capture
					manager.ref()->capture_start();
				}
				else if (manager.ref()->at_upgrade_point())
				{
					upgrade_menu_open = true;
					menu.animate();
					upgrade_animation_time = Game::real_time.total;
					manager.ref()->entity.ref()->get<Awk>()->current_ability = Ability::None;
				}
			}
			break;
		}
		case UIMode::Upgrading:
		{
			// upgrade menu
			if (u.last_input->get(Controls::Cancel, gamepad)
				&& !u.input->get(Controls::Cancel, gamepad)
				&& !Game::cancel_event_eaten[gamepad])
			{
				Game::cancel_event_eaten[gamepad] = true;
				upgrade_menu_open = false;
			}
			else
			{
				b8 upgrade_in_progress = !manager.ref()->can_transition_state();

				u8 last_selected = menu.selected;

				menu.start(u, gamepad);

				if (menu.item(u, _(strings::close), nullptr))
					upgrade_menu_open = false;

				for (s32 i = 0; i < (s32)Upgrade::count; i++)
				{
					Upgrade upgrade = (Upgrade)i;
					b8 can_upgrade = !upgrade_in_progress
						&& manager.ref()->upgrade_available(upgrade)
						&& manager.ref()->credits >= manager.ref()->upgrade_cost(upgrade);
					const UpgradeInfo& info = UpgradeInfo::list[(s32)upgrade];
					if (menu.item(u, _(info.name), nullptr, !can_upgrade, info.icon))
						manager.ref()->upgrade_start(upgrade);
				}

				menu.end();

				if (menu.selected != last_selected
					|| upgrade_in_progress) // once the upgrade is done, animate the new ability description
					upgrade_animation_time = Game::real_time.total;
			}
			break;
		}
		case UIMode::Pause:
		{
			Menu::pause_menu(u, gamepad, &menu, &menu_state);
			break;
		}
		case UIMode::Dead:
		{
			if (Game::level.continue_match_after_death)
			{
				// noclip
				update_camera_rotation(u);

				r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
				camera->perspective(fov_map_view, aspect, 0.02f, Game::level.skybox.far_plane);
				camera->range = 0;
				camera->cull_range = 0;

				if (!Console::visible)
				{
					r32 speed = u.input->keys[(s32)KeyCode::LShift] ? 24.0f : 4.0f;
					if (u.input->get(Controls::Forward, gamepad))
						camera->pos += camera->rot * Vec3(0, 0, 1) * u.time.delta * speed;
					if (u.input->get(Controls::Backward, gamepad))
						camera->pos += camera->rot * Vec3(0, 0, -1) * u.time.delta * speed;
					if (u.input->get(Controls::Right, gamepad))
						camera->pos += camera->rot * Vec3(-1, 0, 0) * u.time.delta * speed;
					if (u.input->get(Controls::Left, gamepad))
						camera->pos += camera->rot * Vec3(1, 0, 0) * u.time.delta * speed;
				}
			}
			else if (manager.ref()->spawn_timer > 0.0f)
			{
				// we're spawning
			}
			else
			{
				// we're dead but others still playing; spectate
				update_camera_rotation(u);

				r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
				camera->perspective(fov_default, aspect, 0.02f, Game::level.skybox.far_plane);

				if (PlayerCommon::list.count() > 0)
				{
					spectate_index += UI::input_delta_horizontal(u, gamepad);
					if (spectate_index < 0)
						spectate_index = PlayerCommon::list.count() - 1;
					else if (spectate_index >= PlayerCommon::list.count())
						spectate_index = 0;

					Entity* spectating = live_player_get(spectate_index);

					if (spectating)
						camera_setup(spectating, camera, 6.0f);
				}
			}
			break;
		}
		case UIMode::GameOver:
		{
			camera->range = 0;
			if (Game::real_time.total - Team::game_over_real_time > score_summary_delay)
			{
				// update score summary scroll
				s32 score_summary_count = 0;
				for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
					score_summary_count += 1 + player.item()->credits_summary.length;
				score_summary_scroll.update(u, score_summary_count, gamepad);

				if (Game::real_time.total - Team::game_over_real_time > score_summary_delay + score_summary_accept_delay)
				{
					// accept score summary
					if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
						manager.ref()->score_accepted = true;
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

	Audio::listener_update(gamepad, camera->pos, camera->rot);
}

void LocalPlayer::spawn()
{
	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	Vec3 dir = rot * Vec3(0, 1, 0);
	r32 angle = atan2f(dir.x, dir.z);

	// Spawn AWK
	pos += Quat::euler(0, angle + (gamepad * PI * 0.5f), 0) * Vec3(0, 0, CONTROL_POINT_RADIUS * 0.5f); // spawn it around the edges
	Entity* spawned = World::create<AwkEntity>(manager.ref()->team.ref()->team());

	spawned->get<Transform>()->absolute_pos(pos);
	PlayerCommon* common = spawned->add<PlayerCommon>(manager.ref());
	common->angle_horizontal = angle;

	manager.ref()->entity = spawned;

	LocalPlayerControl* control = spawned->add<LocalPlayerControl>(gamepad);
	control->player = this;
}

void draw_icon_text(const RenderParams& params, const Vec2& pos, AssetID icon, char* string, const Vec4& color)
{
	r32 icon_size = text_size * UI::scale;
	r32 padding = 8 * UI::scale;

	UIText text;
	text.color = color;
	text.size = text_size;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Center;
	text.text(string);

	r32 total_width = icon_size + padding + text.bounds().x;

	UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, icon_size * -0.5f), Vec2(total_width, icon_size)).outset(padding), UI::color_background);
	UI::mesh(params, icon, pos + Vec2(total_width * -0.5f + icon_size - padding, 0), Vec2(icon_size), text.color);
	text.draw(params, pos + Vec2(total_width * -0.5f + icon_size + padding, 0));
}

void ability_draw(const RenderParams& params, const LocalPlayer* player, const Vec2& pos, Ability ability, AssetID icon, const char* control_binding)
{
	char string[255];

	u16 cost = AbilityInfo::list[(s32)ability].spawn_cost;
	sprintf(string, "%s", control_binding);
	const Vec4* color;
	PlayerManager* manager = player->manager.ref();
	if (!manager->ability_valid(ability))
		color = &UI::color_alert;
	else if (manager->entity.ref()->get<Awk>()->current_ability == ability)
		color = &UI::color_default;
	else
		color = &UI::color_accent;
	draw_icon_text(params, pos, icon, string, *color);
}

void scoreboard_draw(const RenderParams& params, const PlayerManager* manager)
{
	const Rect2& vp = params.camera->viewport;
	Vec2 p = vp.size * Vec2(0.5f);

	UIText text;
	text.size = text_size;
	r32 wrap = text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::color_default;

	// "spawning..."
	if (!manager->entity.ref() && manager->respawns > 0)
	{
		text.text(_(strings::deploy_timer), s32(manager->spawn_timer + 1));
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	// show remaining drones label
	text.text(_(strings::drones_remaining));
	text.color = UI::color_accent;
	UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
	text.draw(params, p);
	p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;

	// show player list
	p.x += wrap * -0.5f;
	for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
	{
		text.wrap_width = wrap;
		text.anchor_x = UIText::Anchor::Min;
		text.color = Team::ui_color(manager->team.ref()->team(), i.item()->team.ref()->team());
		text.text_raw(i.item()->username);
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);

		text.anchor_x = UIText::Anchor::Max;
		text.wrap_width = 0;
		text.text("%d", s32(i.item()->respawns));
		text.draw(params, p + Vec2(wrap, 0));

		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}
}

void LocalPlayer::draw_alpha(const RenderParams& params) const
{
	if (params.camera != camera || Game::level.continue_match_after_death)
		return;

	const r32 line_thickness = 2.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	UIMode mode = ui_mode();

	r32 radius = 64.0f * UI::scale;
	Vec2 center = vp.size * Vec2(0.1f, 0.1f) + Vec2(radius, radius * 0.5f + (text_size * UI::scale * 0.5f));

	if (Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& (mode == UIMode::Default || mode == UIMode::Upgrading))
	{
		// credits
		b8 draw = true;
		b8 flashing = manager.ref()->credits_flash_timer > 0.0f;
		if (flashing)
			draw = UI::flash_function(Game::real_time.total);

		char buffer[128];
		sprintf(buffer, "%d", manager.ref()->credits);
		Vec2 credits_pos = center + Vec2(0, radius * -0.5f);
		draw_icon_text(params, credits_pos, Asset::Mesh::icon_energy, buffer, draw ? (flashing ? UI::color_default : UI::color_accent) : UI::color_background);

		// control point increment amount
		{
			r32 icon_size = text_size * UI::scale;
			r32 padding = 8 * UI::scale;

			UIText text;
			text.color = UI::color_accent;
			text.text("+%d", manager.ref()->increment());
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;

			r32 total_width = icon_size + padding + text.bounds().x;

			Vec2 pos = credits_pos + Vec2(0, text_size * UI::scale * -2.0f);
			UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, icon_size * -0.5f), Vec2(total_width, icon_size)).outset(padding), UI::color_background);
			UI::triangle_percentage
			(
				params,
				{ pos + Vec2(total_width * -0.5f + icon_size - padding, 3.0f * UI::scale), Vec2(icon_size * 1.25f) },
				1.0f - (PlayerManager::timer / CONTROL_POINT_INTERVAL),
				text.color,
				PI
			);
			text.draw(params, pos + Vec2(total_width * -0.5f + icon_size + padding, 0));
		}
	}

	// draw abilities
	if (Game::level.has_feature(Game::FeatureLevel::Abilities))
	{
		ControlPoint* control_point = manager.ref()->at_control_point();
		if (mode == UIMode::Default
			&& manager.ref()->can_transition_state()
			&& ((control_point && !control_point->owned_by(manager.ref()->team.ref()->team()))
				|| manager.ref()->at_upgrade_point()))
		{
			UIText text;
			if (manager.ref()->at_upgrade_point())
			{
				// "upgrade!" prompt
				text.color = manager.ref()->upgrade_available() ? UI::color_accent : UI::color_disabled;
				text.text(_(strings::prompt_upgrade));
			}
			else
			{
				// at control point; "capture!" prompt
				text.color = UI::color_accent;
				if (control_point->capture_timer > 0.0f)
					text.text(_(strings::prompt_cancel_hack));
				else
					text.text(_(strings::prompt_hack));
			}
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 pos = vp.size * Vec2(0.5f, 0.55f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(params, pos);
		}

		if ((mode == UIMode::Default || mode == UIMode::Upgrading)
			&& manager.ref()->can_transition_state())
		{
			// draw abilities

			b8 is_gamepad = gamepad > 0 || Game::is_gamepad;

			// ability 1
			{
				Ability ability = manager.ref()->abilities[0];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability1].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(-radius, 0), ability, info.icon, binding);
				}
			}

			// ability 2
			{
				Ability ability = manager.ref()->abilities[1];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability2].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(0, radius * 0.5f), ability, info.icon, binding);
				}
			}

			// ability 3
			{
				Ability ability = manager.ref()->abilities[2];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability3].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(radius, 0), ability, info.icon, binding);
				}
			}
		}
	}

	if (mode == UIMode::Default)
	{
		if (params.sync->input.get(Controls::Scoreboard, gamepad))
			scoreboard_draw(params, manager.ref());
	}
	else if (mode == UIMode::Upgrading)
	{
		Vec2 upgrade_menu_pos = vp.size * Vec2(0.5f, 0.6f);
		menu.draw_alpha(params, upgrade_menu_pos, UIText::Anchor::Center, UIText::Anchor::Center);

		if (menu.selected > 0)
		{
			// show details of currently highlighted upgrade
			Upgrade upgrade = (Upgrade)(menu.selected - 1);
			if (manager.ref()->current_upgrade == Upgrade::None
				&& manager.ref()->upgrade_available(upgrade))
			{
				r32 padding = 8.0f * UI::scale;

				const UpgradeInfo& info = UpgradeInfo::list[(s32)upgrade];
				UIText text;
				text.color = UI::color_accent;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
				u16 cost = manager.ref()->upgrade_cost(upgrade);
				text.text(_(strings::upgrade_description), cost, _(info.description));
				UIMenu::text_clip(&text, upgrade_animation_time, 150.0f);

				Vec2 pos = upgrade_menu_pos + Vec2(MENU_ITEM_WIDTH * -0.5f + padding, menu.height() * -0.5f - padding * 3.0f);
				UI::box(params, text.rect(pos).outset(padding), UI::color_background);
				text.draw(params, pos);
			}
		}
	}
	else if (mode == UIMode::Dead)
	{
		// if we haven't spawned yet, then show the player list
		if (manager.ref()->spawn_timer > 0.0f)
			scoreboard_draw(params, manager.ref());
		else
		{
			// we're dead but others still playing; spectate

			Entity* spectating = live_player_get(spectate_index);
			if (spectating)
			{
				UIText text;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Max;

				// username
				text.color = Team::ui_color(manager.ref()->team.ref()->team(), spectating->get<AIAgent>()->team);
				text.text_raw(spectating->get<PlayerCommon>()->manager.ref()->username);
				Vec2 pos = vp.size * Vec2(0.5f, 0.2f);
				UI::box(params, text.rect(pos).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, pos);

				// "spectating"
				text.color = UI::color_accent;
				text.text(_(strings::spectating));
				pos = vp.size * Vec2(0.5f, 0.1f);
				UI::box(params, text.rect(pos).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, pos);
			}
		}
	}

	if (mode == UIMode::GameOver)
	{
		// show victory/defeat/draw message
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = 32.0f;

		Team* winner = Team::winner.ref();
		if (winner == manager.ref()->team.ref()) // we won
		{
			text.color = UI::color_accent;
			text.text(_(strings::victory));
		}
		else if (!winner) // it's a draw
		{
			text.color = UI::color_alert;
			text.text(_(strings::draw));
		}
		else // we lost
		{
			text.color = UI::color_alert;
			text.text(_(strings::defeat));
		}
		UIMenu::text_clip(&text, Team::game_over_real_time, 20.0f);

		b8 show_score_summary = Game::real_time.total - Team::game_over_real_time > score_summary_delay;
		Vec2 title_pos = show_score_summary
			? vp.size * Vec2(0.5f, 1.0f) + Vec2(0, (text.size + 32) * -UI::scale)
			: vp.size * Vec2(0.5f, 0.5f);
		UI::box(params, text.rect(title_pos).outset(16 * UI::scale), UI::color_background);
		text.draw(params, title_pos);

		if (show_score_summary)
		{
			// score summary screen

			UIText text;
			text.size = text_size;
			text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;

			Vec2 p = title_pos + Vec2(0, -2.0f * (MENU_ITEM_HEIGHT + MENU_ITEM_PADDING));

			score_summary_scroll.start(params, p + Vec2(0, MENU_ITEM_PADDING));
			s32 item_counter = 0;
			for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
			{
				text.color = player.item() == manager.ref() ? UI::color_accent : Team::ui_color(manager.ref()->team.ref()->team(), player.item()->team.ref()->team());

				UIText amount = text;
				amount.anchor_x = UIText::Anchor::Max;
				amount.wrap_width = 0;

				// username
				if (score_summary_scroll.item(item_counter))
				{
					text.text(player.item()->username);
					UIMenu::text_clip(&text, Team::game_over_real_time + score_summary_delay, 50.0f + (r32)vi_min(item_counter, 6) * -5.0f);
					UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
					text.draw(params, p);
					p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
				}
				item_counter++;

				// score breakdown
				const auto& credits_summary = player.item()->credits_summary;
				for (s32 i = 0; i < credits_summary.length; i++)
				{
					if (score_summary_scroll.item(item_counter))
					{
						text.text(_(credits_summary[i].label));
						UIMenu::text_clip(&text, Team::game_over_real_time + score_summary_delay, 50.0f + (r32)vi_min(item_counter, 6) * -5.0f);
						UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
						text.draw(params, p);
						amount.text("%d", credits_summary[i].amount);
						amount.draw(params, p + Vec2(MENU_ITEM_WIDTH * 0.5f - MENU_ITEM_PADDING, 0));
						p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
					}
					item_counter++;
				}
			}
			score_summary_scroll.end(params, p + Vec2(0, MENU_ITEM_PADDING));

			// press x to continue
			if (Game::real_time.total - Team::game_over_real_time > score_summary_delay + score_summary_accept_delay)
			{
				Vec2 p = vp.size * Vec2(0.5f, 0.2f);
				text.wrap_width = 0;
				text.color = UI::color_accent;
				text.text(_(manager.ref()->score_accepted ? strings::waiting : strings::prompt_accept));
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);
			}
		}
	}
	else
	{
		// game is not yet over

		{
			// upgrade / spawn / capture timer
			PlayerManager::State manager_state = manager.ref()->state();
			if (manager_state != PlayerManager::State::Default)
			{
				r32 total_time;
				AssetID string;
				u16 cost;

				switch (manager_state)
				{
					case PlayerManager::State::Upgrading:
					{
						// getting an upgrade
						string = strings::upgrading;

						const UpgradeInfo& info = UpgradeInfo::list[(s32)manager.ref()->current_upgrade];
						cost = info.cost;
						total_time = UPGRADE_TIME;
						break;
					}
					case PlayerManager::State::Capturing:
					{
						// capturing a control point
						ControlPoint* control_point = manager.ref()->at_control_point();
						if (control_point && control_point->team_next != AI::TeamNone) // capture is already in progress
							string = strings::canceling_capture;
						else
							string = strings::starting_capture;
						cost = 0;
						total_time = CAPTURE_TIME;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				// draw bar

				Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);
				Vec2 bar_size(200.0f * UI::scale, 32.0f * UI::scale);
				Rect2 bar = { pos + bar_size * -0.5f, bar_size };
				UI::box(params, bar, UI::color_background);
				UI::border(params, bar, 2, UI::color_accent);
				UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (manager.ref()->state_timer / total_time)), bar.size.y) }, UI::color_accent);

				UIText text;
				text.size = 18.0f;
				text.color = UI::color_background;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.text(_(string), (s32)cost);
				text.draw(params, bar.pos + bar.size * 0.5f);
			}
		}

		if (mode == UIMode::Default || mode == UIMode::Upgrading)
		{
			// draw battery/timer

			r32 remaining = vi_max(0.0f, GAME_TIME_LIMIT - Game::time.total);

			const Vec2 box(text_size * 5 * UI::scale, text_size * UI::scale);
			const r32 padding = 8.0f * UI::scale;

			Vec2 p = vp.size * Vec2(0.9f, 0.1f) + Vec2(-box.x, 0);

			UI::box(params, Rect2(p, box).outset(padding), UI::color_background);

			Vec2 icon_pos = p + Vec2(0.75f, 0.5f) * text_size * UI::scale;

			AssetID icon;
			const Vec4* color;
			if (remaining > GAME_TIME_LIMIT * 0.8f)
			{
				icon = Asset::Mesh::icon_battery_3;
				color = &UI::color_default;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.6f)
			{
				icon = Asset::Mesh::icon_battery_2;
				color = &UI::color_default;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.4f)
			{
				icon = Asset::Mesh::icon_battery_1;
				color = &UI::color_accent;
			}
			else if (remaining > 60.0f)
			{
				icon = Asset::Mesh::icon_battery_1;
				color = &UI::color_alert;
			}
			else
			{
				icon = Asset::Mesh::icon_battery_0;
				color = &UI::color_alert;
			}

			{
				b8 draw;
				if (remaining > GAME_TIME_LIMIT * 0.4f)
					draw = true;
				else if (remaining > 60.0f)
					draw = UI::flash_function_slow(Game::real_time.total);
				else
					draw = UI::flash_function(Game::real_time.total);
				if (draw)
					UI::mesh(params, icon, icon_pos, Vec2(text_size * UI::scale), *color);
			}

			s32 remaining_minutes = remaining / 60.0f;
			s32 remaining_seconds = remaining - (remaining_minutes * 60.0f);

			UIText text;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.color = *color;
			text.text(_(strings::timer), remaining_minutes, remaining_seconds);
			text.draw(params, icon_pos + Vec2(text_size * UI::scale * 1.5f, 0));
		}

		// network error icon
		if (Game::session.network_state == Game::NetworkState::Lag && Game::session.network_time - Game::session.network_timer > 0.25f)
			UI::mesh(params, Asset::Mesh::icon_network_error, vp.size * Vec2(0.9f, 0.5f), Vec2(text_size * 2.0f * UI::scale), UI::color_alert);
	}

	// message
	if (msg_timer < msg_time)
	{
		r32 last_timer = msg_timer;
		b8 flash = UI::flash_function(Game::real_time.total);
		b8 last_flash = UI::flash_function(Game::real_time.total - Game::real_time.delta);
		if (flash)
		{
			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.6f);
			Rect2 box = msg_text.rect(pos).outset(6 * UI::scale);
			UI::box(params, box, UI::color_background);
			msg_text.draw(params, pos);
			if (!last_flash)
				Audio::post_global_event(msg_good ? AK::EVENTS::PLAY_BEEP_GOOD : AK::EVENTS::PLAY_BEEP_BAD);
		}
	}

	if (mode == UIMode::Pause) // pause menu always drawn on top
		menu.draw_alpha(params, Vec2(0, params.camera->viewport.size.y * 0.5f), UIText::Anchor::Min, UIText::Anchor::Center);
}

PlayerCommon::PlayerCommon(PlayerManager* m)
	: angle_horizontal(),
	angle_vertical(),
	attach_quat(Quat::identity),
	manager(m)
{
}

void PlayerCommon::awake()
{
	get<Health>()->hp_max = AWK_HEALTH;
	link<&PlayerCommon::awk_done_flying>(get<Awk>()->done_flying);
	link<&PlayerCommon::awk_detached>(get<Awk>()->detached);
	link_arg<const Vec3&, &PlayerCommon::awk_bounce>(get<Awk>()->bounce);
}

b8 PlayerCommon::movement_enabled() const
{
	return get<Awk>()->state() == Awk::State::Crawl // must be attached to wall
		&& manager.ref()->state() == PlayerManager::State::Default // can't move while upgrading and stuff
		&& (Game::time.total > GAME_BUY_PERIOD || !Game::level.has_feature(Game::FeatureLevel::Abilities)); // or during the buy period
}

void PlayerCommon::update(const Update& u)
{
	Quat rot = get<Transform>()->absolute_rot();
	r32 angle = Quat::angle(attach_quat, rot);
	if (angle > 0)
		attach_quat = Quat::slerp(vi_min(1.0f, rotation_speed * u.time.delta), attach_quat, rot);

	last_angle_horizontal = angle_horizontal;
}

r32 PlayerCommon::detect_danger() const
{
	AI::Team my_team = get<AIAgent>()->team;
	for (s32 i = 0; i < Team::list.length; i++)
	{
		Team* team = &Team::list[i];
		if (team->team() == my_team)
			continue;

		Team::SensorTrack* track = &team->player_tracks[manager.id];
		if (track->entity.ref() == entity())
		{
			if (track->tracking)
				return 1.0f;
			else
				return track->timer / SENSOR_TIME;
		}
	}
	return 0.0f;
}

void PlayerCommon::awk_done_flying()
{
	Quat absolute_rot = get<Transform>()->absolute_rot();
	Vec3 wall_normal = absolute_rot * Vec3(0, 0, 1);

	// If we are spawning on to a flat floor, set attach_quat immediately
	// This preserves the camera rotation set by the PlayerSpawn
	if (Vec3::normalize(get<Awk>()->velocity).y == -1.0f && wall_normal.y > 0.9f)
		attach_quat = absolute_rot;
	else
	{
		// we are attaching to a wall or something
		// set the attach quat to be perpendicular to the camera, so we can ease the camera gently away from the wall
		Vec3 direction = look_dir();

		Vec3 up = Vec3::normalize(wall_normal.cross(direction));
		Vec3 right = direction.cross(up);

		// make sure the up and right vector aren't switched
		if (fabs(up.y) < fabs(right.y))
		{
			Vec3 tmp = right;
			right = up;
			up = tmp;
		}

		// if the right vector is too vertical, force it to be more horizontal
		const r32 threshold = fabs(wall_normal.y) + 0.25f;
		right.y = LMath::clampf(right.y, -threshold, threshold);
		right.normalize();

		if (right.dot(direction - wall_normal * direction.dot(wall_normal)) < 0.0f)
			right *= -1.0f;

		attach_quat = Quat::look(right);
	}
}

void PlayerCommon::awk_detached()
{
	Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
	attach_quat = Quat::look(direction);
}

void PlayerCommon::awk_bounce(const Vec3& new_velocity)
{
	Vec3 direction = Vec3::normalize(new_velocity);
	attach_quat = Quat::look(direction);
}

Vec3 PlayerCommon::look_dir() const
{
	if (has<LocalPlayerControl>()) // HACK for third-person camera
		return Vec3::normalize(get<LocalPlayerControl>()->reticle.pos - get<Transform>()->absolute_pos());
	else
		return Quat::euler(0.0f, angle_horizontal, angle_vertical) * Vec3(0, 0, 1);
}

void PlayerCommon::clamp_rotation(const Vec3& direction, r32 dot_limit)
{
	Quat look_quat = Quat::euler(0.0f, angle_horizontal, angle_vertical);
	Vec3 forward = look_quat * Vec3(0, 0, 1);

	r32 dot = forward.dot(direction);
	if (dot < -dot_limit)
	{
		forward = Vec3::normalize(forward - (forward.dot(direction) + dot_limit) * direction);
		angle_horizontal = atan2f(forward.x, forward.z);
		angle_vertical = -asinf(forward.y);
	}
}

Bitmask<MAX_PLAYERS * MAX_PLAYERS> PlayerCommon::visibility;

s32 PlayerCommon::visibility_hash(const PlayerCommon* awk_a, const PlayerCommon* awk_b)
{
	return awk_a->id() * MAX_PLAYERS + awk_b->id();
}

void LocalPlayerControl::awk_done_flying_or_dashing()
{
	rumble = vi_max(rumble, 0.2f);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

LocalPlayerControl::LocalPlayerControl(u8 gamepad)
	: gamepad(gamepad),
	fov(fov_default),
	try_primary(),
	try_zoom(),
	damage_timer(),
	rumble(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed()
{
}

LocalPlayerControl::~LocalPlayerControl()
{
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

void LocalPlayerControl::awake()
{
	last_pos = get<Awk>()->center_lerped();
	link<&LocalPlayerControl::awk_detached>(get<Awk>()->detached);
	link<&LocalPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_flying);
	link<&LocalPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_dashing);
	link_arg<Entity*, &LocalPlayerControl::hit_target>(get<Awk>()->hit);
	link_arg<const TargetEvent&, &LocalPlayerControl::hit_by>(get<Target>()->target_hit);
}

void LocalPlayerControl::hit_target(Entity* target)
{
	rumble = vi_max(rumble, 0.5f);
	if (target->has<MinionCommon>())
	{
		b8 is_enemy = target->get<AIAgent>()->team != get<AIAgent>()->team;
		player.ref()->msg(_(strings::minion_killed), is_enemy);
	}
	else if (target->has<Sensor>())
	{
		b8 is_enemy = target->get<Sensor>()->team != get<AIAgent>()->team;
		player.ref()->msg(_(strings::sensor_destroyed), is_enemy);
	}
	else if (target->has<ContainmentField>())
	{
		b8 is_enemy = target->get<ContainmentField>()->team != get<AIAgent>()->team;
		player.ref()->msg(_(strings::containment_field_destroyed), is_enemy);
	}
}

void LocalPlayerControl::hit_by(const TargetEvent& e)
{
	// we were physically hit by something; shake the camera
	if (get<Awk>()->state() == Awk::State::Crawl) // don't shake the screen if we reflect off something in the air
	{
		damage_timer = damage_shake_time;
		rumble = vi_max(rumble, 1.0f);
	}
}

void LocalPlayerControl::awk_detached()
{
	damage_timer = 0.0f; // stop screen shake
}

b8 LocalPlayerControl::input_enabled() const
{
	return !Console::visible && player.ref()->ui_mode() == LocalPlayer::UIMode::Default && !Cora::has_focus() && !Team::game_over;
}

b8 LocalPlayerControl::movement_enabled() const
{
	return input_enabled() && get<PlayerCommon>()->movement_enabled();
}

r32 LocalPlayerControl::look_speed() const
{
	if (try_zoom)
		return get<Awk>()->current_ability == Ability::Sniper ? zoom_speed_multiplier_sniper : zoom_speed_multiplier;
	else
		return 1.0f;
}

void LocalPlayerControl::update_camera_input(const Update& u, r32 gamepad_rotation_multiplier)
{
	if (input_enabled())
	{
		if (gamepad == 0)
		{
			r32 s = look_speed() * speed_mouse * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta;
			get<PlayerCommon>()->angle_horizontal -= (r32)u.input->cursor_x * s;
			get<PlayerCommon>()->angle_vertical += (r32)u.input->cursor_y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
		}

		if (u.input->gamepads[gamepad].active)
		{
			Vec2 adjustment = Vec2
			(
				-u.input->gamepads[gamepad].right_x,
				u.input->gamepads[gamepad].right_y * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f)
			);
			Input::dead_zone(&adjustment.x, &adjustment.y);
			adjustment *= look_speed() * speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta * gamepad_rotation_multiplier;
			r32 adjustment_length = adjustment.length();
			if (adjustment_length > 0.0f)
			{
				last_gamepad_input_time = Game::real_time.total;

				// ramp gamepad rotation speed up at a constant rate until we reach the desired speed
				adjustment /= adjustment_length;
				gamepad_rotation_speed = vi_min(adjustment_length, gamepad_rotation_speed + Game::real_time.delta * gamepad_rotation_acceleration);
			}
			else
			{
				// ramp gamepad rotation speed back down
				gamepad_rotation_speed = vi_max(0.0f, gamepad_rotation_speed + Game::real_time.delta * -gamepad_rotation_acceleration);
			}
			get<PlayerCommon>()->angle_horizontal += adjustment.x * gamepad_rotation_speed;
			get<PlayerCommon>()->angle_vertical += adjustment.y * gamepad_rotation_speed;
		}

		get<PlayerCommon>()->angle_vertical = LMath::clampf(get<PlayerCommon>()->angle_vertical, -AWK_VERTICAL_ANGLE_LIMIT, AWK_VERTICAL_ANGLE_LIMIT);
	}
}

Vec3 LocalPlayerControl::get_movement(const Update& u, const Quat& rot)
{
	Vec3 movement = Vec3::zero;
	if (movement_enabled())
	{
		if (u.input->get(Controls::Forward, gamepad))
			movement += Vec3(0, 0, 1);
		if (u.input->get(Controls::Backward, gamepad))
			movement += Vec3(0, 0, -1);
		if (u.input->get(Controls::Right, gamepad))
			movement += Vec3(-1, 0, 0);
		if (u.input->get(Controls::Left, gamepad))
			movement += Vec3(1, 0, 0);

		if (u.input->gamepads[gamepad].active)
		{
			Vec2 gamepad_movement(-u.input->gamepads[gamepad].left_x, -u.input->gamepads[gamepad].left_y);
			Input::dead_zone(&gamepad_movement.x, &gamepad_movement.y);
			movement.x += gamepad_movement.x;
			movement.z += gamepad_movement.y;
		}

		movement = rot * movement;
	}
	return movement;
}

// returns false if there is no more room in the target indicator array
b8 LocalPlayerControl::add_target_indicator(Target* target, TargetIndicator::Type type)
{
	Vec3 me = get<Transform>()->absolute_pos();

	b8 show;

	if (type == TargetIndicator::Type::AwkTracking)
		show = true; // show even out of range
	else
	{
		r32 range = get<Awk>()->range();
		show = (target->absolute_pos() - me).length_squared() < range * range;
	}

	if (show)
	{
		// calculate target intersection trajectory
		Vec3 intersection;
		if (get<Awk>()->predict_intersection(target, &intersection))
		{
			if (reticle.type == ReticleType::Normal && LMath::ray_sphere_intersect(me, reticle.pos, intersection, target->get<RigidBody>()->size.x))
				reticle.type = ReticleType::Target;
			target_indicators.add({ intersection, target->get<RigidBody>()->btBody->getInterpolationLinearVelocity(), type });
			if (target_indicators.length == target_indicators.capacity())
				return false;
		}
	}
	return true;
}

void determine_visibility(PlayerCommon* me, PlayerCommon* other_player, b8* visible, b8* tracking)
{
	// make sure we can see this guy
	AI::Team team = me->get<AIAgent>()->team;
	*tracking = Team::list[(s32)team].player_tracks[other_player->manager.id].tracking;
	*visible = other_player->get<AIAgent>()->team == team
		|| PlayerCommon::visibility.get(PlayerCommon::visibility_hash(me, other_player));
}

void ability_select(Awk* awk, Ability a)
{
	awk->current_ability = a;
}

void ability_cancel(Awk* awk)
{
	awk->current_ability = Ability::None;
}

void ability_update(const Update& u, LocalPlayerControl* control, Controls binding, u8 gamepad, s32 index)
{
	LocalPlayer* player = control->player.ref();
	PlayerManager* manager = player->manager.ref();
	Ability ability = manager->abilities[index];

	if (ability == Ability::None || !control->movement_enabled())
		return;

	Awk* awk = control->get<Awk>();

	b8 current = u.input->get(binding, gamepad);
	b8 last = u.last_input->get(binding, gamepad);
	if (awk->current_ability == ability)
	{
		// cancel current spawn ability
		if (current && !last)
			ability_cancel(awk);
		else if (!Game::cancel_event_eaten[gamepad]
			&& u.input->get(Controls::Cancel, gamepad) && !u.last_input->get(Controls::Cancel, gamepad))
		{
			Game::cancel_event_eaten[gamepad] = true;
			ability_cancel(awk);
		}
	}
	else
	{
		// select new spawn ability
		if (current && !last)
		{
			if (manager->ability_valid(ability))
			{
				b8 ability_already_selected = awk->current_ability != Ability::None;
				if (ability_already_selected)
					ability_cancel(awk);
				if (!ability_already_selected || !Settings::gamepads[gamepad].bindings[(s32)binding].overlaps(Settings::gamepads[gamepad].bindings[(s32)Controls::Cancel]))
					ability_select(awk, ability);
			}
			else
			{
				// for whatever reason, this ability is invalid
				if (awk->current_ability == ability)
					ability_cancel(awk);
			}
		}
	}
}

void LocalPlayerControl::update(const Update& u)
{
	Camera* camera = player.ref()->camera;
	{
		// zoom
		b8 zoom_pressed = u.input->get(Controls::Zoom, gamepad);
		b8 last_zoom_pressed = u.last_input->get(Controls::Zoom, gamepad);
		if (zoom_pressed && !last_zoom_pressed)
		{
			if (get<Transform>()->parent.ref() && input_enabled())
			{
				// we can actually zoom
				try_zoom = true;
				get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
			}
		}
		else if (!zoom_pressed)
		{
			if (try_zoom)
				get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
			try_zoom = false;
		}

		r32 fov_target = try_zoom ? (get<Awk>()->current_ability == Ability::Sniper ? fov_sniper : fov_zoom) : fov_default;

		if (fov < fov_target)
			fov = vi_min(fov + zoom_speed * sinf(fov) * u.time.delta, fov_target);
		else if (fov > fov_target)
			fov = vi_max(fov - zoom_speed * sinf(fov) * u.time.delta, fov_target);
	}

	// update camera projection
	{
		r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
		camera->perspective(fov, aspect, 0.02f, Game::level.skybox.far_plane);
	}

	Quat look_quat;

	if (get<Transform>()->parent.ref())
	{
		r32 gamepad_rotation_multiplier = 1.0f;

		if (input_enabled() && u.input->gamepads[gamepad].active)
		{
			// gamepad aim assist based on data from last frame
			Vec3 to_reticle = reticle.pos - camera->pos;
			r32 reticle_distance = to_reticle.length();
			to_reticle /= reticle_distance;
			for (s32 i = 0; i < target_indicators.length; i++)
			{
				const TargetIndicator indicator = target_indicators[i];
				if (indicator.type == TargetIndicator::Type::AwkVisible
					|| indicator.type == TargetIndicator::Type::AwkTracking
					|| indicator.type == TargetIndicator::Type::Energy
					|| indicator.type == TargetIndicator::Type::Minion
					|| indicator.type == TargetIndicator::Type::MinionAttacking)
				{
					Vec3 to_indicator = indicator.pos - camera->pos;
					r32 indicator_distance = to_indicator.length();
					if (indicator_distance > AWK_THIRD_PERSON_OFFSET && indicator_distance < reticle_distance + 2.5f)
					{
						to_indicator /= indicator_distance;
						if (to_indicator.dot(to_reticle) > 0.99f)
						{
							// slow down gamepad rotation if we're hovering over this target
							gamepad_rotation_multiplier = 0.6f;

							if (Game::real_time.total - last_gamepad_input_time < 0.25f)
							{
								// adjust for relative velocity
								Vec2 predicted_offset;
								{
									Vec3 me = get<Awk>()->center_lerped();
									Vec3 my_velocity = get<Awk>()->center_lerped() - last_pos;
									{
										r32 my_speed = my_velocity.length_squared();
										if (my_speed == 0.0f || my_speed > AWK_CRAWL_SPEED * 1.5f * AWK_CRAWL_SPEED * 1.5f) // don't adjust if we're going too fast or not moving
											break;
									}
									Vec3 me_predicted = me + my_velocity;

									if (indicator.velocity.length_squared() > AWK_CRAWL_SPEED * 1.5f * AWK_CRAWL_SPEED * 1.5f) // enemy moving too fast
										break;

									Vec3 target_predicted = indicator.pos + indicator.velocity * u.time.delta;
									Vec3 predicted_ray = Vec3::normalize(target_predicted - me_predicted);
									Vec2 predicted_angles(atan2f(predicted_ray.x, predicted_ray.z), -asinf(predicted_ray.y));
									predicted_offset = Vec2(LMath::angle_to(get<PlayerCommon>()->angle_horizontal, predicted_angles.x), LMath::angle_to(get<PlayerCommon>()->angle_vertical, predicted_angles.y));
								}

								Vec2 current_offset;
								{
									Vec3 current_ray = Vec3::normalize(indicator.pos - get<Transform>()->absolute_pos());
									Vec2 current_angles(atan2f(current_ray.x, current_ray.z), -asinf(current_ray.y));
									current_offset = Vec2(LMath::angle_to(get<PlayerCommon>()->angle_horizontal, current_angles.x), LMath::angle_to(get<PlayerCommon>()->angle_vertical, current_angles.y));
								}

								Vec2 adjustment(LMath::angle_to(current_offset.x, predicted_offset.x), LMath::angle_to(current_offset.y, predicted_offset.y));
								if (current_offset.x > 0 == adjustment.x > 0 // only adjust if it's an adjustment toward the target
									&& fabs(get<PlayerCommon>()->angle_vertical) < PI * 0.4f) // only adjust if we're not looking straight up or down
									get<PlayerCommon>()->angle_horizontal = LMath::angle_range(get<PlayerCommon>()->angle_horizontal + adjustment.x);
								if (current_offset.y > 0 == adjustment.y > 0) // only adjust if it's an adjustment toward the target
									get<PlayerCommon>()->angle_vertical = LMath::angle_range(get<PlayerCommon>()->angle_vertical + adjustment.y);
							}

							break;
						}
					}
				}
			}
		}

		// look
		update_camera_input(u, gamepad_rotation_multiplier);
		get<PlayerCommon>()->clamp_rotation(get<PlayerCommon>()->attach_quat * Vec3(0, 0, 1), 0.5f);
		look_quat = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

		// crawling
		{
			Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical));
			get<Awk>()->crawl(movement, u);
		}

		last_pos = get<Awk>()->center_lerped();
	}
	else
		look_quat = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

	{
		// abilities
		ability_update(u, this, Controls::Ability1, gamepad, 0);
		ability_update(u, this, Controls::Ability2, gamepad, 1);
		ability_update(u, this, Controls::Ability3, gamepad, 2);
	}

	// camera shake
	if (damage_timer > 0.0f)
	{
		damage_timer -= u.time.delta;
		if (get<Awk>()->state() == Awk::State::Crawl)
		{
			r32 shake = (damage_timer / damage_shake_time) * 0.3f;
			r32 offset = Game::time.total * 10.0f;
			look_quat = look_quat * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}
	}

	camera->rot = look_quat;
	camera_setup(entity(), camera, AWK_THIRD_PERSON_OFFSET);

	// reticle
	{
		Vec3 trace_dir = look_quat * Vec3(0, 0, 1);
		Vec3 trace_start = camera->pos + trace_dir * AWK_THIRD_PERSON_OFFSET;

		reticle.type = ReticleType::None;

		if (movement_enabled())
		{
			Vec3 trace_end = trace_start + trace_dir * (AWK_SNIPE_DISTANCE + AWK_THIRD_PERSON_OFFSET);
			RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
			Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~get<Awk>()->ally_containment_field_mask() & ~CollisionShield);

			Vec3 center = get<Transform>()->absolute_pos();

			if (ray_callback.hasHit())
			{
				reticle.pos = ray_callback.m_hitPointWorld;
				reticle.normal = ray_callback.m_hitNormalWorld;
				reticle.entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
				Vec3 detach_dir = reticle.pos - center;
				r32 distance = detach_dir.length();
				detach_dir /= distance;
				if (get<Awk>()->current_ability == Ability::None) // normal movement
				{
					if (get<Awk>()->direction_is_toward_attached_wall(detach_dir))
						reticle.type = ReticleType::Dash;
					else
					{
						Vec3 hit;
						b8 hit_target;
						if (get<Awk>()->can_shoot(detach_dir, &hit, &hit_target))
						{
							if ((hit - center).length() > distance - AWK_RADIUS)
								reticle.type = hit_target ? ReticleType::Target : ReticleType::Normal;
						}
					}
				}
				else // spawning an ability
				{
					Vec3 hit;
					b8 hit_target;
					if (get<Awk>()->can_spawn(get<Awk>()->current_ability, detach_dir, &hit, nullptr, &hit_target))
					{
						if (get<Awk>()->current_ability == Ability::Sniper && hit_target)
							reticle.type = ReticleType::Target;
						else if ((hit - Vec3(ray_callback.m_hitPointWorld)).length_squared() < AWK_RADIUS * AWK_RADIUS)
							reticle.type = ReticleType::Normal;
					}
				}
			}
			else
			{
				reticle.pos = trace_end;
				reticle.normal = -trace_dir;
				reticle.entity = nullptr;
				if (get<Awk>()->current_ability == Ability::None && get<Awk>()->direction_is_toward_attached_wall(reticle.pos - center))
					reticle.type = ReticleType::Dash;
			}
		}
		else
		{
			reticle.pos = trace_start + trace_dir * AWK_THIRD_PERSON_OFFSET;
			reticle.normal = -trace_dir;
			reticle.entity = nullptr;
		}
	}

	// collect target indicators
	target_indicators.length = 0;

	Vec3 me = get<Transform>()->absolute_pos();
	AI::Team team = get<AIAgent>()->team;

	// awk indicators
	if (target_indicators.length < target_indicators.capacity())
	{
		for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
		{
			if (other_player.item()->get<AIAgent>()->team != team)
			{
				b8 visible, tracking;
				determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

				if (tracking || visible)
				{
					if (!add_target_indicator(other_player.item()->get<Target>(), tracking ? TargetIndicator::Type::AwkTracking : TargetIndicator::Type::AwkVisible))
						break; // no more room for indicators
				}
			}
		}
	}

	// headshot indicators
	if (target_indicators.length < target_indicators.capacity())
	{
		for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<AIAgent>()->team != team)
			{
				TargetIndicator::Type type = i.item()->get<MinionAI>()->goal.entity.ref() == entity() ? TargetIndicator::Type::MinionAttacking : TargetIndicator::Type::Minion;
				if (!add_target_indicator(i.item()->get<Target>(), type))
					break; // no more room for indicators
			}
		}
	}

	// health pickups
	{
		b8 full_health = get<Health>()->hp == get<Health>()->hp_max;
		for (auto i = EnergyPickup::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team != team)
			{
				if (!add_target_indicator(i.item()->get<Target>(), TargetIndicator::Type::Energy))
					break; // no more room for indicators
			}
		}
	}

	{
		b8 primary_pressed = u.input->get(Controls::Primary, gamepad);
		if (primary_pressed && !u.last_input->get(Controls::Primary, gamepad))
			try_primary = true;
		else if (!primary_pressed)
			try_primary = false;
	}

	if (reticle.type == ReticleType::None || !get<Awk>()->cooldown_can_shoot())
	{
		// can't shoot
		if (u.input->get(Controls::Primary, gamepad)) // player is mashing the fire button; give them some feedback
		{
			if (reticle.type == ReticleType::Dash)
				reticle.type = ReticleType::DashError;
			else
				reticle.type = ReticleType::Error;
		}
	}
	else
	{
		// we're aiming at something
		if (try_primary)
		{
			Vec3 dir = reticle.pos - get<Transform>()->absolute_pos();
			if (reticle.type == ReticleType::Dash)
			{
				if (get<Awk>()->dash_start(dir))
				{
					get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					try_primary = false;
					try_zoom = false;
				}
			}
			else
			{
				Ability ability = get<Awk>()->current_ability;
				if (get<Awk>()->go(dir))
				{
					try_primary = false;
					if (ability == Ability::Sniper)
						rumble = vi_max(rumble, 0.5f);
					else
					{
						try_zoom = false;
						if (ability == Ability::None)
							get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					}
				}
			}
		}
	}

	// rumble
	if (rumble > 0.0f)
	{
		u.input->gamepads[gamepad].rumble = vi_min(1.0f, rumble);
		rumble = vi_max(0.0f, rumble - u.time.delta);
	}
}

void LocalPlayerControl::draw_alpha(const RenderParams& params) const
{
	if (params.technique != RenderTechnique::Default || params.camera != player.ref()->camera)
		return;

	if (Team::game_over)
		return;

	const Rect2& viewport = params.camera->viewport;

	r32 range = get<Awk>()->range();

	AI::Team team = get<AIAgent>()->team;

	// target indicators
	{
		for (s32 i = 0; i < target_indicators.length; i++)
		{
			const TargetIndicator& indicator = target_indicators[i];
			switch (indicator.type)
			{
				case TargetIndicator::Type::AwkVisible:
				{
					UI::indicator(params, indicator.pos, UI::color_alert, false);
					break;
				}
				case TargetIndicator::Type::AwkTracking:
				{
					UI::indicator(params, indicator.pos, UI::color_alert, true);
					break;
				}
				case TargetIndicator::Type::Energy:
				{
					UI::indicator(params, indicator.pos, UI::color_accent, true, 1.0f, PI);
					break;
				}
				case TargetIndicator::Type::Minion:
				{
					UI::indicator(params, indicator.pos, UI::color_alert, true);
					break;
				}
				case TargetIndicator::Type::MinionAttacking:
				{
					if (UI::flash_function(Game::real_time.total))
					{
						if (!UI::flash_function(Game::real_time.total - Game::real_time.delta))
							Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
						UI::indicator(params, indicator.pos, UI::color_alert, true);
					}
					break;
				}
			}
		}
	}

	b8 enemy_visible = false;
	b8 enemy_close = false;

	{
		Vec3 me = get<Transform>()->absolute_pos();

		// minion cooldown bars
		for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->attack_timer > 0.0f)
			{
				Vec3 head = i.item()->head_pos();
				if ((head - me).length_squared() < range * range)
				{
					AI::Team minion_team = i.item()->get<AIAgent>()->team;

					if (minion_team != team)
						enemy_visible = true;

					// if the minion is on our team, we can let the indicator go offscreen
					// if it's an enemy minion, clamp the indicator inside the screen
					Vec2 p;
					if (UI::is_onscreen(params, head, &p) || minion_team != team)
					{
						Vec2 bar_size(40.0f * UI::scale, 8.0f * UI::scale);
						Rect2 bar = { p + Vec2(0, 40.0f * UI::scale) + (bar_size * -0.5f), bar_size };
						UI::box(params, bar, UI::color_background);
						const Vec4& color = Team::ui_color(team, minion_team);
						UI::border(params, bar, 2, color);
						UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (i.item()->attack_timer / MINION_ATTACK_TIME)), bar.size.y) }, color);
					}
				}
			}
		}

		// force field battery bars
		for (auto i = ContainmentField::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();
			if ((pos - me).length_squared() < range * range)
			{
				Vec2 p;
				if (UI::project(params, pos, &p))
				{
					Vec2 bar_size(40.0f * UI::scale, 8.0f * UI::scale);
					Rect2 bar = { p + Vec2(0, 40.0f * UI::scale) + (bar_size * -0.5f), bar_size };
					UI::box(params, bar, UI::color_background);
					const Vec4& color = Team::ui_color(team, i.item()->team);
					UI::border(params, bar, 2, color);
					UI::box(params, { bar.pos, Vec2(bar.size.x * (i.item()->remaining_lifetime / CONTAINMENT_FIELD_LIFETIME), bar.size.y) }, color);
				}
			}
		}
	}

	// highlight enemy rockets
	{
		for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->target.ref() == entity())
			{
				enemy_visible = true;

				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				UI::indicator(params, pos, Team::ui_color_enemy, true);

				UIText text;
				text.color = Team::ui_color_enemy;
				text.text(_(strings::rocket_incoming));
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.size = text_size;
				Vec2 p;
				UI::is_onscreen(params, pos, &p);
				p.y += text_size * 2.0f * UI::scale;
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
				if (UI::flash_function(Game::real_time.total))
					text.draw(params, p);
			}
		}
	}

	Vec3 me = get<Transform>()->absolute_pos();

	{
		PlayerManager* manager = player.ref()->manager.ref();

		// highlight upgrade point if there is an upgrade available
		if (Game::level.has_feature(Game::FeatureLevel::Abilities)
			&& manager->upgrade_available()
			&& !manager->at_upgrade_point())
		{
			Vec3 pos = manager->team.ref()->player_spawn.ref()->get<Transform>()->absolute_pos();
			UI::indicator(params, pos, Team::ui_color_friend, true);

			UIText text;
			text.color = Team::ui_color_friend;
			text.text(_(strings::upgrade_notification));
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 p;
			UI::is_onscreen(params, pos, &p);
			p.y += text_size * 2.0f * UI::scale;
			UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
			if (UI::flash_function_slow(Game::real_time.total))
				text.draw(params, p);
		}

		// highlight control points
		for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->capture_timer > 0.0f || i.item()->team == team || i.item()->team == AI::TeamNone)
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				UI::indicator(params, pos, Team::ui_color(team, i.item()->team), i.item()->capture_timer > 0.0f);
				if (i.item()->capture_timer > 0.0f)
				{
					// control point is being captured; show progress bar
					Rect2 bar;
					{
						Vec2 p;
						UI::is_onscreen(params, pos, &p);
						Vec2 bar_size(80.0f * UI::scale, (UI_TEXT_SIZE_DEFAULT + 12.0f) * UI::scale);
						bar = { p + Vec2(0, 40.0f * UI::scale) + (bar_size * -0.5f), bar_size };
						UI::box(params, bar, UI::color_background);
						const Vec4* color;
						r32 percentage;
						if (i.item()->capture_timer > CONTROL_POINT_CAPTURE_TIME * 0.5f)
						{
							color = &Team::ui_color(team, i.item()->team);
							percentage = (i.item()->capture_timer / (CONTROL_POINT_CAPTURE_TIME * 0.5f)) - 1.0f;
						}
						else
						{
							color = &Team::ui_color(team, i.item()->team_next);
							percentage = 1.0f - (i.item()->capture_timer / (CONTROL_POINT_CAPTURE_TIME * 0.5f));
						}
						UI::border(params, bar, 2, *color);
						UI::box(params, { bar.pos, Vec2(bar.size.x * percentage, bar.size.y) }, *color);
					}

					{
						UIText text;
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Min;
						text.color = i.item()->team_next == team ? UI::color_accent : UI::color_alert;
						text.text(_(team == i.item()->team_next ? strings::hacking : strings::losing));

						Vec2 p = bar.pos + Vec2(bar.size.x * 0.5f, bar.size.y + 10.0f * UI::scale);
						UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
						text.draw(params, p);
					}
				}
			}
		}
	}

	// usernames directly over players' 3D positions
	for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
	{
		if (other_player.item() != get<PlayerCommon>())
		{
			b8 visible;
			b8 tracking;
			determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

			b8 friendly = other_player.item()->get<AIAgent>()->team == team;

			if (!friendly)
			{
				if (visible)
					enemy_visible = true;
				if ((other_player.item()->get<Transform>()->absolute_pos() - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
					enemy_close = true;
			}

			b8 draw;
			Vec2 p;
			const Vec4* color;

			Team::SensorTrackHistory history;
			if (friendly)
			{
				Team::extract_history(other_player.item()->manager.ref(), &history);
				Vec3 pos3d = history.pos + Vec3(0, AWK_RADIUS * 2.0f, 0);
				draw = UI::project(params, pos3d, &p);
				color = &Team::ui_color_friend;
			}
			else
			{
				history = Team::list[(s32)team].player_track_history[other_player.item()->manager.id];

				// highlight the username and draw it even if it's offscreen
				if (tracking || visible)
				{
					if (team == other_player.item()->get<AIAgent>()->team) // friend
						color = &Team::ui_color_friend;
					else // enemy
						color = &Team::ui_color_enemy;

					// if we can see or track them, the indicator has already been added using add_target_indicator in the update function

					Vec3 pos3d = history.pos + Vec3(0, AWK_RADIUS * 2.0f, 0);
					draw = UI::project(params, pos3d, &p);
				}
				else
				{
					// not visible or tracking right now
					draw = false;
				}
			}

			if (draw)
			{
				Vec2 username_pos = p;
				username_pos.y += text_size * UI::scale;

				UIText username;
				username.size = text_size;
				username.anchor_x = UIText::Anchor::Center;
				username.anchor_y = UIText::Anchor::Min;
				username.color = *color;
				username.text(other_player.item()->manager.ref()->username);

				UI::box(params, username.rect(username_pos).outset(HP_BOX_SPACING), UI::color_background);

				username.draw(params, username_pos);
			}
		}
	}

	const Health* health = get<Health>();

	b8 is_vulnerable = !get<AIAgent>()->stealth && get<Awk>()->invincible_timer == 0.0f && health->hp == 1 && health->shield == 0;

	// compass
	{
		Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);
		if (is_vulnerable && get<Awk>()->incoming_attacker())
		{
			// we're being attacked; flash the compass
			b8 show = UI::flash_function(Game::real_time.total);
			if (show)
				UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_alert);
			if (show && !UI::flash_function(Game::real_time.total - Game::real_time.delta))
				Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
		}
		else if (enemy_visible)
			UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_alert);
		else if (enemy_close)
			UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_accent);
	}

	if (Game::level.has_feature(Game::FeatureLevel::EnergyPickups))
	{
		// danger indicator
		b8 danger = enemy_visible && is_vulnerable;
		if (danger)
		{
			UIText text;
			text.size = 24.0f;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;

			text.text(_(strings::danger));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.2f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (UI::flash_function(Game::real_time.total))
				text.draw(params, pos);
		}

		// shield indicator
		if (is_vulnerable)
		{
			UIText text;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.text(_(strings::shield_down));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.1f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (danger)
			{
				if (UI::flash_function(Game::real_time.total))
					text.draw(params, pos);
			}
			else
			{
				if (UI::flash_function_slow(Game::real_time.total))
					text.draw(params, pos);
			}
		}
	}

	// stealth indicator
	if (get<AIAgent>()->stealth)
	{
		UIText text;
		text.color = UI::color_accent;
		text.text(_(strings::stealth));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.7f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
		text.draw(params, pos);
	}

	// invincibility indicator
	{
		r32 invincible_timer = get<Awk>()->invincible_timer;
		if (invincible_timer > 0.0f)
		{
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { viewport.size * Vec2(0.5f, 0.75f) + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::color_background);
			UI::border(params, bar, 2, UI::color_accent);
			UI::box(params, { bar.pos, Vec2(bar.size.x * (invincible_timer / AWK_INVINCIBLE_TIME), bar.size.y) }, UI::color_accent);

			UIText text;
			text.size = 18.0f;
			text.color = UI::color_background;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(strings::invincible));
			text.draw(params, bar.pos + bar.size * 0.5f);
		}
	}

	// detect danger
	{
		r32 detect_danger = get<PlayerCommon>()->detect_danger();
		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.4f) + Vec2(0.0f, -32.0f * UI::scale);
		if (detect_danger == 1.0f)
		{
			UIText text;
			text.size = 18.0f;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;

			text.text(_(strings::enemy_tracking));

			Rect2 box = text.rect(pos).outset(6 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (UI::flash_function_slow(Game::real_time.total))
				text.draw(params, pos);
		}
		else if (detect_danger > 0.0f)
		{
			// draw bar
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { pos + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::color_background);
			UI::border(params, bar, 2, UI::color_alert);
			UI::box(params, { bar.pos, Vec2(bar.size.x * detect_danger, bar.size.y) }, UI::color_alert);

			UIText text;
			text.size = 18.0f;
			text.color = UI::color_background;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(strings::enemy_tracking));
			text.draw(params, bar.pos + bar.size * 0.5f);

			// todo: sound
		}
	}

	// reticle
	if (movement_enabled())
	{
		Vec2 pos = viewport.size * Vec2(0.5f, 0.5f);
		const r32 spoke_length = 10.0f;
		const r32 spoke_width = 3.0f;
		const r32 start_radius = 8.0f + spoke_length * 0.5f;

		b8 cooldown_can_go = get<Awk>()->cooldown_can_shoot();

		const Vec4* color;
		if (reticle.type == ReticleType::Error || reticle.type == ReticleType::DashError)
			color = &UI::color_disabled;
		else if (reticle.type != ReticleType::None
			&& cooldown_can_go
			&& (get<Awk>()->current_ability == Ability::None || player.ref()->manager.ref()->ability_valid(get<Awk>()->current_ability)))
			color = &UI::color_accent;
		else
			color = &UI::color_alert;

		// cooldown indicator
		{
			s32 charges = get<Awk>()->charges;
			if (charges == 0)
				UI::triangle_border(params, { pos, Vec2((start_radius + spoke_length) * (2.5f + 5.0f * (get<Awk>()->cooldown / AWK_COOLDOWN)) * UI::scale) }, spoke_width, UI::color_alert, PI);
			else if (get<Awk>()->current_ability == Ability::None)
			{
				const Vec2 box_size = Vec2(10.0f) * UI::scale;
				for (s32 i = 0; i < charges; i++)
				{
					Vec2 p = pos + Vec2(0.0f, -36.0f + i * -16.0f) * UI::scale;
					UI::triangle(params, { p, box_size }, *color, PI);
				}
			}
		}

		if (reticle.type == ReticleType::Dash || reticle.type == ReticleType::DashError)
		{
			Vec2 a;
			if (UI::project(params, reticle.pos, &a))
				UI::mesh(params, Asset::Mesh::reticle_dash, a, Vec2(10.0f * UI::scale), *color);
		}
		else
		{
			const r32 ratio = 0.8660254037844386f;
			UI::centered_box(params, { pos + Vec2(ratio, 0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, *color, PI * 0.5f * 0.33f);
			UI::centered_box(params, { pos + Vec2(-ratio, 0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, *color, PI * 0.5f * -0.33f);
			UI::centered_box(params, { pos + Vec2(0, -1.0f) * UI::scale * start_radius, Vec2(spoke_width, spoke_length) * UI::scale }, *color);

			if (get<Awk>()->current_ability != Ability::None)
			{
				Vec2 p = pos + Vec2(0, -48.0f * UI::scale);
				UI::centered_box(params, { p, Vec2(34.0f * UI::scale) }, UI::color_background);
				UI::mesh(params, AbilityInfo::list[(s32)get<Awk>()->current_ability].icon, p, Vec2(18.0f * UI::scale), *color);

				// cancel prompt
				UIText text;
				text.color = UI::color_accent;
				text.text(_(strings::prompt_cancel));
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Max;
				text.size = text_size;
				p = pos + Vec2(0, -96.0f * UI::scale);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, p);
			}

			if (reticle.type == ReticleType::Normal || reticle.type == ReticleType::Target)
			{
				Vec2 a;
				if (UI::project(params, reticle.pos, &a))
					UI::triangle(params, { a, Vec2(10.0f * UI::scale) }, reticle.type == ReticleType::Target ? UI::color_alert : *color, PI);
			}
		}
	}

	// buy period indicator
	if (Game::level.has_feature(Game::FeatureLevel::Abilities) && Game::time.total < GAME_BUY_PERIOD)
	{
		UIText text;
		text.color = UI::color_accent;
		text.text(_(strings::buy_period), (s32)(GAME_BUY_PERIOD - Game::time.total) + 1);
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.25f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
		text.draw(params, pos);
	}
}


}
