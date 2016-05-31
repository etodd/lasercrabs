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

#define fov_initial (80.0f * PI * 0.5f / 180.0f)
#define zoom_ratio 0.5f
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse 0.0025f
#define fov_zoom (fov_initial * zoom_ratio)
#define speed_mouse_zoom (speed_mouse * zoom_ratio * 0.5f)
#define speed_joystick 4.0f
#define speed_joystick_zoom (speed_joystick * zoom_ratio * 0.5f)
#define attach_speed 5.0f
#define max_attach_time 0.35f
#define rotation_speed 20.0f
#define msg_time 0.75f
#define text_size 16.0f
#define damage_shake_time 0.7f
#define third_person_offset 2.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u16 hp)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return (hp * (box_size.x + HP_BOX_SPACING)) - HP_BOX_SPACING;
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

	for (s32 i = 0; i < hp_max; i++)
	{
		UI::triangle_border(params, { pos, box_size }, 2, color, PI);
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
	upgrade_animation_time()
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

	if (msg_timer < msg_time)
		msg_timer += u.time.delta;

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
			if (u.last_input->get(Controls::Cancel, gamepad) && !u.input->get(Controls::Cancel, gamepad))
			{
				if (manager.ref()->current_upgrade_ability == Ability::None)
					upgrading = false;
			}
			else
			{
				b8 upgrade_in_progress = manager.ref()->current_upgrade_ability != Ability::None;
				if (upgrade_in_progress)
				{
					// we are upgrading an ability; disable all menu input
					UIMenu::active[gamepad] = (UIMenu*)1; // hack! invalid menu pointer to make this menu think that it doesn't have focus
				}

				u8 last_selected = menu.selected;

				menu.start(u, gamepad);

				const Rect2& viewport = camera ? camera->viewport : manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera->viewport;

				Vec2 pos(viewport.size.x * 0.5f + MENU_ITEM_WIDTH * -0.5f, viewport.size.y * 0.6f + UIMenu::height(3) * 0.5f);

				if (menu.item(u, &pos, _(strings::close), nullptr, upgrade_in_progress))
				{
					if (manager.ref()->current_upgrade_ability == Ability::None)
						upgrading = false;
				}

				char lvl[255];
				{
					b8 can_upgrade = !upgrade_in_progress
						&& manager.ref()->ability_upgrade_available(Ability::Sensor)
						&& manager.ref()->credits >= manager.ref()->ability_upgrade_cost(Ability::Sensor);
					u8 level = manager.ref()->ability_level[(s32)Ability::Sensor];
					sprintf(lvl, _(level == MAX_ABILITY_LEVELS ? strings::ability_max_lvl : strings::ability_lvl), level);
					if (menu.item(u, &pos, _(strings::sensor), level == 0 ? nullptr : lvl, !can_upgrade, Asset::Mesh::icon_sensor))
						manager.ref()->ability_upgrade_start(Ability::Sensor);
				}
				{
					b8 can_upgrade = Game::level.has_feature(Game::FeatureLevel::All)
						&& !upgrade_in_progress
						&& manager.ref()->ability_upgrade_available(Ability::Teleporter)
						&& manager.ref()->credits >= manager.ref()->ability_upgrade_cost(Ability::Teleporter);
					u8 level = manager.ref()->ability_level[(s32)Ability::Teleporter];
					sprintf(lvl, _(level == MAX_ABILITY_LEVELS ? strings::ability_max_lvl : strings::ability_lvl), level);
					if (menu.item(u, &pos, _(strings::teleporter), level == 0 ? nullptr : lvl, !can_upgrade, Asset::Mesh::icon_teleporter))
						manager.ref()->ability_upgrade_start(Ability::Teleporter);
				}
				{
					b8 can_upgrade = Game::level.has_feature(Game::FeatureLevel::All)
						&& !upgrade_in_progress
						&& manager.ref()->ability_upgrade_available(Ability::Minion)
						&& manager.ref()->credits >= manager.ref()->ability_upgrade_cost(Ability::Minion);
					u8 level = manager.ref()->ability_level[(s32)Ability::Minion];
					sprintf(lvl, _(level == MAX_ABILITY_LEVELS ? strings::ability_max_lvl : strings::ability_lvl), level);
					if (menu.item(u, &pos, _(strings::minion), level == 0 ? nullptr : lvl, !can_upgrade, Asset::Mesh::icon_minion))
						manager.ref()->ability_upgrade_start(Ability::Minion);
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
			// Player is currently dead
			ensure_camera(u, true);

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
			text.text("+%d", ControlPoint::increment(manager.ref()->team.ref()->team()));
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
				text.color = manager.ref()->ability_upgrade_available() ? UI::accent_color : UI::disabled_color;
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
			if (manager.ref()->ability_level[(s32)Ability::Sensor] > 0)
			{
				const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability1].string(is_gamepad);
				draw_ability(params, manager.ref(), center + Vec2(-radius, 0), Ability::Sensor, Asset::Mesh::icon_sensor, binding);
			}

			// ability 2
			if (manager.ref()->ability_level[(s32)Ability::Teleporter] > 0)
			{
				const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability2].string(is_gamepad);
				draw_ability(params, manager.ref(), center + Vec2(0, radius * 0.5f), Ability::Teleporter, Asset::Mesh::icon_teleporter, binding);
			}

			// ability 3
			if (manager.ref()->ability_level[(s32)Ability::Minion] > 0)
			{
				const char* binding = Settings::gamepads[gamepad].bindings[(s32)Controls::Ability3].string(is_gamepad);
				draw_ability(params, manager.ref(), center + Vec2(radius, 0), Ability::Minion, Asset::Mesh::icon_minion, binding);
			}
		}
	}
	else if (mode == UIMode::Upgrading)
	{
		menu.draw_alpha(params);

		if (menu.selected > 0)
		{
			// show details of currently highlighted upgrade
			Ability ability = (Ability)(menu.selected - 1);
			if (manager.ref()->current_upgrade_ability == Ability::None
				&& manager.ref()->ability_upgrade_available(ability))
			{
				r32 padding = 8.0f * UI::scale;

				const AbilityInfo& info = AbilityInfo::list[(s32)ability];
				UIText text;
				text.color = UI::accent_color;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.clip = 1 + (s32)((Game::real_time.total - upgrade_animation_time) * 150.0f);
				text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
				u8 level = manager.ref()->ability_level[(s32)ability];
				u16 cost = manager.ref()->ability_upgrade_cost(ability);
				text.text(_(strings::ability_description), level + 1, cost, _(info.description[level]));

				const Rect2& last_item = menu.items[menu.items.length - 1].rect();
				Vec2 pos(last_item.pos.x + padding, last_item.pos.y - padding * 2.0f);
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
				text.text(_(strings::spawn_timer), (s32)manager.ref()->spawn_timer + 1);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
			}

			// show map name
			text.text(AssetLookup::Level::names[Game::state.level]);
			text.color = UI::default_color;
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
		{
			// timer

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

			if (remaining > 60.0f || UI::flash_function(Game::real_time.total))
				UI::mesh(params, icon, icon_pos, Vec2(text_size * UI::scale), *color);
			
			s32 remaining_minutes = remaining / 60.0f;
			s32 remaining_seconds = remaining - (remaining_minutes * 60.0f);

			UIText text;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.color = *color;
			text.text(_(strings::timer), remaining_minutes, remaining_seconds);
			text.draw(params, icon_pos + Vec2(text_size * UI::scale * 1.5f, 0));
		}

		if (Team::game_over())
		{
			// show victory/defeat/draw message
			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = 32.0f;

			if (Team::is_draw())
			{
				text.color = UI::alert_color;
				text.text(_(strings::draw));
			}
			else if (manager.ref()->team.ref()->has_player())
			{
				text.color = UI::accent_color;
				text.text(_(strings::victory));
			}
			else
			{
				text.color = UI::alert_color;
				text.text(_(strings::defeat));
			}
			text.clip = 1 + (s32)(Team::game_over_time() * 20.0f);
			Vec2 pos = vp.size * Vec2(0.5f, 0.5f);
			UI::box(params, text.rect(pos).outset(16 * UI::scale), UI::background_color);
			text.draw(params, pos);
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
			&& !get<Teleportee>()->in_progress() // or while teleporting
			&& get<Awk>()->stun_timer == 0.0f; // or while stunned
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
	allow_zoom(true),
	try_jump(),
	try_parkour(),
	damage_timer(),
	health_flash_timer(),
	rumble(),
	target_indicators(),
	last_gamepad_input_time()
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
	{
		player.ref()->msg(_(strings::minion_killed), true);
		if (target->get<AIAgent>()->team != get<AIAgent>()->team)
			player.ref()->manager.ref()->add_credits(CREDITS_MINION);
	}
}

void LocalPlayerControl::damaged(const DamageEvent& e)
{
	health_flash_timer = msg_time; // damaged in some way; flash the HP indicator
}

void LocalPlayerControl::hit_by(const TargetEvent& e)
{
	if (get<Health>()->hp <= e.hit_by->get<Health>()->hp)
	{
		// we were physically hit by the enemy; shake the camera
		damage_timer = damage_shake_time;
		rumble = vi_max(rumble, 1.0f);
	}
}

void LocalPlayerControl::health_picked_up()
{
	health_flash_timer = msg_time;
}

b8 LocalPlayerControl::input_enabled() const
{
	return !Console::visible && player.ref()->ui_mode() == LocalPlayer::UIMode::Default && !Penelope::has_focus() && !Team::game_over();
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
			) * s * u.time.delta * gamepad_rotation_multiplier;
			if (adjustment.length_squared() > 0.0f)
				last_gamepad_input_time = Game::real_time.total;
			get<PlayerCommon>()->angle_horizontal += adjustment.x;
			get<PlayerCommon>()->angle_vertical += adjustment.y;
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
		allow_zoom = false;
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
		// calculate head intersection trajectory
		Vec3 intersection;
		if (get<Awk>()->predict_intersection(target, &intersection))
		{
			if (reticle.type == ReticleType::Normal && LMath::ray_sphere_intersect(me, reticle.pos, intersection, MINION_HEAD_RADIUS))
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
		camera->perspective(LMath::lerpf(fov_blend, fov_initial, fov_zoom), aspect, 0.02f, Game::level.skybox.far_plane);
	}

	if (has<Awk>())
	{
		// pvp mode
		{
			// zoom
			r32 fov_blend_target = 0.0f;
			if (get<Transform>()->parent.ref() && input_enabled())
			{
				if (u.input->get(Controls::Secondary, gamepad))
				{
					if (allow_zoom)
					{
						fov_blend_target = 1.0f;
						if (!u.last_input->get(Controls::Secondary, gamepad))
							get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
					}
				}
				else
				{
					if (allow_zoom && u.last_input->get(Controls::Secondary, gamepad))
						get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
					allow_zoom = true;
				}
			}

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
						|| indicator.type == TargetIndicator::Type::Minion)
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
											if (my_speed == 0.0f || my_speed > AWK_CRAWL_SPEED * AWK_CRAWL_SPEED * 1.5f) // don't adjust if we're going too fast or not moving
												break;
										}
										Vec3 me_predicted = me + my_velocity;

										if (indicator.velocity.length_squared() > AWK_CRAWL_SPEED * AWK_CRAWL_SPEED * 1.5f) // enemy moving too fast
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
									get<PlayerCommon>()->angle_horizontal += adjustment.x;
									get<PlayerCommon>()->angle_vertical += adjustment.y;
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
			{
				b8 current = u.input->get(Controls::Ability1, gamepad);
				b8 last = u.last_input->get(Controls::Ability1, gamepad);
				if (current && !last)
					player.ref()->manager.ref()->ability_spawn_start(Ability::Sensor);
				else if (!current && last)
					player.ref()->manager.ref()->ability_spawn_stop(Ability::Sensor);
			}

			{
				b8 current = u.input->get(Controls::Ability2, gamepad);
				b8 last = u.last_input->get(Controls::Ability2, gamepad);
				if (current && !last)
					player.ref()->manager.ref()->ability_spawn_start(Ability::Teleporter);
				else if (!current && last)
					player.ref()->manager.ref()->ability_spawn_stop(Ability::Teleporter);
			}

			{
				b8 current = u.input->get(Controls::Ability3, gamepad);
				b8 last = u.last_input->get(Controls::Ability3, gamepad);
				if (current && !last)
					player.ref()->manager.ref()->ability_spawn_start(Ability::Minion);
				else if (!current && last)
					player.ref()->manager.ref()->ability_spawn_stop(Ability::Minion);
			}
		}

		// camera setup
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
		if (!get<Transform>()->parent.ref())
			camera->cull_range = 0.0f; // there is no wall; no need to cull anything
		else
			camera->cull_range = third_person_offset + 0.5f; // need to cull stuff

		health_flash_timer = vi_max(0.0f, health_flash_timer - Game::real_time.delta);

		camera->rot = look_quat;

		// reticle
		{
			Vec3 trace_dir = look_quat * Vec3(0, 0, 1);
			Vec3 trace_start = camera->pos + trace_dir * third_person_offset;
			Vec3 trace_end = trace_start + trace_dir * (AWK_MAX_DISTANCE + third_person_offset);

			reticle.type = ReticleType::None;

			if (movement_enabled())
			{
				AwkRaycastCallback ray_callback(trace_start, trace_end, entity());
				Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~get<Awk>()->ally_containment_field_mask());

				if (ray_callback.hasHit())
				{
					reticle.pos = ray_callback.m_hitPointWorld;
					Vec3 center = get<Awk>()->center();
					detach_dir = reticle.pos - center;
					r32 distance = detach_dir.length();
					detach_dir /= distance;
					Vec3 hit;
					if (get<Awk>()->can_go(detach_dir, &hit))
					{
						if (fabs((hit - center).length() - distance) < AWK_RADIUS)
						{
							if (get<Awk>()->cooldown == 0.0f)
								reticle.type = ray_callback.hit_target() ? ReticleType::Target : ReticleType::Normal;
						}
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
					if (!add_target_indicator(i.item()->get<Target>(), TargetIndicator::Type::Minion))
						break; // no more room for indicators
				}
			}
		}

		// health pickups
		if (get<Health>()->hp < get<Health>()->hp_max)
		{
			b8 can_steal_health = player.ref()->manager.ref()->can_steal_health();
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

		if (reticle.type == ReticleType::None)
		{
			// can't shoot
			if (u.input->get(Controls::Primary, gamepad)) // player is mashing the fire button; give them some feedback
				reticle.type = ReticleType::Error;
		}
		else
		{
			// we're aiming at something
			if (u.input->get(Controls::Primary, gamepad) && !u.last_input->get(Controls::Primary, gamepad))
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
			try_parkour = true;
		else if (!parkour_pressed)
			try_parkour = false;

		if (try_parkour)
		{
			if (get<Parkour>()->try_parkour())
			{
				try_parkour = false;
				try_jump = false;
				try_slide = false;
			}
		}

		// jump button
		b8 jump_pressed = movement_enabled() && u.input->get(Controls::Jump, gamepad);
		if (jump_pressed && !u.last_input->get(Controls::Jump, gamepad))
			try_jump = true;
		else if (!jump_pressed)
			try_jump = false;

		if (try_jump)
		{
			if (get<Parkour>()->try_jump(get<PlayerCommon>()->angle_horizontal))
			{
				try_parkour = false;
				try_jump = false;
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
				try_parkour = false;
				try_jump = false;
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
					UI::indicator(params, indicator.pos, UI::alert_color, false);
					break;
				}
			}
		}
	}

	// highlight control points
	{
		Vec3 me = get<Transform>()->absolute_pos();
		for (auto i = ControlPoint::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();
			if ((pos - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
			{
				Vec2 p;
				if (UI::project(params, pos, &p))
				{
					Vec2 icon_size(text_size * UI::scale);
					UI::box(params, Rect2(p + icon_size * -0.5f, icon_size).outset(4.0f * UI::scale), UI::background_color);

					AI::Team control_point_team = i.item()->team;
					const Vec4& color = control_point_team == team ? Team::ui_color_friend : (control_point_team == AI::Team::None ? UI::accent_color : Team::ui_color_enemy);
					UI::mesh(params, Asset::Mesh::icon_credits, p, icon_size, color);
				}
			}
		}
	}

	// highlight teleporters
	{
		Teleporter* target = get<Teleportee>()->target.ref();
		// if we are in the act of teleporting, draw big highlights
		r32 scale = target ? 1.0f : 0.5f;
		for (auto t = Teleporter::list.iterator(); !t.is_last(); t.next())
		{
			if (t.item()->team == team)
				UI::indicator(params, t.item()->get<Transform>()->absolute_pos(), target && t.item() == target ? UI::accent_color : Team::ui_color_friend, t.item() == target, scale);
		}

		if (target)
		{
			// highlight targeted teleporter
			Vec3 pos3d = target->get<Transform>()->absolute_pos() + Vec3(0, AWK_RADIUS * 2.0f, 0);

			Vec2 p;
			UI::is_onscreen(params, pos3d, &p);

			p.y += text_size * UI::scale;

			UIText text;
			text.size = text_size;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::accent_color;
			text.text(_(strings::teleport));

			UI::box(params, text.rect(p).outset(HP_BOX_SPACING), UI::background_color);
			text.draw(params, p);
		}
	}

	// upgrade notification
	if (Game::level.has_feature(Game::FeatureLevel::ControlPoints)
		&& player.ref()->manager.ref()->ability_upgrade_available()
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

				if (!tracking && !visible) // if we can actually see them, the indicator has already been added using add_target_indicator in the update function
					UI::indicator(params, history.pos, UI::disabled_color, false);

				Vec3 pos3d = history.pos + Vec3(0, AWK_RADIUS * 2.0f, 0);

				if (tracking || visible)
				{
					// highlight the username and draw it even if it's offscreen
					UI::is_onscreen(params, pos3d, &p);
					draw = true;
					if (team == other_player.item()->get<AIAgent>()->team) // friend
						color = &Team::ui_color_friend;
					else // enemy
						color = visible ? &Team::ui_color_enemy : &UI::accent_color;
				}
				else
				{
					draw = UI::project(params, pos3d, &p);
					color = &UI::disabled_color;
				}
			}

			if (draw)
			{
				Vec2 hp_pos = p;
				hp_pos.y += text_size * UI::scale;

				Vec2 username_pos = hp_pos;
				username_pos.y += (text_size * UI::scale) + HP_BOX_SPACING * 0.5f;

				Vec2 invincible_pos = username_pos;
				invincible_pos.y += (text_size * UI::scale) + HP_BOX_SPACING * 0.5f;

				UIText username;
				username.size = text_size;
				username.anchor_x = UIText::Anchor::Center;
				username.anchor_y = UIText::Anchor::Min;
				username.color = *color;
				username.text(other_player.item()->manager.ref()->username);

				u16 my_hp = get<Health>()->hp;
				UIText invincible;
				// normally, enemies are invincible if they have higher health
				// but if we buy the right upgrade they become vulnerable
				b8 draw_invincible = !friendly && visible && history.hp > my_hp && !player.ref()->manager.ref()->can_steal_health();
				if (draw_invincible)
				{
					invincible = username;
					invincible.text(_(strings::invincible));
					UI::box(params, invincible.rect(invincible_pos).outset(HP_BOX_SPACING), UI::background_color);
				}

				UI::box(params, username.rect(username_pos).outset(HP_BOX_SPACING), UI::background_color);

				if (Game::level.has_feature(Game::FeatureLevel::HealthPickups))
				{
					draw_hp_box(params, hp_pos, history.hp_max);
					draw_hp_indicator(params, hp_pos, history.hp, history.hp_max, *color);
				}

				username.draw(params, username_pos);

				if (draw_invincible && UI::flash_function(Game::real_time.total))
					invincible.draw(params, invincible_pos); // danger!
			}
		}
	}

	{
		// compass
		b8 enemy_attacking = false;

		Vec3 me = get<Transform>()->absolute_pos();

		for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
		{
			if (PlayerCommon::visibility.get(PlayerCommon::visibility_hash(get<PlayerCommon>(), i.item())))
			{
				// determine if they're attacking us
				if (!i.item()->get<Transform>()->parent.ref()
					&& Vec3::normalize(i.item()->get<Awk>()->velocity).dot(Vec3::normalize(me - i.item()->get<Transform>()->absolute_pos())) > 0.95f)
					enemy_attacking = true;
			}
		}

		Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);

		if (enemy_attacking)
		{
			// we're being attacked; flash the compass
			b8 show = UI::flash_function(Game::real_time.total);
			if (show)
				UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::alert_color);
			if (show && !UI::flash_function(Game::real_time.total - Game::real_time.delta))
				Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
		}
	}

	// health indicator
	if (Game::level.has_feature(Game::FeatureLevel::HealthPickups))
	{
		const Health* health = get<Health>();
		
		Vec2 pos = viewport.size * Vec2(0.5f, 0.1f);

		draw_hp_box(params, pos, health->hp_max);

		b8 flash_hp = health_flash_timer > 0.0f;
		if (!flash_hp || UI::flash_function(Game::real_time.total))
			draw_hp_indicator(params, pos, health->hp, health->hp_max, flash_hp ? UI::default_color : Team::ui_color_friend);
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

	// stunned indicator
	if (get<Awk>()->stun_timer > 0.0f)
	{
		UIText text;
		text.color = UI::alert_color;
		text.text(_(strings::stunned));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.6f);
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
			text.text(_(strings::alarm));
			text.draw(params, bar.pos + bar.size * 0.5f);

			// todo: sound
		}
	}

	// ability spawn timer
	{
		PlayerManager* manager = get<PlayerCommon>()->manager.ref();
		if (manager->current_spawn_ability != Ability::None
			|| manager->current_upgrade_ability != Ability::None)
		{
			const AbilityInfo& info = AbilityInfo::list[manager->current_upgrade_ability != Ability::None ? (s32)manager->current_upgrade_ability : (s32)manager->current_spawn_ability];
			r32 ability_timer = manager->current_upgrade_ability != Ability::None ? manager->upgrade_timer : manager->spawn_ability_timer;
			
			// draw bar

			r32 total_time;
			AssetID string;
			if (manager->current_upgrade_ability != Ability::None)
			{
				// if we're at the spawn, we're upgrading the ability, not spawning it
				total_time = ABILITY_UPGRADE_TIME;
				string = strings::upgrading;
			}
			else
			{
				total_time = info.spawn_time;
				string = strings::ability_spawn_cost;
			}

			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { pos + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::background_color);
			UI::border(params, bar, 2, UI::accent_color);
			UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (ability_timer / total_time)), bar.size.y) }, UI::accent_color);

			UIText text;
			text.size = 18.0f;
			text.color = UI::background_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(string), info.spawn_cost);
			text.draw(params, bar.pos + bar.size * 0.5f);
		}
	}

	// reticle
	if (movement_enabled())
	{
		switch (reticle.type)
		{
			case ReticleType::None:
			case ReticleType::Error:
			{
				// hollow reticle
				r32 cooldown = get<Awk>()->cooldown;
				r32 radius = cooldown == 0.0f ? 0.0f : vi_max(0.0f, 32.0f * (cooldown / AWK_MAX_DISTANCE_COOLDOWN));
				UI::triangle_border(params, { viewport.size * Vec2(0.5f, 0.5f), Vec2(4.0f + radius) * 2 * UI::scale }, 2, reticle.type == ReticleType::Error ? UI::alert_color : UI::accent_color, PI);
				break;
			}
			case ReticleType::Normal:
			{
				// solid reticle
				Vec2 a;
				if (UI::project(params, reticle.pos, &a))
					UI::triangle(params, { a, Vec2(12) * UI::scale }, UI::accent_color, PI);
				break;
			}
			case ReticleType::Target:
			{
				Vec2 a;
				if (UI::project(params, reticle.pos, &a))
				{
					UI::triangle(params, { a, Vec2(12) * UI::scale }, UI::alert_color, PI);

					const r32 ratio = 0.8660254037844386f;
					UI::centered_box(params, { a + Vec2(12.0f * ratio, -6.0f) * UI::scale, Vec2(8.0f, 2.0f) * UI::scale }, UI::accent_color, PI * 0.5f * -0.33f);
					UI::centered_box(params, { a + Vec2(-12.0f * ratio, -6.0f) * UI::scale, Vec2(8.0f, 2.0f) * UI::scale }, UI::accent_color, PI * 0.5f * 0.33f);
					UI::centered_box(params, { a + Vec2(0, 12.0f) * UI::scale, Vec2(2.0f, 8.0f) * UI::scale }, UI::accent_color);
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
}


}
