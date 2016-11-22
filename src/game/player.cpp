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
#include "cora.h"
#include "net.h"
#include "net_serialize.h"
#include "team.h"
#include "overworld.h"

namespace VI
{


#define fov_map_view (60.0f * PI * 0.5f / 180.0f)
#define fov_default (70.0f * PI * 0.5f / 180.0f)
#define fov_zoom (fov_default * 0.5f)
#define fov_sniper (fov_default * 0.25f)
#define zoom_speed_multiplier 0.25f
#define zoom_speed_multiplier_sniper 0.15f
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse (0.1f / 60.0f)
#define speed_joystick 5.0f
#define gamepad_rotation_acceleration (1.0f / 0.2f)
#define attach_speed 5.0f
#define rotation_speed 20.0f
#define msg_time 0.75f
#define text_size 16.0f
#define camera_shake_time 0.7f
#define score_summary_delay 2.0f
#define score_summary_accept_delay 3.0f

#define INTERACT_TIME 2.5f
#define INTERACT_LERP_ROTATION_SPEED 5.0f
#define INTERACT_LERP_TRANSLATION_SPEED 5.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u8 hp, s8 shield, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return scale * ((shield + (hp - 1)) * (box_size.x + HP_BOX_SPACING) - HP_BOX_SPACING);
}

void camera_setup_awk(Entity* e, Camera* camera, r32 offset)
{
	Quat awk_rot = e->get<Awk>()->lerped_rotation;
	Vec3 center = e->get<Awk>()->center_lerped();
	camera->pos = center + camera->rot * Vec3(0, 0, -offset);
	Vec3 abs_wall_normal;
	if (e->get<Transform>()->parent.ref())
	{
		abs_wall_normal = awk_rot * Vec3(0, 0, 1);
		camera->pos += abs_wall_normal * 0.5f;
		camera->pos.y += 0.5f - vi_min((r32)fabs(abs_wall_normal.y), 0.5f);
	}
	else
		abs_wall_normal = camera->rot * Vec3(0, 0, 1);

	Quat rot_inverse = camera->rot.inverse();

	camera->range = e->get<Awk>()->range();
	camera->range_center = rot_inverse * (center - camera->pos);
	Vec3 wall_normal_viewspace = rot_inverse * abs_wall_normal;
	camera->clip_planes[0].redefine(wall_normal_viewspace, camera->range_center + wall_normal_viewspace * -AWK_RADIUS);
	camera->cull_behind_wall = abs_wall_normal.dot(camera->pos - center) < -AWK_RADIUS + 0.02f; // camera is behind wall; set clip plane to wall
	camera->cull_range = camera->range_center.length();
	camera->colors = false;
	camera->fog = false;
}

s32 PlayerHuman::count_local()
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->local)
			count++;
	}
	return count;
}

PlayerHuman* PlayerHuman::player_for_camera(const Camera* camera)
{
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->camera == camera)
			return i.item();
	}
	return nullptr;
}

s32 PlayerHuman::count_local_before(PlayerHuman* h)
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item() == h)
			break;
		if (i.item()->local)
			count++;
	}
	return count;
}

PlayerHuman::PlayerHuman(b8 local, s8 g)
	: gamepad(g),
	camera(),
	msg_text(),
	msg_timer(msg_time),
	menu(),
	angle_horizontal(),
	angle_vertical(),
	menu_state(),
	upgrade_menu_open(),
	upgrade_animation_time(),
	score_summary_scroll(),
	spectate_index(),
	local(local)
{
	if (local)
		uuid = Game::session.local_player_uuids[gamepad];
}

PlayerHuman::UIMode PlayerHuman::ui_mode() const
{
	if (menu_state != Menu::State::Hidden)
		return UIMode::Pause;
	else if (Team::game_over)
		return UIMode::GameOver;
	else if (get<PlayerManager>()->instance.ref())
	{
		if (upgrade_menu_open)
			return UIMode::Upgrading;
		else
		{
			if (get<PlayerManager>()->instance.ref()->has<Awk>())
				return UIMode::PvpDefault;
			else
				return UIMode::ParkourDefault;
		}
	}
	else
		return UIMode::Dead;
}

void PlayerHuman::msg(const char* msg, b8 good)
{
	msg_text.text(msg);
	msg_text.color = good ? UI::color_accent : UI::color_alert;
	msg_timer = 0.0f;
	msg_good = good;
}

void PlayerHuman::awake()
{
	Audio::listener_enable(gamepad);

	get<PlayerManager>()->spawn.link<PlayerHuman, &PlayerHuman::spawn>(this);

	msg_text.size = text_size;
	msg_text.anchor_x = UIText::Anchor::Center;
	msg_text.anchor_y = UIText::Anchor::Center;

	camera = Camera::add();
	camera->team = (s8)get<PlayerManager>()->team.ref()->team();
	camera->mask = 1 << camera->team;
	camera->colors = false;

	Quat rot;
	Game::level.map_view.ref()->absolute(&camera->pos, &rot);
	camera->rot = Quat::look(rot * Vec3(0, -1, 0));
}

#define DANGER_RAMP_UP_TIME 2.0f
#define DANGER_LINGER_TIME 3.0f
#define DANGER_RAMP_DOWN_TIME 4.0f
r32 PlayerHuman::danger;
void PlayerHuman::update_all(const Update& u)
{
	for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		i.item()->update(u);

	// update audio danger parameter

	b8 visible_enemy = false;
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		PlayerManager* local_common = i.item()->get<PlayerCommon>()->manager.ref();
		for (auto j = PlayerCommon::list.iterator(); !j.is_last(); j.next())
		{
			if (PlayerManager::visibility[PlayerManager::visibility_hash(local_common, j.item()->manager.ref())].ref())
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

void PlayerHuman::update_camera_rotation(const Update& u)
{
	{
		r32 s = speed_mouse * Settings::gamepads[gamepad].effective_sensitivity() * Game::session.effective_time_scale();
		angle_horizontal -= (r32)u.input->cursor_x * s;
		angle_vertical += (r32)u.input->cursor_y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
	}

	if (u.input->gamepads[gamepad].active)
	{
		r32 s = speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::time.delta;
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

namespace PlayerControlHumanNet
{

struct Message
{
	enum class Type
	{
		Dash,
		Go,
		Reflect,
		UpgradeStart,
		count,
	};

	Vec3 pos;
	Vec3 dir;
	Ability ability = Ability::None;
	Upgrade upgrade = Upgrade::None;
	Type type;
};

template<typename Stream> b8 serialize_msg(Stream* p, Message* msg)
{
	serialize_enum(p, Message::Type, msg->type);

	// position/dir
	if (msg->type != Message::Type::UpgradeStart)
	{
		serialize_position(p, &msg->pos, Net::Resolution::High);
		serialize_r32_range(p, msg->dir.x, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.y, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.z, -1.0f, 1.0f, 16);
	}

	// ability
	if (msg->type == Message::Type::Go)
	{
		b8 has_ability;
		if (Stream::IsWriting)
			has_ability = msg->ability != Ability::None;
		serialize_bool(p, has_ability);
		if (has_ability)
			serialize_enum(p, Ability, msg->ability);
		else if (Stream::IsReading)
			msg->ability = Ability::None;
	}
	else if (Stream::IsReading)
		msg->ability = Ability::None;

	// upgrade
	if (msg->type == Message::Type::UpgradeStart)
		serialize_enum(p, Upgrade, msg->upgrade);
	else if (Stream::IsReading)
		msg->upgrade = Upgrade::None;

	return true;
}

b8 send(PlayerControlHuman* c, Message* msg)
{
	using Stream = Net::StreamWrite;
	Net::StreamWrite* p = Net::msg_new(Net::MessageType::PlayerControlHuman);
	Ref<PlayerControlHuman> ref = c;
	serialize_ref(p, ref);
	if (!serialize_msg(p, msg))
		net_error();
	Net::msg_finalize(p);
	return true;
}

}

void PlayerHuman::update(const Update& u)
{
	if (!local)
		return;

	Entity* entity = get<PlayerManager>()->instance.ref();

	if (camera)
	{
		s32 player_count;
#if DEBUG_AI_CONTROL
		player_count = count_local() + PlayerAI::list.count();
#else
		player_count = count_local();
#endif
		Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
		Camera::ViewportBlueprint* blueprint = &viewports[count_local_before(this)];

		camera->viewport =
		{
			Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
			Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
		};

		if (!entity)
		{
			r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
			camera->perspective(fov_map_view, aspect, 1.0f, Game::level.skybox.far_plane);
		}
	}

	if (Console::visible || Overworld::active())
		return;

	// flash message when the buy period expires
	if (!Team::game_over
		&& Game::level.mode == Game::Mode::Pvp
		&& !Game::session.story_mode
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& Game::time.total > GAME_BUY_PERIOD
		&& Game::time.total - Game::time.delta <= GAME_BUY_PERIOD)
	{
		if (Game::level.type == Game::Type::Rush)
			msg(_(get<PlayerManager>()->team.ref()->team() == 0 ? strings::defend : strings::attack), true);
		else
			msg(_(strings::attack), true);
	}

	if (msg_timer < msg_time)
		msg_timer += Game::real_time.delta;

	// close/open pause menu if needed
	{
		if (Game::time.total > 0.5f
			&& camera->active
			&& u.last_input->get(Controls::Pause, gamepad)
			&& !u.input->get(Controls::Pause, gamepad)
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
	if (!get<PlayerManager>()->instance.ref())
	{
		camera->range = 0;
		camera->colors = Game::level.mode == Game::Mode::Parkour;
	}

	switch (ui_mode())
	{
		case UIMode::PvpDefault:
		{
			if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
			{
				if (get<PlayerManager>()->at_control_point()
					&& get<PlayerManager>()->can_transition_state()
					&& !get<PlayerManager>()->friendly_control_point(get<PlayerManager>()->at_control_point()))
				{
					// enemy control point; capture
					get<PlayerManager>()->capture_start();
				}
				else if (get<PlayerManager>()->at_upgrade_point())
				{
					upgrade_menu_open = true;
					menu.animate();
					upgrade_animation_time = Game::real_time.total;
					get<PlayerManager>()->instance.ref()->get<Awk>()->current_ability = Ability::None;
				}
			}
			break;
		}
		case UIMode::ParkourDefault:
		{
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
				b8 upgrade_in_progress = !get<PlayerManager>()->can_transition_state();

				s8 last_selected = menu.selected;

				menu.start(u, gamepad);

				if (menu.item(u, _(strings::close), nullptr))
					upgrade_menu_open = false;

				for (s32 i = 0; i < (s32)Upgrade::count; i++)
				{
					Upgrade upgrade = (Upgrade)i;
					b8 can_upgrade = !upgrade_in_progress
						&& get<PlayerManager>()->upgrade_available(upgrade)
						&& get<PlayerManager>()->credits >= get<PlayerManager>()->upgrade_cost(upgrade);
					const UpgradeInfo& info = UpgradeInfo::list[(s32)upgrade];
					if (menu.item(u, _(info.name), nullptr, !can_upgrade, info.icon))
					{
						PlayerControlHumanNet::Message msg;
						msg.type = PlayerControlHumanNet::Message::Type::UpgradeStart;
						msg.upgrade = upgrade;
						PlayerControlHumanNet::send(entity->get<PlayerControlHuman>(), &msg);
					}
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
			else if (get<PlayerManager>()->spawn_timer > 0.0f)
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
						camera_setup_awk(spectating, camera, 6.0f);
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
						get<PlayerManager>()->score_accepted = true;
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

void PlayerHuman::spawn()
{
	Vec3 pos;
	r32 angle;
	Entity* spawned;

	if (Game::level.mode == Game::Mode::Pvp)
	{
		// spawn AWK
		Quat rot;
		get<PlayerManager>()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
		Vec3 dir = rot * Vec3(0, 1, 0);
		angle = atan2f(dir.x, dir.z);

		pos += Quat::euler(0, angle + (gamepad * PI * 0.5f), 0) * Vec3(0, 0, CONTROL_POINT_RADIUS * 0.5f); // spawn it around the edges
		spawned = World::create<AwkEntity>(get<PlayerManager>()->team.ref()->team());
	}
	else
	{
		// spawn traceur
		Quat rot;
		get<PlayerManager>()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
		Vec3 dir = rot * Vec3(0, 1, 0);
		angle = atan2f(dir.x, dir.z);

		spawned = World::create<Traceur>(pos + Vec3(0, 2, 0), Quat::euler(0, angle, 0), get<PlayerManager>()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute_pos(pos);
	PlayerCommon* common = spawned->add<PlayerCommon>(get<PlayerManager>());
	common->angle_horizontal = angle;

	get<PlayerManager>()->instance = spawned;

	spawned->add<PlayerControlHuman>(this);

	Net::finalize(spawned);
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

void ability_draw(const RenderParams& params, const PlayerHuman* player, const Vec2& pos, Ability ability, AssetID icon, s8 gamepad, Controls binding)
{
	char string[255];

	s16 cost = AbilityInfo::list[(s32)ability].spawn_cost;
	sprintf(string, "%s", Settings::gamepads[gamepad].bindings[(s32)binding].string(Game::is_gamepad));
	const Vec4* color;
	PlayerManager* manager = player->get<PlayerManager>();
	if (!manager->ability_valid(ability) || !manager->instance.ref()->get<PlayerCommon>()->movement_enabled())
		color = params.sync->input.get(binding, gamepad) ? &UI::color_disabled : &UI::color_alert;
	else if (manager->instance.ref()->get<Awk>()->current_ability == ability)
		color = &UI::color_default;
	else
		color = &UI::color_accent;
	draw_icon_text(params, pos, icon, string, *color);
}

void battery_timer_draw(const RenderParams& params, const Vec2& pos)
{
	r32 remaining = vi_max(0.0f, Game::level.time_limit - Game::time.total);

	const Vec2 box(text_size * 5 * UI::scale, text_size * UI::scale);
	const r32 padding = 8.0f * UI::scale;

	const Vec2 p = pos + Vec2(box.x * -0.5f, 0);
		
	UI::box(params, Rect2(p, box).outset(padding), UI::color_background);

	Vec2 icon_pos = p + Vec2(0.75f, 0.5f) * text_size * UI::scale;

	AssetID icon;
	const Vec4* color;
	if (remaining > Game::level.time_limit * 0.8f)
	{
		icon = Asset::Mesh::icon_battery_3;
		color = &UI::color_default;
	}
	else if (remaining > Game::level.time_limit * 0.6f)
	{
		icon = Asset::Mesh::icon_battery_2;
		color = &UI::color_default;
	}
	else if (remaining > Game::level.time_limit * 0.4f)
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
		if (remaining > Game::level.time_limit * 0.4f)
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

	if (Game::level.mode == Game::Mode::Pvp)
		battery_timer_draw(params, p);
	p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;

	// "spawning..."
	if (!manager->instance.ref() && manager->respawns != 0)
	{
		text.text(_(strings::deploy_timer), s32(manager->spawn_timer + 1));
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	if (Game::level.type == Game::Type::Rush)
	{
		// show remaining drones label
		text.text(_(strings::drones_remaining));
		text.color = UI::color_accent;
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

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
		if (Game::level.type == Game::Type::Deathmatch)
			text.text("%d", s32(i.item()->kills));
		else
			text.text("%d", s32(i.item()->respawns));
		text.draw(params, p + Vec2(wrap, 0));

		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}
}

void PlayerHuman::draw_alpha(const RenderParams& params) const
{
	if (params.camera != camera
		|| Overworld::active()
		|| Game::level.continue_match_after_death
		|| !local)
		return;

	const r32 line_thickness = 2.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	UIMode mode = ui_mode();

	r32 radius = 64.0f * UI::scale;
	Vec2 center = vp.size * Vec2(0.1f, 0.1f) + Vec2(radius, radius * 0.5f + (text_size * UI::scale * 0.5f));

	// todo: revamp game mode system
	if (Game::level.mode == Game::Mode::Pvp
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& (mode == UIMode::PvpDefault || mode == UIMode::Upgrading))
	{
		// credits
		b8 draw = true;
		b8 flashing = get<PlayerManager>()->credits_flash_timer > 0.0f;
		if (flashing)
			draw = UI::flash_function(Game::real_time.total);

		char buffer[128];
		sprintf(buffer, "%d", get<PlayerManager>()->credits);
		Vec2 credits_pos = center + Vec2(0, radius * -0.5f);
		draw_icon_text(params, credits_pos, Asset::Mesh::icon_energy, buffer, draw ? (flashing ? UI::color_default : UI::color_accent) : UI::color_background);

		// control point increment amount
		{
			r32 icon_size = text_size * UI::scale;
			r32 padding = 8 * UI::scale;

			UIText text;
			text.color = UI::color_accent;
			text.text("+%d", get<PlayerManager>()->increment());
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
				1.0f - (PlayerManager::timer / ENERGY_INCREMENT_INTERVAL),
				text.color,
				PI
			);
			text.draw(params, pos + Vec2(total_width * -0.5f + icon_size + padding, 0));
		}
	}

	// draw abilities
	if (Game::level.has_feature(Game::FeatureLevel::Abilities))
	{
		ControlPoint* control_point = get<PlayerManager>()->at_control_point();
		if (mode == UIMode::PvpDefault
			&& get<PlayerManager>()->can_transition_state()
			&& ((control_point && !control_point->owned_by(get<PlayerManager>()->team.ref()->team()))
				|| get<PlayerManager>()->at_upgrade_point()))
		{
			UIText text;
			if (get<PlayerManager>()->at_upgrade_point())
			{
				// "upgrade!" prompt
				text.color = get<PlayerManager>()->upgrade_available() ? UI::color_accent : UI::color_disabled;
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

		if ((mode == UIMode::PvpDefault || mode == UIMode::Upgrading)
			&& get<PlayerManager>()->can_transition_state())
		{
			// draw abilities

			// ability 1
			{
				Ability ability = get<PlayerManager>()->abilities[0];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(-radius, 0), ability, info.icon, gamepad, Controls::Ability1);
				}
			}

			// ability 2
			{
				Ability ability = get<PlayerManager>()->abilities[1];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(0, radius * 0.5f), ability, info.icon, gamepad, Controls::Ability2);
				}
			}

			// ability 3
			{
				Ability ability = get<PlayerManager>()->abilities[2];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[(s32)ability];
					ability_draw(params, this, center + Vec2(radius, 0), ability, info.icon, gamepad, Controls::Ability3);
				}
			}
		}
	}

	if (mode == UIMode::PvpDefault)
	{
		if (params.sync->input.get(Controls::Scoreboard, gamepad))
			scoreboard_draw(params, get<PlayerManager>());
	}
	else if (mode == UIMode::Upgrading)
	{
		Vec2 upgrade_menu_pos = vp.size * Vec2(0.5f, 0.6f);
		menu.draw_alpha(params, upgrade_menu_pos, UIText::Anchor::Center, UIText::Anchor::Center);

		if (menu.selected > 0)
		{
			// show details of currently highlighted upgrade
			Upgrade upgrade = (Upgrade)(menu.selected - 1);
			if (get<PlayerManager>()->current_upgrade == Upgrade::None
				&& get<PlayerManager>()->upgrade_available(upgrade))
			{
				r32 padding = 8.0f * UI::scale;

				const UpgradeInfo& info = UpgradeInfo::list[(s32)upgrade];
				UIText text;
				text.color = UI::color_accent;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
				s16 cost = get<PlayerManager>()->upgrade_cost(upgrade);
				text.text(_(strings::upgrade_description), cost, _(info.description));
				UIMenu::text_clip(&text, upgrade_animation_time, 150.0f);

				Vec2 pos = upgrade_menu_pos + Vec2(MENU_ITEM_WIDTH * -0.5f + padding, menu.height() * -0.5f - padding * 7.0f);
				UI::box(params, text.rect(pos).outset(padding), UI::color_background);
				text.draw(params, pos);
			}
		}
	}
	else if (mode == UIMode::Dead)
	{
		if (Game::level.mode == Game::Mode::Pvp)
		{
			// if we haven't spawned yet, then show the player list
			if (get<PlayerManager>()->spawn_timer > 0.0f)
				scoreboard_draw(params, get<PlayerManager>());
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
					text.color = Team::ui_color(get<PlayerManager>()->team.ref()->team(), spectating->get<AIAgent>()->team);
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
	}

	if (mode == UIMode::GameOver && Game::level.mode == Game::Mode::Pvp)
	{
		// show victory/defeat/draw message
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = 32.0f;

		Team* winner = Team::winner.ref();
		if (winner == get<PlayerManager>()->team.ref()) // we won
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
				text.color = player.item() == get<PlayerManager>() ? UI::color_accent : Team::ui_color(get<PlayerManager>()->team.ref()->team(), player.item()->team.ref()->team());

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
				text.text(_(get<PlayerManager>()->score_accepted ? strings::waiting : strings::prompt_accept));
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);
			}
		}
	}
	else if (Game::level.mode == Game::Mode::Pvp)
	{
		// game is not yet over

		{
			// upgrade / spawn / capture timer
			PlayerManager::State manager_state = get<PlayerManager>()->state();
			if (manager_state != PlayerManager::State::Default)
			{
				r32 total_time;
				AssetID string;
				s16 cost;

				switch (manager_state)
				{
					case PlayerManager::State::Upgrading:
					{
						// getting an upgrade
						string = strings::upgrading;

						const UpgradeInfo& info = UpgradeInfo::list[(s32)get<PlayerManager>()->current_upgrade];
						cost = info.cost;
						total_time = UPGRADE_TIME;
						break;
					}
					case PlayerManager::State::Capturing:
					{
						// capturing a control point
						ControlPoint* control_point = get<PlayerManager>()->at_control_point();
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
				UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (get<PlayerManager>()->state_timer / total_time)), bar.size.y) }, UI::color_accent);

				UIText text;
				text.size = 18.0f;
				text.color = UI::color_background;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.text(_(string), (s32)cost);
				text.draw(params, bar.pos + bar.size * 0.5f);
			}
		}

		if (mode == UIMode::PvpDefault || mode == UIMode::Upgrading) // show game timer
			battery_timer_draw(params, vp.size * Vec2(0.9f, 0.1f));

		// network error icon
#if !SERVER
		if (!Game::level.local && Net::Client::lagging())
			UI::mesh(params, Asset::Mesh::icon_network_error, vp.size * Vec2(0.9f, 0.5f), Vec2(text_size * 2.0f * UI::scale), UI::color_alert);
#endif
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
			Rect2 box = msg_text.rect(pos).outset(MENU_ITEM_PADDING);
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
	if (has<Awk>())
	{
		get<Health>()->hp_max = AWK_HEALTH;
		link<&PlayerCommon::awk_done_flying>(get<Awk>()->done_flying);
		link<&PlayerCommon::awk_detached>(get<Awk>()->detached);
		link_arg<const Vec3&, &PlayerCommon::awk_reflecting>(get<Awk>()->reflecting);
	}
}

b8 PlayerCommon::movement_enabled() const
{
	if (has<Awk>())
	{
		return get<Awk>()->state() == Awk::State::Crawl // must be attached to wall
			&& manager.ref()->state() == PlayerManager::State::Default // can't move while upgrading and stuff
			&& (Game::time.total > GAME_BUY_PERIOD || !Game::level.has_feature(Game::FeatureLevel::Abilities) || Game::session.story_mode); // or during the buy period
	}
	else
		return true;
}

Entity* PlayerCommon::incoming_attacker() const
{
	Vec3 me = get<Transform>()->absolute_pos();

	// check incoming Awks
	PlayerManager* manager = get<PlayerCommon>()->manager.ref();
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		if (PlayerManager::visibility[PlayerManager::visibility_hash(manager, i.item()->manager.ref())].ref())
		{
			// determine if they're attacking us
			if (i.item()->get<Awk>()->state() != Awk::State::Crawl
				&& Vec3::normalize(i.item()->get<Awk>()->velocity).dot(Vec3::normalize(me - i.item()->get<Transform>()->absolute_pos())) > 0.98f)
			{
				return i.item()->entity();
			}
		}
	}

	// check incoming projectiles
	for (auto i = Projectile::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 velocity = Vec3::normalize(i.item()->velocity);
		Vec3 projectile_pos = i.item()->get<Transform>()->absolute_pos();
		Vec3 to_me = me - projectile_pos;
		r32 dot = velocity.dot(to_me);
		if (dot > 0.0f && dot < AWK_MAX_DISTANCE && velocity.dot(Vec3::normalize(to_me)) > 0.98f)
		{
			// only worry about it if it can actually see us
			btCollisionWorld::ClosestRayResultCallback ray_callback(me, projectile_pos);
			Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~CollisionShield);
			if (!ray_callback.hasHit())
				return i.item()->entity();
		}
	}

	// check incoming rockets
	if (!get<AIAgent>()->stealth)
	{
		Rocket* rocket = Rocket::inbound(entity());
		if (rocket)
		{
			// only worry about it if the rocket can actually see us
			btCollisionWorld::ClosestRayResultCallback ray_callback(me, rocket->get<Transform>()->absolute_pos());
			Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~CollisionShield);
			if (!ray_callback.hasHit())
				return rocket->entity();
		}
	}

	return nullptr;
}

void PlayerCommon::update(const Update& u)
{
	Quat rot = get<Transform>()->absolute_rot();
	r32 angle = Quat::angle(attach_quat, rot);
	if (angle > 0)
		attach_quat = Quat::slerp(vi_min(1.0f, rotation_speed * u.time.delta), attach_quat, rot);
}

r32 PlayerCommon::detect_danger() const
{
	AI::Team my_team = get<AIAgent>()->team;
	for (auto i = Team::list.iterator(); !i.is_last(); i.next())
	{
		Team* team = i.item();
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

void PlayerCommon::awk_reflecting(const Vec3& new_velocity)
{
	Vec3 direction = Vec3::normalize(new_velocity);
	attach_quat = Quat::look(direction);
}

Vec3 PlayerCommon::look_dir() const
{
	if (has<PlayerControlHuman>()) // HACK for third-person camera
		return Vec3::normalize(get<PlayerControlHuman>()->reticle.pos - get<Transform>()->absolute_pos());
	else
		return Quat::euler(0.0f, angle_horizontal, angle_vertical) * Vec3(0, 0, 1);
}

Quat PlayerCommon::look() const
{
	return Quat::euler(0, angle_horizontal, angle_vertical);
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

b8 PlayerControlHuman::net_msg(Net::StreamRead* p, PlayerControlHuman* c, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	PlayerControlHumanNet::Message msg;
	if (!serialize_msg(p, &msg))
		net_error();

	if (src != Net::MessageSource::Loopback // it's from remote
		&& !Game::level.local // we are a client
		&& msg.type != PlayerControlHumanNet::Message::Type::Reflect) // reflect messages can go both ways; all others go only from client to server
		net_error();

	if (!c)
	{
		// ignore, we're probably just receiving this message from the client after the player has already been destroyed
		return true;
	}

	if (src == Net::MessageSource::Invalid // message is from a client who doesn't actually own this player
		|| (msg.type != PlayerControlHumanNet::Message::Type::UpgradeStart && msg.dir.length_squared() == 0.0f)
		|| (msg.ability != Ability::None && !c->player.ref()->get<PlayerManager>()->has_upgrade(Upgrade(msg.ability)))
		|| (msg.ability != Ability::None && msg.type != PlayerControlHumanNet::Message::Type::Go))
	{
		// malformed message, ignore
		net_error();
	}

	if (src == Net::MessageSource::Remote)
	{
		// make sure we are where the remote thinks we are when we start processing this message
		r32 dist_sq = (c->get<Transform>()->absolute_pos() - msg.pos).length_squared();
		if (dist_sq < NET_SYNC_TOLERANCE_POS * NET_SYNC_TOLERANCE_POS)
			c->get<Transform>()->absolute_pos(msg.pos);
	}

	switch (msg.type)
	{
		case PlayerControlHumanNet::Message::Type::Dash:
		{
			c->get<Awk>()->current_ability = Ability::None;
			if (c->get<Awk>()->dash_start(msg.dir))
			{
				c->get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
				c->try_primary = false;
				c->try_secondary = false;
			}
			break;
		}
		case PlayerControlHumanNet::Message::Type::Go:
		{
			c->get<Awk>()->current_ability = msg.ability;
			if (c->get<Awk>()->go(msg.dir))
			{
				c->try_primary = false;
				if (msg.ability == Ability::None)
				{
					c->get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					c->try_secondary = false;
				}
				else
					c->rumble = vi_max(c->rumble, 0.5f);
			}

			break;
		}
		case PlayerControlHumanNet::Message::Type::UpgradeStart:
		{
			if (Game::level.local)
				c->get<PlayerCommon>()->manager.ref()->upgrade_start(msg.upgrade);
			break;
		}
		case PlayerControlHumanNet::Message::Type::Reflect:
		{
			if (src == Net::MessageSource::Remote)
				c->get<Awk>()->handle_remote_reflection(msg.pos, msg.dir);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	return true;
}

s32 PlayerControlHuman::count_local()
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->local())
			count++;
	}
	return count;
}

void PlayerControlHuman::awk_done_flying_or_dashing()
{
	rumble = vi_max(rumble, 0.2f);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

void PlayerControlHuman::health_changed(const HealthEvent& e)
{
	if (e.hp + e.shield < 0)
		camera_shake();
}

void PlayerControlHuman::awk_reflecting(const Vec3& new_velocity)
{
	// send message if we are a server or client in a network game. don't if it's an all-local game
#if !SERVER
	if (!Game::level.local)
#endif
	{
		PlayerControlHumanNet::Message msg;
		msg.pos = get<Transform>()->absolute_pos();
		msg.dir = Vec3::normalize(new_velocity);
		msg.type = PlayerControlHumanNet::Message::Type::Reflect;
		PlayerControlHumanNet::send(this, &msg);
	}
}

PlayerControlHuman::PlayerControlHuman(PlayerHuman* p)
	: fov(fov_default),
	try_primary(),
	try_secondary(),
	try_slide(),
	camera_shake_timer(),
	rumble(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed(),
	remote_control(),
	player(p),
	position_history(),
	interactable()
{
}

PlayerControlHuman::~PlayerControlHuman()
{
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

void PlayerControlHuman::awake()
{
	if (player.ref()->local && !Game::level.local)
	{
		Transform* t = get<Transform>();
		remote_control.pos = t->pos;
		remote_control.rot = t->rot;
		remote_control.parent = t->parent;
	}

	link_arg<const HealthEvent&, &PlayerControlHuman::health_changed>(get<Health>()->changed);

	if (has<Awk>())
	{
		last_pos = get<Awk>()->center_lerped();
		link<&PlayerControlHuman::awk_detached>(get<Awk>()->detached);
		link<&PlayerControlHuman::awk_done_flying_or_dashing>(get<Awk>()->done_flying);
		link<&PlayerControlHuman::awk_done_flying_or_dashing>(get<Awk>()->done_dashing);
		link_arg<const Vec3&, &PlayerControlHuman::awk_reflecting>(get<Awk>()->reflecting);
		link_arg<Entity*, &PlayerControlHuman::hit_target>(get<Awk>()->hit);
	}
	else
	{
		link_arg<r32, &PlayerControlHuman::parkour_landed>(get<Walker>()->land);
		link<&PlayerControlHuman::interact_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_terminal_enter, 1.2f));
		get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
		get<Audio>()->param(AK::GAME_PARAMETERS::FLY_VOLUME, 0.0f);
	}
}

void PlayerControlHuman::parkour_landed(r32 velocity_diff)
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

void PlayerControlHuman::interact_animation_callback()
{
	if (interactable.ref())
	{
		interactable.ref()->interact();
		interactable = nullptr;
	}
}

void PlayerControlHuman::hit_target(Entity* target)
{
	rumble = vi_max(rumble, 0.5f);
	if (target->has<MinionCommon>())
	{
		b8 is_enemy = target->get<AIAgent>()->team != get<AIAgent>()->team;
		player.ref()->msg(_(strings::minion_killed), is_enemy);
	}
	else if (target->has<Sensor>() && !target->has<EnergyPickup>())
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

void PlayerControlHuman::awk_detached()
{
	camera_shake_timer = 0.0f; // stop screen shake
}

void PlayerControlHuman::camera_shake(r32 amount) // amount ranges from 0 to 1
{
	if (!has<Awk>() || get<Awk>()->state() == Awk::State::Crawl) // don't shake the screen if we reflect off something in the air
	{
		camera_shake_timer = vi_max(camera_shake_timer, camera_shake_time * amount);
		rumble = vi_max(rumble, amount);
	}
}

b8 PlayerControlHuman::input_enabled() const
{
	PlayerHuman::UIMode ui_mode = player.ref()->ui_mode();
	return !Console::visible
		&& !Overworld::active()
		&& (ui_mode == PlayerHuman::UIMode::PvpDefault || ui_mode == PlayerHuman::UIMode::ParkourDefault)
		&& !Cora::has_focus()
		&& !Team::game_over
		&& !interactable.ref();
}

b8 PlayerControlHuman::movement_enabled() const
{
	return input_enabled() && get<PlayerCommon>()->movement_enabled();
}

r32 PlayerControlHuman::look_speed() const
{
	if (has<Awk>() && try_secondary)
		return get<Awk>()->current_ability == Ability::Sniper ? zoom_speed_multiplier_sniper : zoom_speed_multiplier;
	else
		return 1.0f;
}

void PlayerControlHuman::update_camera_input(const Update& u, r32 gamepad_rotation_multiplier)
{
	if (input_enabled())
	{
		s32 gamepad = player.ref()->gamepad;
		if (gamepad == 0)
		{
			r32 s = look_speed() * speed_mouse * Settings::gamepads[gamepad].effective_sensitivity();
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
			adjustment *= look_speed() * speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::time.delta * gamepad_rotation_multiplier;
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

Vec3 PlayerControlHuman::get_movement(const Update& u, const Quat& rot)
{
	Vec3 movement = Vec3::zero;
	if (movement_enabled())
	{
		s32 gamepad = player.ref()->gamepad;
		if (u.input->gamepads[gamepad].active)
		{
			Vec2 gamepad_movement(-u.input->gamepads[gamepad].left_x, -u.input->gamepads[gamepad].left_y);
			Input::dead_zone(&gamepad_movement.x, &gamepad_movement.y);
			movement.x = gamepad_movement.x;
			movement.z = gamepad_movement.y;
		}

		if (gamepad == 0)
		{
			if (u.input->get(Controls::Forward, gamepad))
				movement += Vec3(0, 0, 1);
			if (u.input->get(Controls::Backward, gamepad))
				movement += Vec3(0, 0, -1);
			if (u.input->get(Controls::Right, gamepad))
				movement += Vec3(-1, 0, 0);
			if (u.input->get(Controls::Left, gamepad))
				movement += Vec3(1, 0, 0);
		}

		r32 length_squared = movement.length_squared();
		if (length_squared > 0.0f)
			movement /= sqrtf(length_squared);

		movement = rot * movement;
	}
	return movement;
}

// returns false if there is no more room in the target indicator array
b8 PlayerControlHuman::add_target_indicator(Target* target, TargetIndicator::Type type)
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
		r32 speed;
		switch (get<Awk>()->current_ability)
		{
			case Ability::Sniper:
			{
				speed = 0.0f;
				break;
			}
			case Ability::Bolter:
			{
				speed = PROJECTILE_SPEED;
				break;
			}
			default:
			{
				speed = AWK_FLY_SPEED;
				break;
			}
		}
		Vec3 intersection;
		if (get<Awk>()->predict_intersection(target, nullptr, &intersection, speed))
		{
			target_indicators.add({ intersection, target->velocity(), type });
			if (target_indicators.length == target_indicators.capacity())
				return false;
		}
	}
	return true;
}

// returns the actual detected entity, if any. could be the original player, or a decoy.
Entity* determine_visibility(PlayerCommon* me, PlayerCommon* other_player, b8* visible, b8* tracking)
{
	// make sure we can see this guy
	AI::Team team = me->get<AIAgent>()->team;
	const Team::SensorTrack track = Team::list[(s32)team].player_tracks[other_player->manager.id];
	*tracking = track.tracking;

	if (other_player->get<AIAgent>()->team == team)
	{
		*visible = true;
		return other_player->entity();
	}
	else
	{
		Entity* visible_entity = PlayerManager::visibility[PlayerManager::visibility_hash(me->manager.ref(), other_player->manager.ref())].ref();
		*visible = visible_entity != nullptr;

		if (track.tracking)
			return track.entity.ref();
		else
			return visible_entity;
	}
}

void ability_select(Awk* awk, Ability a)
{
	awk->current_ability = a;
}

void ability_cancel(Awk* awk)
{
	awk->current_ability = Ability::None;
}

void ability_update(const Update& u, PlayerControlHuman* control, Controls binding, s8 gamepad, s32 index)
{
	PlayerHuman* player = control->player.ref();
	PlayerManager* manager = player->get<PlayerManager>();
	Ability ability = manager->abilities[index];

	if (ability == Ability::None || !control->movement_enabled())
		return;

	Awk* awk = control->get<Awk>();

	if (u.input->get(binding, gamepad) && !u.last_input->get(binding, gamepad))
	{
		if (awk->current_ability == ability)
		{
			// cancel current spawn ability
			ability_cancel(awk);
		}
		else if (manager->ability_valid(ability)) // select new spawn ability
		{
			if (awk->current_ability != Ability::None)
				ability_cancel(awk);
			ability_select(awk, ability);
		}
	}
}

b8 PlayerControlHuman::local() const
{
	return player.ref()->local;
}

void PlayerControlHuman::remote_control_handle(const PlayerControlHuman::RemoteControl& control)
{
#if !SERVER
	vi_assert(false); // this should only get called on the server
#endif

	remote_control = control;

	// if the remote position is close to what we think it is, snap to it
	Transform* t = get<Transform>();
	Vec3 abs_pos;
	Quat abs_rot;
	t->absolute(&abs_pos, &abs_rot);

	Vec3 remote_abs_pos = remote_control.pos;
	Quat remote_abs_rot = remote_control.rot;
	if (remote_control.parent.ref())
		remote_control.parent.ref()->to_world(&remote_abs_pos, &remote_abs_rot);
	if ((remote_abs_pos - abs_pos).length_squared() < NET_SYNC_TOLERANCE_POS * NET_SYNC_TOLERANCE_POS)
		t->absolute_pos(remote_abs_pos);
	if (Quat::angle(remote_abs_rot, abs_rot) < NET_SYNC_TOLERANCE_ROT)
		t->absolute_rot(remote_abs_rot);
}

void PlayerControlHuman::camera_shake_update(const Update& u, Camera* camera)
{
	if (camera_shake_timer > 0.0f)
	{
		camera_shake_timer -= u.time.delta;
		if (!has<Awk>() || get<Awk>()->state() == Awk::State::Crawl)
		{
			r32 shake = (camera_shake_timer / camera_shake_time) * 0.3f;
			r32 offset = Game::time.total * 10.0f;
			camera->rot = camera->rot * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}
	}
}

void PlayerControlHuman::update(const Update& u)
{
	s32 gamepad = player.ref()->gamepad;

	if (has<Awk>())
	{
		if (local())
		{
			if (!Game::level.local)
			{
				// we are a client and this is a local player
				if (position_history.length == 0 || Game::real_time.total > position_history[position_history.length - 1].timestamp + NET_TICK_RATE)
				{
					// save our position history
					if (position_history.length == position_history.capacity())
						position_history.remove_ordered(0);
					Transform* t = get<Transform>();
					Vec3 abs_pos;
					Quat abs_rot;
					t->absolute(&abs_pos, &abs_rot);
					position_history.add(
					{
						abs_rot,
						abs_pos,
						Game::real_time.total,
					});
				}

				// make sure we never get too far from where the server says we should be
				r32 rtt = Net::rtt(player.ref());
				vi_assert(rtt > 0.0f);

				// get the position entry at this time in the history
				r32 timestamp = Game::real_time.total - rtt;
				const PositionEntry* position = nullptr;
				r32 tolerance_pos = 0.0f;
				r32 tolerance_rot = 0.0f;
				for (s32 i = position_history.length - 1; i >= 0; i--)
				{
					const PositionEntry& entry = position_history[i];
					if (entry.timestamp < timestamp)
					{
						position = &entry;
						// calculate tolerance based on velocity
						const s32 radius = 3;
						for (s32 j = vi_max(0, i - radius); j < vi_min(s32(position_history.length - 1), i + radius); j++)
						{
							tolerance_pos = vi_max(tolerance_pos, (position_history[j].pos - position_history[j + 1].pos).length());
							tolerance_rot = vi_max(tolerance_rot, Quat::angle(position_history[j].rot, position_history[j + 1].rot));
						}
						tolerance_pos *= 6.0f;
						tolerance_rot *= 6.0f;
						break;
					}
				}
				tolerance_pos += NET_SYNC_TOLERANCE_POS;
				tolerance_rot += NET_SYNC_TOLERANCE_ROT;

				// make sure we're not too far from it
				if (position)
				{
					Vec3 remote_abs_pos = remote_control.pos;
					Quat remote_abs_rot = remote_control.rot;
					if (remote_control.parent.ref())
						remote_control.parent.ref()->to_world(&remote_abs_pos, &remote_abs_rot);

					if ((position->pos - remote_abs_pos).length_squared() > tolerance_pos * tolerance_pos
						|| Quat::angle(position->rot, remote_abs_rot) > tolerance_rot)
					{
						// snap our position to the server's position
						position_history.length = 0;
						Transform* t = get<Transform>();
						t->pos = remote_control.pos;
						t->rot = remote_control.rot;
						t->parent = remote_control.parent;
					}
				}
			}

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
						try_secondary = true;
						get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
					}
				}
				else if (!zoom_pressed)
				{
					if (try_secondary)
						get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
					try_secondary = false;
				}

				r32 fov_target = try_secondary ? (get<Awk>()->current_ability == Ability::Sniper ? fov_sniper : fov_zoom) : fov_default;

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
				camera->rot = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

				// crawling
				{
					Vec3 movement = get_movement(u, get<PlayerCommon>()->look());
					get<Awk>()->crawl(movement, u);
				}

				last_pos = get<Awk>()->center_lerped();
			}
			else
				camera->rot = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

			{
				// abilities
				ability_update(u, this, Controls::Ability1, gamepad, 0);
				ability_update(u, this, Controls::Ability2, gamepad, 1);
				ability_update(u, this, Controls::Ability3, gamepad, 2);
			}

			camera_shake_update(u, camera);

			camera_setup_awk(entity(), camera, AWK_THIRD_PERSON_OFFSET);

			// reticle
			{
				Vec3 trace_dir = camera->rot * Vec3(0, 0, 1);
				Vec3 me = get<Transform>()->absolute_pos();
				Vec3 trace_start = camera->pos + trace_dir * trace_dir.dot(me - camera->pos);

				reticle.type = ReticleType::None;

				if (movement_enabled())
				{
					Vec3 trace_end = trace_start + trace_dir * (AWK_SNIPE_DISTANCE + AWK_THIRD_PERSON_OFFSET);
					RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
					Physics::raycast(&ray_callback, ~CollisionAwkIgnore & ~get<Awk>()->ally_containment_field_mask());

					Ability ability = get<Awk>()->current_ability;

					if (ray_callback.hasHit())
					{
						reticle.pos = ray_callback.m_hitPointWorld;
						reticle.normal = ray_callback.m_hitNormalWorld;
						Entity* hit_entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
						Vec3 detach_dir = reticle.pos - me;
						r32 distance = detach_dir.length();
						detach_dir /= distance;
						r32 dot_tolerance = distance < AWK_DASH_DISTANCE ? 0.3f : 0.1f;
						if (ability == Ability::None) // normal movement
						{
							if (get<Awk>()->direction_is_toward_attached_wall(detach_dir))
							{
								Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
								if (hit_entity->has<Target>()
									|| (detach_dir.dot(wall_normal) > -dot_tolerance && reticle.normal.dot(wall_normal) > 1.0f - dot_tolerance))
								{
									reticle.type = ReticleType::Dash;
								}
							}
							else
							{
								Vec3 hit;
								b8 hit_target;
								if (get<Awk>()->can_shoot(detach_dir, &hit, &hit_target))
								{
									if (hit_target)
										reticle.type = ReticleType::Target;
									else if ((reticle.pos - hit).length_squared() < AWK_RADIUS * AWK_RADIUS)
										reticle.type = ReticleType::Normal;
								}
								else
								{
									// when you're aiming at a target that is attached to the same surface you are,
									// sometimes the point you're aiming at is actually away from the wall,
									// so it registers as a shot rather than a dash.
									// and sometimes that shot can't actually be taken.
									// so we need to check for this case and turn it into a dash if we can.

									if (distance < AWK_DASH_DISTANCE && hit_entity->has<Target>() && hit_entity->get<Transform>()->parent.ref())
									{
										Quat my_rot = get<Transform>()->absolute_rot();
										Vec3 target_pos;
										Quat target_rot;
										hit_entity->get<Transform>()->absolute(&target_pos, &target_rot);
										Vec3 my_normal = my_rot * Vec3(0, 0, 1);
										if (my_normal.dot(target_rot * Vec3(0, 0, 1)) > 1.0f - dot_tolerance
											&& fabs(my_normal.dot(target_pos - me)) < dot_tolerance)
										{
											reticle.type = ReticleType::Dash;
										}
									}
								}
							}
						}
						else // spawning an ability
						{
							Vec3 hit;
							b8 hit_target;
							if (get<Awk>()->can_spawn(ability, detach_dir, &hit, nullptr, nullptr, &hit_target))
							{
								if (AbilityInfo::list[s32(ability)].type == AbilityInfo::Type::Shoot)
								{
									reticle.type = ReticleType::Normal;
									if (hit_target)
										reticle.type = ReticleType::Target;
								}
								else if ((hit - Vec3(ray_callback.m_hitPointWorld)).length_squared() < AWK_RADIUS * AWK_RADIUS)
									reticle.type = ReticleType::Normal;
							}
						}
					}
					else
					{
						reticle.pos = trace_end;
						reticle.normal = -trace_dir;
						if (ability != Ability::None
							&& get<Awk>()->can_spawn(ability, trace_dir)) // spawning an ability
						{
							reticle.type = ReticleType::Normal;
						}
					}
				}
				else
				{
					reticle.pos = trace_start + trace_dir * AWK_THIRD_PERSON_OFFSET;
					reticle.normal = -trace_dir;
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
						b8 tracking;
						b8 visible;
						Entity* detected_entity = determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

						if (visible)
						{
							if (!add_target_indicator(detected_entity->get<Target>(), tracking ? TargetIndicator::Type::AwkTracking : TargetIndicator::Type::AwkVisible))
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
						TargetIndicator::Type type =
							i.item()->get<MinionAI>()->goal.entity.ref() == entity() && i.item()->get<MinionAI>()->can_see(entity())
							? TargetIndicator::Type::MinionAttacking
							: TargetIndicator::Type::Minion;
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
					PlayerControlHumanNet::Message msg;
					msg.dir = Vec3::normalize(reticle.pos - get<Transform>()->absolute_pos());
					msg.pos = get<Transform>()->absolute_pos();
					if (reticle.type == ReticleType::Dash)
					{
						msg.type = PlayerControlHumanNet::Message::Type::Dash;
						PlayerControlHumanNet::send(this, &msg);
					}
					else
					{
						msg.type = PlayerControlHumanNet::Message::Type::Go;
						msg.ability = get<Awk>()->current_ability;
						PlayerControlHumanNet::send(this, &msg);
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
		else if (Game::level.local)
		{
			// we are a server, but this Awk is being controlled by a client
			get<Awk>()->crawl(remote_control.movement, u);
			last_pos = get<Awk>()->center_lerped();
		}
		else
		{
			// we are a client and this Awk is not local
			// do nothing
		}
	}
	else
	{
		// parkour mode

		if (local())
		{
			if (input_enabled() && !u.input->get(Controls::InteractSecondary, gamepad) && u.last_input->get(Controls::InteractSecondary, gamepad))
			{
				interactable = Interactable::closest(get<Transform>()->absolute_pos());
				if (interactable.ref())
					get<Animator>()->layers[1].play(Asset::Animation::character_terminal_enter); // this animation will eventually trigger the interactable
			}

			if (interactable.ref())
			{
				// position player directly in front of the interactable

				// desired rotation / position
				r32 target_angle;
				Vec3 target_pos;
				{
					Vec3 i_pos;
					Quat i_rot;
					interactable.ref()->get<Transform>()->absolute(&i_pos, &i_rot);
					Vec3 dir = i_rot * Vec3(0, 0, -1);
					dir.y = 0.0f;
					dir.normalize();
					target_angle = atan2f(dir.x, dir.z);
					target_pos = i_pos + dir * Vec3(0, 0, -0.5f);
					target_pos.y += (get<Walker>()->capsule_height() * 0.5f) + get<Walker>()->support_height;
				}

				r32 angle = fabs(LMath::angle_to(get<PlayerCommon>()->angle_horizontal, target_angle));
				get<PlayerCommon>()->angle_horizontal = LMath::lerpf(vi_min(1.0f, (INTERACT_LERP_ROTATION_SPEED / angle) * u.time.delta), get<PlayerCommon>()->angle_horizontal, LMath::closest_angle(target_angle, get<PlayerCommon>()->angle_horizontal));

				Vec3 abs_pos = get<Transform>()->absolute_pos();
				r32 distance = (abs_pos - target_pos).length();
				if (distance > 0.0f)
					get<Walker>()->absolute_pos(Vec3::lerp(vi_min(1.0f, (INTERACT_LERP_TRANSLATION_SPEED / distance) * u.time.delta), abs_pos, target_pos));
			}

			update_camera_input(u);

			if (input_enabled() && u.last_input->get(Controls::Scoreboard, gamepad) && !u.input->get(Controls::Scoreboard, gamepad))
			{
				if (Game::save.zones[Game::level.id] == Game::ZoneState::Friendly)
					Overworld::show(player.ref()->camera);
				else
					player.ref()->msg(_(strings::error_hostile_zone), false);
			}
		
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

			if (jump_pressed)
				get<Parkour>()->lessen_gravity(); // jump higher when the player holds the jump button

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
			/*
			// todo: third-person
			if (Game::state.third_person)
				camera_pos = get<Transform>()->absolute_pos() + look_quat * Vec3(0, 1, -7);
			else
			*/
			{
				camera_pos = Vec3(0, 0, 0.1f);
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
			if (!Overworld::active())
			{
				Camera* camera = player.ref()->camera;
				r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
				camera->perspective(fov_default, aspect, 0.02f, Game::level.skybox.far_plane);
				camera->clip_planes[0] = Plane();
				camera->pos = camera_pos;
				camera->cull_range = 0.0f;
				camera->cull_behind_wall = false;
				camera->colors = true;
				camera->fog = true;
				camera->range = 0.0f;
				camera->rot = look_quat;
				camera_shake_update(u, camera);
			}
		}
		else
		{
			// remote control by a client
			if (Game::level.local)
			{
				// just trust the client, it's k
				get<Transform>()->pos = remote_control.pos;
				get<Transform>()->rot = remote_control.rot;
				get<Transform>()->parent = remote_control.parent;
				last_pos = get<Transform>()->absolute_pos();
			}
		}
	}
}

void draw_mesh_clipped(const RenderParams& p, AssetID mesh, const Mat4& m, const Plane& plane)
{
	RenderSync* sync = p.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::flat_clipped);
	sync->write(p.technique);

	Mat4 mvp = m * p.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * p.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(0, 0, 0, MATERIAL_NO_OVERRIDE));

	// write culling info
	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::plane);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Plane>(plane);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(mesh);
}

void PlayerControlHuman::draw(const RenderParams& p) const
{
	if (p.technique != RenderTechnique::Default
		|| p.camera != player.ref()->camera
		|| Overworld::active()
		|| p.camera->cull_range <= 0.0f
		|| !get<Transform>()->parent.ref())
		return;

	Loader::mesh_permanent(Asset::Mesh::cylinder_inside);
	Loader::shader_permanent(Asset::Shader::flat_clipped);

	if (p.camera->cull_behind_wall) // primary wall
	{
		Mat4 m;
		m.make_transform(p.camera->pos, Vec3(p.camera->cull_range, p.camera->cull_range, 10), p.camera->rot);
		draw_mesh_clipped(p, Asset::Mesh::cylinder_inside, m, p.camera->clip_planes[0]);
	}
}

void PlayerControlHuman::draw_alpha(const RenderParams& params) const
{
	if (params.technique != RenderTechnique::Default
		|| params.camera != player.ref()->camera
		|| Overworld::active()
		|| Team::game_over)
		return;

	const Rect2& viewport = params.camera->viewport;

	r32 range = has<Awk>() ? get<Awk>()->range() : AWK_MAX_DISTANCE;

	AI::Team team = get<AIAgent>()->team;

	// target indicators
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

	if (has<Awk>())
	{
		PlayerManager* manager = player.ref()->get<PlayerManager>();

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
	else
	{
		// highlight terminal location
		Entity* terminal = Game::level.terminal.ref();
		if (terminal)
			UI::indicator(params, terminal->get<Transform>()->absolute_pos(), UI::color_default, true);
	}

	// usernames directly over players' 3D positions
	for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
	{
		if (other_player.item() != get<PlayerCommon>())
		{
			b8 tracking;
			b8 visible;
			Entity* detected_entity = determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

			b8 friendly = other_player.item()->get<AIAgent>()->team == team;

			if (visible && !friendly)
			{
				enemy_visible = true;
				if ((detected_entity->get<Transform>()->absolute_pos() - me).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
					enemy_close = true;
			}

			b8 draw;
			Vec2 p;
			const Vec4* color;

			if (friendly)
			{
				Vec3 pos3d = detected_entity->get<Transform>()->absolute_pos() + Vec3(0, AWK_RADIUS * 2.0f, 0);
				draw = UI::project(params, pos3d, &p);
				color = &Team::ui_color_friend;
			}
			else
			{
				// highlight the username and draw it even if it's offscreen
				if (detected_entity)
				{
					Vec3 pos3d = detected_entity->get<Transform>()->absolute_pos() + Vec3(0, AWK_RADIUS * 2.0f, 0);

					if (friendly)
						color = &Team::ui_color_friend;
					else
						color = &Team::ui_color_enemy;

					// if we can see or track them, the indicator has already been added using add_target_indicator in the update function

					if (tracking)
					{
						// if we're tracking them, clamp their username to the screen
						draw = true;
						UI::is_onscreen(params, pos3d, &p);
					}
					else // if not tracking, only draw username if it's directly visible on screen
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

	b8 is_vulnerable = !get<AIAgent>()->stealth && (!has<Awk>() || get<Awk>()->overshield_timer == 0.0f) && health->hp == 1 && health->shield == 0;

	// compass
	{
		Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);
		if (is_vulnerable && get<PlayerCommon>()->incoming_attacker())
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
	if (has<Awk>())
	{
		r32 overshield_timer = get<Awk>()->overshield_timer;
		if (overshield_timer > 0.0f)
		{
			Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
			Rect2 bar = { viewport.size * Vec2(0.5f, 0.75f) + bar_size * -0.5f, bar_size };
			UI::box(params, bar, UI::color_background);
			UI::border(params, bar, 2, UI::color_accent);
			UI::box(params, { bar.pos, Vec2(bar.size.x * (overshield_timer / AWK_OVERSHIELD_TIME), bar.size.y) }, UI::color_accent);

			UIText text;
			text.size = 18.0f;
			text.color = UI::color_background;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(strings::overshield));
			text.draw(params, bar.pos + bar.size * 0.5f);
		}
	}

	// detect danger
	{
		r32 detect_danger = get<PlayerCommon>()->detect_danger();
		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.3f) + Vec2(0.0f, -32.0f * UI::scale);
		if (detect_danger > 0.0f)
		{
			UIText text;
			text.size = 18.0f;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.text(_(strings::enemy_tracking));
			if (detect_danger == 1.0f)
			{
				Rect2 box = text.rect(pos).outset(MENU_ITEM_PADDING);
				UI::box(params, box, UI::color_background);
				if (UI::flash_function_slow(Game::real_time.total))
				{
					text.color = UI::color_alert;
					text.draw(params, pos);
				}
			}
			else
			{
				// draw bar
				Rect2 box = text.rect(pos).outset(MENU_ITEM_PADDING);
				UI::box(params, box, UI::color_background);
				UI::border(params, box, 2, UI::color_alert);
				UI::box(params, { box.pos, Vec2(box.size.x * detect_danger, box.size.y) }, UI::color_alert);

				text.color = UI::color_background;
				text.draw(params, pos);

				// todo: sound
			}
		}
	}

	// reticle
	if (has<Awk>() && movement_enabled())
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
			&& (get<Awk>()->current_ability == Ability::None || player.ref()->get<PlayerManager>()->ability_valid(get<Awk>()->current_ability)))
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
				Ability a = get<Awk>()->current_ability;
				Vec2 p = pos + Vec2(0, -48.0f * UI::scale);
				UI::centered_box(params, { p, Vec2(34.0f * UI::scale) }, UI::color_background);
				UI::mesh(params, AbilityInfo::list[(s32)a].icon, p, Vec2(18.0f * UI::scale), *color);

				// cancel prompt
				UIText text;
				text.color = UI::color_accent;
				Controls binding = Controls::count;
				PlayerManager* manager = player.ref()->get<PlayerManager>();
				for (s32 i = 0; i < manager->ability_count(); i++)
				{
					if (a == manager->abilities[i])
					{
						const Controls bindings[3] = { Controls::Ability1, Controls::Ability2, Controls::Ability3 };
						binding = bindings[i];
						break;
					}
				}
				vi_assert(binding != Controls::count);
				text.text(_(strings::prompt_cancel_ability), Settings::gamepads[player.ref()->gamepad].bindings[(s32)binding].string(Game::is_gamepad));
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
	if (Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& Game::level.mode == Game::Mode::Pvp
		&& !Game::session.story_mode
		&& Game::time.total < GAME_BUY_PERIOD)
	{
		UIText text;
		text.color = UI::color_accent;
		text.text(_(strings::buy_period), (s32)(GAME_BUY_PERIOD - Game::time.total) + 1);
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.15f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
		text.draw(params, pos);
	}

	if (has<Parkour>() && Interactable::closest(get<Transform>()->absolute_pos()))
	{
		// interact prompt
		UIText text;
		text.color = UI::color_accent;
		text.text(_(strings::prompt_interact));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.15f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
		text.draw(params, pos);
	}
}


}
