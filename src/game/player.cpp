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
#include "asset/font.h"
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

b8 flash_function(r32 time)
{
	return (b8)((s32)(time * 16.0f) % 2);
}

void draw_indicator(const RenderParams& params, const Vec3& pos, const Vec4& color, b8 offscreen)
{
	const Rect2& viewport = params.camera->viewport;
	Vec2 screen_pos;
	b8 on_screen = UI::project(params, pos, &screen_pos);
	Vec2 center = viewport.size * 0.5f;
	Vec2 offset = screen_pos - center;
	if (!on_screen)
	{
		if (offscreen)
			offset *= -1.0f;
		else
			return;
	}

	r32 pointer_size = 48 * UI::scale;

	r32 radius = vi_min(viewport.size.x, viewport.size.y) * 0.5f - pointer_size;

	r32 offset_length = offset.length();
	if (offscreen && (offset_length > radius || (offset_length > 0.0f && !on_screen)))
	{
		offset *= radius / offset_length;
		UI::triangle(params, { center + offset, Vec2(pointer_size) }, color, atan2f(offset.y, offset.x) + PI * -0.5f);
	}
	else
		UI::triangle_border(params, { screen_pos, Vec2(32) * UI::scale }, 4, color);
}

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)
void draw_hp_indicator(const RenderParams& params, Vec2 pos, u16 hp, u16 hp_max, const Vec4& color)
{
	const Vec2 box_size = HP_BOX_SIZE;
	pos.x += HP_BOX_SPACING * 1.25f;
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
	credits_text(),
	msg_text(),
	msg_timer(msg_time),
	menu(),
	revision(),
	menu_state(),
	ability_menu()
{
	sprintf(manager.ref()->username, _(strings::player), gamepad);

	msg_text.font = Asset::Font::lowpoly;
	msg_text.size = text_size;
	msg_text.anchor_x = UIText::Anchor::Center;
	msg_text.anchor_y = UIText::Anchor::Center;

	credits_text.font = Asset::Font::lowpoly;
	credits_text.size = text_size;
	credits_text.anchor_x = UIText::Anchor::Min;
	credits_text.anchor_y = UIText::Anchor::Min;

	m->spawn.link<LocalPlayer, &LocalPlayer::spawn>(this);
}

LocalPlayer::UIMode LocalPlayer::ui_mode() const
{
	if (menu_state != Menu::State::Hidden)
		return UIMode::Pause;
	else if (ability_menu != AbilityMenu::None)
		return UIMode::AbilityMenu;
	else if (manager.ref()->entity.ref() || NoclipControl::list.count() > 0)
		return UIMode::Default;
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
		camera->fog = Game::data.mode == Game::Mode::Parkour;
		camera->team = (u8)manager.ref()->team.ref()->team();
		camera->mask = 1 << camera->team;
		s32 player_count;
#if DEBUG_AI_CONTROL
		player_count = list.count() + AIPlayer::list.count();
#else
		player_count = list.count();
#endif
		Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
		Camera::ViewportBlueprint* blueprint = &viewports[gamepad];

		camera->viewport =
		{
			Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
			Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
		};
		r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
		camera->perspective(60.0f * PI * 0.5f / 180.0f, aspect, 1.0f, Skybox::far_plane * 2.0f);
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

#define TUTORIAL_TIME 3.0f

void LocalPlayer::update(const Update& u)
{
	if (Console::visible)
		return;

	// credits
	{
		credits_text.text(_(strings::credits), manager.ref()->credits);
		credits_text.color = manager.ref()->credits_flash_timer > 0.0f ? UI::default_color : UI::accent_color;
	}

	if (msg_timer < msg_time)
		msg_timer += u.time.delta;

	if (manager.ref()->entity.ref())
		manager.ref()->entity.ref()->get<LocalPlayerControl>()->enable_input = ui_mode() == UIMode::Default;

	// close/open pause menu if needed
	{
		b8 pause_hit = Game::time.total > 0.5f && u.input->get(Controls::Pause, gamepad) && !u.last_input->get(Controls::Pause, gamepad);
		if (pause_hit && ability_menu == AbilityMenu::None && (menu_state == Menu::State::Hidden || menu_state == Menu::State::Visible))
			menu_state = menu_state == Menu::State::Hidden ? Menu::State::Visible : Menu::State::Hidden;
	}

	switch (ui_mode())
	{
		case UIMode::Default:
		{
			// nothing going on
			ensure_camera(u, false);
			if (manager.ref()->entity.ref())
				manager.ref()->entity.ref()->get<LocalPlayerControl>()->enable_input = true;
			if (Game::data.mode == Game::Mode::Pvp && Game::data.has_feature(Game::FeatureLevel::Abilities))
			{
				if (u.input->get(Controls::Menu, gamepad) && !u.last_input->get(Controls::Menu, gamepad))
				{
					if (manager.ref()->at_spawn())
						ability_menu = AbilityMenu::Upgrade;
					else
						manager.ref()->ability_switch();
				}
			}

			break;
		}
		case UIMode::AbilityMenu:
		{
			menu.start(u, gamepad);
			Rect2& viewport = camera ? camera->viewport : manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera->viewport;

			if ((u.input->get(Controls::Cancel, gamepad) && !u.last_input->get(Controls::Cancel, gamepad))
				|| (u.input->get(Controls::Menu, gamepad) && !u.last_input->get(Controls::Menu, gamepad)))
				ability_menu = AbilityMenu::None;

			// do menu items
			Vec2 pos = Vec2(viewport.size.x * 0.5f + (MENU_ITEM_WIDTH * -0.5f), viewport.size.y * 0.75f);

			if (ability_menu == AbilityMenu::Upgrade)
			{
				if (menu.item(u, &pos, _(strings::back)))
					ability_menu = AbilityMenu::None;

				// upgrade
				for (s32 i = 0; i < (s32)Ability::count; i++)
				{
					Ability a = (Ability)i;

					// make sure there's an upgrade available for this ability
					if (manager.ref()->ability_upgrade_available(a))
					{
						u16 cost = manager.ref()->ability_upgrade_cost(a);
						char cost_str[255];
						sprintf(cost_str, _(strings::credits), cost);

						const AbilityInfo& info = AbilityInfo::list[(s32)a];
						char upgrade_str[255];
						sprintf(upgrade_str, _(strings::ability_lvl), _(info.name), manager.ref()->ability_level[(s32)a] + 1);

						if (menu.item(u, &pos, upgrade_str, cost_str, manager.ref()->credits < cost, info.icon))
						{
							manager.ref()->ability_upgrade(a);
							manager.ref()->ability_switch(a);
						}
					}
				}
			}

			menu.end();
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

			// if we are showing a tutorial message, make the user accept it
			if (!manager.ref()->ready)
			{
				if (Game::time.total > TUTORIAL_TIME && !u.input->get(Controls::Action, gamepad) && u.last_input->get(Controls::Action, gamepad))
					manager.ref()->ready = true;
			}

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
	Vec3 dir = rot * Vec3(0, 0, 1);
	r32 angle = atan2f(dir.x, dir.z);

	Entity* spawned;

	if (Game::data.mode == Game::Mode::Pvp)
	{
		// Spawn AWK
		pos += Quat::euler(0, angle + (gamepad * PI * 0.5f), 0) * Vec3(0, 0, PLAYER_SPAWN_RADIUS * 0.5f); // spawn it around the edges
		spawned = World::create<AwkEntity>(manager.ref()->team.ref()->team());
	}
	else // parkour mode
	{
		// Spawn traceur
		spawned = World::create<Traceur>(pos, Quat::euler(0, angle, 0), manager.ref()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute(pos, rot);
	PlayerCommon* common = spawned->add<PlayerCommon>(manager.ref());
	common->angle_horizontal = angle;

	manager.ref()->entity = spawned;

	LocalPlayerControl* control = spawned->add<LocalPlayerControl>(gamepad);
	control->player = this;
}

AssetID tutorial_messages[(s32)Game::FeatureLevel::count] =
{
	strings::tut_base,
	strings::tut_hp,
	strings::tut_abilities,
	strings::tut_control_points,
	strings::tut_minion_spawns,
};

void LocalPlayer::draw_alpha(const RenderParams& params) const
{
	if (params.camera != camera && (!manager.ref()->entity.ref() || params.camera != manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera))
		return;

	const r32 line_thickness = 2.0f * UI::scale;
	const r32 padding = 6.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	// message
	if (msg_timer < msg_time)
	{
		r32 last_timer = msg_timer;
		b8 flash = flash_function(Game::real_time.total);
		b8 last_flash = flash_function(Game::real_time.total - Game::real_time.delta);
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

	if (Game::data.mode == Game::Mode::Pvp && Game::data.has_feature(Game::FeatureLevel::Abilities))
	{
		b8 draw = true;
		if (manager.ref()->credits_flash_timer > 0.0f)
		{
			draw = flash_function(Game::real_time.total);
			b8 last_flash = flash_function(Game::real_time.total - Game::real_time.delta);
			if (draw && !last_flash)
				Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
		}

		Vec2 pos = vp.size * Vec2(0.1f, 0.1f);
		UI::box(params, credits_text.rect(pos).outset(8 * UI::scale), UI::background_color);
		if (draw)
			credits_text.draw(params, pos);
	}

	UIMode mode = ui_mode();
	if (mode == UIMode::Default)
	{
		if (Game::data.has_feature(Game::FeatureLevel::Abilities))
		{
			b8 at_spawn = manager.ref()->at_spawn();
			if (manager.ref()->ability_upgrade_available() || at_spawn)
			{
				// show ability menu / upgrade prompt
				UIText text;
				text.font = Asset::Font::lowpoly;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Min;
				text.size = text_size;
				text.color = UI::accent_color;
				text.text(_(at_spawn ? strings::ability_menu : strings::upgrade_prompt));
				Vec2 p = vp.size * Vec2(0.1f, 0.1f) + Vec2(0, (44.0f + text_size * 1.75f) * UI::scale);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
		}

		Team* team = manager.ref()->team.ref();

		// display status of other players in upper right corner
		UIText text;
		text.font = Asset::Font::lowpoly;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Min;
		text.size = text_size;
		text.color = UI::default_color;
		const r32 row_padding = 8.0f * UI::scale;
		const r32 row_height = (text_size * UI::scale) + row_padding;
		const Vec2 box_size(128.0f * UI::scale, (row_height * 3) + (row_padding * 2));
		Vec2 ui_pos = vp.size * Vec2(0.9f, 0.9f) + Vec2(-box_size.x, 0);
		Entity* entity = manager.ref()->entity.ref();
		for (auto other_player = PlayerManager::list.iterator(); !other_player.is_last(); other_player.next())
		{
			if (other_player.item()->id() != manager.id)
			{
				// extract player information
				b8 friendly = other_player.item()->team.ref() == team;
				const Vec4& team_color = Team::ui_color(team->team(), other_player.item()->team.ref()->team());
				Entity* other_player_entity = other_player.item()->entity.ref();
				const b8 tracking = friendly || team->player_tracks[other_player.index].tracking;
				const b8 visible = tracking || (entity && other_player_entity && PlayerCommon::visibility.get(PlayerCommon::visibility_hash(entity->get<PlayerCommon>(), other_player_entity->get<PlayerCommon>())));
				const Vec4& color = visible ? team_color : UI::disabled_color;

				Team::SensorTrackHistory history;
				if (friendly)
					Team::extract_history(other_player.item(), &history);
				else
					history = team->player_track_history[other_player.index];

				// background
				Vec2 box_pos = ui_pos + Vec2(0, (text_size * UI::scale) - box_size.y);
				UI::box(params, Rect2(box_pos, box_size).outset(row_padding), UI::background_color);

				// username
				text.color = team_color;
				text.text(other_player.item()->username);
				text.draw(params, ui_pos);
				ui_pos.y -= row_height;
				text.color = color;

				// hp
				if (history.hp_max > 0)
				{
					draw_hp_indicator(params, ui_pos, history.hp, history.hp_max, color);
					ui_pos.y -= row_height;
				}

				// ability / level
				if (history.ability != Ability::None)
				{
					UI::mesh(params, AbilityInfo::list[(s32)history.ability].icon, ui_pos + Vec2(text_size * UI::scale * 0.5f), Vec2(text_size * UI::scale), color);
					text.text(_(strings::lvl), history.ability_level);
					text.draw(params, ui_pos + Vec2(text_size * 1.5f, 0));
					ui_pos.y -= row_height;
				}

				// credits
				if (Game::data.has_feature(Game::FeatureLevel::Abilities))
				{
					text.text("%d", history.credits);
					text.draw(params, ui_pos);
					ui_pos.y -= row_height;
				}

				ui_pos = box_pos + Vec2(0, -box_size.y - row_height);
			}
		}
	}
	else if (mode == UIMode::Spawning)
	{
		// player is dead

		b8 show_player_list;
		b8 show_spawning;
		if (Game::data.mode == Game::Mode::Pvp)
		{
			if (Team::game_over())
			{
				// we lost, we're not spawning again
				show_player_list = false;
				show_spawning = false;
			}
			else
			{
				// haven't spawned yet
				if (PlayerManager::all_ready())
				{
					show_player_list = true;
					show_spawning = true;
				}
				else 
				{
					// we need to show a tutorial message first
					show_spawning = false;

					if (manager.ref()->ready)
						show_player_list = true;
					else
					{
						show_player_list = false;

						UIText text;
						text.size = text_size;
						text.font = Asset::Font::lowpoly;
						text.wrap_width = MENU_ITEM_WIDTH;
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Center;
						text.color = UI::default_color;
						text.text(_(tutorial_messages[(s32)Game::data.feature_level]));
						Vec2 p = vp.size * Vec2(0.5f);
						const r32 padding = 8.0f * UI::scale;
						UI::box(params, text.rect(p).outset(padding), UI::background_color);
						text.draw(params, p);

						if (Game::time.total > TUTORIAL_TIME)
						{
							p.y -= text.bounds().y + padding * 2.0f;
							text.text(_(strings::accept));
							text.color = UI::accent_color;
							UI::box(params, text.rect(p).outset(padding), UI::background_color);
							text.draw(params, p);
						}
					}
				}
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
			text.font = Asset::Font::lowpoly;
			text.wrap_width = MENU_ITEM_WIDTH;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.color = UI::default_color;

			if (show_spawning)
			{
				// "spawning..."
				text.text(_(strings::spawning), (s32)manager.ref()->spawn_timer + 1);
				const r32 padding = 8.0f * UI::scale;
				UI::box(params, text.rect(p).outset(padding), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + padding * 2.0f;
			}

			// show map name
			text.text(AssetLookup::Level::names[Game::data.level]);
			text.color = UI::default_color;
			UI::box(params, text.rect(p).outset(padding), UI::background_color);
			text.draw(params, p);
			p.y -= text.bounds().y + padding * 2.0f;

			// show player list
			text.anchor_x = UIText::Anchor::Min;
			p.x -= MENU_ITEM_WIDTH * 0.5f;
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				text.text(i.item()->username);
				text.color = Team::ui_color(manager.ref()->team.ref()->team(), i.item()->team.ref()->team());
				UI::box(params, text.rect(p).outset(padding), UI::background_color);
				text.draw(params, p);
				p.y -= text.bounds().y + padding * 2.0f;
			}
		}
	}
	else if (mode == UIMode::AbilityMenu)
		menu.draw_alpha(params);

	if (Game::data.mode == Game::Mode::Pvp)
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
			if (remaining > GAME_TIME_LIMIT * 0.75f)
			{
				icon = Asset::Mesh::icon_battery_3;
				color = &UI::default_color;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.5f)
			{
				icon = Asset::Mesh::icon_battery_2;
				color = &UI::default_color;
			}
			else if (remaining > GAME_TIME_LIMIT * 0.25f)
			{
				icon = Asset::Mesh::icon_battery_1;
				color = &UI::accent_color;
			}
			else
			{
				icon = Asset::Mesh::icon_battery_0;
				color = &UI::alert_color;
			}

			if (remaining > 30.0f || flash_function(Game::real_time.total))
				UI::mesh(params, icon, icon_pos, Vec2(text_size * UI::scale), *color);
			
			s32 remaining_minutes = remaining / 60.0f;
			s32 remaining_seconds = remaining - (remaining_minutes * 60.0f);

			UIText text;
			text.font = Asset::Font::lowpoly;
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
			text.font = Asset::Font::lowpoly;
			text.color = UI::alert_color;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = 32.0f;

			if (Team::is_draw())
				text.text(_(strings::draw));
			else if (manager.ref()->team.ref()->has_player())
				text.text(_(strings::victory));
			else
				text.text(_(strings::defeat));
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
	cooldown(),
	username_text(),
	manager(m),
	cooldown_multiplier(1.0f)
{
	username_text.font = Asset::Font::lowpoly;
	username_text.size = 18.0f;
	username_text.anchor_x = UIText::Anchor::Center;
	username_text.text(m->username);
}

void PlayerCommon::awake()
{
	username_text.color = Team::ui_color_friend;
	if (has<Awk>())
	{
		get<Health>()->hp_max = AWK_HEALTH;
		link<&PlayerCommon::awk_attached>(get<Awk>()->attached);
		link<&PlayerCommon::awk_detached>(get<Awk>()->detached);
	}
}

void PlayerCommon::update(const Update& u)
{
	if (has<Parkour>() || get<Transform>()->parent.ref())
	{
		// Either we are a Minion, or we're an Awk and we're attached to a surface
		// Either way, we need to decrement the cooldown timer
		cooldown = vi_max(0.0f, cooldown - u.time.delta * cooldown_multiplier);
	}

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

		for (s32 j = 0; j < MAX_PLAYERS; j++)
		{
			Team::SensorTrack* track = &team->player_tracks[j];
			if (track->entity.ref() == entity())
			{
				if (track->tracking)
					return 1.0f;
				else
					return track->timer / team->sensor_time;
			}
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

Bitmask<MAX_PLAYERS> PlayerCommon::visibility;

s32 PlayerCommon::visibility_hash(const PlayerCommon* awk_a, const PlayerCommon* awk_b)
{
	b8 a_index_lower = awk_a->id() < awk_b->id();
	s32 a = a_index_lower ? awk_a->id() : awk_b->id();
	s32 b = a_index_lower ? awk_b->id() : awk_a->id();
	s32 a_index = MAX_PLAYERS - combination(MAX_PLAYERS - a, 2);
	s32 hash = a_index + (b - a) - 1;
	s32 max_hash = MAX_PLAYERS - 1;
	return hash > 0 ? (hash < max_hash ? hash : max_hash) : 0;
}

void LocalPlayerControl::awk_attached()
{
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

LocalPlayerControl::LocalPlayerControl(u8 gamepad)
	: gamepad(gamepad),
	fov_blend(),
	allow_zoom(true),
	try_jump(),
	try_parkour(),
	enable_input(),
	damage_timer(),
	health_pickup_timer()
{
	camera = Camera::add();
}

LocalPlayerControl::~LocalPlayerControl()
{
	Audio::listener_disable(gamepad);
	camera->remove();
}

void LocalPlayerControl::awake()
{
	Audio::listener_enable(gamepad);

	if (has<Awk>())
	{
		link<&LocalPlayerControl::awk_attached>(get<Awk>()->attached);
		link_arg<Entity*, &LocalPlayerControl::hit_target>(get<Awk>()->hit);
		link_arg<const DamageEvent&, &LocalPlayerControl::damaged>(get<Health>()->damaged);
		link<&LocalPlayerControl::health_picked_up>(get<Health>()->added);
	}

	camera->fog = Game::data.mode == Game::Mode::Parkour;
	camera->team = (u8)get<AIAgent>()->team;
	camera->mask = 1 << camera->team;
}

void LocalPlayerControl::hit_target(Entity* target)
{
	if (target->has<MinionAI>())
	{
		player.ref()->msg(_(strings::target_killed), true);
		if (target->get<AIAgent>()->team != get<AIAgent>()->team)
			player.ref()->manager.ref()->add_credits(CREDITS_MINION);
	}
	else if (target->has<Sensor>())
		player.ref()->msg(_(strings::sensor_destroyed), true);
}

void LocalPlayerControl::damaged(const DamageEvent& e)
{
	char msg[255];
	sprintf(msg, _(strings::damaged), e.amount);
	player.ref()->msg(msg, false);
	damage_timer = damage_shake_time;
}

void LocalPlayerControl::health_picked_up()
{
	health_pickup_timer = msg_time;
}

b8 LocalPlayerControl::input_enabled() const
{
	return !Console::visible && enable_input;
}

void LocalPlayerControl::update_camera_input(const Update& u)
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
			get<PlayerCommon>()->angle_horizontal -= s * u.time.delta * Input::dead_zone(u.input->gamepads[gamepad].right_x);
			get<PlayerCommon>()->angle_vertical += s * u.time.delta * Input::dead_zone(u.input->gamepads[gamepad].right_y);
		}

		get<PlayerCommon>()->angle_vertical = LMath::clampf(get<PlayerCommon>()->angle_vertical, PI * -0.495f, PI * 0.495f);
	}
}

Vec3 LocalPlayerControl::get_movement(const Update& u, const Quat& rot)
{
	Vec3 movement = Vec3::zero;
	if (input_enabled())
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

void LocalPlayerControl::detach(const Vec3& dir)
{
	if (get<Awk>()->detach(dir))
	{
		allow_zoom = false;
		get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
	}
}

void LocalPlayerControl::add_target_indicator(Target* target, const Vec4& color, b8 offscreen)
{
	Vec3 me = get<Awk>()->center();
	if (offscreen || (target->absolute_pos() - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
	{
		// calculate head intersection trajectory
		Vec3 intersection;
		if (get<Awk>()->predict_intersection(target, &intersection))
		{
			if (tracer.type == TraceType::Normal && LMath::ray_sphere_intersect(me, tracer.pos, intersection, MINION_HEAD_RADIUS))
				tracer.type = TraceType::Target;
			indicators.add({ intersection, &color, offscreen });
		}
	}
}

void determine_visibility(PlayerCommon* me, PlayerCommon* other_player, b8* visible, b8* tracking)
{
	// make sure we can see this guy
	AI::Team team = me->get<AIAgent>()->team;
	*tracking = Team::list[(s32)team].player_tracks[other_player->id()].tracking;
	*visible = other_player->get<AIAgent>()->team == team
		|| PlayerCommon::visibility.get(PlayerCommon::visibility_hash(me, other_player));
}

void LocalPlayerControl::update(const Update& u)
{
	{
		// Zoom
		r32 fov_blend_target = 0.0f;
		if (has<Awk>() && get<Transform>()->parent.ref() && Game::data.allow_detach && input_enabled())
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

	Vec3 camera_pos;
	Quat look_quat;

	if (has<Awk>())
	{
		// Awk control code
		if (get<Transform>()->parent.ref())
		{
			// Look
			update_camera_input(u);
			get<PlayerCommon>()->clamp_rotation(get<PlayerCommon>()->attach_quat * Vec3(0, 0, 1), 0.5f);
			look_quat = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

			// crawling
			Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical));
			get<Awk>()->crawl(movement, u);
		}
		else
			look_quat = get<PlayerCommon>()->attach_quat;

		camera_pos = get<Awk>()->center();

		// abilities
		if (!Console::visible && u.input->get(Controls::Ability, gamepad) && !u.last_input->get(Controls::Ability, gamepad))
			player.ref()->manager.ref()->ability_use();
	}
	else
	{
		// Minion control code
		update_camera_input(u);
		look_quat = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);
		{
			camera_pos = Vec3(0, 0, 0.05f);
			Quat q = Quat::identity;
			get<Parkour>()->head_to_object_space(&camera_pos, &q);
			camera_pos = get<Transform>()->to_world(camera_pos);
		}

		Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, 0));
		Vec2 dir = Vec2(movement.x, movement.z);
		get<Walker>()->dir = dir;

		// parkour button
		b8 parkour_pressed = input_enabled() && u.input->get(Controls::Parkour, gamepad);

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
		b8 jump_pressed = input_enabled() && u.input->get(Controls::Jump, gamepad);
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
		b8 slide_pressed = input_enabled() && u.input->get(Controls::Slide, gamepad);

		if (get<Parkour>()->fsm.current == Parkour::State::Slide && !slide_pressed)
			get<Parkour>()->fsm.transition(Parkour::State::Normal);

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

		{
			r32 arm_angle = LMath::clampf(get<PlayerCommon>()->angle_vertical * 0.5f, -PI * 0.15f, PI * 0.15f);
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_L, Vec3::zero, Quat::euler(arm_angle, 0, 0));
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_R, Vec3::zero, Quat::euler(-arm_angle, 0, 0));
		}

		r32 lean_target = 0.0f;

		if (get<Parkour>()->fsm.current == Parkour::State::WallRun)
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
		else if (get<Parkour>()->fsm.current == Parkour::State::Slide)
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

		// camera bone affects rotation only
		Quat camera_animation = Quat::euler(PI * -0.5f, 0, 0);
		get<Animator>()->bone_transform(Asset::Bone::character_camera, nullptr, &camera_animation);

		look_quat = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Quat::euler(0, PI * 0.5f, 0) * camera_animation * Quat::euler(0, PI * -0.5f, 0);
	}
	
	s32 player_count;
#if DEBUG_AI_CONTROL
	player_count = LocalPlayer::list.count() + AIPlayer::list.count(); // AI player views are shown on screen as well
#else
	player_count = LocalPlayer::list.count();
#endif
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[gamepad];

	if (has<Awk>())
		camera->range = AWK_MAX_DISTANCE;
	else
		camera->range = 0.0f;

	camera->viewport =
	{
		Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
		Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective(LMath::lerpf(fov_blend, fov_initial, fov_zoom), aspect, 0.02f, Skybox::far_plane);

	// Camera matrix
	if (has<Awk>())
		camera->wall_normal = look_quat.inverse() * ((get<Transform>()->absolute_rot() * get<Awk>()->lerped_rotation) * Vec3(0, 0, 1));
	else
		camera->wall_normal = Vec3(0, 0, 1);
	camera->pos = camera_pos + (Game::data.third_person ? look_quat * Vec3(0, 0, -2) : Vec3::zero);
	if (damage_timer > 0.0f)
	{
		damage_timer -= u.time.delta;
		r32 shake = (damage_timer / damage_shake_time) * 0.3f;
		r32 offset = Game::time.total * 10.0f;
		look_quat = look_quat * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
	}
	health_pickup_timer = vi_max(0.0f, health_pickup_timer - Game::real_time.delta);
	camera->rot = look_quat;

	tracer.type = TraceType::None;
	if (has<Awk>() && get<Transform>()->parent.ref() && get<PlayerCommon>()->cooldown == 0.0f)
	{
		// Display trajectory
		Vec3 trace_dir = look_quat * Vec3(0, 0, 1);
		// Make sure we're not shooting into the wall we're on.
		// If we're a sentinel, no need to worry about that.
		if (has<Parkour>() || trace_dir.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1)) > 0.0f)
		{
			Vec3 trace_start = camera_pos;
			Vec3 trace_end = trace_start + trace_dir * AWK_MAX_DISTANCE;

			AwkRaycastCallback rayCallback(trace_start, trace_end, entity());
			rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces
				| btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
			rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = ~CollisionAwkIgnore;

			Physics::btWorld->rayTest(trace_start, trace_end, rayCallback);

			if (rayCallback.hasHit())
			{
				tracer.type = rayCallback.hit_target() ? TraceType::Target : TraceType::Normal;
				tracer.pos = rayCallback.m_hitPointWorld;

				short group = rayCallback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
				if (group & (CollisionWalker | CollisionInaccessible))
					tracer.type = TraceType::None; // We can't go there
			}
		}

		if (tracer.type != TraceType::None && Game::data.allow_detach && input_enabled())
		{
			if (u.input->get(Controls::Primary, gamepad) && !u.last_input->get(Controls::Primary, gamepad))
				detach(look_quat * Vec3(0, 0, 1));
		}
	}

	Audio::listener_update(gamepad, camera_pos, look_quat);

	// collect indicators
	indicators.length = 0;
	if (Game::data.mode == Game::Mode::Pvp)
	{
		Vec3 me = get<Transform>()->absolute_pos();

		// awk indicators
		if (indicators.length < indicators.capacity())
		{
			for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
			{
				if (other_player.item() != get<PlayerCommon>())
				{
					b8 visible, tracking;
					determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

					if (tracking || visible)
						add_target_indicator(other_player.item()->get<Target>(), UI::alert_color, true);

					if (indicators.length == indicators.capacity())
						break;
				}
			}
		}

		// headshot indicators
		if (indicators.length < indicators.capacity())
		{
			AI::Team team = get<AIAgent>()->team;
			for (auto i = MinionCommon::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->get<AIAgent>()->team != team)
				{
					add_target_indicator(i.item()->get<Target>(), UI::alert_color);
					if (indicators.length == indicators.capacity())
						break;
				}
			}
		}

		// minion spawns
		if (indicators.length < indicators.capacity())
		{
			for (auto i = MinionSpawn::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->minion.ref())
				{
					add_target_indicator(i.item()->get<Target>(), UI::default_color);
					if (indicators.length == indicators.capacity())
						break;
				}
			}
		}

		// health pickups
		if (get<Health>()->hp < get<Health>()->hp_max)
		{
			for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
			{
				if (!i.item()->owner.ref())
				{
					add_target_indicator(i.item()->get<Target>(), UI::accent_color);
					if (indicators.length == indicators.capacity())
						break;
				}
			}
		}
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
	if (params.technique != RenderTechnique::Default || params.camera != camera)
		return;

	AI::Team team = get<AIAgent>()->team;

	const Rect2& viewport = params.camera->viewport;

	// compass
	{
		Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);
		if (Game::data.mode == Game::Mode::Pvp)
		{
			b8 enemy_attacking = false;

			Vec3 me = get<Transform>()->absolute_pos();

			for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
			{
				if (PlayerCommon::visibility.get(PlayerCommon::visibility_hash(i.item(), get<PlayerCommon>())))
				{
					// determine if they're attacking us
					if (!i.item()->get<Transform>()->parent.ref()
						&& Vec3::normalize(i.item()->get<Awk>()->velocity).dot(Vec3::normalize(me - i.item()->get<Transform>()->absolute_pos())) > 0.95f)
						enemy_attacking = true;
				}
			}

			if (enemy_attacking)
			{
				// we're being attacked; flash the compass
				b8 show = flash_function(Game::real_time.total);
				if (show)
					UI::mesh(params, Asset::Mesh::compass_inner, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::alert_color);
				if (show && !flash_function(Game::real_time.total - Game::real_time.delta))
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_BAD);
			}

			// draw indicators pointing toward player spawns
			for (s32 i = 0; i < Team::list.capacity(); i++)
			{
				Team* t = &Team::list[i];
				if (t->player_spawn.ref())
				{
					Vec3 to_spawn = t->player_spawn.ref()->get<Transform>()->absolute_pos() - me;
					r32 distance = to_spawn.length();
					if (distance > PLAYER_SPAWN_RADIUS)
					{
						to_spawn /= distance;
						UI::mesh(params, Asset::Mesh::compass_indicator, viewport.size * Vec2(0.5f, 0.5f), compass_size, Team::ui_color(team, t->team()), atan2f(to_spawn.x, to_spawn.z) - get<PlayerCommon>()->angle_horizontal);
					}
				}
			}
		}
	}

	if (Game::data.mode == Game::Mode::Pvp)
	{
		// draw indicators
		for (s32 i = 0; i < indicators.length; i++)
		{
			const Indicator& indicator = indicators[i];
			draw_indicator(params, indicator.pos, *indicator.color, indicator.offscreen);
		}
	}
	else
	{
		// todo: highlight notes
	}

	// draw usernames directly over players' 3D positions
	for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
	{
		if (other_player.item() != get<PlayerCommon>())
		{
			b8 visible, tracking;
			determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

			Vec3 other_pos = Team::list[(s32)team].player_track_history[other_player.item()->manager.id].pos;

			if (!tracking && !visible) // if we can actually see them, the indicator has already been added using add_target_indicator in the update function
				draw_indicator(params, other_pos, UI::disabled_color, false);

			Vec2 p;
			if (UI::project(params, other_pos + Vec3(0, AWK_RADIUS * 2.0f, 0), &p))
			{
				p.y += 16.0f * UI::scale;
				UIText text;
				text.size = text_size;
				text.font = Asset::Font::lowpoly;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.color = tracking || visible ? Team::ui_color(team, other_player.item()->get<AIAgent>()->team) : UI::disabled_color;
				text.text(other_player.item()->manager.ref()->username);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
		}
	}

	// health indicator
	if (has<Health>())
	{
		const Health* health = get<Health>();

		UIText text;
		text.font = Asset::Font::lowpoly;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Min;
		text.size = text_size;
		text.color = UI::default_color;
		text.text(_(strings::hp));

		const Vec2 box_size = HP_BOX_SIZE;

		r32 total_width = (text.bounds().x + HP_BOX_SPACING + health->hp_max * (box_size.x + HP_BOX_SPACING)) - HP_BOX_SPACING;

		Vec2 pos = viewport.size * Vec2(0.5f, 0.1f) + Vec2(total_width * -0.5f, 0.0f);

		UI::box(params, Rect2(pos, Vec2(total_width, box_size.y)).outset(HP_BOX_SPACING), UI::background_color);

		text.draw(params, pos);
		
		pos.x += text.bounds().x + HP_BOX_SPACING * 0.5f;

		b8 flash_hp = damage_timer > 0.0f || health_pickup_timer > 0.0f;
		if (!flash_hp || flash_function(Game::real_time.total))
			draw_hp_indicator(params, pos, health->hp, health->hp_max, flash_hp ? UI::default_color : Team::ui_color_friend);
	}

	if (has<Awk>())
	{
		// ability icon/status
		Ability ability = player.ref()->manager.ref()->ability;
		if (ability != Ability::None)
		{
			UIText text;
			text.font = Asset::Font::lowpoly;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 icon_pos = viewport.size * Vec2(0.1f, 0.1f) + Vec2(8.0f * UI::scale, 44.0f * UI::scale);

			const AbilityInfo& info = AbilityInfo::list[(s32)ability];
			text.text(_(strings::binding), Settings::gamepads[gamepad].bindings[(s32)Controls::Ability].string(params.sync->input.gamepads[gamepad].active));

			r32 text_offset = 24.0f * UI::scale;
			r32 icon_size = 8.0f * UI::scale;
			Vec2 text_pos = icon_pos + Vec2(text_offset, 0);

			Rect2 box = text.rect(text_pos).outset(8.0f * UI::scale);
			box.pos.x -= (text_offset + icon_size);
			box.size.x += (text_offset + icon_size);
			UI::box(params, box, UI::background_color);

			r32 ability_cooldown = player.ref()->manager.ref()->ability_cooldown;
			if (ability_cooldown > 0.0f)
			{
				if (ability_cooldown > ABILITY_COOLDOWN_USABLE_RANGE)
				{
					// ability is not usable
					r32 cooldown_normalized = (ability_cooldown - ABILITY_COOLDOWN_USABLE_RANGE) / info.cooldown;
					UI::box(params, { box.pos, Vec2(box.size.x * cooldown_normalized, box.size.y) }, UI::subtle_color);
					text.color = UI::disabled_color;
				}

				if (ability_cooldown < ABILITY_COOLDOWN_USABLE_RANGE || ability_cooldown > info.cooldown - ABILITY_COOLDOWN_USABLE_RANGE)
				{
					// we either just used it, or the cooldown is about to expire
					// flash it
					text.color = UI::default_color;
					b8 show = flash_function(Game::real_time.total);
					if (show)
						text.color.w = 0.0f;
					if (show && !flash_function(Game::real_time.total - Game::real_time.delta))
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
			else
				text.color = UI::accent_color;

			if (info.icon != AssetNull)
				UI::mesh(params, info.icon, icon_pos, Vec2(UI::scale * text_size), text.color);

			text.draw(params, text_pos);

			icon_pos.y += box.size.y;
		}

		// stealth indicator
		{
			r32 stealth_timer = get<Awk>()->stealth_timer;
			if (stealth_timer > 0.0f)
			{
				UIText text;
				text.color = UI::accent_color;
				text.text("%s %d", _(strings::stealth), (s32)stealth_timer + 1);
				text.font = Asset::Font::lowpoly;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.size = text_size;
				Vec2 pos = viewport.size * Vec2(0.5f, 0.65f);
				UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, pos);
			}
		}

		// detect danger
		{
			r32 detect_danger = get<PlayerCommon>()->detect_danger();
			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.4f);
			if (detect_danger == 1.0f)
			{
				if (flash_function(Game::real_time.total))
				{
					UIText text;
					text.font = Asset::Font::lowpoly;
					text.size = 18.0f;
					text.color = UI::alert_color;
					text.anchor_x = UIText::Anchor::Center;
					text.anchor_y = UIText::Anchor::Center;

					text.text(_(strings::detected));

					Rect2 box = text.rect(pos).outset(6 * UI::scale);
					UI::box(params, box, UI::background_color);
					text.draw(params, pos);
				}
			}
			else if (detect_danger > 0.0f)
			{
				// draw bar
				Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
				Rect2 bar = { pos + bar_size * -0.5f, bar_size };
				UI::box(params, bar, UI::background_color);
				UI::border(params, bar, 2, UI::alert_color);
				UI::box(params, { bar.pos, Vec2(bar.size.x * detect_danger, bar.size.y) }, UI::alert_color);

				// todo: sound
			}
		}

		// reticle
		if (get<Transform>()->parent.ref())
		{
			r32 cooldown = get<PlayerCommon>()->cooldown;
			r32 radius = cooldown == 0.0f ? 0.0f : vi_max(0.0f, 32.0f * (get<PlayerCommon>()->cooldown / AWK_MAX_DISTANCE_COOLDOWN));
			if (radius > 0 || tracer.type == TraceType::None || !Game::data.allow_detach)
			{
				// hollow reticle
				UI::triangle_border(params, { viewport.size * Vec2(0.5f, 0.5f), Vec2(4.0f + radius) * 2 * UI::scale }, 2, UI::accent_color, PI);
			}
			else
			{
				// solid reticle
				Vec2 a;
				if (UI::project(params, tracer.pos, &a))
				{
					if (tracer.type == TraceType::Target)
					{
						UI::triangle(params, { a, Vec2(12) * UI::scale }, UI::alert_color, PI);

						const r32 ratio = 0.8660254037844386f;
						UI::centered_box(params, { a + Vec2(12.0f * ratio, -6.0f) * UI::scale, Vec2(8.0f, 2.0f) * UI::scale }, UI::accent_color, PI * 0.5f * -0.33f);
						UI::centered_box(params, { a + Vec2(-12.0f * ratio, -6.0f) * UI::scale, Vec2(8.0f, 2.0f) * UI::scale }, UI::accent_color, PI * 0.5f * 0.33f);
						UI::centered_box(params, { a + Vec2(0, 12.0f) * UI::scale, Vec2(2.0f, 8.0f) * UI::scale }, UI::accent_color);
					}
					else
						UI::triangle(params, { a, Vec2(12) * UI::scale }, UI::accent_color, PI);
				}
			}
		}
	}
}


}