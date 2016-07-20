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
#include "parkour.h"
#include "noise.h"
#include "settings.h"
#if DEBUG_AI_CONTROL
#include "ai_player.h"
#endif
#include "scripts.h"

namespace VI
{

#define fov_pvp (70.0f * PI * 0.5f / 180.0f)
#define fov_parkour (80.0f * PI * 0.5f / 180.0f)
#define zoom_ratio 0.5f
#define fov_pvp_zoom (fov_pvp * zoom_ratio)
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse 0.001f
#define speed_mouse_zoom (speed_mouse * zoom_ratio * 0.5f)
#define speed_joystick 4.0f
#define speed_joystick_zoom (speed_joystick * zoom_ratio * 0.5f)
#define gamepad_rotation_acceleration (1.0f / 0.2f)
#define attach_speed 5.0f
#define max_attach_time 0.35f
#define rotation_speed 20.0f
#define msg_time 0.75f
#define text_size 16.0f
#define damage_shake_time 0.7f
#define third_person_offset 2.0f
#define score_summary_delay 2.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u16 hp)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return ((hp - 1) * (box_size.x + HP_BOX_SPACING)) - HP_BOX_SPACING;
}

void draw_hp_box(const RenderParams& params, const Vec2& pos, u16 hp_max)
{
	const Vec2 box_size = HP_BOX_SIZE;

	r32 total_width = hp_width(hp_max);

	UI::box(params, Rect2(pos + Vec2(total_width * -0.5f, 0), Vec2(total_width, box_size.y)).outset(HP_BOX_SPACING), UI::background_color);
}

void draw_hp_indicator(const RenderParams& params, Vec2 pos, u16 hp, u16 hp_max, const Vec4& color)
{
	const Vec2 box_size = HP_BOX_SIZE;
	r32 total_width = hp_width(hp_max);
	pos.x += total_width * -0.5f + HP_BOX_SPACING;
	pos.y += box_size.y * 0.6f;

	for (s32 i = 1; i < hp_max; i++)
	{
		UI::triangle_border(params, { pos, box_size }, 3, color, PI);
		if (i < hp)
			UI::triangle(params, { pos, box_size }, color, PI);
		pos.x += box_size.x + HP_BOX_SPACING;
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
	menu_state(),
	upgrading(),
	upgrade_animation_time(),
	score_summary_scroll()
{
	sprintf(manager.ref()->username, gamepad == 0 ? "etodd" : "etodd[%d]", gamepad); // todo: actual usernames

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
	else if (manager.ref()->entity.ref() || NoclipControl::list.count() > 0)
	{
		if (upgrading)
			return UIMode::Upgrading;
		else
			return UIMode::Default;
	}
	else
		return UIMode::Spawning;
}

void LocalPlayer::msg(const char* msg, b8 good)
{
	msg_text.text(msg);
	msg_text.color = good ? UI::accent_color : UI::alert_color;
	msg_timer = 0.0f;
	msg_good = good;
}

void LocalPlayer::ensure_camera(const Update& u, b8 active)
{
	if (active && !camera && !manager.ref()->entity.ref())
	{
		camera = Camera::add();
		camera->fog = Game::state.mode == Game::Mode::Parkour;
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
		camera->perspective(60.0f * PI * 0.5f / 180.0f, aspect, 1.0f, Game::level.skybox.far_plane * 2.0f);
		Quat rot;
		map_view.ref()->absolute(&camera->pos, &rot);
		camera->rot = Quat::look(rot * Vec3(0, -1, 0));
	}
	else if (camera && (!active || manager.ref()->entity.ref()))
	{
		camera->remove();
		camera = nullptr;
	}
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
	if (Game::state.mode == Game::Mode::Pvp
		&& !Team::game_over
		&& Game::level.has_feature(Game::FeatureLevel::ControlPoints)
		&& Game::time.total > GAME_BUY_PERIOD
		&& Game::time.total - Game::time.delta <= GAME_BUY_PERIOD)
		msg(_(strings::buy_period_expired), true);

	if (msg_timer < msg_time)
		msg_timer += Game::real_time.delta;

	// close/open pause menu if needed
	{
		b8 pause_hit = Game::time.total > 0.5f && u.last_input->get(Controls::Pause, gamepad) && !u.input->get(Controls::Pause, gamepad);
		if (pause_hit
			&& !upgrading
			&& (menu_state == Menu::State::Hidden || menu_state == Menu::State::Visible)
			&& !Penelope::has_focus())
		{
			menu_state = (menu_state == Menu::State::Hidden) ? Menu::State::Visible : Menu::State::Hidden;
			menu.animate();
		}
		else if (menu_state == Menu::State::Visible && u.last_input->get(Controls::Cancel, gamepad) && !u.input->get(Controls::Cancel, gamepad))
		{
			menu_state = Menu::State::Hidden;
		}
	}

	switch (ui_mode())
	{
		case UIMode::Default:
		{
			// nothing going on
			ensure_camera(u, false);

			if (Game::state.mode == Game::Mode::Pvp
				&& Game::level.has_feature(Game::FeatureLevel::ControlPoints)
				&& manager.ref()->at_spawn())
			{
				if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
				{
					upgrading = true;
					menu.animate();
					upgrade_animation_time = Game::real_time.total;
				}
			}

			break;
		}
		case UIMode::Upgrading:
		{
			// upgrade menu
			if (u.last_input->get(Controls::Cancel, gamepad) && !u.input->get(Controls::Cancel, gamepad))
			{
				if (manager.ref()->current_upgrade == Upgrade::None)
					upgrading = false;
			}
			else
			{
				b8 upgrade_in_progress = manager.ref()->current_upgrade != Upgrade::None;
				if (upgrade_in_progress)
				{
					// we are upgrading an ability; disable all menu input
					UIMenu::active[gamepad] = (UIMenu*)1; // hack! invalid menu pointer to make this menu think that it doesn't have focus
				}

				u8 last_selected = menu.selected;

				menu.start(u, gamepad, (s32)Upgrade::count + 1);

				const Rect2& viewport = camera ? camera->viewport : manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera->viewport;

				Vec2 pos(viewport.size.x * 0.5f + MENU_ITEM_WIDTH * -0.5f, viewport.size.y * 0.8f);

				if (menu.item(u, &pos, _(strings::close), nullptr, upgrade_in_progress))
				{
					if (manager.ref()->current_upgrade == Upgrade::None)
						upgrading = false;
				}

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
			ensure_camera(u, true);
			const Rect2& viewport = camera ? camera->viewport : manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera->viewport;
			Menu::pause_menu(u, viewport, gamepad, &menu, &menu_state);
			break;
		}
		case UIMode::Spawning:
		{
			ensure_camera(u, true);
			break;
		}
		case UIMode::GameOver:
		{
			ensure_camera(u, true);

			if (Game::real_time.total - Team::game_over_real_time > score_summary_delay)
			{
				// update score summary scroll
				s32 score_summary_count = 0;
				for (auto player = PlayerManager::list.iterator(); !player.is_last(); player.next())
					score_summary_count += 2 + player.item()->rating_summary.length;
				score_summary_scroll.update(u, score_summary_count, gamepad);

				// accept score summary
				if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
					manager.ref()->score_accepted = true;
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
	if (NoclipControl::list.count() > 0)
		return;

	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	Vec3 dir = rot * Vec3(0, 1, 0);
	r32 angle = atan2f(dir.x, dir.z);

	Entity* spawned;

	if (Game::state.mode == Game::Mode::Pvp)
	{
		// Spawn AWK
		pos += Quat::euler(0, angle + (gamepad * PI * 0.5f), 0) * Vec3(0, 0, PLAYER_SPAWN_RADIUS * 0.5f); // spawn it around the edges
		spawned = World::create<AwkEntity>(manager.ref()->team.ref()->team());
	}
	else // parkour mode
	{
		// Spawn traceur
		spawned = World::create<Traceur>(pos + Vec3(0, 1.0f, 0), Quat::euler(0, angle, 0), manager.ref()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute(pos, rot);
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
	if (params.camera != camera && (!manager.ref()->entity.ref() || params.camera != manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera))
		return;

	const r32 line_thickness = 2.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	UIMode mode = ui_mode();

	r32 radius = 64.0f * UI::scale;
	Vec2 center = vp.size * Vec2(0.1f, 0.1f) + Vec2(radius, radius * 0.5f + (text_size * UI::scale * 0.5f));

	if (Game::state.mode == Game::Mode::Pvp
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
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
			UIText text;
			text.color = UI::accent_color;
			text.text("+%d", HealthPickup::increment(manager.ref()->entity.ref() ? manager.ref()->entity.ref()->get<PlayerCommon>() : nullptr));
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 pos = credits_pos + Vec2(0, text_size * UI::scale * -2.0f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
			text.draw(params, pos);
		}
	}

	if (mode == UIMode::Default)
	{
		if (Game::state.mode == Game::Mode::Pvp && Game::level.has_feature(Game::FeatureLevel::Abilities))
		{
			if (Game::level.has_feature(Game::FeatureLevel::ControlPoints) && manager.ref()->at_spawn())
			{
				// "upgrade!"
				UIText text;
				text.color = manager.ref()->upgrade_available() ? UI::accent_color : UI::disabled_color;
				text.text(_(strings::upgrade_prompt));
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.size = text_size;
				Vec2 pos = vp.size * Vec2(0.5f, 0.55f);
				UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, pos);
			}

			// ability 1
			b8 is_gamepad = params.sync->input.gamepads[gamepad].active;
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
	else if (mode == UIMode::Upgrading)
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
	else if (mode == UIMode::Spawning)
	{
		// player is dead

		b8 show_player_list;
		b8 show_spawning;
		if (Game::state.mode == Game::Mode::Pvp)
		{
			if (manager.ref()->spawn_timer > 0.0f)
			{
				// haven't spawned yet
				show_player_list = true;
				show_spawning = true;
			}
			else
			{
				// we're dead and we're not spawning again
				show_player_list = false;
				show_spawning = false;
			}
		}
		else
		{
			// always spawn in parkour mode
			show_player_list = true;
			show_spawning = true;
		}

		if (show_player_list)
		{
			Vec2 p = vp.size * Vec2(0.5f);

			UIText text;
			text.size = text_size;
			text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.color = UI::default_color;

			if (show_spawning)
			{
				// "spawning..."
				text.text(_(Game::state.mode == Game::Mode::Pvp ? strings::deploy_timer : strings::spawn_timer), (s32)manager.ref()->spawn_timer + 1);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
			}

			// show map name
			text.text(Game::state.mode == Game::Mode::Pvp ? "%s" : _(strings::map_simulation), AssetLookup::Level::names[Game::state.level]);
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

	if (Game::state.mode == Game::Mode::Pvp)
	{
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

					// rating breakdown
					s32 total = 0;
					const auto& rating_summary = player.item()->rating_summary;
					for (s32 i = 0; i < rating_summary.length; i++)
					{
						if (score_summary_scroll.item(item_counter))
						{
							text.text(_(rating_summary[i].label));
							UIMenu::text_clip(&text, Team::game_over_real_time + score_summary_delay, 50.0f + (r32)vi_min(item_counter, 6) * -5.0f);
							UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
							text.draw(params, p);
							amount.text("%d", rating_summary[i].amount);
							amount.draw(params, p + Vec2(MENU_ITEM_WIDTH * 0.5f - MENU_ITEM_PADDING, 0));
							p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
						}
						total += rating_summary[i].amount;
						item_counter++;
					}

					// total
					if (score_summary_scroll.item(item_counter))
					{
						text.text(_(strings::total_rating_gain));
						UIMenu::text_clip(&text, Team::game_over_real_time + score_summary_delay, 50.0f + (r32)vi_min(item_counter, 6) * -5.0f);
						UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
						text.draw(params, p);
						amount.text("%d", total);
						amount.draw(params, p + Vec2(MENU_ITEM_WIDTH * 0.5f - MENU_ITEM_PADDING, 0));
						p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
					}
					item_counter++;
				}
				score_summary_scroll.end(params, p);

				// press x to continue
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
				// upgrade / ability spawn timer
				if (manager.ref()->current_spawn_ability != Ability::None
					|| manager.ref()->current_upgrade != Upgrade::None)
				{
					r32 timer;
					r32 total_time;
					AssetID string;
					u16 cost;

					if (manager.ref()->current_spawn_ability != Ability::None)
					{
						// spawning an ability
						timer = manager.ref()->spawn_ability_timer;
						string = strings::ability_spawn_cost;

						const AbilityInfo& info = AbilityInfo::list[(s32)manager.ref()->current_spawn_ability];
						cost = info.spawn_cost;
						total_time = info.spawn_time;
					}
					else
					{
						// getting an upgrade
						timer = manager.ref()->upgrade_timer;
						string = strings::upgrading;

						const UpgradeInfo& info = UpgradeInfo::list[(s32)manager.ref()->current_upgrade];
						cost = info.cost;
						total_time = UPGRADE_TIME;
					}

					// draw bar

					Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);
					Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
					Rect2 bar = { pos + bar_size * -0.5f, bar_size };
					UI::box(params, bar, UI::background_color);
					UI::border(params, bar, 2, UI::accent_color);
					UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (timer / total_time)), bar.size.y) }, UI::accent_color);

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
	if (has<Awk>())
	{
		get<Health>()->hp_max = AWK_HEALTH;
		link<&PlayerCommon::awk_attached>(get<Awk>()->attached);
		link<&PlayerCommon::awk_detached>(get<Awk>()->detached);
	}
}

b8 PlayerCommon::movement_enabled() const
{
	if (has<Awk>())
	{
		return get<Transform>()->parent.ref() // must be attached to wall
			&& manager.ref()->current_spawn_ability == Ability::None // can't move while trying to spawn an ability
			&& get<Awk>()->stun_timer == 0.0f // or while stunned
			&& (Game::state.mode != Game::Mode::Pvp || Game::time.total > GAME_BUY_PERIOD || !Game::level.has_feature(Game::FeatureLevel::Abilities)); // or during the buy period
	}
	else
		return true;
}

void PlayerCommon::update(const Update& u)
{
	if (has<Awk>())
	{
		Quat rot = get<Transform>()->absolute_rot();
		r32 angle = Quat::angle(attach_quat, rot);
		if (angle > 0)
			attach_quat = Quat::slerp(vi_min(1.0f, rotation_speed * u.time.delta), attach_quat, rot);
	}

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

void PlayerCommon::awk_attached()
{
	Quat absolute_rot = get<Transform>()->absolute_rot();
	Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
	Vec3 wall_normal = absolute_rot * Vec3(0, 0, 1);

	// If we are spawning on to a flat floor, set attach_quat immediately
	// This preserves the camera rotation set by the PlayerSpawn
	if (direction.y == -1.0f && wall_normal.y > 0.9f)
		attach_quat = absolute_rot;
	else
	{
		// set our angles when we land, that way if we bounce off anything in transit,
		// the new direction will be taken into account
		angle_horizontal = atan2f(direction.x, direction.z);
		angle_vertical = -asinf(direction.y);

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

Vec3 PlayerCommon::look_dir() const
{
	if (has<LocalPlayerControl>()) // HACK for third-person camera
		return get<LocalPlayerControl>()->detach_dir;
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

s32 factorial(s32 x)
{
	s32 accum = 1;
	while (x > 1)
	{
		accum *= x;
		x--;
	}
	return accum;
}

s32 combination(s32 n, s32 choose)
{
	return factorial(n) / (factorial(n - choose) * factorial(choose));
}

Bitmask<MAX_PLAYERS * MAX_PLAYERS> PlayerCommon::visibility;

s32 PlayerCommon::visibility_hash(const PlayerCommon* awk_a, const PlayerCommon* awk_b)
{
	return awk_a->id() * MAX_PLAYERS + awk_b->id();
}

void LocalPlayerControl::awk_attached()
{
	rumble = vi_max(rumble, 0.2f);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

LocalPlayerControl::LocalPlayerControl(u8 gamepad)
	: gamepad(gamepad),
	fov_blend(),
	try_primary(),
	try_secondary(),
	damage_timer(),
	health_flash_timer(),
	rumble(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed()
{
	camera = Camera::add();
}

LocalPlayerControl::~LocalPlayerControl()
{
	Audio::listener_disable(gamepad);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
	camera->remove();
}

void LocalPlayerControl::awake()
{
	Audio::listener_enable(gamepad);

	if (has<Awk>())
	{
		last_pos = get<Awk>()->center();
		link<&LocalPlayerControl::awk_attached>(get<Awk>()->attached);
		link_arg<Entity*, &LocalPlayerControl::hit_target>(get<Awk>()->hit);
		link_arg<const DamageEvent&, &LocalPlayerControl::damaged>(get<Health>()->damaged);
		link_arg<const TargetEvent&, &LocalPlayerControl::hit_by>(get<Target>()->target_hit);
		link<&LocalPlayerControl::health_picked_up>(get<Health>()->added);
	}
	else
	{
		link_arg<r32, &LocalPlayerControl::parkour_landed>(get<Walker>()->land);
		get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
		get<Audio>()->param(AK::GAME_PARAMETERS::FLY_VOLUME, 0.0f);
	}

	camera->fog = Game::state.mode == Game::Mode::Parkour;
	camera->team = (u8)get<AIAgent>()->team;
	camera->mask = 1 << camera->team;
}

void LocalPlayerControl::parkour_landed(r32 velocity_diff)
{
	Parkour::State parkour_state = get<Parkour>()->fsm.current;
	if (velocity_diff < LANDING_VELOCITY_LIGHT
		&& (parkour_state == Parkour::State::Normal || parkour_state == Parkour::State::HardLanding))
	{
		if (velocity_diff < LANDING_VELOCITY_HARD)
			rumble = vi_max(rumble, 0.5f);
		else
			rumble = vi_max(rumble, 0.2f);
	}
}

void LocalPlayerControl::hit_target(Entity* target)
{
	rumble = vi_max(rumble, 0.5f);
	if (target->has<MinionAI>())
		player.ref()->msg(_(strings::minion_killed), true);
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

void LocalPlayerControl::update_camera_input(const Update& u, r32 gamepad_rotation_multiplier)
{
	if (input_enabled())
	{
		if (gamepad == 0)
		{
			r32 s = LMath::lerpf(fov_blend, speed_mouse, speed_mouse_zoom);
			get<PlayerCommon>()->angle_horizontal -= s * (r32)u.input->cursor_x;
			get<PlayerCommon>()->angle_vertical += s * (r32)u.input->cursor_y;
		}

		if (u.input->gamepads[gamepad].active)
		{
			r32 s = LMath::lerpf(fov_blend, speed_joystick, speed_joystick_zoom);
			Vec2 adjustment = Vec2
			(
				-Input::dead_zone(u.input->gamepads[gamepad].right_x),
				Input::dead_zone(u.input->gamepads[gamepad].right_y)
			) * s * Game::real_time.delta * gamepad_rotation_multiplier;
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

		get<PlayerCommon>()->angle_vertical = LMath::clampf(get<PlayerCommon>()->angle_vertical, PI * -0.495f, PI * 0.495f);
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

void LocalPlayerControl::detach()
{
	if (get<Awk>()->detach(detach_dir))
	{
		try_primary = false;
		try_secondary = false;
		get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
	}
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
	// camera viewport
	{
		s32 player_count;
#if DEBUG_AI_CONTROL
		player_count = LocalPlayer::list.count() + AIPlayer::list.count(); // AI player views are shown on screen as well
#else
		player_count = LocalPlayer::list.count();
#endif
		Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
		Camera::ViewportBlueprint* blueprint = &viewports[player.ref()->id()];

		camera->viewport =
		{
			Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
			Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
		};
		r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
		if (has<Awk>())
			camera->perspective(LMath::lerpf(fov_blend, fov_pvp, fov_pvp_zoom), aspect, 0.02f, Game::level.skybox.far_plane);
		else
			camera->perspective(fov_parkour, aspect, 0.02f, Game::level.skybox.far_plane);
	}

	if (has<Awk>())
	{
		// pvp mode
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

			r32 fov_blend_target = try_secondary ? 1.0f : 0.0f;

			if (fov_blend < fov_blend_target)
				fov_blend = vi_min(fov_blend + u.time.delta * zoom_speed, fov_blend_target);
			else if (fov_blend > fov_blend_target)
				fov_blend = vi_max(fov_blend - u.time.delta * zoom_speed, fov_blend_target);
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
						if (indicator_distance < reticle_distance)
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
			look_quat = get<PlayerCommon>()->attach_quat;

		// abilities
		if (input_enabled())
		{
			b8 just_attached = u.time.total - get<Awk>()->attach_time < 0.2f;
			PlayerManager* manager = player.ref()->manager.ref();
			{
				b8 current = u.input->get(Controls::Ability1, gamepad);
				b8 last = u.last_input->get(Controls::Ability1, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[0]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[0]);
			}

			{
				b8 current = u.input->get(Controls::Ability2, gamepad);
				b8 last = u.last_input->get(Controls::Ability2, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[1]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[1]);
			}

			{
				b8 current = u.input->get(Controls::Ability3, gamepad);
				b8 last = u.last_input->get(Controls::Ability3, gamepad);
				if (current && (!last || just_attached))
					manager->ability_spawn_start(manager->abilities[2]);
				else if (!current)
					manager->ability_spawn_stop(manager->abilities[2]);
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

			camera->range = AWK_MAX_DISTANCE;
			camera->range_center = look_quat.inverse() * (get<Awk>()->center() - camera->pos);
			camera->cull_range = third_person_offset + 0.5f;
			camera->cull_behind_wall = abs_wall_normal.dot(camera->pos - get<Awk>()->attach_point()) < 0.0f;
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
				Vec3 trace_end = trace_start + trace_dir * (AWK_MAX_DISTANCE + third_person_offset);
				btCollisionWorld::ClosestRayResultCallback ray_callback(trace_start, trace_end);
				Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~get<Awk>()->ally_containment_field_mask());

				if (ray_callback.hasHit())
				{
					reticle.pos = ray_callback.m_hitPointWorld;
					Vec3 center = get<Awk>()->center();
					detach_dir = reticle.pos - center;
					r32 distance = detach_dir.length();
					detach_dir /= distance;
					Vec3 hit;
					b8 hit_target;
					if (get<Awk>()->can_go(detach_dir, &hit, &hit_target))
					{
						if ((hit - center).length() > distance - AWK_RADIUS)
							reticle.type = hit_target ? ReticleType::Target : ReticleType::Normal;
					}
				}
				else
					reticle.pos = trace_end;
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
			b8 can_steal_health = player.ref()->manager.ref()->has_upgrade(Upgrade::HealthSteal);
			for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
			{
				Health* owner = i.item()->owner.ref();
				if (!owner || (can_steal_health && owner != get<Health>()))
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
				reticle.type = ReticleType::Error;
				if (get<Awk>()->cooldown > 0.0f) // prevent the player from mashing the fire button to nail the cooldown skip
					get<Awk>()->disable_cooldown_skip = true;
			}
		}
		else
		{
			// we're aiming at something
			if (try_primary)
				detach();
		}
	}
	else
	{
		// parkour mode
		update_camera_input(u);

		Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, 0));
		Vec2 dir = Vec2(movement.x, movement.z);
		get<Walker>()->dir = dir;

		// parkour button
		b8 parkour_pressed = movement_enabled() && u.input->get(Controls::Parkour, gamepad);

		if (get<Parkour>()->fsm.current == Parkour::State::WallRun && !parkour_pressed)
			get<Parkour>()->fsm.transition(Parkour::State::Normal);

		if (parkour_pressed && !u.last_input->get(Controls::Parkour, gamepad))
			try_secondary = true;
		else if (!parkour_pressed)
			try_secondary = false;

		if (try_secondary)
		{
			if (get<Parkour>()->try_parkour())
			{
				try_secondary = false;
				try_primary = false;
				try_slide = false;
			}
		}

		// jump button
		b8 jump_pressed = movement_enabled() && u.input->get(Controls::Jump, gamepad);
		if (jump_pressed && !u.last_input->get(Controls::Jump, gamepad))
			try_primary = true;
		else if (!jump_pressed)
			try_primary = false;

		if (try_primary)
		{
			if (get<Parkour>()->try_jump(get<PlayerCommon>()->angle_horizontal))
			{
				try_secondary = false;
				try_primary = false;
				try_slide = false;
			}
		}

		// slide button
		b8 slide_pressed = movement_enabled() && u.input->get(Controls::Slide, gamepad);

		get<Parkour>()->slide_continue = slide_pressed;

		if (slide_pressed && !u.last_input->get(Controls::Slide, gamepad))
			try_slide = true;
		else if (!slide_pressed)
			try_slide = false;

		if (try_slide)
		{
			if (get<Parkour>()->try_slide())
			{
				try_secondary = false;
				try_primary = false;
				try_slide = false;
			}
		}

		Parkour::State parkour_state = get<Parkour>()->fsm.current;

		{
			// if we're just running and not doing any parkour
			// rotate arms to match the camera view
			// blend smoothly between the two states (rotating and not rotating)

			r32 arm_angle = LMath::clampf(get<PlayerCommon>()->angle_vertical * 0.5f, -PI * 0.15f, PI * 0.15f);

			const r32 blend_time = 0.2f;
			r32 blend;
			if (parkour_state == Parkour::State::Normal)
				blend = vi_min(1.0f, get<Parkour>()->fsm.time / blend_time);
			else if (get<Parkour>()->fsm.last == Parkour::State::Normal)
				blend = vi_max(0.0f, 1.0f - (get<Parkour>()->fsm.time / blend_time));
			else
				blend = 0.0f;
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_L, Vec3::zero, Quat::euler(arm_angle * blend, 0, 0));
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_R, Vec3::zero, Quat::euler(arm_angle * -blend, 0, 0));
		}

		r32 lean_target = 0.0f;

		Quat look_quat = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

		if (parkour_state == Parkour::State::WallRun)
		{
			Parkour::WallRunState state = get<Parkour>()->wall_run_state;

			Vec3 wall_normal = get<Parkour>()->last_support.ref()->get<Transform>()->to_world_normal(get<Parkour>()->relative_wall_run_normal);

			Vec3 forward = look_quat * Vec3(0, 0, 1);

			if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Forward)
				wall_normal *= -1.0f; // Make sure we're always facing the wall
			else
			{
				// We're running along the wall
				// Make sure we can't look backward
				get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1));
			}

			get<PlayerCommon>()->clamp_rotation(wall_normal);
		}
		else if (parkour_state == Parkour::State::Slide || parkour_state == Parkour::State::Roll || parkour_state == Parkour::State::HardLanding || parkour_state == Parkour::State::Mantle)
		{
			get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1));
		}
		else
		{
			get<Walker>()->target_rotation = get<PlayerCommon>()->angle_horizontal;

			// make sure our body is facing within 90 degrees of our target rotation
			r32 delta = LMath::angle_to(get<Walker>()->rotation, get<PlayerCommon>()->angle_horizontal);
			if (delta > PI * 0.5f)
				get<Walker>()->rotation = LMath::angle_range(get<Walker>()->rotation + delta - PI * 0.5f);
			else if (delta < PI * -0.5f)
				get<Walker>()->rotation = LMath::angle_range(get<Walker>()->rotation + delta + PI * 0.5f);
		}

		Vec3 camera_pos;
		if (Game::state.third_person)
			camera_pos = get<Transform>()->absolute_pos() + look_quat * Vec3(0, 1, -3);
		else
		{
			camera_pos = Vec3(0, 0, 0.05f);
			Quat q = Quat::identity;
			get<Parkour>()->head_to_object_space(&camera_pos, &q);
			camera_pos = get<Transform>()->to_world(camera_pos);

			// camera bone affects rotation only
			Quat camera_animation = Quat::euler(PI * -0.5f, 0, 0);
			get<Animator>()->bone_transform(Asset::Bone::character_camera, nullptr, &camera_animation);
			look_quat = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Quat::euler(0, PI * 0.5f, 0) * camera_animation * Quat::euler(0, PI * -0.5f, 0);
		}

		// wind sound and camera shake at high speed
		{
			r32 speed = get<RigidBody>()->btBody->getInterpolationLinearVelocity().length();
			get<Audio>()->param(AK::GAME_PARAMETERS::FLY_VOLUME, LMath::clampf((speed - 8.0f) / 25.0f, 0, 1));
			r32 shake = LMath::clampf((speed - 13.0f) / 30.0f, 0, 1);
			rumble = vi_max(rumble, shake);
			shake *= 0.2f;
			r32 offset = Game::time.total * 10.0f;
			look_quat = look_quat * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}

		// camera setup
		camera->wall_normal = Vec3(0, 0, 1);
		camera->pos = camera_pos;
		camera->range = 0.0f;
		camera->rot = look_quat;
	}

	Audio::listener_update(gamepad, camera->pos, camera->rot);

	// rumble
	if (rumble > 0.0f)
	{
		u.input->gamepads[gamepad].rumble = vi_min(1.0f, rumble);
		rumble = vi_max(0.0f, rumble - u.time.delta);
	}
}

LocalPlayerControl* LocalPlayerControl::player_for_camera(const Camera* cam)
{
	for (auto i = LocalPlayerControl::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->camera == cam)
			return i.item();
	}
	return nullptr;
}

void LocalPlayerControl::draw_alpha(const RenderParams& params) const
{
	if (!has<Awk>() || params.technique != RenderTechnique::Default || params.camera != camera)
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
	}

	// highlight enemy rockets
	{
		for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->target.ref() == entity())
			{
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

	// upgrade notification
	if (Game::level.has_feature(Game::FeatureLevel::ControlPoints)
		&& player.ref()->manager.ref()->upgrade_available()
		&& !player.ref()->manager.ref()->at_spawn())
	{
		Vec3 spawn_pos = Team::list[(s32)team].player_spawn.ref()->absolute_pos();
		UI::indicator(params, spawn_pos, Team::ui_color_friend, true);

		UIText text;
		text.color = Team::ui_color_friend;
		text.text(_(strings::upgrade_notification));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 p;
		UI::is_onscreen(params, spawn_pos, &p);
		p.y += text_size * 2.0f * UI::scale;
		UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
		if (UI::flash_function_slow(Game::real_time.total))
			text.draw(params, p);
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
				hp_pos.y += text_size * 1.5f * UI::scale;

				Vec2 username_pos = hp_pos;
				username_pos.y += (text_size * UI::scale) + HP_BOX_SPACING * 0.5f;

				UIText username;
				username.size = text_size;
				username.anchor_x = UIText::Anchor::Center;
				username.anchor_y = UIText::Anchor::Min;
				username.color = *color;
				username.text(other_player.item()->manager.ref()->username);

				UI::box(params, username.rect(username_pos).outset(HP_BOX_SPACING), UI::background_color);

				if (Game::level.has_feature(Game::FeatureLevel::HealthPickups))
				{
					draw_hp_box(params, hp_pos, history.hp_max);
					draw_hp_indicator(params, hp_pos, history.hp, history.hp_max, *color);
				}

				username.draw(params, username_pos);

				// invincible indicator
				if (!friendly && (tracking || visible))
				{
					r32 enemy_invincible_timer = other_player.item()->get<Awk>()->invincible_timer;
					if (enemy_invincible_timer > 0.0f)
					{
						UIText text;
						text.size = text_size - 2.0f;
						text.color = UI::background_color;
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Center;
						text.text(_(strings::invincible));

						Vec2 invincible_pos = username_pos;
						invincible_pos.y += (text_size * UI::scale) + HP_BOX_SPACING;

						Vec2 bar_size = text.bounds() + Vec2(HP_BOX_SPACING);

						const Vec4& color = visible ? UI::alert_color : UI::accent_color;

						Rect2 bar = { invincible_pos + Vec2(bar_size.x * -0.5f, 0), bar_size };
						UI::box(params, bar, UI::background_color);
						UI::border(params, bar, 2, color);
						UI::box(params, { bar.pos, Vec2(bar.size.x * (enemy_invincible_timer / AWK_INVINCIBLE_TIME), bar.size.y) }, color);

						text.draw(params, bar.pos + bar.size * 0.5f);
					}
				}
			}
		}
	}

	// incoming attack warning indicator
	if (get<Awk>()->incoming_attacker())
	{
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

	// health indicator
	if (Game::level.has_feature(Game::FeatureLevel::HealthPickups))
	{
		const Health* health = get<Health>();
		
		Vec2 pos = viewport.size * Vec2(0.5f, 0.1f);

		draw_hp_box(params, pos, health->hp_max);

		b8 flash_hp = health_flash_timer > 0.0f;
		b8 draw_hp;
		if (flash_hp)
			draw_hp = UI::flash_function(Game::real_time.total);
		else
			draw_hp = health->hp > 1 || UI::flash_function_slow(Game::real_time.total);
		if (draw_hp)
			draw_hp_indicator(params, pos, health->hp, health->hp_max, flash_hp ? UI::default_color : UI::accent_color);
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
			text.text(_(strings::ability_spawn_cost), CREDITS_DETECT);
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
		r32 blend = vi_max(0.0f, cooldown / AWK_MAX_DISTANCE_COOLDOWN);
		const r32 spoke_length = 10.0f;
		const r32 spoke_width = 3.0f;
		r32 start_radius = 8.0f + blend * 32.0f + spoke_length * 0.5f;
		const Vec4& color =
			reticle.type == ReticleType::Error
			? UI::disabled_color
			: (reticle.type == ReticleType::None ? UI::alert_color : UI::accent_color);
		const r32 ratio = 0.8660254037844386f;
		UI::centered_box(params, { pos + Vec2(ratio, -0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, color, PI * 0.5f * -0.33f);
		UI::centered_box(params, { pos + Vec2(-ratio, -0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, color, PI * 0.5f * 0.33f);
		UI::centered_box(params, { pos + Vec2(0, 1.0f) * UI::scale * start_radius, Vec2(spoke_width, spoke_length) * UI::scale }, color);

		b8 cooldown_can_go = get<Awk>()->cooldown_can_go();

		if (cooldown > 0.0f)
			UI::triangle_border(params, { pos, Vec2(start_radius * 6.0f * UI::scale) }, spoke_width, cooldown_can_go ? color : UI::alert_color);

		if (cooldown_can_go && (reticle.type == ReticleType::Normal || reticle.type == ReticleType::Target))
		{
			Vec2 a;
			if (UI::project(params, reticle.pos, &a))
				UI::triangle(params, { a, Vec2(10.0f * UI::scale) }, reticle.type == ReticleType::Normal ? UI::accent_color : UI::alert_color, PI);
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
