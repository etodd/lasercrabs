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
#include "penelope.h"

namespace VI
{


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
#define third_person_offset 2.0f
#define score_summary_delay 2.0f
#define score_summary_accept_delay 3.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u16 hp, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return scale * ((hp - 1) * (box_size.x + HP_BOX_SPACING) - HP_BOX_SPACING);
}

void draw_hp_box(const RenderParams& params, const Vec2& pos, u16 hp_max, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE * scale;

	r32 total_width = hp_width(hp_max, scale);

	UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, 0), Vec2(total_width, box_size.y)).outset(HP_BOX_SPACING), UI::background_color);
}

void draw_hp_indicator(const RenderParams& params, Vec2 pos, u16 hp, u16 hp_max, const Vec4& color, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE * scale;
	r32 total_width = hp_width(hp_max, scale);
	pos.x += total_width * -0.5f + HP_BOX_SPACING * scale;
	pos.y += box_size.y * 0.6f;

	for (s32 i = 1; i < hp_max; i++)
	{
		UI::triangle_border(params, { pos, box_size }, 3 * scale, color, PI);
		if (i < hp)
			UI::triangle(params, { pos, box_size }, color, PI);
		pos.x += box_size.x + HP_BOX_SPACING * scale;
	}
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
	score_summary_scroll()
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
	msg_text.color = good ? UI::accent_color : UI::alert_color;
	msg_timer = 0.0f;
	msg_good = good;
}

void LocalPlayer::awake(const Update& u)
{
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
	camera->perspective(60.0f * PI * 0.5f / 180.0f, aspect, 1.0f, Game::level.skybox.far_plane);
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

	// noclip
	if (Game::level.continue_match_after_death)
	{
		r32 s = speed_mouse * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta;
		angle_horizontal -= (r32)u.input->cursor_x * s;
		angle_vertical += (r32)u.input->cursor_y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);

		if (u.input->gamepads[gamepad].active)
		{
			r32 s = speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta;
			angle_horizontal -= Input::dead_zone(u.input->gamepads[gamepad].right_x) * s;
			angle_vertical += Input::dead_zone(u.input->gamepads[gamepad].right_y) * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
		}

		if (angle_vertical < PI * -0.495f)
			angle_vertical = PI * -0.495f;
		if (angle_vertical > PI * 0.495f)
			angle_vertical = PI * 0.495f;

		Quat look_quat = Quat::euler(0, angle_horizontal, angle_vertical);
		camera->rot = look_quat;
		camera->range = 0;
		camera->cull_range = 0;

		if (!Console::visible)
		{
			r32 speed = u.input->keys[(s32)KeyCode::LShift] ? 24.0f : 4.0f;
			if (u.input->get(Controls::Up, gamepad))
				camera->pos += Vec3(0, 1, 0) * u.time.delta * speed;
			if (u.input->get(Controls::Down, gamepad))
				camera->pos += Vec3(0, -1, 0) * u.time.delta * speed;
			if (u.input->get(Controls::Forward, gamepad))
				camera->pos += look_quat * Vec3(0, 0, 1) * u.time.delta * speed;
			if (u.input->get(Controls::Backward, gamepad))
				camera->pos += look_quat * Vec3(0, 0, -1) * u.time.delta * speed;
			if (u.input->get(Controls::Right, gamepad))
				camera->pos += look_quat * Vec3(-1, 0, 0) * u.time.delta * speed;
			if (u.input->get(Controls::Left, gamepad))
				camera->pos += look_quat * Vec3(1, 0, 0) * u.time.delta * speed;

			if (u.input->keys[(s32)KeyCode::MouseLeft] && !u.last_input->keys[(s32)KeyCode::MouseLeft])
			{
				static const Vec3 scale = Vec3(0.1f, 0.2f, 0.1f);
				Entity* box = World::create<PhysicsEntity>(Asset::Mesh::cube, camera->pos + look_quat * Vec3(0, 0, 0.25f), look_quat, RigidBody::Type::Box, scale, 1.0f, btBroadphaseProxy::AllFilter, btBroadphaseProxy::AllFilter);
				box->get<RigidBody>()->btBody->setLinearVelocity(look_quat * Vec3(0, 0, 15));
			}
		}
	}

	if (msg_timer < msg_time)
		msg_timer += Game::real_time.delta;

	// close/open pause menu if needed
	{
		b8 pause_hit = Game::time.total > 0.5f
			&& u.last_input->get(Controls::Pause, gamepad)
			&& !u.input->get(Controls::Pause, gamepad)
			&& (!manager.ref()->entity.ref() || !manager.ref()->entity.ref()->get<Awk>()->snipe) // HACK because cancel and pause are on the same dang key
			&& !Game::cancel_event_eaten[gamepad];
		if (pause_hit
			&& !upgrade_menu_open
			&& (menu_state == Menu::State::Hidden || menu_state == Menu::State::Visible)
			&& !Penelope::has_focus())
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
			if (manager.ref()->at_control_point() && manager.ref()->can_transition_state())
			{
				if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
				{
					if (manager.ref()->friendly_control_point(manager.ref()->at_control_point()))
					{
						// friendly control point; upgrade
						upgrade_menu_open = true;
						menu.animate();
						upgrade_animation_time = Game::real_time.total;
					}
					else
					{
						// enemy control point; capture
						manager.ref()->capture_start();
					}
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

				menu.start(u, camera->viewport, gamepad, (s32)Upgrade::count + 1);

				Vec2 pos(camera->viewport.size.x * 0.5f + MENU_ITEM_WIDTH * -0.5f, camera->viewport.size.y * 0.8f);

				if (menu.item(u, &pos, _(strings::close), nullptr))
					upgrade_menu_open = false;

				for (s32 i = 0; i < (s32)Upgrade::count; i++)
				{
					Upgrade upgrade = (Upgrade)i;
					b8 can_upgrade = !upgrade_in_progress
						&& manager.ref()->upgrade_available(upgrade)
						&& manager.ref()->credits >= manager.ref()->upgrade_cost(upgrade);
					const UpgradeInfo& info = UpgradeInfo::list[(s32)upgrade];
					if (menu.item(u, &pos, _(info.name), nullptr, !can_upgrade, info.icon))
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
			Menu::pause_menu(u, camera->viewport, gamepad, &menu, &menu_state);
			break;
		}
		case UIMode::Dead:
		{
			break;
		}
		case UIMode::GameOver:
		{
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

	UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, icon_size * -0.5f), Vec2(total_width, icon_size)).outset(padding), UI::background_color);
	UI::mesh(params, icon, pos + Vec2(total_width * -0.5f + icon_size - padding, 0), Vec2(icon_size), text.color);
	text.draw(params, pos + Vec2(total_width * -0.5f + icon_size + padding, 0));
}

void draw_ability(const RenderParams& params, PlayerManager* manager, const Vec2& pos, Ability ability, AssetID icon, const char* control_binding)
{
	char string[255];

	u16 cost = AbilityInfo::list[(s32)ability].spawn_cost;
	sprintf(string, "%s", control_binding);
	const Vec4* color;
	if (manager->current_spawn_ability == ability)
		color = &UI::default_color;
	else if (manager->credits >= cost)
		color = &UI::accent_color;
	else
		color = &UI::alert_color;
	draw_icon_text(params, pos, icon, string, *color);
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
		draw_icon_text(params, credits_pos, Asset::Mesh::icon_credits, buffer, draw ? (flashing ? UI::default_color : UI::accent_color) : UI::background_color);

		// control point increment amount
		{
			r32 icon_size = text_size * UI::scale;
			r32 padding = 8 * UI::scale;

			UIText text;
			text.color = UI::accent_color;
			text.text("+%d", manager.ref()->increment());
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;

			r32 total_width = icon_size + padding + text.bounds().x;

			Vec2 pos = credits_pos + Vec2(0, text_size * UI::scale * -2.0f);
			UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, icon_size * -0.5f), Vec2(total_width, icon_size)).outset(padding), UI::background_color);
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
		if (mode == UIMode::Default
			&& manager.ref()->at_control_point()
			&& manager.ref()->can_transition_state())
		{
			UIText text;
			if (manager.ref()->friendly_control_point(manager.ref()->at_control_point()))
			{
				// "upgrade!" prompt
				text.color = manager.ref()->upgrade_available() ? UI::accent_color : UI::disabled_color;
				text.text(_(strings::upgrade_prompt));
			}
			else
			{
				// "capture!" prompt
				text.color = UI::accent_color;
				text.text(_(strings::capture_prompt));
			}
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 pos = vp.size * Vec2(0.5f, 0.55f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
			text.draw(params, pos);
		}

		if ((mode == UIMode::Default || mode == UIMode::Upgrading)
			&& manager.ref()->can_transition_state())
		{
			// draw abilities

			b8 is_gamepad = params.sync->input.gamepads[gamepad].active;

			// ability 1
			{
				Ability ability = manager.ref()->abilities[0];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability1].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					draw_ability(params, manager.ref(), center + Vec2(-radius, 0), ability, info.icon, binding);
				}
			}

			// ability 2
			{
				Ability ability = manager.ref()->abilities[1];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability2].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					draw_ability(params, manager.ref(), center + Vec2(0, radius * 0.5f), ability, info.icon, binding);
				}
			}

			// ability 3
			{
				Ability ability = manager.ref()->abilities[2];
				if (ability != Ability::None)
				{
					const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability3].string(is_gamepad);
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					draw_ability(params, manager.ref(), center + Vec2(radius, 0), ability, info.icon, binding);
				}
			}
		}
	}

	if (mode == UIMode::Upgrading)
	{
		menu.draw_alpha(params);

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
				text.color = UI::accent_color;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
				u16 cost = manager.ref()->upgrade_cost(upgrade);
				text.text(_(strings::upgrade_description), cost, _(info.description));
				UIMenu::text_clip(&text, upgrade_animation_time, 150.0f);

				const Rect2& last_item = menu.last_visible_item()->rect();
				Vec2 pos(last_item.pos.x + padding, last_item.pos.y - MENU_ITEM_HEIGHT - padding * 2.0f);
				UI::box(params, text.rect(pos).outset(padding), UI::background_color);
				text.draw(params, pos);
			}
		}
	}
	else if (mode == UIMode::Dead)
	{
		// if we haven't spawned yet, or if other players are still playing, then show the player list
		b8 spawning = manager.ref()->spawn_timer > 0.0f;
		if (spawning || !Team::game_over)
		{
			Vec2 p = vp.size * Vec2(0.5f);

			UIText text;
			text.size = text_size;
			text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.color = UI::default_color;

			if (spawning)
			{
				// "spawning..."
				text.text(_(strings::deploy_timer), (s32)manager.ref()->spawn_timer + 1);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
			}

			// show map name
			text.text("%s", AssetLookup::Level::names[Game::state.level]);
			text.color = UI::accent_color;
			UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
			text.draw(params, p);
			p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;

			// show player list
			text.anchor_x = UIText::Anchor::Min;
			p.x -= (MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f) * 0.5f;
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				text.text(i.item()->username);
				text.color = Team::ui_color(manager.ref()->team.ref()->team(), i.item()->team.ref()->team());
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
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
			text.color = UI::accent_color;
			text.text(_(strings::victory));
		}
		else if (!winner) // it's a draw
		{
			text.color = UI::alert_color;
			text.text(_(strings::draw));
		}
		else // we lost
		{
			text.color = UI::alert_color;
			text.text(_(strings::defeat));
		}
		UIMenu::text_clip(&text, Team::game_over_real_time, 20.0f);

		b8 show_score_summary = Game::real_time.total - Team::game_over_real_time > score_summary_delay;
		Vec2 title_pos = show_score_summary
			? vp.size * Vec2(0.5f, 1.0f) + Vec2(0, (text.size + 32) * -UI::scale)
			: vp.size * Vec2(0.5f, 0.5f);
		UI::box(params, text.rect(title_pos).outset(16 * UI::scale), UI::background_color);
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

			score_summary_scroll.start(params, p + Vec2(0, MENU_ITEM_PADDING + MENU_ITEM_HEIGHT * 0.5f));
			s32 item_counter = 0;
			for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
			{
				text.color = player.item() == manager.ref() ? UI::accent_color : Team::ui_color(manager.ref()->team.ref()->team(), player.item()->team.ref()->team());

				UIText amount = text;
				amount.anchor_x = UIText::Anchor::Max;
				amount.wrap_width = 0;

				// username
				if (score_summary_scroll.item(item_counter))
				{
					text.text(player.item()->username);
					UIMenu::text_clip(&text, Team::game_over_real_time + score_summary_delay, 50.0f + (r32)vi_min(item_counter, 6) * -5.0f);
					UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
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
						UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
						text.draw(params, p);
						amount.text("%d", credits_summary[i].amount);
						amount.draw(params, p + Vec2(MENU_ITEM_WIDTH * 0.5f - MENU_ITEM_PADDING, 0));
						p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
					}
					item_counter++;
				}
			}
			score_summary_scroll.end(params, p);

			// press x to continue
			if (Game::real_time.total - Team::game_over_real_time > score_summary_delay + score_summary_accept_delay)
			{
				Vec2 p = vp.size * Vec2(0.5f, 0.2f);
				text.wrap_width = 0;
				text.color = UI::accent_color;
				text.text(_(manager.ref()->score_accepted ? strings::waiting : strings::accept));
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
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
					case PlayerManager::State::Spawning:
					{
						// spawning an ability
						string = strings::ability_spawn_cost;

						const AbilityInfo& info = AbilityInfo::list[(s32)manager.ref()->current_spawn_ability];
						cost = info.spawn_cost;
						total_time = info.spawn_time;
						break;
					}
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
						string = strings::capturing;
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
				Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
				Rect2 bar = { pos + bar_size * -0.5f, bar_size };
				UI::box(params, bar, UI::background_color);
				UI::border(params, bar, 2, UI::accent_color);
				UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (manager.ref()->state_timer / total_time)), bar.size.y) }, UI::accent_color);

				UIText text;
				text.size = 18.0f;
				text.color = UI::background_color;
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

			UI::box(params, Rect2(p, box).outset(padding), UI::background_color);

			Vec2 icon_pos = p + Vec2(0.75f, 0.5f) * text_size * UI::scale;

			AssetID icon;
			const Vec4* color;
			if (remaining > GAME_TIME_LIMIT * 0.8f)
			{
				icon = Asset::Mesh::icon_battery_3;
				color = &UI::default_color;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.6f)
			{
				icon = Asset::Mesh::icon_battery_2;
				color = &UI::default_color;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.4f)
			{
				icon = Asset::Mesh::icon_battery_1;
				color = &UI::accent_color;
			}
			else if (remaining > 60.0f)
			{
				icon = Asset::Mesh::icon_battery_1;
				color = &UI::alert_color;
			}
			else
			{
				icon = Asset::Mesh::icon_battery_0;
				color = &UI::alert_color;
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
		if (Game::state.network_state == Game::NetworkState::Lag && Game::state.network_time - Game::state.network_timer > 0.25f)
			UI::mesh(params, Asset::Mesh::icon_network_error, vp.size * Vec2(0.9f, 0.5f), Vec2(text_size * 2.0f * UI::scale), UI::alert_color);
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
			UI::box(params, box, UI::background_color);
			msg_text.draw(params, pos);
			if (!last_flash)
				Audio::post_global_event(msg_good ? AK::EVENTS::PLAY_BEEP_GOOD : AK::EVENTS::PLAY_BEEP_BAD);
		}
	}

	if (mode == UIMode::Pause) // pause menu always drawn on top
		menu.draw_alpha(params);
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
		&& get<Awk>()->stun_timer == 0.0f // or while stunned
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
		return Vec3::normalize(get<LocalPlayerControl>()->reticle.pos - get<Awk>()->center());
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
	try_secondary(),
	damage_timer(),
	health_flash_timer(),
	rumble(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed()
{
}

LocalPlayerControl::~LocalPlayerControl()
{
	Audio::listener_disable(gamepad);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

void LocalPlayerControl::awake()
{
	Audio::listener_enable(gamepad);

	last_pos = get<Awk>()->center();
	link<&LocalPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_flying);
	link<&LocalPlayerControl::awk_done_flying_or_dashing>(get<Awk>()->done_dashing);
	link_arg<Entity*, &LocalPlayerControl::hit_target>(get<Awk>()->hit);
	link_arg<const DamageEvent&, &LocalPlayerControl::damaged>(get<Health>()->damaged);
	link_arg<const TargetEvent&, &LocalPlayerControl::hit_by>(get<Target>()->target_hit);
	link<&LocalPlayerControl::health_picked_up>(get<Health>()->added);
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

void LocalPlayerControl::damaged(const DamageEvent& e)
{
	health_flash_timer = msg_time; // damaged in some way; flash the HP indicator
}

void LocalPlayerControl::hit_by(const TargetEvent& e)
{
	// we were physically hit by something; shake the camera
	damage_timer = damage_shake_time;
	rumble = vi_max(rumble, 1.0f);
}

void LocalPlayerControl::health_picked_up()
{
	if (Game::time.total > PLAYER_SPAWN_DELAY + 0.5f) // if we're picking up initial health at the very beginning of the match, don't flash the message
	{
		player.ref()->msg(_(strings::hp_added), true);
		health_flash_timer = msg_time;
	}
}

b8 LocalPlayerControl::input_enabled() const
{
	return !Console::visible && player.ref()->ui_mode() == LocalPlayer::UIMode::Default && !Penelope::has_focus() && !Team::game_over;
}

b8 LocalPlayerControl::movement_enabled() const
{
	return input_enabled() && get<PlayerCommon>()->movement_enabled();
}

r32 LocalPlayerControl::look_speed() const
{
	if (try_secondary)
		return get<Awk>()->snipe ? zoom_speed_multiplier_sniper : zoom_speed_multiplier;
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
			r32 s = look_speed() * speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::real_time.delta * gamepad_rotation_multiplier;
			Vec2 adjustment = Vec2
			(
				-Input::dead_zone(u.input->gamepads[gamepad].right_x) * s,
				Input::dead_zone(u.input->gamepads[gamepad].right_y) * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f)
			);
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
			movement += Vec3(-Input::dead_zone(u.input->gamepads[gamepad].left_x), 0, 0);
			movement += Vec3(0, 0, -Input::dead_zone(u.input->gamepads[gamepad].left_y));
		}

		movement = rot * movement;

		if (u.input->get(Controls::Up, gamepad))
			movement.y += 1;
		if (u.input->get(Controls::Down, gamepad))
			movement.y -= 1;
	}
	return movement;
}

// returns false if there is no more room in the target indicator array
b8 LocalPlayerControl::add_target_indicator(Target* target, TargetIndicator::Type type)
{
	Vec3 me = get<Awk>()->center();

	b8 show_even_out_of_range = type == TargetIndicator::Type::AwkTracking;

	if (show_even_out_of_range || (target->absolute_pos() - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
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

void LocalPlayerControl::update(const Update& u)
{
	Camera* camera = player.ref()->camera;
	{
		// zoom
		b8 secondary_pressed = u.input->get(Controls::Secondary, gamepad);
		b8 last_secondary_pressed = u.last_input->get(Controls::Secondary, gamepad);
		if (secondary_pressed && !last_secondary_pressed)
		{
			if (get<Transform>()->parent.ref() && input_enabled())
			{
				// we can actually zoom
				try_secondary = true;
				get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
			}
		}
		else if (!secondary_pressed)
		{
			if (try_secondary)
				get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
			try_secondary = false;
		}

		r32 fov_target = try_secondary ? (get<Awk>()->snipe ? fov_sniper : fov_zoom) : fov_default;

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
					|| indicator.type == TargetIndicator::Type::Health
					|| indicator.type == TargetIndicator::Type::Minion
					|| indicator.type == TargetIndicator::Type::MinionAttacking)
				{
					Vec3 to_indicator = indicator.pos - camera->pos;
					r32 indicator_distance = to_indicator.length();
					if (indicator_distance > third_person_offset && indicator_distance < reticle_distance + 2.5f)
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
									Vec3 me = get<Awk>()->center();
									Vec3 my_velocity = get<Awk>()->center() - last_pos;
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
									Vec3 current_ray = Vec3::normalize(indicator.pos - get<Awk>()->center());
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
		Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical));
		get<Awk>()->crawl(movement, u);

		last_pos = get<Awk>()->center();
	}
	else
		look_quat = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

	if (input_enabled())
	{
		if (get<Awk>()->snipe)
		{
			// cancel snipe
			if (u.input->get(Controls::Cancel, gamepad)
				&& !u.last_input->get(Controls::Cancel, gamepad)
				&& !Game::cancel_event_eaten[gamepad])
			{
				Game::cancel_event_eaten[gamepad] = true;
				get<Awk>()->snipe = false;
				player.ref()->manager.ref()->add_credits(AbilityInfo::list[(s32)Ability::Sniper].spawn_cost);
			}
		}
		else
		{
			// abilities

			b8 just_attached = u.time.total - get<Awk>()->attach_time < 0.2f;
			PlayerManager* manager = player.ref()->manager.ref();
			if (manager->abilities[0] != Ability::None)
			{
				b8 current = u.input->get(Controls::Ability1, gamepad);
				b8 last = u.last_input->get(Controls::Ability1, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[0]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[0]);
			}

			if (manager->abilities[1] != Ability::None)
			{
				b8 current = u.input->get(Controls::Ability2, gamepad);
				b8 last = u.last_input->get(Controls::Ability2, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[1]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[1]);
			}

			if (manager->abilities[2] != Ability::None)
			{
				b8 current = u.input->get(Controls::Ability3, gamepad);
				b8 last = u.last_input->get(Controls::Ability3, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[2]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[2]);
			}
		}
	}

	// camera setup
	{
		Vec3 abs_wall_normal = (get<Transform>()->absolute_rot() * get<Awk>()->lerped_rotation) * Vec3(0, 0, 1);
		camera->wall_normal = look_quat.inverse() * abs_wall_normal;
		camera->pos = get<Awk>()->center() + look_quat * Vec3(0, 0, -third_person_offset);
		if (get<Transform>()->parent.ref())
		{
			camera->pos += abs_wall_normal * 0.5f;
			camera->pos.y += 0.5f - vi_min((r32)fabs(abs_wall_normal.y), 0.5f);
		}

		if (damage_timer > 0.0f)
		{
			damage_timer -= u.time.delta;
			r32 shake = (damage_timer / damage_shake_time) * 0.3f;
			r32 offset = Game::time.total * 10.0f;
			look_quat = look_quat * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}

		camera->range = get<Awk>()->snipe ? AWK_SNIPE_DISTANCE : AWK_MAX_DISTANCE;
		camera->range_center = look_quat.inverse() * (get<Awk>()->center() - camera->pos);
		camera->cull_range = third_person_offset + 0.5f;
		camera->cull_behind_wall = abs_wall_normal.dot(camera->pos - get<Awk>()->center()) < 0.0f;
	}

	health_flash_timer = vi_max(0.0f, health_flash_timer - Game::real_time.delta);

	camera->rot = look_quat;

	// reticle
	{
		Vec3 trace_dir = look_quat * Vec3(0, 0, 1);
		Vec3 trace_start = camera->pos + trace_dir * third_person_offset;

		reticle.type = ReticleType::None;

		if (movement_enabled())
		{
			r32 range = get<Awk>()->snipe ? AWK_SNIPE_DISTANCE : AWK_MAX_DISTANCE;
			Vec3 trace_end = trace_start + trace_dir * (range + third_person_offset);
			RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
			Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~get<Awk>()->ally_containment_field_mask());

			Vec3 center = get<Awk>()->center();

			if (ray_callback.hasHit())
			{
				reticle.pos = ray_callback.m_hitPointWorld;
				Vec3 detach_dir = reticle.pos - center;
				r32 distance = detach_dir.length();
				detach_dir /= distance;
				if (get<Awk>()->direction_is_toward_attached_wall(detach_dir) && !get<Awk>()->snipe)
					reticle.type = ReticleType::Dash;
				else
				{
					Vec3 hit;
					b8 hit_target;
					if (get<Awk>()->can_go(detach_dir, &hit, &hit_target))
					{
						if ((hit - center).length() > distance - AWK_RADIUS)
							reticle.type = hit_target ? ReticleType::Target : ReticleType::Normal;
					}
				}
			}
			else
			{
				reticle.pos = trace_end;
				if (get<Awk>()->direction_is_toward_attached_wall(reticle.pos - center) && !get<Awk>()->snipe)
					reticle.type = ReticleType::Dash;
			}
		}
		else
			reticle.pos = trace_start + trace_dir * third_person_offset;
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
					if (!add_target_indicator(other_player.item()->get<Target>(), visible ? TargetIndicator::Type::AwkVisible : TargetIndicator::Type::AwkTracking))
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
	if (get<Health>()->hp < get<Health>()->hp_max)
	{
		for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
		{
			Health* owner = i.item()->owner.ref();
			if (!owner || owner != get<Health>())
			{
				if (!add_target_indicator(i.item()->get<Target>(), TargetIndicator::Type::Health))
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

	if (reticle.type == ReticleType::None || !get<Awk>()->cooldown_can_go())
	{
		// can't shoot
		if (u.input->get(Controls::Primary, gamepad)) // player is mashing the fire button; give them some feedback
		{
			if (reticle.type == ReticleType::Dash)
				reticle.type = ReticleType::DashError;
			else
				reticle.type = ReticleType::Error;
			if (get<Awk>()->cooldown > 0.0f) // prevent the player from mashing the fire button to nail the cooldown skip
				get<Awk>()->disable_cooldown_skip = true;
		}
	}
	else
	{
		// we're aiming at something
		if (try_primary)
		{
			Vec3 detach_dir = reticle.pos - get<Awk>()->center();
			if (reticle.type == ReticleType::Dash)
			{
				if (get<Awk>()->dash_start(detach_dir))
				{
					get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					try_primary = false;
					try_secondary = false;
				}
			}
			else if (get<Awk>()->snipe)
			{
				if (get<Awk>()->detach(detach_dir))
				{
					rumble = vi_max(rumble, 0.5f);
					try_primary = false;
				}
			}
			else
			{
				if (get<Awk>()->detach(detach_dir))
				{
					get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					try_primary = false;
					try_secondary = false;
				}
			}
		}
	}

	Audio::listener_update(gamepad, camera->pos, camera->rot);

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
					UI::indicator(params, indicator.pos, UI::alert_color, true);
					break;
				}
				case TargetIndicator::Type::AwkTracking:
				{
					UI::indicator(params, indicator.pos, UI::accent_color, true);
					break;
				}
				case TargetIndicator::Type::Health:
				{
					UI::indicator(params, indicator.pos, UI::accent_color, true, 1.0f, PI);
					break;
				}
				case TargetIndicator::Type::Minion:
				{
					UI::indicator(params, indicator.pos, UI::alert_color, true);
					break;
				}
				case TargetIndicator::Type::MinionAttacking:
				{
					if (UI::flash_function(Game::real_time.total))
					{
						if (!UI::flash_function(Game::real_time.total - Game::real_time.delta))
							Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
						UI::indicator(params, indicator.pos, UI::alert_color, true);
					}
					break;
				}
			}
		}
	}

	b8 enemy_visible = false;

	{
		Vec3 me = get<Transform>()->absolute_pos();

		// minion cooldown bars
		for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->attack_timer > 0.0f)
			{
				Vec3 head = i.item()->head_pos();
				if ((head - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
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
						UI::box(params, bar, UI::background_color);
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
			if ((pos - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
			{
				Vec2 p;
				if (UI::project(params, pos, &p))
				{
					Vec2 bar_size(40.0f * UI::scale, 8.0f * UI::scale);
					Rect2 bar = { p + Vec2(0, 40.0f * UI::scale) + (bar_size * -0.5f), bar_size };
					UI::box(params, bar, UI::background_color);
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
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				if (UI::flash_function(Game::real_time.total))
					text.draw(params, p);
			}
		}
	}

	// highlight control points
	{
		PlayerManager* manager = player.ref()->manager.ref();
		if (Game::level.has_feature(Game::FeatureLevel::Abilities) && !manager->at_control_point())
		{
			// highlight friendly control points if there is an upgrade available
			b8 highlight_friendlies = manager->upgrade_available();

			Vec3 me = get<Transform>()->absolute_pos();
			for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->get<PlayerTrigger>()->is_triggered(entity()))
				{
					if (manager->friendly_control_point(i.item()))
					{
						if (highlight_friendlies)
						{
							Vec3 pos = i.item()->get<Transform>()->absolute_pos();
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
							UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
							if (UI::flash_function_slow(Game::real_time.total))
								text.draw(params, p);
						}
					}
					else
					{
						Vec3 pos = i.item()->get<Transform>()->absolute_pos();
						if ((pos - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
							UI::indicator(params, pos, i.item()->team == AI::NoTeam ? UI::accent_color : Team::ui_color_enemy, true);
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

			if (visible && !friendly)
				enemy_visible = true;

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

				Vec3 pos3d = history.pos + Vec3(0, AWK_RADIUS * 2.0f, 0);

				// highlight the username and draw it even if it's offscreen
				UI::is_onscreen(params, pos3d, &p);
				draw = true;
				if (tracking || visible)
				{
					if (team == other_player.item()->get<AIAgent>()->team) // friend
						color = &Team::ui_color_friend;
					else // enemy
						color = visible ? &Team::ui_color_enemy : &UI::accent_color;
				}
				else
				{
					color = &UI::disabled_color; // not visible or tracking right now
					// if we can see or track them, the indicator has already been added using add_target_indicator in the update function
					UI::indicator(params, history.pos, UI::disabled_color, true);
				}
			}

			if (draw)
			{
				Vec2 hp_pos = p;
				hp_pos.y += text_size * UI::scale;

				Vec2 username_pos = hp_pos;
				username_pos.y += text_size * UI::scale;

				UIText username;
				username.size = text_size;
				username.anchor_x = UIText::Anchor::Center;
				username.anchor_y = UIText::Anchor::Min;
				username.color = *color;
				username.text(other_player.item()->manager.ref()->username);

				UI::box(params, username.rect(username_pos).outset(HP_BOX_SPACING), UI::background_color);

				// invincible indicator
				b8 invincible = false;
				if (!friendly && (tracking || visible))
				{
					r32 enemy_invincible_timer = other_player.item()->get<Awk>()->invincible_timer;
					if (enemy_invincible_timer > 0.0f)
					{
						invincible = true;
						UIText text;
						text.size = text_size - 2.0f;
						text.color = UI::background_color;
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Center;
						text.text(_(strings::invincible));

						Vec2 bar_size = text.bounds() + Vec2(HP_BOX_SPACING);

						const Vec4& color = visible ? UI::alert_color : UI::accent_color;

						Rect2 bar = { hp_pos + Vec2(bar_size.x * -0.5f, bar_size.y * -0.5f), bar_size };
						UI::box(params, bar, UI::background_color);
						UI::border(params, bar, 2, color);
						UI::box(params, { bar.pos, Vec2(bar.size.x * (enemy_invincible_timer / AWK_INVINCIBLE_TIME), bar.size.y) }, color);

						text.draw(params, bar.pos + bar.size * 0.5f);
					}
				}

				if (!invincible && Game::level.has_feature(Game::FeatureLevel::HealthPickups))
				{
					draw_hp_box(params, hp_pos, history.hp_max, 0.75f);
					draw_hp_indicator(params, hp_pos, history.hp, history.hp_max, *color, 0.75f);
				}

				username.draw(params, username_pos);
			}
		}
	}

	b8 is_vulnerable = !get<AIAgent>()->stealth && get<Awk>()->invincible_timer == 0.0f;

	// incoming attack warning indicator
	if (is_vulnerable && get<Awk>()->incoming_attacker())
	{
		enemy_visible = true;
		// we're being attacked; flash the compass
		b8 show = UI::flash_function(Game::real_time.total);
		if (show)
		{
			Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);
			UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::alert_color);
		}
		if (show && !UI::flash_function(Game::real_time.total - Game::real_time.delta))
			Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
	}

	if (Game::level.has_feature(Game::FeatureLevel::HealthPickups))
	{
		const Health* health = get<Health>();

		// danger indicator
		b8 danger = enemy_visible && is_vulnerable && health->hp == 1;
		if (danger)
		{
			UIText text;
			text.size = 24.0f;
			text.color = UI::alert_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;

			text.text(_(strings::danger));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.2f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::background_color);
			if (UI::flash_function(Game::real_time.total))
				text.draw(params, pos);
		}

		// health indicator
		{
			Vec2 pos = viewport.size * Vec2(0.5f, 0.1f);

			draw_hp_box(params, pos, health->hp_max);

			b8 draw_hp;
			const Vec4* color;

			if (danger)
			{
				draw_hp = UI::flash_function(Game::real_time.total);
				color = &UI::alert_color;
			}
			else if (health_flash_timer > 0.0f)
			{
				draw_hp = UI::flash_function(Game::real_time.total);
				color = &UI::default_color;
			}
			else if (health->hp == 1)
			{
				draw_hp = UI::flash_function_slow(Game::real_time.total);
				color = &UI::alert_color;
			}
			else
			{
				draw_hp = true;
				color = &UI::accent_color;
			}

			if (draw_hp)
				draw_hp_indicator(params, pos, health->hp, health->hp_max, *color);
		}
	}

	// stealth indicator
	if (get<AIAgent>()->stealth)
	{
		UIText text;
		text.color = UI::accent_color;
		text.text(_(strings::stealth));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.7f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
		text.draw(params, pos);
	}

	// invincibility indicator
	{
		r32 invincible_timer = get<Awk>()->invincible_timer;
		if (invincible_timer > 0.0f)
		{
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { viewport.size * Vec2(0.5f, 0.75f) + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::background_color);
			UI::border(params, bar, 2, UI::accent_color);
			UI::box(params, { bar.pos, Vec2(bar.size.x * (invincible_timer / AWK_INVINCIBLE_TIME), bar.size.y) }, UI::accent_color);

			UIText text;
			text.size = 18.0f;
			text.color = UI::background_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(strings::invincible));
			text.draw(params, bar.pos + bar.size * 0.5f);
		}
	}

	// stunned indicator
	if (get<Awk>()->stun_timer > 0.0f)
	{
		UIText text;
		text.color = UI::alert_color;
		text.text(_(strings::stunned));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.55f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
		text.draw(params, pos);
	}

	// detect danger
	{
		r32 detect_danger = get<PlayerCommon>()->detect_danger();
		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.4f);
		if (detect_danger == 1.0f)
		{
			UIText text;
			text.size = 18.0f;
			text.color = UI::alert_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;

			text.text(_(strings::enemy_tracking));

			Rect2 box = text.rect(pos).outset(6 * UI::scale);
			UI::box(params, box, UI::background_color);
			if (UI::flash_function_slow(Game::real_time.total))
				text.draw(params, pos);
		}
		else if (detect_danger > 0.0f)
		{
			// draw bar
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { pos + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::background_color);
			UI::border(params, bar, 2, UI::alert_color);
			UI::box(params, { bar.pos, Vec2(bar.size.x * detect_danger, bar.size.y) }, UI::alert_color);

			UIText text;
			text.size = 18.0f;
			text.color = UI::background_color;
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
		// cooldown indicator
		r32 cooldown = get<Awk>()->cooldown;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.5f);
		r32 cooldown_blend = vi_max(0.0f, cooldown / AWK_MAX_DISTANCE_COOLDOWN);
		const r32 spoke_length = 10.0f;
		const r32 spoke_width = 3.0f;
		r32 start_radius = 8.0f + cooldown_blend * 32.0f + spoke_length * 0.5f;

		b8 cooldown_can_go = get<Awk>()->cooldown_can_go();
		const Vec4& color =
			(reticle.type == ReticleType::Error || reticle.type == ReticleType::DashError)
			? UI::disabled_color
			: ((reticle.type != ReticleType::None && cooldown_can_go) ? UI::accent_color : UI::alert_color);

		if (cooldown > 0.0f)
			UI::triangle_border(params, { pos, Vec2(start_radius * 6.0f * UI::scale) }, spoke_width, color);

		if (reticle.type == ReticleType::Dash || reticle.type == ReticleType::DashError)
		{
			Vec2 a;
			if (UI::project(params, reticle.pos, &a))
				UI::mesh(params, Asset::Mesh::reticle_dash, a, Vec2(10.0f * UI::scale), color);
		}
		else
		{
			const r32 ratio = 0.8660254037844386f;
			UI::centered_box(params, { pos + Vec2(ratio, -0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, color, PI * 0.5f * -0.33f);
			UI::centered_box(params, { pos + Vec2(-ratio, -0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, color, PI * 0.5f * 0.33f);
			UI::centered_box(params, { pos + Vec2(0, 1.0f) * UI::scale * start_radius, Vec2(spoke_width, spoke_length) * UI::scale }, color);

			if (get<Awk>()->snipe)
			{
				UI::mesh(params, Asset::Mesh::icon_sniper, pos + Vec2(0, -32.0f * UI::scale), Vec2(18.0f * UI::scale), color);

				// cancel prompt
				UIText text;
				text.color = UI::accent_color;
				text.text(_(strings::cancel_prompt));
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Max;
				text.size = text_size;
				Vec2 p = pos + Vec2(0, -80.0f *UI::scale);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}

			if (cooldown_can_go && (reticle.type == ReticleType::Normal || reticle.type == ReticleType::Target))
			{
				Vec2 a;
				if (UI::project(params, reticle.pos, &a))
					UI::triangle(params, { a, Vec2(10.0f * UI::scale) }, reticle.type == ReticleType::Normal ? UI::accent_color : UI::alert_color, PI);
			}
		}
	}

	// buy period indicator
	if (Game::level.has_feature(Game::FeatureLevel::Abilities) && Game::time.total < GAME_BUY_PERIOD)
	{
		UIText text;
		text.color = UI::accent_color;
		text.text(_(strings::buy_period), (s32)(GAME_BUY_PERIOD - Game::time.total) + 1);
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.25f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
		text.draw(params, pos);
	}
}


}
