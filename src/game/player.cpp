#include "player.h"
#include "drone.h"
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
#include "net.h"
#include "team.h"
#include "overworld.h"
#include "load.h"
#include "asset/level.h"

namespace VI
{


#define fov_map_view (60.0f * PI * 0.5f / 180.0f)
#define fov_default (70.0f * PI * 0.5f / 180.0f)
#define fov_zoom (fov_default * 0.5f)
#define fov_sniper (fov_default * 0.25f)
#define zoom_speed_multiplier 0.25f
#define zoom_speed_multiplier_sniper 0.15f
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse (0.05f / 60.0f)
#define speed_joystick 5.0f
#define gamepad_rotation_acceleration (1.0f / 0.2f)
#define attach_speed 5.0f
#define rotation_speed 20.0f
#define msg_time 0.75f
#define text_size 16.0f
#define camera_shake_time 0.7f
#define arm_angle_offset -0.2f

#define INTERACT_TIME 2.5f
#define INTERACT_LERP_ROTATION_SPEED 5.0f
#define INTERACT_LERP_TRANSLATION_SPEED 5.0f
#define LOG_TIME 5.0f

#define HP_BOX_SIZE (Vec2(text_size) * UI::scale)
#define HP_BOX_SPACING (8.0f * UI::scale)

r32 hp_width(u8 hp, s8 shield, r32 scale = 1.0f)
{
	const Vec2 box_size = HP_BOX_SIZE;
	return scale * ((shield + (hp - 1)) * (box_size.x + HP_BOX_SPACING) - HP_BOX_SPACING);
}

void PlayerHuman::camera_setup_drone(Entity* e, Camera* camera, r32 offset)
{
	Quat drone_rot = e->get<Drone>()->lerped_rotation;
	Vec3 center = e->get<Drone>()->center_lerped();
	camera->pos = center + camera->rot * Vec3(0, 0, -offset);
	Vec3 abs_wall_normal;
	if (e->get<Transform>()->parent.ref())
	{
		abs_wall_normal = drone_rot * Vec3(0, 0, 1);
		camera->pos += abs_wall_normal * 0.5f;
		camera->pos.y += 0.5f - vi_min((r32)fabsf(abs_wall_normal.y), 0.5f);
	}
	else
		abs_wall_normal = camera->rot * Vec3(0, 0, 1);

	Quat rot_inverse = camera->rot.inverse();

	camera->range = e->get<Drone>()->range();
	camera->range_center = rot_inverse * (center - camera->pos);
	camera->cull_center = Vec3(0, 0, offset + DRONE_SHIELD_RADIUS);
	Vec3 wall_normal_viewspace = rot_inverse * abs_wall_normal;
	camera->clip_planes[0].redefine(wall_normal_viewspace, camera->range_center + wall_normal_viewspace * -DRONE_RADIUS);
	camera->flag(CameraFlagCullBehindWall, abs_wall_normal.dot(camera->pos - center) < -DRONE_RADIUS + 0.02f); // camera is behind wall; set clip plane to wall
	camera->cull_range = camera->range_center.length();
	camera->flag(CameraFlagColors, false);
	camera->flag(CameraFlagFog, false);
}

s32 PlayerHuman::count_local()
{
	s32 count = 0;
	for (auto i = list.iterator(); !i.is_last(); i.next())
	{
#if !SERVER
		if (i.item()->local || Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying)
#else
		if (i.item()->local)
#endif
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
#if !SERVER
		if (i.item()->local || Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying)
#else
		if (i.item()->local)
#endif
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
	rumble(),
	upgrade_menu_open(),
	upgrade_animation_time(),
	upgrade_last_visit_highest_available(Upgrade::None),
	score_summary_scroll(),
	spectate_index(),
	try_capture(),
#if SERVER
	ai_record(),
#endif
	local(local)
{
	if (local)
		uuid = Game::session.local_player_uuids[gamepad];
}

void PlayerHuman::rumble_add(r32 r)
{
	rumble = vi_max(rumble, r);
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
			if (get<PlayerManager>()->instance.ref()->has<Drone>())
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
	msg_text.text(gamepad, msg);
	msg_text.color = good ? UI::color_accent : UI::color_alert;
	msg_timer = 0.0f;
	msg_good = good;
}

void PlayerHuman::awake()
{
	get<PlayerManager>()->spawn.link<PlayerHuman, const SpawnPosition&, &PlayerHuman::spawn>(this);

	msg_text.size = text_size;
	msg_text.anchor_x = UIText::Anchor::Center;
	msg_text.anchor_y = UIText::Anchor::Center;

	if (local
#if !SERVER
		|| Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying
#endif
		)
	{
		Audio::listener_enable(gamepad);

		camera = Camera::add(gamepad);
		camera->team = s8(get<PlayerManager>()->team.ref()->team());
		camera->mask = 1 << camera->team;
		camera->flag(CameraFlagColors, false);

		Quat rot;
		Game::level.map_view.ref()->absolute(&camera->pos, &rot);
		camera->rot = Quat::look(rot * Vec3(0, -1, 0));
	}
}

PlayerHuman::~PlayerHuman()
{
	if (camera)
	{
		camera->remove();
		camera = nullptr;
		Audio::listener_disable(gamepad);
	}
#if SERVER
	ai_record_save();
#endif
}

#define DANGER_RAMP_UP_TIME 2.0f
#define DANGER_LINGER_TIME 3.0f
#define DANGER_RAMP_DOWN_TIME 4.0f
r32 PlayerHuman::danger;
StaticArray<PlayerHuman::LogEntry, 4> PlayerHuman::logs;
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
			const PlayerManager::Visibility& visibility = PlayerManager::visibility[PlayerManager::visibility_hash(local_common, j.item()->manager.ref())];
			if (visibility.type == PlayerManager::Visibility::Type::Direct && visibility.entity.ref())
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

	for (s32 i = logs.length - 1; i >= 0; i--)
	{
		if (logs[i].timestamp < Game::real_time.total - LOG_TIME)
			logs.remove_ordered(i);
	}
}

void PlayerHuman::log_add(const char* msg, AI::Team team)
{
	if (logs.length == logs.capacity())
		logs.remove_ordered(0);
	PlayerHuman::LogEntry* entry = logs.add();
	entry->timestamp = Game::real_time.total;
	entry->team = team;
	strncpy(entry->text, msg, 511);
}

void PlayerHuman::clear()
{
	danger = 0.0f;
	logs.length = 0;
}

void PlayerHuman::update_camera_rotation(const Update& u)
{
	{
		r32 s = speed_mouse * Settings::gamepads[gamepad].effective_sensitivity() * Game::session.effective_time_scale();
		angle_horizontal -= (r32)u.input->cursor_x * s;
		angle_vertical += (r32)u.input->cursor_y * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
	}

	if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
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
		AbilitySelect,
		count,
	};

	Vec3 pos;
	Vec3 dir;
	Ability ability = Ability::None;
	Upgrade upgrade = Upgrade::None;
	Type type;
	Ref<Entity> entity;
};

template<typename Stream> b8 serialize_msg(Stream* p, Message* msg)
{
	serialize_enum(p, Message::Type, msg->type);

	// position/dir
	if (msg->type == Message::Type::Dash
		|| msg->type == Message::Type::Go
		|| msg->type == Message::Type::Reflect)
	{
		serialize_position(p, &msg->pos, Net::Resolution::High);
		serialize_r32_range(p, msg->dir.x, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.y, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.z, -1.0f, 1.0f, 16);
	}

	// ability
	if (msg->type == Message::Type::Go
		|| msg->type == Message::Type::AbilitySelect)
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

	// what did we reflect off of
	if (msg->type == Message::Type::Reflect)
		serialize_ref(p, msg->entity);
	else
		msg->entity = nullptr;

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

void PlayerHuman::upgrade_menu_show()
{
	upgrade_menu_open = true;
	menu.animate();
	upgrade_animation_time = Game::real_time.total;
	Entity* instance = get<PlayerManager>()->instance.ref();
	if (instance)
		instance->get<Drone>()->ability(Ability::None);
}

void PlayerHuman::upgrade_menu_hide()
{
	if (upgrade_menu_open)
		upgrade_last_visit_highest_available = get<PlayerManager>()->upgrade_highest_owned_or_available();
	upgrade_menu_open = false;
}

void PlayerHuman::update(const Update& u)
{
	if (!local
#if !SERVER
		&& Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying
#endif
		)
		return;

	Entity* entity = get<PlayerManager>()->instance.ref();

#if !SERVER
	// if anyone hits a button, go back to the main menu
	if (Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying)
	{
		if (Game::scheduled_load_level == AssetNull
			&& ((gamepad == 0 && u.input->keys.any()) || u.input->gamepads[gamepad].btns))
			Menu::title();
	}
	else // no rumble when replaying
#endif
	if (rumble > 0.0f)
	{
		u.input->gamepads[gamepad].rumble = vi_min(1.0f, rumble);
		rumble = vi_max(0.0f, rumble - u.time.delta);
	}

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
			Vec2(s32(blueprint->x * r32(u.input->width)), s32(blueprint->y * r32(u.input->height))),
			Vec2(s32(blueprint->w * r32(u.input->width)), s32(blueprint->h * r32(u.input->height))),
		};

		if (!entity)
		{
			r32 aspect = camera->viewport.size.y == 0 ? 1.0f : camera->viewport.size.x / camera->viewport.size.y;
			camera->perspective(fov_map_view, aspect, 1.0f, Game::level.skybox.far_plane);
		}

		// reset camera range after the player dies
		if (!get<PlayerManager>()->instance.ref())
		{
			camera->range = 0;
			camera->flag(CameraFlagColors, Game::level.mode == Game::Mode::Parkour);
			upgrade_menu_hide();
		}

		Audio::listener_update(gamepad, camera->pos, camera->rot);
	}

	if (msg_timer < msg_time)
		msg_timer += Game::real_time.delta;

	// after this point, it's all input-related stuff
	if (Console::visible || Overworld::active()
#if !SERVER
		|| Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying
#endif
		)
		return;

	// close/open pause menu if needed
	{
		if (Game::time.total > 0.5f
			&& u.last_input->get(Controls::Pause, gamepad)
			&& !u.input->get(Controls::Pause, gamepad)
			&& !Game::cancel_event_eaten[gamepad]
			&& !upgrade_menu_open
			&& (menu_state == Menu::State::Hidden || menu_state == Menu::State::Visible))
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

	switch (ui_mode())
	{
		case UIMode::PvpDefault:
		{
			if (get<PlayerManager>()->at_spawn_point())
			{
				if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
					upgrade_menu_show();
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
				upgrade_menu_hide();
			}
			else
			{
				b8 upgrade_in_progress = !get<PlayerManager>()->can_transition_state();

				s8 last_selected = menu.selected;

				menu.start(u, gamepad);

				if (menu.item(u, _(strings::close), nullptr, false, Asset::Mesh::icon_close))
					upgrade_menu_hide();

				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					Upgrade upgrade = Upgrade(i);
					if (!get<PlayerManager>()->has_upgrade(upgrade))
					{
						const UpgradeInfo& info = UpgradeInfo::list[s32(upgrade)];
						b8 can_upgrade = !upgrade_in_progress
							&& get<PlayerManager>()->upgrade_available(upgrade)
							&& get<PlayerManager>()->energy >= get<PlayerManager>()->upgrade_cost(upgrade)
							&& (AbilityInfo::list[i].type != AbilityInfo::Type::Other || Game::level.has_feature(Game::FeatureLevel::All)); // don't allow Other ability upgrades in tutorial
						if (menu.item(u, _(info.name), nullptr, !can_upgrade, info.icon))
						{
							PlayerControlHumanNet::Message msg;
							msg.type = PlayerControlHumanNet::Message::Type::UpgradeStart;
							msg.upgrade = upgrade;
							PlayerControlHumanNet::send(entity->get<PlayerControlHuman>(), &msg);
						}
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

				r32 aspect = camera->viewport.size.y == 0 ? 1 : r32(camera->viewport.size.x) / r32(camera->viewport.size.y);
				camera->perspective(fov_map_view, aspect, 0.02f, Game::level.skybox.far_plane);
				camera->range = 0;
				camera->cull_range = 0;

				if (!Console::visible)
				{
					r32 speed = u.input->keys.get(s32(KeyCode::LShift)) ? 24.0f : 4.0f;
					if (u.input->get(Controls::Forward, gamepad))
						camera->pos += camera->rot * Vec3(0, 0, 1) * u.time.delta * speed;
					if (u.input->get(Controls::Backward, gamepad))
						camera->pos += camera->rot * Vec3(0, 0, -1) * u.time.delta * speed;
					if (u.input->get(Controls::Right, gamepad))
						camera->pos += camera->rot * Vec3(-1, 0, 0) * u.time.delta * speed;
					if (u.input->get(Controls::Left, gamepad))
						camera->pos += camera->rot * Vec3(1, 0, 0) * u.time.delta * speed;

#if DEBUG
					if (Game::level.local && u.input->keys.get(s32(KeyCode::MouseLeft)) && !u.last_input->keys.get(s32(KeyCode::MouseLeft)))
					{
						Entity* box = World::create<PhysicsEntity>(Asset::Mesh::cube, camera->pos, camera->rot, RigidBody::Type::Box, Vec3(0.25f, 0.25f, 0.5f), 1.0f, CollisionDefault, ~CollisionDroneIgnore);
						box->get<RigidBody>()->btBody->setLinearVelocity(camera->rot * Vec3(0, 0, 15));
						Net::finalize(box);
					}
#endif
				}
			}
			else if (get<PlayerManager>()->spawn_timer > 0.0f)
			{
				// we're spawning
				if (Game::level.mode == Game::Mode::Pvp && !get<PlayerManager>()->can_spawn)
				{
					// player can't spawn yet; needs to solve sudoku
					sudoku.update(u, gamepad, this);
					if (sudoku.complete() && sudoku.timer_animation == 0.0f)
						get<PlayerManager>()->set_can_spawn();
				}
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
						camera_setup_drone(spectating, camera, 6.0f);
				}
			}
			break;
		}
		case UIMode::GameOver:
		{
			camera->range = 0;
			if (Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY)
			{
				// update score summary scroll
				score_summary_scroll.update(u, Team::score_summary.length, gamepad);

				if (Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY + SCORE_SUMMARY_ACCEPT_DELAY)
				{
					// accept score summary
					if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
						get<PlayerManager>()->score_accept();
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

void PlayerHuman::update_late(const Update& u)
{
#if !SERVER
	if (Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying)
	{
		Entity* e = get<PlayerManager>()->instance.ref();
		if (e)
		{
			camera->rot = Quat::euler(0.0f, PI * 0.25f, PI * 0.25f);
			camera_setup_drone(e, camera, 8.0f);
		}
	}
#endif
}

void get_interactable_standing_position(Transform* i, Vec3* pos, r32* angle)
{
	Vec3 i_pos;
	Quat i_rot;
	i->absolute(&i_pos, &i_rot);
	Vec3 dir = i_rot * Vec3(-1, 0, 0);
	dir.y = 0.0f;
	dir.normalize();
	if (angle)
		*angle = atan2f(dir.x, dir.z);
	*pos = i_pos + dir * -1.0f;
	pos->y += (WALKER_DEFAULT_CAPSULE_HEIGHT * 0.5f) + WALKER_SUPPORT_HEIGHT;
}

#if SERVER
void PlayerHuman::ai_record_save()
{
	if (ai_record.action.length > 0)
	{
		// save AI record
		char path[512];
		Loader::ai_record_path(path, Game::level.id, Game::level.type);
		FILE* f = fopen(path, "ab");
		if (!f)
		{
			fprintf(stderr, "Can't open air file '%s'\n", path);
			vi_assert(false);
		}
		ai_record.serialize(f, &AI::RecordedLife::custom_fwrite);
		fclose(f);
	}
	ai_record.reset();
}
#endif

void PlayerHuman::spawn(const SpawnPosition& normal_spawn_pos)
{
	Entity* spawned;

	SpawnPosition spawn_pos;

#if SERVER
	ai_record_save();
	ai_record.reset(Game::level.team_lookup_reverse(get<PlayerManager>()->team.ref()->team()), get<PlayerManager>()->respawns);
#endif

	if (Game::level.mode == Game::Mode::Pvp)
	{
		// spawn drone
		spawned = World::create<DroneEntity>(get<PlayerManager>()->team.ref()->team());
		spawn_pos = normal_spawn_pos;
	}
	else
	{
		// spawn traceur
		if (Game::save.zone_current_restore)
		{
			spawn_pos.angle = Game::save.zone_current_restore_rotation;
			spawn_pos.pos = Game::save.zone_current_restore_position;
		}
		else if (Game::level.post_pvp)
		{
			// we have already played a PvP match on this level; we must be exiting PvP mode.
			// spawn the player at the terminal.
			get_interactable_standing_position(Game::level.terminal_interactable.ref()->get<Transform>(), &spawn_pos.pos, &spawn_pos.angle);
		}
		else
		{
			// we are entering a level. if we're entering by tram, spawn in the tram. otherwise spawn at the PlayerSpawn

			s8 track = -1;
			for (s32 i = 0; i < Game::level.tram_tracks.length; i++)
			{
				const Game::TramTrack& t = Game::level.tram_tracks[i];
				if (t.level == Game::save.zone_last)
				{
					track = s8(i);
					break;
				}
			}

			Tram* tram = Tram::by_track(track);

			if (tram)
			{
				// spawn in tram
				Quat rot;
				tram->get<Transform>()->absolute(&spawn_pos.pos, &rot);
				spawn_pos.pos.y -= 1.0f;
				Vec3 dir = rot * Vec3(0, 0, -1);
				dir.y = 0.0f;
				dir.normalize();
				spawn_pos.angle = atan2f(dir.x, dir.z);
			}
			else // spawn at normal position
				spawn_pos = normal_spawn_pos;
			spawn_pos.pos.y += 1.0f;
		}

		spawned = World::create<Traceur>(spawn_pos.pos, spawn_pos.angle, get<PlayerManager>()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute_pos(spawn_pos.pos);
	PlayerCommon* common = spawned->add<PlayerCommon>(get<PlayerManager>());
	common->angle_horizontal = spawn_pos.angle;

	get<PlayerManager>()->instance = spawned;

	spawned->add<PlayerControlHuman>(this);

	if (Game::level.mode == Game::Mode::Parkour)
	{
		if (Game::level.post_pvp && !Game::save.zone_current_restore)
		{
			// player is getting out of the terminal
			spawned->get<Animator>()->layers[3].set(Asset::Animation::character_terminal_exit, 0.0f); // bypass animation blending
			spawned->get<PlayerControlHuman>()->interactable = Game::level.terminal_interactable.ref()->get<Interactable>();
		}

		Game::save.zone_current_restore = false;
		Game::level.post_pvp = false;
	}

	Net::finalize(spawned);
}

r32 draw_icon_text(const RenderParams& params, s8 gamepad, const Vec2& pos, AssetID icon, char* string, const Vec4& color, r32 total_width = 0.0f)
{
	r32 icon_size = text_size * UI::scale;
	r32 padding = 8 * UI::scale;

	UIText text;
	text.color = color;
	text.size = text_size;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Center;
	text.text(gamepad, string);

	if (total_width == 0.0f)
		total_width = icon_size + padding + text.bounds().x;
	else
		total_width -= padding * 2.0f;

	UI::box(params, Rect2(pos, Vec2(total_width, icon_size)).outset(padding), UI::color_background);
	if (icon != AssetNull)
		UI::mesh(params, icon, pos + Vec2(icon_size - padding, icon_size * 0.5f), Vec2(icon_size), text.color);
	text.draw(params, pos + Vec2(icon == AssetNull ? 0 : icon_size + padding, padding));

	return total_width + padding * 2.0f;
}

r32 ability_draw(const RenderParams& params, const PlayerHuman* player, const Vec2& pos, Ability ability, AssetID icon, s8 gamepad, Controls binding)
{
	char string[255];

	s16 cost = AbilityInfo::list[s32(ability)].spawn_cost;
	sprintf(string, "%s", Settings::gamepads[gamepad].bindings[s32(binding)].string(Game::ui_gamepad_types[gamepad]));
	const Vec4* color;
	PlayerManager* manager = player->get<PlayerManager>();
	if (!manager->ability_valid(ability) || !manager->instance.ref()->get<PlayerCommon>()->movement_enabled())
		color = params.sync->input.get(binding, gamepad) ? &UI::color_disabled : &UI::color_alert;
	else if (manager->instance.ref()->get<Drone>()->current_ability == ability)
		color = &UI::color_default;
	else
		color = &UI::color_accent;
	return draw_icon_text(params, gamepad, pos, icon, string, *color);
}

r32 battery_timer_width()
{
	return text_size * 5 * UI::scale;
}

void battery_timer_draw(const RenderParams& params, const Vec2& pos, UIText::Anchor anchor_x)
{
	r32 remaining = vi_max(0.0f, Game::level.time_limit - Team::match_time);

	Vec2 box(battery_timer_width(), text_size * UI::scale);
	r32 padding = 8.0f * UI::scale;

	Vec2 p = pos;
	switch (anchor_x)
	{
		case UIText::Anchor::Min:
		{
			break;
		}
		case UIText::Anchor::Center:
		{
			p.x += box.x * -0.5f;
			break;
		}
		case UIText::Anchor::Max:
		{
			p.x -= box.x;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
		
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
	text.text(0, _(strings::timer), remaining_minutes, remaining_seconds);
	text.draw(params, icon_pos + Vec2(text_size * UI::scale * 1.5f, 0));
}

void scoreboard_draw(const RenderParams& params, const PlayerManager* manager)
{
	const Rect2& vp = params.camera->viewport;
	Vec2 p = vp.size * Vec2(0.5f, 0.8f);

	UIText text;
	text.size = text_size;
	r32 wrap = text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::color_default;

	if (Game::level.mode == Game::Mode::Pvp)
		battery_timer_draw(params, p, UIText::Anchor::Center);
	p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;

	// "spawning..."
	if (!manager->instance.ref() && manager->respawns != 0)
	{
		text.text(0, _(strings::deploy_timer), s32(manager->spawn_timer + 1));
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	if (Game::level.type == GameType::Assault)
	{
		// show remaining drones label
		text.text(0, _(strings::drones_remaining));
		text.color = UI::color_accent;
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	// show player list
	p.x += wrap * -0.5f;

	// sort by team
	AI::Team team_mine = manager->team.ref()->team();
	AI::Team team = team_mine;
	while (true)
	{
		for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team.ref()->team() == team)
			{
				text.wrap_width = wrap;
				text.anchor_x = UIText::Anchor::Min;
				text.color = Team::ui_color(manager->team.ref()->team(), i.item()->team.ref()->team());
				text.text_raw(0, i.item()->username);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);

				text.anchor_x = UIText::Anchor::Max;
				text.wrap_width = 0;
				if (Game::level.type == GameType::Deathmatch)
					text.text(0, "%d", s32(i.item()->kills));
				else
					text.text(0, "%d", s32(i.item()->respawns));
				text.draw(params, p + Vec2(wrap, 0));

				p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
			}
		}
		team = AI::Team(s32(team) + 1 % Team::list.count());
		if (team == team_mine)
			break;
	}
}

void PlayerHuman::draw_ui(const RenderParams& params) const
{
	if (params.camera != camera
		|| Overworld::active()
		|| Game::level.continue_match_after_death
		|| !local)
		return;

	const r32 line_thickness = 2.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	UIMode mode = ui_mode();

	Vec2 ui_anchor;
	if (Game::ui_gamepad_types[gamepad] == Gamepad::Type::None) // left side
		ui_anchor = vp.size * Vec2(0.1f, 0.1f) + Vec2(0, text_size * UI::scale * 0.5f);
	else // right side
		ui_anchor = vp.size * Vec2(0.9f, 0.1f) + Vec2(text_size * UI::scale * -14.0f, text_size * UI::scale * 0.5f);

	// draw abilities
	if (Game::level.has_feature(Game::FeatureLevel::Abilities))
	{
		if (mode == UIMode::PvpDefault
			&& get<PlayerManager>()->can_transition_state()
			&& get<PlayerManager>()->at_spawn_point())
		{
			// "upgrade!" prompt
			UIText text;
			text.color = get<PlayerManager>()->upgrade_available() ? UI::color_accent : UI::color_disabled;
			text.text(gamepad, _(strings::prompt_upgrade));
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 pos = vp.size * Vec2(0.5f, 0.15f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(params, pos);
		}

		if ((mode == UIMode::PvpDefault || mode == UIMode::Upgrading)
			&& get<PlayerManager>()->can_transition_state())
		{
			// draw abilities

			Vec2 pos = ui_anchor;
			// ability 1
			{
				Ability ability = get<PlayerManager>()->abilities[0];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[s32(ability)];
					pos.x += ability_draw(params, this, pos, ability, info.icon, gamepad, Controls::Ability1);
				}
			}

			// ability 2
			{
				Ability ability = get<PlayerManager>()->abilities[1];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[s32(ability)];
					pos.x += ability_draw(params, this, pos, ability, info.icon, gamepad, Controls::Ability2);
				}
			}

			// ability 3
			{
				Ability ability = get<PlayerManager>()->abilities[2];
				if (ability != Ability::None)
				{
					const AbilityInfo& info = AbilityInfo::list[s32(ability)];
					pos.x += ability_draw(params, this, pos, ability, info.icon, gamepad, Controls::Ability3);
				}
			}
		}
	}

	if (Game::level.mode == Game::Mode::Pvp
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& (mode == UIMode::PvpDefault || mode == UIMode::Upgrading))
	{
		// energy
		char buffer[128];
		sprintf(buffer, "%d", get<PlayerManager>()->energy);
		Vec2 p = ui_anchor + Vec2(battery_timer_width() + text_size * UI::scale, (text_size + 16.0f) * -UI::scale);
		draw_icon_text(params, gamepad, p, Asset::Mesh::icon_energy, buffer, UI::color_accent, text_size * 5 * UI::scale);

		// battery counts
		AI::Team my_team = get<PlayerManager>()->team.ref()->team();
		p.x += text_size * 5 * UI::scale;
		sprintf(buffer, "%d", Battery::count(1 << s32(my_team)));
		draw_icon_text(params, gamepad, p, AssetNull, buffer, Team::ui_color_friend, text_size * 1.5f * UI::scale);
		p.x += text_size * 1.5f * UI::scale;
		sprintf(buffer, "%d", Battery::count(AI::TeamNone));
		draw_icon_text(params, gamepad, p, AssetNull, buffer, UI::color_accent, text_size * 1.5f * UI::scale);
		p.x += text_size * 1.5f * UI::scale;
		sprintf(buffer, "%d", Battery::count(~(1 << s32(my_team))));
		draw_icon_text(params, gamepad, p, AssetNull, buffer, Team::ui_color_enemy, text_size * 1.5f * UI::scale);
	}

	if (mode == UIMode::PvpDefault)
	{
		if (params.sync->input.get(Controls::Scoreboard, gamepad))
			scoreboard_draw(params, get<PlayerManager>());
	}
	else if (mode == UIMode::Upgrading)
	{
		Vec2 upgrade_menu_pos = vp.size * Vec2(0.5f, 0.6f);
		menu.draw_ui(params, upgrade_menu_pos, UIText::Anchor::Center, UIText::Anchor::Center);

		if (menu.selected > 0)
		{
			// show details of currently highlighted upgrade
			Upgrade upgrade = Upgrade::None;
			{
				// purchased upgrades are removed from the menu
				// we have to figure out which one is selected
				s32 index = 0;
				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					if (!get<PlayerManager>()->has_upgrade(Upgrade(i)))
					{
						if (index == menu.selected - 1)
						{
							upgrade = Upgrade(i);
							break;
						}
						index++;
					}
				}
			}
			vi_assert(upgrade != Upgrade::None);

			if (get<PlayerManager>()->current_upgrade == Upgrade::None
				&& get<PlayerManager>()->upgrade_available(upgrade))
			{
				r32 padding = 8.0f * UI::scale;

				const UpgradeInfo& info = UpgradeInfo::list[s32(upgrade)];
				UIText text;
				text.color = UI::color_accent;
				text.size = text_size;
				text.anchor_x = UIText::Anchor::Min;
				text.anchor_y = UIText::Anchor::Max;
				text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
				s16 cost = get<PlayerManager>()->upgrade_cost(upgrade);
				text.text(gamepad, _(strings::upgrade_description), cost, _(info.description));
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
			{
				if (!get<PlayerManager>()->can_spawn)
				{
					// player can't spawn yet; needs to solve sudoku
					if (UI::flash_function_slow(Game::real_time.total))
					{
						// alarm!

						UIText text;
						text.size = text_size * 1.5f;
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Min;
						text.color = UI::color_alert;
						text.text(gamepad, _(strings::alarm));

						Vec2 pos = vp.size * Vec2(0.5f, 0.8f);
						Rect2 rect = text.rect(pos).outset(MENU_ITEM_PADDING * 2.0f);
						UI::box(params, rect, UI::color_background);
						text.draw(params, pos);
						UI::border(params, rect, 4.0f, UI::color_alert);
					}
					sudoku.draw(params, gamepad);
				}
				else
					scoreboard_draw(params, get<PlayerManager>());
			}
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
					text.text_raw(gamepad, spectating->get<PlayerCommon>()->manager.ref()->username);
					Vec2 pos = vp.size * Vec2(0.5f, 0.2f);
					UI::box(params, text.rect(pos).outset(MENU_ITEM_PADDING), UI::color_background);
					text.draw(params, pos);

					// "spectating"
					text.color = UI::color_accent;
					text.text(gamepad, _(strings::spectating));
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
			text.text(gamepad, _(strings::victory));
		}
		else if (!winner) // it's a draw
		{
			text.color = UI::color_alert;
			text.text(gamepad, _(strings::draw));
		}
		else // we lost
		{
			text.color = UI::color_alert;
			text.text(gamepad, _(strings::defeat));
		}
		UIMenu::text_clip(&text, Team::game_over_real_time, 20.0f);

		b8 show_score_summary = Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY;
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
			AI::Team team = get<PlayerManager>()->team.ref()->team();
			for (s32 i = 0; i < Team::score_summary.length; i++)
			{
				const Team::ScoreSummaryItem& item = Team::score_summary[i];
				text.color = item.player.ref() == get<PlayerManager>() ? UI::color_accent : Team::ui_color(team, item.team);

				UIText amount = text;
				amount.anchor_x = UIText::Anchor::Max;
				amount.wrap_width = 0;

				if (score_summary_scroll.item(i))
				{
					text.text_raw(gamepad, item.label);
					UIMenu::text_clip(&text, Team::game_over_real_time + SCORE_SUMMARY_DELAY, 50.0f + r32(vi_min(i, 6)) * -5.0f);
					UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
					text.draw(params, p);
					if (item.amount != -1)
					{
						amount.text(gamepad, "%d", item.amount);
						amount.draw(params, p + Vec2(MENU_ITEM_WIDTH * 0.5f - MENU_ITEM_PADDING, 0));
					}
					p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
				}
			}
			score_summary_scroll.end(params, p + Vec2(0, MENU_ITEM_PADDING));

			// press x to continue
			if (Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY + SCORE_SUMMARY_ACCEPT_DELAY)
			{
				Vec2 p = vp.size * Vec2(0.5f, 0.2f);
				text.wrap_width = 0;
				text.color = UI::color_accent;
				text.text(gamepad, _(get<PlayerManager>()->score_accepted ? strings::waiting : strings::prompt_accept));
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

						const UpgradeInfo& info = UpgradeInfo::list[s32(get<PlayerManager>()->current_upgrade)];
						cost = info.cost;
						total_time = UPGRADE_TIME;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				// draw bar

				UIText text;
				text.size = 18.0f;
				text.color = UI::color_background;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				text.text(gamepad, _(string), s32(cost));
				Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);
				Rect2 bar = text.rect(pos).outset(MENU_ITEM_PADDING);
				UI::box(params, bar, UI::color_background);
				UI::border(params, bar, 2, UI::color_accent);
				UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (get<PlayerManager>()->state_timer / total_time)), bar.size.y) }, UI::color_accent);
				text.draw(params, pos);
			}
		}

		if (mode == UIMode::PvpDefault || mode == UIMode::Upgrading) // show game timer
			battery_timer_draw(params, ui_anchor + Vec2(0, (text_size + 16.0f) * -UI::scale), UIText::Anchor::Min);
	}

	// network error icon
#if !SERVER
	if (!Game::level.local && Net::Client::lagging())
		UI::mesh(params, Asset::Mesh::icon_network_error, vp.size * Vec2(0.9f, 0.5f), Vec2(text_size * 2.0f * UI::scale), UI::color_alert);
#endif

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

	// logs
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Max;
		text.anchor_y = UIText::Anchor::Max;
		text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
		Vec2 p = params.camera->viewport.size + Vec2(MENU_ITEM_PADDING * -5.0f);
		AI::Team my_team = get<PlayerManager>()->team.ref()->team();
		for (s32 i = 0; i < logs.length; i++)
		{
			const LogEntry& entry = logs[i];
			text.color = Team::ui_color(my_team, entry.team);
			text.text_raw(gamepad, entry.text);
			UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
			UIMenu::text_clip(&text, entry.timestamp, 80.0f);
			text.draw(params, p);
			p.y -= (text.size * UI::scale) + MENU_ITEM_PADDING * 2.0f;
		}
	}

	// overworld notifications
	if (Game::level.mode == Game::Mode::Parkour)
	{
		r32 timer = Overworld::zone_under_attack_timer();
		Vec2 p = Vec2(params.camera->viewport.size.x, 0) + Vec2(MENU_ITEM_PADDING * -5.0f, MENU_ITEM_PADDING * 5.0f);
		if (timer > 0.0f)
		{
			UIText text;
			text.anchor_x = UIText::Anchor::Max;
			text.anchor_y = UIText::Anchor::Min;
			text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			text.color = UI::color_alert;
			text.text(gamepad, _(strings::prompt_zone_defend), Loader::level_name(Overworld::zone_under_attack()), s32(ceilf(timer)));
			UIMenu::text_clip_timer(&text, ZONE_UNDER_ATTACK_THRESHOLD - timer, 80.0f);
			UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
			text.draw(params, p);
			p.y += text.bounds().y + MENU_ITEM_PADDING;
		}
	}

	if (mode == UIMode::Pause) // pause menu always drawn on top
		menu.draw_ui(params, Vec2(0, params.camera->viewport.size.y * 0.5f), UIText::Anchor::Min, UIText::Anchor::Center);
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
	if (has<Drone>())
	{
		get<Health>()->hp_max = DRONE_HEALTH;
		link<&PlayerCommon::drone_done_flying>(get<Drone>()->done_flying);
		link<&PlayerCommon::drone_detaching>(get<Drone>()->detaching);
		link_arg<const DroneReflectEvent&, &PlayerCommon::drone_reflecting>(get<Drone>()->reflecting);
	}
}

b8 PlayerCommon::movement_enabled() const
{
	if (has<Drone>())
	{
		return get<Drone>()->state() == Drone::State::Crawl // must be attached to wall
			&& manager.ref()->state() == PlayerManager::State::Default; // can't move while upgrading and stuff
	}
	else
		return true;
}

Entity* PlayerCommon::incoming_attacker() const
{
	Vec3 me = get<Transform>()->absolute_pos();

	// check incoming Drones
	PlayerManager* manager = get<PlayerCommon>()->manager.ref();
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		const PlayerManager::Visibility& visibility = PlayerManager::visibility[PlayerManager::visibility_hash(manager, i.item()->manager.ref())];
		if (visibility.entity.ref() && visibility.type == PlayerManager::Visibility::Type::Direct)
		{
			// determine if they're attacking us
			if (i.item()->get<Drone>()->state() != Drone::State::Crawl
				&& Vec3::normalize(i.item()->get<Drone>()->velocity).dot(Vec3::normalize(me - i.item()->get<Transform>()->absolute_pos())) > 0.98f)
			{
				return i.item()->entity();
			}
		}
	}

	// check incoming bolts
	for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
	{
		Vec3 velocity = Vec3::normalize(i.item()->velocity);
		Vec3 bolt_pos = i.item()->get<Transform>()->absolute_pos();
		Vec3 to_me = me - bolt_pos;
		r32 dot = velocity.dot(to_me);
		if (dot > 0.0f && dot < DRONE_MAX_DISTANCE && velocity.dot(Vec3::normalize(to_me)) > 0.98f)
		{
			// only worry about it if it can actually see us
			btCollisionWorld::ClosestRayResultCallback ray_callback(me, bolt_pos);
			Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionShield);
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
			Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionShield);
			if (!ray_callback.hasHit())
				return rocket->entity();
		}
	}

	return nullptr;
}

void PlayerCommon::update(const Update& u)
{
	if (has<Drone>())
	{
		Quat rot = get<Transform>()->absolute_rot();
		r32 angle = Quat::angle(attach_quat, rot);
		if (angle > 0)
			attach_quat = Quat::slerp(vi_min(1.0f, rotation_speed * u.time.delta), attach_quat, rot);
	}
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
				return track->timer / SENSOR_TRACK_TIME;
		}
	}
	return 0.0f;
}

void PlayerCommon::drone_done_flying()
{
	Quat absolute_rot = get<Transform>()->absolute_rot();
	Vec3 wall_normal = absolute_rot * Vec3(0, 0, 1);

	// If we are spawning on to a flat floor, set attach_quat immediately
	// This preserves the camera rotation set by the PlayerSpawn
	if (Vec3::normalize(get<Drone>()->velocity).y == -1.0f && wall_normal.y > 0.9f)
		attach_quat = absolute_rot;
	else
	{
		// we are attaching to a wall or something
		// set the attach quat to be perpendicular to the camera, so we can ease the camera gently away from the wall
		Vec3 direction = look_dir();

		Vec3 up = Vec3::normalize(wall_normal.cross(direction));
		Vec3 right = direction.cross(up);

		// make sure the up and right vector aren't switched
		if (fabsf(up.y) < fabsf(right.y))
		{
			Vec3 tmp = right;
			right = up;
			up = tmp;
		}

		// if the right vector is too vertical, force it to be more horizontal
		const r32 threshold = fabsf(wall_normal.y) + 0.25f;
		right.y = LMath::clampf(right.y, -threshold, threshold);
		right.normalize();

		if (right.dot(direction - wall_normal * direction.dot(wall_normal)) < 0.0f)
			right *= -1.0f;

		attach_quat = Quat::look(right);
	}
}

void PlayerCommon::drone_detaching()
{
	Vec3 direction = Vec3::normalize(get<Drone>()->velocity);
	attach_quat = Quat::look(direction);
}

void PlayerCommon::drone_reflecting(const DroneReflectEvent& e)
{
	attach_quat = Quat::look(Vec3::normalize(e.new_velocity));
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

b8 PlayerControlHuman::net_msg(Net::StreamRead* p, PlayerControlHuman* c, Net::MessageSource src, Net::SequenceID seq)
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
		|| (msg.ability != Ability::None && !c->player.ref()->get<PlayerManager>()->has_upgrade(Upgrade(msg.ability)))) // don't have the upgrade for that ability
	{
		// invalid message, ignore
		net_error();
	}

	if (src == Net::MessageSource::Remote)
	{
		// make sure we are where the remote thinks we are when we start processing this message
		r32 dist_sq = (c->get<Transform>()->absolute_pos() - msg.pos).length_squared();
		if (dist_sq < NET_SYNC_TOLERANCE_POS * NET_SYNC_TOLERANCE_POS)
			c->get<Transform>()->absolute_pos(msg.pos);

#if SERVER
		// update RTT based on the sequence number
		c->rtt = Net::Server::rtt(c->player.ref(), seq);
#endif
	}

	switch (msg.type)
	{
		case PlayerControlHumanNet::Message::Type::Dash:
		{
			c->get<Drone>()->current_ability = Ability::None;
			if (c->get<Drone>()->dash_start(msg.dir))
			{
				c->get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
				c->try_primary = false;
				c->try_secondary = false;
			}
			break;
		}
		case PlayerControlHumanNet::Message::Type::Go:
		{
			c->get<Drone>()->current_ability = msg.ability;
			if (c->get<Drone>()->go(msg.dir))
			{
				c->try_primary = false;
				if (msg.ability == Ability::None)
				{
					c->get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
					c->try_secondary = false;
				}
				else
					c->player.ref()->rumble_add(0.5f);
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
				c->get<Drone>()->handle_remote_reflection(msg.entity.ref(), msg.pos, msg.dir);
			break;
		}
		case PlayerControlHumanNet::Message::Type::AbilitySelect:
		{
			if (msg.ability == Ability::None || c->get<PlayerCommon>()->manager.ref()->has_upgrade(Upgrade(msg.ability)))
				c->get<Drone>()->ability(msg.ability);
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

void PlayerControlHuman::drone_done_flying_or_dashing()
{
	player.ref()->rumble_add(0.2f);
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
#if SERVER
	if (!get<Health>()->invincible())
	{
		AI::RecordedLife::Action action;
		action.type = AI::RecordedLife::Action::TypeMove;
		Quat rot;
		get<Transform>()->absolute(&action.pos, &rot);
		action.normal = rot * Vec3(0, 0, 1);
		player.ref()->ai_record.add(ai_record_tag, action);
		ai_record_wait_timer = AI_RECORD_WAIT_TIME;
	}
	ai_record_tag.init(entity());
#endif
}

void ability_select(PlayerControlHuman* control, Ability a)
{
	vi_assert(AbilityInfo::list[s32(a)].type != AbilityInfo::Type::Other);
	PlayerControlHumanNet::Message msg;
	msg.type = PlayerControlHumanNet::Message::Type::AbilitySelect;
	msg.ability = a;
	PlayerControlHumanNet::send(control, &msg);
}

void ability_cancel(PlayerControlHuman* control)
{
	PlayerControlHumanNet::Message msg;
	msg.type = PlayerControlHumanNet::Message::Type::AbilitySelect;
	msg.ability = Ability::None;
	PlayerControlHumanNet::send(control, &msg);
}

void player_add_target_indicator(PlayerControlHuman* p, Target* target, PlayerControlHuman::TargetIndicator::Type type)
{
	Vec3 me = p->get<Transform>()->absolute_pos();

	b8 show;

	if (type == PlayerControlHuman::TargetIndicator::Type::DroneTracking)
		show = true; // show even out of range
	else
	{
		r32 range = p->get<Drone>()->range();
		show = (target->absolute_pos() - me).length_squared() < range * range;
	}

	if (show)
	{
		// calculate target intersection trajectory
		Vec3 intersection;
		if (p->get<Drone>()->predict_intersection(target, nullptr, &intersection, p->get<Drone>()->target_prediction_speed()))
			p->target_indicators.add({ intersection, target->velocity(), type });
	}
}

// returns the actual detected entity, if any. could be the original player, or a decoy.
Entity* player_determine_visibility(PlayerCommon* me, PlayerCommon* other_player, b8* visible, b8* tracking)
{
	// make sure we can see this guy
	AI::Team team = me->get<AIAgent>()->team;
	const Team::SensorTrack track = Team::list[s32(team)].player_tracks[other_player->manager.id];
	*tracking = track.tracking;

	if (other_player->get<AIAgent>()->team == team)
	{
		*visible = true;
		return other_player->entity();
	}
	else
	{
		const PlayerManager::Visibility& visibility = PlayerManager::visibility[PlayerManager::visibility_hash(me->manager.ref(), other_player->manager.ref())];
		Entity* visible_entity = visibility.entity.ref();
		*visible = visibility.type == PlayerManager::Visibility::Type::Direct && visibility.entity.ref();

		if (track.tracking)
			return track.entity.ref();
		else
			return visibility.entity.ref();
	}
}

void player_collect_target_indicators(PlayerControlHuman* p)
{
	p->target_indicators.length = 0;

	Vec3 me = p->get<Transform>()->absolute_pos();
	AI::Team team = p->get<AIAgent>()->team;

	// drone indicators
	for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
	{
		if (other_player.item()->get<AIAgent>()->team != team)
		{
			b8 tracking;
			b8 visible;
			Entity* detected_entity = player_determine_visibility(p->get<PlayerCommon>(), other_player.item(), &visible, &tracking);

			if (visible || tracking)
				player_add_target_indicator(p, detected_entity->get<Target>(), tracking ? PlayerControlHuman::TargetIndicator::Type::DroneTracking : PlayerControlHuman::TargetIndicator::Type::DroneVisible);
		}
	}

	// headshot indicators
	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->get<AIAgent>()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Minion);
	}

	// energy pickups
	for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Energy);
	}

	// sensors
	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Sensor);
	}

	// turrets
	for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Turret);
	}

	// rockets
	for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team() != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Rocket);
	}

	// grenades
	for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team() != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Grenade);
	}

	// force fields
	for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::ForceField);
	}
}

void player_ability_update(const Update& u, PlayerControlHuman* control, Controls binding, s8 gamepad, s32 index)
{
	PlayerHuman* player = control->player.ref();
	PlayerManager* manager = player->get<PlayerManager>();
	Ability ability = manager->abilities[index];

	if (ability == Ability::None || !control->input_enabled())
		return;

	Drone* drone = control->get<Drone>();

	if (u.input->get(binding, gamepad) && !u.last_input->get(binding, gamepad))
	{
		if (AbilityInfo::list[s32(ability)].type == AbilityInfo::Type::Other)
		{
			if (manager->ability_valid(ability))
			{
				PlayerControlHumanNet::Message msg;
				msg.pos = control->get<Transform>()->absolute_pos();
				msg.dir = Vec3::normalize(control->reticle.pos - msg.pos);
				msg.type = PlayerControlHumanNet::Message::Type::Go;
				msg.ability = ability;
				PlayerControlHumanNet::send(control, &msg);
			}
		}
		else
		{
			if (drone->current_ability == ability)
				ability_cancel(control); // cancel current spawn ability
			else
				ability_select(control, ability);
		}
	}
}

PlayerControlHuman::PlayerControlHuman(PlayerHuman* p)
	: fov(fov_default),
	try_primary(),
	try_secondary(),
	try_slide(),
	camera_shake_timer(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed(),
	remote_control(),
	player(p),
	position_history(),
	interactable(),
#if SERVER
	ai_record_tag(),
	ai_record_wait_timer(AI_RECORD_WAIT_TIME),
	rtt(),
#endif
	sudoku_active()
{
}

void PlayerControlHuman::awake()
{
#if SERVER
	rtt = Net::rtt(player.ref());
#endif

	if (player.ref()->local && !Game::level.local)
	{
		Transform* t = get<Transform>();
		remote_control.pos = t->pos;
		remote_control.rot = t->rot;
		remote_control.parent = t->parent;
	}

	link_arg<const HealthEvent&, &PlayerControlHuman::health_changed>(get<Health>()->changed);

	if (has<Drone>())
	{
		last_pos = get<Drone>()->center_lerped();
		link<&PlayerControlHuman::drone_detaching>(get<Drone>()->detaching);
		link<&PlayerControlHuman::drone_done_flying_or_dashing>(get<Drone>()->done_flying);
		link<&PlayerControlHuman::drone_done_flying_or_dashing>(get<Drone>()->done_dashing);
		link_arg<const DroneReflectEvent&, &PlayerControlHuman::drone_reflecting>(get<Drone>()->reflecting);
		link_arg<Entity*, &PlayerControlHuman::hit_target>(get<Drone>()->hit);

		if (!Team::game_over
			&& Game::level.has_feature(Game::FeatureLevel::All)
			&& player.ref()->get<PlayerManager>()->deaths == 0)
		{
			if (Game::level.type == GameType::Assault
				&& player.ref()->get<PlayerManager>()->team.ref()->team() == 0)
				player.ref()->msg(_(strings::defend), true);
			else
				player.ref()->msg(_(strings::attack), true);
		}
	}
	else
	{
		last_pos = get<Transform>()->absolute_pos();
		link_arg<r32, &PlayerControlHuman::parkour_landed>(get<Walker>()->land);
		link<&PlayerControlHuman::terminal_enter_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_terminal_enter, 2.5f));
		link<&PlayerControlHuman::interact_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_interact, 3.8f));
		link<&PlayerControlHuman::interact_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_terminal_exit, 4.0f));
		get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
		get<Audio>()->param(AK::GAME_PARAMETERS::FLY_VOLUME, 0.0f);
	}
}

PlayerControlHuman::~PlayerControlHuman()
{
	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

void PlayerControlHuman::health_changed(const HealthEvent& e)
{
	if (e.hp + e.shield < 0)
	{
		// de-scope when damaged
		try_secondary = false;
		camera_shake();
	}
}

void PlayerControlHuman::drone_reflecting(const DroneReflectEvent& e)
{
	// send message if we are a client in a network game.
	if (!Game::level.local)
	{
		PlayerControlHumanNet::Message msg;
		msg.pos = get<Transform>()->absolute_pos();
		msg.dir = Vec3::normalize(e.new_velocity);
		msg.entity = e.entity;
		msg.type = PlayerControlHumanNet::Message::Type::Reflect;
		PlayerControlHumanNet::send(this, &msg);
	}
}

void PlayerControlHuman::parkour_landed(r32 velocity_diff)
{
	Parkour::State parkour_state = get<Parkour>()->fsm.current;
	if (velocity_diff < LANDING_VELOCITY_LIGHT
		&& (parkour_state == Parkour::State::Normal || parkour_state == Parkour::State::HardLanding))
	{
		player.ref()->rumble_add(velocity_diff < LANDING_VELOCITY_HARD ? 0.5f : 0.2f);
	}
}

void PlayerControlHuman::terminal_enter_animation_callback()
{
	Game::level.terminal_interactable.ref()->get<Interactable>()->interact_no_animation();
}

void PlayerControlHuman::interact_animation_callback()
{
	interactable = nullptr;
}

void PlayerControlHuman::hit_target(Entity* target)
{
	player.ref()->rumble_add(0.5f);
}

void PlayerControlHuman::drone_detaching()
{
	camera_shake_timer = 0.0f; // stop screen shake

#if SERVER
	ai_record_tag.init(entity());

	player_collect_target_indicators(this);

	AI::Team my_team = get<AIAgent>()->team;
	Vec3 me = get<Transform>()->absolute_pos();
	Vec3 dir = Vec3::normalize(get<Drone>()->velocity);
	r32 closest_distance = DRONE_MAX_DISTANCE;
	r32 closest_dot = 0.8f;
	s8 closest_entity_type = AI::RecordedLife::EntityNone;
	r32 target_prediction_speed = get<Drone>()->target_prediction_speed();

	Net::StateFrame state_frame_data;
	Net::StateFrame* state_frame = nullptr;
	if (get<Drone>()->net_state_frame(&state_frame_data))
		state_frame = &state_frame_data;

	for (auto i = Entity::iterator(AI::entity_mask & ~Bolt::component_mask); !i.is_last(); i.next())
	{
		AI::Team team;
		s8 entity_type;
		AI::entity_info(i.item(), my_team, &team, &entity_type);
		if (team != my_team)
		{
			Vec3 pos;
			if (!i.item()->has<Target>() || !get<Drone>()->predict_intersection(i.item()->get<Target>(), state_frame, &pos, target_prediction_speed))
				pos = i.item()->get<Transform>()->absolute_pos();

			Vec3 to_target = pos - me;
			r32 distance = to_target.length();
			if (distance < closest_distance)
			{
				to_target /= distance;
				r32 dot = to_target.dot(dir);
				if (dot > closest_dot)
				{
					RaycastCallbackExcept ray_callback(me, pos, entity());
					Physics::raycast(&ray_callback, CollisionStatic);
					if (!ray_callback.hasHit())
					{
						closest_distance = distance;
						closest_dot = dot;
						closest_entity_type = entity_type;
					}
				}
			}
		}
	}

	// todo: add AI record
#endif
}

void PlayerControlHuman::camera_shake(r32 amount) // amount ranges from 0 to 1
{
	if (!has<Drone>() || get<Drone>()->state() == Drone::State::Crawl) // don't shake the screen if we reflect off something in the air
		camera_shake_timer = vi_max(camera_shake_timer, camera_shake_time * amount);
	player.ref()->rumble_add(amount);
}

b8 PlayerControlHuman::input_enabled() const
{
	PlayerHuman::UIMode ui_mode = player.ref()->ui_mode();
	if (has<Parkour>())
	{
		// disable input if we're playing an animation on layer 3
		// however we can still move while picking things up
		AssetID anim = get<Animator>()->layers[3].animation;
		if (anim != AssetNull && anim != Asset::Animation::character_pickup)
			return false;
	}

	return !Console::visible
		&& !Overworld::active()
		&& (ui_mode == PlayerHuman::UIMode::PvpDefault || ui_mode == PlayerHuman::UIMode::ParkourDefault)
		&& !Team::game_over
		&& !Menu::dialog_active(player.ref()->gamepad)
		&& !interactable.ref();
}

b8 PlayerControlHuman::movement_enabled() const
{
	return input_enabled() && get<PlayerCommon>()->movement_enabled();
}

r32 PlayerControlHuman::look_speed() const
{
	if (has<Drone>() && try_secondary)
		return get<Drone>()->current_ability == Ability::Sniper ? zoom_speed_multiplier_sniper : zoom_speed_multiplier;
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

		if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
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

		get<PlayerCommon>()->angle_vertical = LMath::clampf(get<PlayerCommon>()->angle_vertical, -DRONE_VERTICAL_ANGLE_LIMIT, DRONE_VERTICAL_ANGLE_LIMIT);
	}
}

Vec3 PlayerControlHuman::get_movement(const Update& u, const Quat& rot)
{
	Vec3 movement = Vec3::zero;
	if (movement_enabled())
	{
		s32 gamepad = player.ref()->gamepad;
		if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
		{
			Vec2 gamepad_movement(-u.input->gamepads[gamepad].left_x, -u.input->gamepads[gamepad].left_y);
			if (has<Drone>())
				Input::dead_zone(&gamepad_movement.x, &gamepad_movement.y);
			else
				Input::dead_zone_cross(&gamepad_movement.x, &gamepad_movement.y);
			movement.x = gamepad_movement.x;
			movement.z = gamepad_movement.y;
		}

		if (gamepad == 0)
		{
			if (u.input->get(Controls::Forward, 0))
				movement += Vec3(0, 0, 1);
			if (u.input->get(Controls::Backward, 0))
				movement += Vec3(0, 0, -1);
			if (u.input->get(Controls::Right, 0))
				movement += Vec3(-1, 0, 0);
			if (u.input->get(Controls::Left, 0))
				movement += Vec3(1, 0, 0);
		}

		r32 length_squared = movement.length_squared();
		if (length_squared > 1.0f)
			movement /= sqrtf(length_squared);

		movement = rot * movement;
	}
	return movement;
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

	if (has<Parkour>())
	{
		// remote control by a client
		// just trust the client, it's k
		Vec3 abs_pos_last = last_pos;
		get<Transform>()->pos = remote_control.pos;
		get<Transform>()->rot = remote_control.rot;
		get<Transform>()->parent = remote_control.parent;
		last_pos = get<Transform>()->absolute_pos();
		get<Walker>()->absolute_pos(last_pos); // force rigid body
		get<Target>()->net_velocity = get<Target>()->net_velocity * 0.7f + ((last_pos - abs_pos_last) / NET_TICK_RATE) * 0.3f;
	}
	else if (input_enabled())
	{
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
}

void PlayerControlHuman::camera_shake_update(const Update& u, Camera* camera)
{
	if (camera_shake_timer > 0.0f)
	{
		camera_shake_timer -= u.time.delta;
		if (!has<Drone>() || get<Drone>()->state() == Drone::State::Crawl)
		{
			r32 shake = (camera_shake_timer / camera_shake_time) * 0.3f;
			r32 offset = Game::time.total * 10.0f;
			camera->rot = camera->rot * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}
	}
}

void player_confirm_tram_interactable(s8 gamepad)
{
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		PlayerHuman* player = i.item()->player.ref();
		if (player->gamepad == gamepad)
		{
			vi_assert(Game::save.resources[s32(Resource::HackKits)] > 0);
			i.item()->interactable = Interactable::closest(i.item()->get<Transform>()->absolute_pos());
			if (i.item()->interactable.ref())
			{
				player->sudoku.reset();
				i.item()->sudoku_active = true;
			}
			break;
		}
	}
}

void player_confirm_skip_tutorial(s8 gamepad)
{
	Menu::dialog(gamepad, &player_confirm_tram_interactable, _(strings::confirm_spend), 1, _(strings::hack_kits));
}

void player_confirm_terminal_interactable(s8 gamepad)
{
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		PlayerHuman* player = i.item()->player.ref();
		if (player->gamepad == gamepad)
		{
			i.item()->get<Animator>()->layers[3].play(Asset::Animation::character_terminal_enter); // animation will eventually trigger the interactable
			break;
		}
	}
}

void player_cancel_interactable(s8 gamepad)
{
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		PlayerHuman* player = i.item()->player.ref();
		if (player->gamepad == gamepad)
		{
			i.item()->interactable = nullptr;
			break;
		}
	}
}

void PlayerControlHuman::update(const Update& u)
{
	s32 gamepad = player.ref()->gamepad;

	if (has<Drone>())
	{
		if (local())
		{
			if (!Game::level.local)
			{
				// we are a client and this is a local player
				if (position_history.length == 0 || Game::real_time.total > position_history[position_history.length - 1].timestamp + NET_TICK_RATE * 0.5f)
				{
					// save our position history
					if (position_history.length == 60)
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

				// get the position entry at this time in the history
				r32 timestamp = Game::real_time.total - Net::rtt(player.ref());
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
						const s32 radius = 4;
						for (s32 j = vi_max(0, i - radius); j < vi_min(s32(position_history.length), i + radius + 1); j++)
						{
							if (i != j)
							{
								tolerance_pos = vi_max(tolerance_pos, (position_history[i].pos - position_history[j].pos).length());
								tolerance_rot = vi_max(tolerance_rot, Quat::angle(position_history[i].rot, position_history[j].rot));
							}
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
						if (!t->parent.ref())
							get<Drone>()->velocity = t->rot * Vec3(0, 0, vi_max(DRONE_DASH_SPEED, get<Drone>()->velocity.length()));
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
						if (Settings::gamepads[gamepad].zoom_toggle)
						{
							try_secondary = !try_secondary;
							get<Audio>()->post_event(try_secondary ? AK::EVENTS::PLAY_ZOOM_IN : AK::EVENTS::PLAY_ZOOM_OUT);
						}
						else
						{
							try_secondary = true;
							get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
						}
					}
				}
				else if (!Settings::gamepads[gamepad].zoom_toggle && !zoom_pressed)
				{
					if (try_secondary)
						get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
					try_secondary = false;
				}

				r32 fov_target = try_secondary ? (get<Drone>()->current_ability == Ability::Sniper ? fov_sniper : fov_zoom) : fov_default;

				if (fov < fov_target)
					fov = vi_min(fov + zoom_speed * sinf(fov) * u.time.delta, fov_target);
				else if (fov > fov_target)
					fov = vi_max(fov - zoom_speed * sinf(fov) * u.time.delta, fov_target);
			}

			// update camera projection
			{
				r32 aspect = camera->viewport.size.y == 0 ? 1 : r32(camera->viewport.size.x) / r32(camera->viewport.size.y);
				camera->perspective(fov, aspect, 0.005f, Game::level.skybox.far_plane);
			}

			// collect target indicators
			player_collect_target_indicators(this);

			if (get<Transform>()->parent.ref())
			{
				r32 gamepad_rotation_multiplier = 1.0f;

				if (input_enabled() && u.input->gamepads[gamepad].type != Gamepad::Type::None)
				{
					// gamepad aim assist
					Vec3 to_reticle = reticle.pos - camera->pos;
					r32 reticle_distance = to_reticle.length();
					to_reticle /= reticle_distance;
					for (s32 i = 0; i < target_indicators.length; i++)
					{
						const TargetIndicator indicator = target_indicators[i];
						Vec3 to_indicator = indicator.pos - camera->pos;
						r32 indicator_distance = to_indicator.length();
						if (indicator_distance > DRONE_THIRD_PERSON_OFFSET && indicator_distance < reticle_distance + 2.5f)
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
										Vec3 me = get<Drone>()->center_lerped();
										Vec3 my_velocity = get<Drone>()->center_lerped() - last_pos;
										{
											r32 my_speed = my_velocity.length_squared();
											if (my_speed == 0.0f || my_speed > DRONE_CRAWL_SPEED * 1.5f * DRONE_CRAWL_SPEED * 1.5f) // don't adjust if we're going too fast or not moving
												break;
										}
										Vec3 me_predicted = me + my_velocity;

										if (indicator.velocity.length_squared() > DRONE_CRAWL_SPEED * 1.5f * DRONE_CRAWL_SPEED * 1.5f) // enemy moving too fast
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
										&& fabsf(get<PlayerCommon>()->angle_vertical) < PI * 0.4f) // only adjust if we're not looking straight up or down
										get<PlayerCommon>()->angle_horizontal = LMath::angle_range(get<PlayerCommon>()->angle_horizontal + adjustment.x);
									if (current_offset.y > 0 == adjustment.y > 0) // only adjust if it's an adjustment toward the target
										get<PlayerCommon>()->angle_vertical = LMath::angle_range(get<PlayerCommon>()->angle_vertical + adjustment.y);
								}

								break;
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
					get<Drone>()->crawl(movement, u);
				}

				last_pos = get<Drone>()->center_lerped();
			}
			else
				camera->rot = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

			{
				// abilities
				player_ability_update(u, this, Controls::Ability1, gamepad, 0);
				player_ability_update(u, this, Controls::Ability2, gamepad, 1);
				player_ability_update(u, this, Controls::Ability3, gamepad, 2);
			}

			camera_shake_update(u, camera);

			PlayerHuman::camera_setup_drone(entity(), camera, DRONE_THIRD_PERSON_OFFSET);

			// reticle
			{
				Vec3 trace_dir = camera->rot * Vec3(0, 0, 1);
				Vec3 me = get<Transform>()->absolute_pos();
				Vec3 trace_start = camera->pos + trace_dir * trace_dir.dot(me - camera->pos);

				reticle.type = ReticleType::None;

				if (movement_enabled())
				{
					Vec3 trace_end = trace_start + trace_dir * (DRONE_SNIPE_DISTANCE + DRONE_THIRD_PERSON_OFFSET);
					RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
					Physics::raycast(&ray_callback, ~CollisionDroneIgnore & ~CollisionAllTeamsForceField);

					Ability ability = get<Drone>()->current_ability;

					if (ray_callback.hasHit())
					{
						reticle.pos = ray_callback.m_hitPointWorld;
						reticle.normal = ray_callback.m_hitNormalWorld;
						Entity* hit_entity = &Entity::list[ray_callback.m_collisionObject->getUserIndex()];
						Vec3 detach_dir = reticle.pos - me;
						r32 distance = detach_dir.length();
						detach_dir /= distance;
						r32 dot_tolerance = distance < DRONE_DASH_DISTANCE ? 0.3f : 0.1f;
						if (ability == Ability::None) // normal movement
						{
							if (get<Drone>()->direction_is_toward_attached_wall(detach_dir))
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
								if (get<Drone>()->can_shoot(detach_dir, &hit, &hit_target))
								{
									if (hit_target)
										reticle.type = ReticleType::Target;
									else if ((reticle.pos - hit).length_squared() < DRONE_RADIUS * DRONE_RADIUS)
										reticle.type = ReticleType::Normal;
								}
								else
								{
									// when you're aiming at a target that is attached to the same surface you are,
									// sometimes the point you're aiming at is actually away from the wall,
									// so it registers as a shot rather than a dash.
									// and sometimes that shot can't actually be taken.
									// so we need to check for this case and turn it into a dash if we can.

									if (distance < DRONE_DASH_DISTANCE && hit_entity->has<Target>() && hit_entity->get<Transform>()->parent.ref())
									{
										Quat my_rot = get<Transform>()->absolute_rot();
										Vec3 target_pos;
										Quat target_rot;
										hit_entity->get<Transform>()->absolute(&target_pos, &target_rot);
										Vec3 my_normal = my_rot * Vec3(0, 0, 1);
										if (my_normal.dot(target_rot * Vec3(0, 0, 1)) > 1.0f - dot_tolerance
											&& fabsf(my_normal.dot(target_pos - me)) < dot_tolerance)
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
							if (get<Drone>()->can_spawn(ability, detach_dir, &hit, nullptr, nullptr, &hit_target))
							{
								if (AbilityInfo::list[s32(ability)].type == AbilityInfo::Type::Shoot)
								{
									reticle.type = ReticleType::Normal;
									if (hit_target)
										reticle.type = ReticleType::Target;
								}
								else if ((hit - Vec3(ray_callback.m_hitPointWorld)).length_squared() < DRONE_RADIUS * DRONE_RADIUS)
									reticle.type = ReticleType::Normal;
							}
						}
					}
					else
					{
						reticle.pos = trace_end;
						reticle.normal = -trace_dir;
						if (ability != Ability::None
							&& get<Drone>()->can_spawn(ability, trace_dir)) // spawning an ability
						{
							reticle.type = ReticleType::Normal;
						}
					}
				}
				else
				{
					reticle.pos = trace_start + trace_dir * DRONE_THIRD_PERSON_OFFSET;
					reticle.normal = -trace_dir;
				}
			}

			{
				b8 primary_pressed = u.input->get(Controls::Primary, gamepad);
				if (primary_pressed && !u.last_input->get(Controls::Primary, gamepad))
					try_primary = true;
				else if (!primary_pressed)
					try_primary = false;
			}

			if (reticle.type == ReticleType::None || !get<Drone>()->cooldown_can_shoot())
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
						msg.ability = get<Drone>()->current_ability;
						PlayerControlHumanNet::send(this, &msg);
					}
				}
			}
		}
		else if (Game::level.local)
		{
			// we are a server, but this Drone is being controlled by a client
#if SERVER
			// if we are crawling, update the RTT every frame
			// if we're dashing or flying, the RTT is set based on the sequence number of the command we received
			if (get<Drone>()->state() == Drone::State::Crawl)
				rtt = Net::rtt(player.ref());

			ai_record_wait_timer -= u.time.delta;
			if (ai_record_wait_timer < 0.0f)
			{
				ai_record_wait_timer += AI_RECORD_WAIT_TIME;
				if (!get<Health>()->invincible())
				{
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeWait;
					player.ref()->ai_record.add(ai_record_tag, action);
				}
				ai_record_tag.init(entity());
			}

			get<Drone>()->crawl(remote_control.movement, u);
			last_pos = get<Drone>()->center_lerped();
#else
			vi_assert(false);
#endif
		}
		else
		{
			// we are a client and this Drone is not local
			// do nothing
		}
	}
	else
	{
		// parkour mode

		if (local())
		{
			// start interaction
			if (input_enabled() && !u.input->get(Controls::InteractSecondary, gamepad) && u.last_input->get(Controls::InteractSecondary, gamepad))
			{
				interactable = Interactable::closest(get<Transform>()->absolute_pos());
				if (interactable.ref())
				{
					switch (interactable.ref()->type)
					{
						case Interactable::Type::Terminal:
						{
							switch (Game::save.zones[Game::level.id])
							{
								case ZoneState::Hostile:
								{
									if (Game::level.post_pvp || platform::timestamp() - Game::save.zone_lost_times[Game::level.id] < ZONE_LOST_COOLDOWN)
									{
										// terminal is temporarily locked, must leave and come back
										player.ref()->msg(_(strings::terminal_locked), false);
										interactable = nullptr;
									}
									else if (Game::level.max_teams <= 2 || Game::save.group != Net::Master::Group::None) // if the map requires more than two players, you must be in a group
									{
										if (Game::save.resources[s32(Resource::Drones)] < DEFAULT_RUSH_DRONES)
										{
											Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_resource), DEFAULT_RUSH_DRONES, _(strings::drones));
											interactable = nullptr;
										}
										else if (Game::save.resources[s32(Resource::HackKits)] < 1)
										{
											Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_resource), 1, _(strings::hack_kits));
											interactable = nullptr;
										}
										else
											Menu::dialog_with_cancel(gamepad, &player_confirm_terminal_interactable, &player_cancel_interactable, _(strings::confirm_capture), DEFAULT_RUSH_DRONES, 1);
									}
									else
									{
										// must be in a group
										player.ref()->msg(_(strings::group_required), false);
										interactable = nullptr;
									}
									break;
								}
								case ZoneState::Locked:
								{
									interactable.ref()->interact();
									get<Animator>()->layers[3].play(Asset::Animation::character_interact);
									break;
								}
								case ZoneState::Friendly:
								case ZoneState::GroupOwned:
								{
									// zone is already owned
									player.ref()->msg(_(strings::zone_already_captured), false);
									interactable = nullptr;
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
						case Interactable::Type::Tram: // tram interactable
						{
							s8 track = s8(interactable.ref()->user_data);
							AssetID target_level = Game::level.tram_tracks[track].level;
							Tram* tram = Tram::by_track(track);
							if (tram->doors_open() || Game::save.zones[target_level] == ZoneState::Friendly || Game::save.zones[target_level] == ZoneState::Hostile)
							{
								// go right ahead
								interactable.ref()->interact();
								get<Animator>()->layers[3].play(Asset::Animation::character_interact);
							}
							else if (Game::save.zones[Game::level.id] == ZoneState::Locked
								&& (Game::save.zones[target_level] == ZoneState::Locked || Game::save.zones[target_level] == ZoneState::GroupOwned))
							{
								// unlock terminal first
								player.ref()->msg(_(strings::error_locked_zone), false);
								interactable = nullptr;
							}
							else
							{
								if (Game::save.zones[target_level] == ZoneState::Locked)
								{
									if (Game::save.resources[s32(Resource::HackKits)] > 0) // zone is unlocked, but need to hack this tram first
									{
										if (Game::level.id == Asset::Level::Port_District && s32(Game::level.feature_level) < s32(Game::FeatureLevel::TutorialAll))
										{
											// player is about to skip tutorial
											Menu::dialog(gamepad, &player_confirm_skip_tutorial, _(strings::confirm_skip_tutorial));
											interactable = nullptr;
										}
										else
										{
											Menu::dialog(gamepad, &player_confirm_tram_interactable, _(strings::confirm_spend), 1, _(strings::hack_kits));
											interactable = nullptr;
										}
									}
									else // not enough
									{
										Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_resource), 1, _(strings::hack_kits));
										interactable = nullptr;
									}
								}
								else // no need to hack, just go
								{
									interactable.ref()->interact();
									get<Animator>()->layers[3].play(Asset::Animation::character_interact);
								}
							}
							break;
						}
						case Interactable::Type::Shop:
						{
							Overworld::show(player.ref()->camera, Overworld::State::StoryModeOverlay, Overworld::Tab::Inventory);
							interactable = nullptr;
							break;
						}
						default:
						{
							vi_assert(false); // invalid interactable type
							break;
						}
					}
				}
			}

			if (interactable.ref())
			{
				// position player directly in front of the interactable

				// desired rotation / position
				Vec3 target_pos;
				r32 target_angle;
				get_interactable_standing_position(interactable.ref()->get<Transform>(), &target_pos, &target_angle);

				r32 angle = fabsf(LMath::angle_to(get<PlayerCommon>()->angle_horizontal, target_angle));
				get<PlayerCommon>()->angle_horizontal = LMath::lerpf(vi_min(1.0f, (INTERACT_LERP_ROTATION_SPEED / angle) * u.time.delta), get<PlayerCommon>()->angle_horizontal, LMath::closest_angle(target_angle, get<PlayerCommon>()->angle_horizontal));
				get<PlayerCommon>()->angle_vertical = LMath::lerpf(vi_min(1.0f, (INTERACT_LERP_ROTATION_SPEED / fabsf(get<PlayerCommon>()->angle_vertical)) * u.time.delta), get<PlayerCommon>()->angle_vertical, -arm_angle_offset);

				Vec3 abs_pos = get<Transform>()->absolute_pos();
				r32 distance = (abs_pos - target_pos).length();
				if (distance > 0.0f)
					get<Walker>()->absolute_pos(Vec3::lerp(vi_min(1.0f, (INTERACT_LERP_TRANSLATION_SPEED / distance) * u.time.delta), abs_pos, target_pos));

				if (sudoku_active)
				{
					player.ref()->sudoku.update(u, gamepad, player.ref());
					if (player.ref()->sudoku.complete() && player.ref()->sudoku.timer_animation == 0.0f)
					{
						interactable.ref()->interact();
						get<Animator>()->layers[3].play(Asset::Animation::character_interact);
						sudoku_active = false;
					}
				}
			}

			update_camera_input(u);

			if (get<Parkour>()->fsm.current == Parkour::State::Climb
				&& input_enabled()
				&& u.input->get(Controls::Parkour, gamepad))
			{
				Vec3 movement = get_movement(u, Quat::identity);
				get<Parkour>()->climb_velocity = movement.z;
			}
			else
				get<Parkour>()->climb_velocity = 0.0f;

			if (input_enabled() && u.last_input->get(Controls::Scoreboard, gamepad) && !u.input->get(Controls::Scoreboard, gamepad))
			{
				if (Tram::player_inside(entity()))
					player.ref()->msg(_(strings::error_inside_tram), false);
				else
					Overworld::show(player.ref()->camera, Overworld::State::StoryMode);
			}
		
			// set movement unless we're climbing up and down
			if (!(get<Parkour>()->fsm.current == Parkour::State::Climb && u.input->get(Controls::Parkour, gamepad)))
			{
				Vec3 movement = get_movement(u, Quat::euler(0, get<PlayerCommon>()->angle_horizontal, 0));
				Vec2 dir = Vec2(movement.x, movement.z);
				get<Walker>()->dir = dir;
			}

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

				r32 arm_angle = LMath::clampf(get<PlayerCommon>()->angle_vertical * 0.75f + arm_angle_offset, -PI * 0.2f, PI * 0.25f);

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

			if (parkour_state == Parkour::State::WallRun)
			{
				Parkour::WallRunState state = get<Parkour>()->wall_run_state;

				Vec3 wall_normal = get<Parkour>()->last_support.ref()->get<Transform>()->to_world_normal(get<Parkour>()->relative_wall_run_normal);

				Vec3 forward = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Vec3(0, 0, 1);

				if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Forward)
					get<PlayerCommon>()->clamp_rotation(-wall_normal); // Make sure we're always facing the wall
				else
				{
					// We're running along the wall
					// Make sure we can't look backward
					get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1));
					if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Left)
						get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation + PI * -0.5f, 0) * Vec3(0, 0, 1));
					else
						get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0) * Vec3(0, 0, 1));
				}
			}
			else if (parkour_state == Parkour::State::Slide
				|| parkour_state == Parkour::State::Roll
				|| parkour_state == Parkour::State::HardLanding
				|| parkour_state == Parkour::State::Mantle
				|| parkour_state == Parkour::State::Climb)
			{
				get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1));
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
		}
	}
}

void PlayerControlHuman::update_late(const Update& u)
{
	if (has<Parkour>()
		&& !Overworld::active()
		&& local())
	{
		Camera* camera = player.ref()->camera;

		{
			r32 aspect = camera->viewport.size.y == 0 ? 1 : camera->viewport.size.x / camera->viewport.size.y;
			camera->perspective(fov_default, aspect, 0.02f, Game::level.skybox.far_plane);
			camera->clip_planes[0] = Plane();
			camera->cull_range = 0.0f;
			camera->flag(CameraFlagCullBehindWall, false);
			camera->flag(CameraFlagColors, true);
			camera->flag(CameraFlagFog, true);
			camera->range = 0.0f;
		}

		/*
		// todo: third-person
		if (Game::state.third_person)
			camera_pos = get<Transform>()->absolute_pos() + look_quat * Vec3(0, 1, -7);
		else
		*/
		{
			camera->pos = Vec3(0, 0, 0.1f);
			Quat q = Quat::identity;
			get<Parkour>()->head_to_object_space(&camera->pos, &q);
			camera->pos = get<Transform>()->to_world(camera->pos);

			// camera bone affects rotation only
			Quat camera_animation = Quat::euler(PI * -0.5f, 0, 0);
			get<Animator>()->bone_transform(Asset::Bone::character_camera, nullptr, &camera_animation);
			camera->rot = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Quat::euler(0, PI * 0.5f, 0) * camera_animation * Quat::euler(0, PI * -0.5f, 0);
		}

		// wind sound and camera shake at high speed
		{
			r32 speed = get<Parkour>()->fsm.current == Parkour::State::Mantle || get<Walker>()->support.ref()
				? 0.0f
				: get<RigidBody>()->btBody->getInterpolationLinearVelocity().length();
			get<Audio>()->param(AK::GAME_PARAMETERS::FLY_VOLUME, LMath::clampf((speed - 8.0f) / 25.0f, 0, 1));
			r32 shake = LMath::clampf((speed - 13.0f) / 30.0f, 0, 1);
			player.ref()->rumble_add(shake);
			shake *= 0.2f;
			r32 offset = Game::time.total * 10.0f;
			camera->rot = camera->rot * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
		}

		camera_shake_update(u, camera);
	}
}

void PlayerControlHuman::draw_ui(const RenderParams& params) const
{
	if (params.technique != RenderTechnique::Default
		|| params.camera != player.ref()->camera
		|| Overworld::active()
		|| Team::game_over)
		return;

	const Rect2& viewport = params.camera->viewport;

	r32 range = has<Drone>() ? get<Drone>()->range() : DRONE_MAX_DISTANCE;

	AI::Team team = get<AIAgent>()->team;

	// target indicators
	for (s32 i = 0; i < target_indicators.length; i++)
	{
		const TargetIndicator& indicator = target_indicators[i];
		switch (indicator.type)
		{
			case TargetIndicator::Type::DroneVisible:
			{
				UI::indicator(params, indicator.pos, UI::color_alert, false);
				break;
			}
			case TargetIndicator::Type::DroneTracking:
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
			case TargetIndicator::Type::Sensor:
			case TargetIndicator::Type::Rocket:
			case TargetIndicator::Type::ForceField:
			case TargetIndicator::Type::Grenade:
			case TargetIndicator::Type::Turret:
			{
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}

	b8 enemy_visible = false;
	b8 enemy_dangerous_visible = false; // an especially dangerous enemy is visible

	{
		Vec3 me = get<Transform>()->absolute_pos();

		// turret health bars
		for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 turret_pos = i.item()->get<Transform>()->absolute_pos();
			if ((turret_pos - me).length_squared() < range * range)
			{
				Vec2 p;
				if (UI::is_onscreen(params, turret_pos, &p) || i.item()->team != team)
				{
					Vec2 bar_size(40.0f * UI::scale, 8.0f * UI::scale);
					Rect2 bar = { p + Vec2(0, 40.0f * UI::scale) + (bar_size * -0.5f), bar_size };
					UI::box(params, bar, UI::color_background);
					const Vec4& color = Team::ui_color(team, i.item()->team);
					UI::border(params, bar, 2, color);
					Health* health = i.item()->get<Health>();
					UI::box(params, { bar.pos, Vec2(bar.size.x * (r32(health->hp) / r32(health->hp_max)), bar.size.y) }, color);
				}

				if (i.item()->target.ref() == entity())
				{
					if (UI::flash_function(Game::time.total))
						UI::indicator(params, turret_pos, Team::ui_color_enemy, true);
					enemy_dangerous_visible = true;
				}
			}
		}

		// force field battery bars
		for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
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
					UI::box(params, { bar.pos, Vec2(bar.size.x * (i.item()->remaining_lifetime / FORCE_FIELD_LIFETIME), bar.size.y) }, color);
				}
			}
		}

		// highlight enemy rockets
		for (auto i = Rocket::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->target.ref() == entity())
			{
				enemy_visible = true;

				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				UI::indicator(params, pos, Team::ui_color_enemy, true);

				UIText text;
				text.color = Team::ui_color_enemy;
				text.text(player.ref()->gamepad, _(strings::rocket_incoming));
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

		// highlight enemy grenades in-air
		for (auto i = Grenade::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->team() != team
				&& !i.item()->get<Transform>()->parent.ref())
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				if ((me - pos).length_squared() < DRONE_MAX_DISTANCE * DRONE_MAX_DISTANCE)
				{
					enemy_visible = true;

					UI::indicator(params, pos, Team::ui_color_enemy, true);

					UIText text;
					text.color = Team::ui_color(team, i.item()->team());
					text.text(player.ref()->gamepad, _(strings::grenade_incoming));
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

		// highlight incoming bolts
		for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();
			Vec3 diff = me - pos;
			r32 distance = diff.length();
			if (distance < DRONE_MAX_DISTANCE
				&& (diff / distance).dot(Vec3::normalize(i.item()->velocity)) > 0.7f)
			{
				if (UI::flash_function(Game::real_time.total))
					UI::indicator(params, pos, Team::ui_color_enemy, true);
			}
		}
	}

	if (has<Drone>())
	{
		PlayerManager* manager = player.ref()->get<PlayerManager>();

		// highlight upgrade point if there is an upgrade available
		if (Game::level.has_feature(Game::FeatureLevel::Abilities)
			&& (Game::level.has_feature(Game::FeatureLevel::All) || Game::level.feature_level == Game::FeatureLevel::Abilities) // disable prompt in tutorial after ability has been purchased
			&& manager->upgrade_available() && manager->upgrade_highest_owned_or_available() != player.ref()->upgrade_last_visit_highest_available
			&& !manager->at_spawn_point())
		{
			Vec3 pos = SpawnPoint::closest(1 << s32(get<AIAgent>()->team), get<Transform>()->absolute_pos())->get<Transform>()->absolute_pos();
			UI::indicator(params, pos, Team::ui_color_friend, true);

			UIText text;
			text.color = Team::ui_color_friend;
			text.text(player.ref()->gamepad, _(strings::upgrade_notification));
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
	}
	else
	{
		// parkour mode

		if (sudoku_active) // sudoku
			player.ref()->sudoku.draw(params, player.ref()->gamepad);
		else
		{
			Interactable* closest_interactable = Interactable::closest(get<Transform>()->absolute_pos());

			if (closest_interactable || get<Animator>()->layers[3].animation == Asset::Animation::character_pickup)
			{
				// draw resources
				const Vec2 panel_size(MENU_ITEM_WIDTH * 0.3f, MENU_ITEM_PADDING * 2.0f + UI_TEXT_SIZE_DEFAULT * UI::scale);
				Vec2 pos(viewport.size.x * 0.9f, viewport.size.y * 0.1f);
				UIText text;
				text.anchor_y = UIText::Anchor::Center;
				text.anchor_x = UIText::Anchor::Max;
				text.size = UI_TEXT_SIZE_DEFAULT;
				for (s32 i = s32(Resource::count) - 1; i >= 0; i--)
				{
					UI::box(params, { pos + Vec2(-panel_size.x, 0), panel_size }, UI::color_background);

					r32 icon_size = (UI_TEXT_SIZE_DEFAULT + 2.0f) * UI::scale;

					const Overworld::ResourceInfo& info = Overworld::resource_info[i];

					b8 blink = Game::real_time.total - Overworld::resource_change_time(Resource(i)) < 0.5f;
					b8 draw = !blink || UI::flash_function(Game::real_time.total);

					if (draw)
					{
						const Vec4& color = blink
							?  UI::color_default
							: (Game::save.resources[i] == 0 ? UI::color_alert : UI::color_accent);
						UI::mesh(params, info.icon, pos + Vec2(-panel_size.x + MENU_ITEM_PADDING + icon_size * 0.5f, panel_size.y * 0.5f), Vec2(icon_size), color);
						text.color = color;
						text.text(player.ref()->gamepad, "%d", Game::save.resources[i]);
						text.draw(params, pos + Vec2(-MENU_ITEM_PADDING, panel_size.y * 0.5f));
					}

					pos.y += panel_size.y;
				}
			}

			if (input_enabled())
			{
				// interact prompt
				if (closest_interactable)
				{
					UIText text;
					text.color = UI::color_accent;
					text.text(player.ref()->gamepad, _(strings::prompt_interact));
					text.anchor_x = UIText::Anchor::Center;
					text.anchor_y = UIText::Anchor::Center;
					text.size = text_size;
					Vec2 pos = viewport.size * Vec2(0.5f, 0.15f);
					UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
					text.draw(params, pos);
				}

				if (Settings::waypoints)
				{
					// highlight terminal location
					if (!closest_interactable && Game::save.zones[Game::level.id] == ZoneState::Locked)
					{
						Entity* terminal = Game::level.terminal.ref();
						if (terminal)
							UI::indicator(params, terminal->get<Transform>()->absolute_pos(), UI::color_default, true);
					}

					// highlight trams
					Vec3 look_dir = params.camera->rot * Vec3(0, 0, 1);
					for (auto i = Tram::list.iterator(); !i.is_last(); i.next())
					{
						Vec3 pos = i.item()->get<Transform>()->absolute_pos();
						Vec3 to_tram = pos - params.camera->pos;
						r32 distance = to_tram.length();
						if (distance > 8.0f)
						{
							to_tram /= distance;
							if (to_tram.dot(look_dir) > 0.92f)
							{
								Vec2 p;
								if (UI::project(params, pos + Vec3(0, 3, 0), &p))
								{
									AssetID zone = Game::level.tram_tracks[i.item()->track()].level;

									if (zone == AssetNull)
										continue;

									UIText text;
									switch (Game::save.zones[zone])
									{
										case ZoneState::Friendly:
										{
											text.color = Team::ui_color_friend;
											break;
										}
										case ZoneState::GroupOwned:
										{
											text.color = UI::color_default;
											break;
										}
										case ZoneState::Hostile:
										{
											text.color = Team::ui_color_enemy;
											break;
										}
										case ZoneState::Locked:
										{
											text.color = UI::color_disabled;
											break;
										}
										default:
										{
											vi_assert(false);
											break;
										}
									}
									text.text(player.ref()->gamepad, Loader::level_name(zone));
									text.anchor_x = UIText::Anchor::Center;
									text.anchor_y = UIText::Anchor::Center;
									text.size = text_size * 0.75f;
									UI::box(params, text.rect(p).outset(4.0f * UI::scale), UI::color_background);
									text.draw(params, p);
								}
							}
						}
					}

					if (get<Parkour>()->fsm.current == Parkour::State::Climb)
					{
						// show climb controls
						UIText text;
						text.color = UI::color_accent;
						text.text(player.ref()->gamepad, "{{ClimbingMovement}}");
						text.anchor_x = UIText::Anchor::Center;
						text.anchor_y = UIText::Anchor::Center;
						Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.1f);
						UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
						text.draw(params, pos);
					}
				}
			}
		}
	}

	// common UI for both parkour and PvP modes

	// usernames directly over players' 3D positions
	for (auto other_player = PlayerCommon::list.iterator(); !other_player.is_last(); other_player.next())
	{
		if (other_player.item() != get<PlayerCommon>())
		{
			b8 tracking;
			b8 visible;
			Entity* detected_entity = player_determine_visibility(get<PlayerCommon>(), other_player.item(), &visible, &tracking);

			b8 friendly = other_player.item()->get<AIAgent>()->team == team;

			if (visible && !friendly)
			{
				enemy_visible = true;
				enemy_dangerous_visible = true;
			}

			b8 draw;
			Vec2 p;
			const Vec4* color;

			if (friendly)
			{
				Vec3 pos3d = detected_entity->get<Transform>()->absolute_pos() + Vec3(0, DRONE_RADIUS * 2.0f, 0);
				draw = UI::project(params, pos3d, &p);
				color = &Team::ui_color_friend;
			}
			else
			{
				// highlight the username and draw it even if it's offscreen
				if (detected_entity)
				{
					Vec3 pos3d = detected_entity->get<Transform>()->absolute_pos() + Vec3(0, DRONE_RADIUS * 2.0f, 0);

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
				username.text_raw(player.ref()->gamepad, other_player.item()->manager.ref()->username);

				UI::box(params, username.rect(username_pos).outset(HP_BOX_SPACING), UI::color_background);

				username.draw(params, username_pos);
			}
		}
	}

	const Health* health = get<Health>();

	b8 is_vulnerable = !get<AIAgent>()->stealth && !get<Health>()->invincible() && health->hp == 1 && health->shield == 0;

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
		else if (enemy_visible && !get<AIAgent>()->stealth)
			UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_alert);
	}

	{
		// danger indicator

		b8 danger = enemy_visible && (enemy_dangerous_visible || is_vulnerable) && !get<AIAgent>()->stealth;

		if (danger)
		{
			UIText text;
			text.size = 24.0f;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;

			text.text(player.ref()->gamepad, _(strings::danger));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.2f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (is_vulnerable ? UI::flash_function(Game::real_time.total) : UI::flash_function_slow(Game::real_time.total))
				text.draw(params, pos);
		}

		// shield indicator
		if (is_vulnerable)
		{
			UIText text;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.text(player.ref()->gamepad, _(strings::shield_down));

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
		text.text(player.ref()->gamepad, _(strings::stealth));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.7f);
		UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
		text.draw(params, pos);
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
			text.text(player.ref()->gamepad, _(strings::enemy_tracking));
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
	if (has<Drone>() && movement_enabled())
	{
		Vec2 pos = viewport.size * Vec2(0.5f, 0.5f);
		const r32 spoke_length = 10.0f;
		const r32 spoke_width = 3.0f;
		const r32 start_radius = 8.0f + spoke_length * 0.5f;

		b8 cooldown_can_go = get<Drone>()->cooldown_can_shoot();

		const Vec4* color;
		if (reticle.type == ReticleType::Error || reticle.type == ReticleType::DashError)
			color = &UI::color_disabled;
		else if (reticle.type != ReticleType::None
			&& cooldown_can_go
			&& (get<Drone>()->current_ability == Ability::None || player.ref()->get<PlayerManager>()->ability_valid(get<Drone>()->current_ability)))
			color = &UI::color_accent;
		else
			color = &UI::color_alert;

		// cooldown indicator
		{
			s32 charges = get<Drone>()->charges;
			if (charges == 0)
				UI::triangle_border(params, { pos, Vec2((start_radius + spoke_length) * (2.5f + 5.0f * (get<Drone>()->cooldown / DRONE_COOLDOWN)) * UI::scale) }, spoke_width, UI::color_alert, PI);
			else if (get<Drone>()->current_ability == Ability::None)
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

			if (get<Drone>()->current_ability != Ability::None)
			{
				Ability a = get<Drone>()->current_ability;
				Vec2 p = pos + Vec2(0, -48.0f * UI::scale);
				UI::centered_box(params, { p, Vec2(34.0f * UI::scale) }, UI::color_background);
				UI::mesh(params, AbilityInfo::list[s32(a)].icon, p, Vec2(18.0f * UI::scale), *color);

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
				text.text(player.ref()->gamepad, _(strings::prompt_cancel_ability), Settings::gamepads[player.ref()->gamepad].bindings[s32(binding)].string(Game::ui_gamepad_types[player.ref()->gamepad]));
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
}


}