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


#define DEBUG_NET_SYNC 0

#define fov_map_view (60.0f * PI * 0.5f / 180.0f)
#define fov_default (70.0f * PI * 0.5f / 180.0f)
#define fov_zoom (fov_default * 0.5f)
#define fov_sniper (fov_default * 0.25f)
#define zoom_speed_multiplier 0.25f
#define zoom_speed_multiplier_sniper 0.15f
#define zoom_speed (1.0f / 0.15f)
#define speed_mouse (0.05f / 60.0f)
#define speed_joystick 5.0f
#define gamepad_rotation_acceleration (1.0f / 0.4f)
#define attach_lerp_speed 30.0f
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
	Quat lerped_rot = e->get<Drone>()->lerped_rotation;
	Vec3 lerped_pos = e->get<Drone>()->center_lerped();
	Quat abs_rot;
	Vec3 abs_pos;
	e->get<Transform>()->absolute(&abs_pos, &abs_rot);

	Vec3 abs_offset = camera->rot * Vec3(0, 0, -offset);
	camera->pos = lerped_pos + abs_offset;
	Vec3 camera_pos_final = abs_pos + abs_offset;
	Vec3 abs_wall_normal;

	b8 attached = e->get<Transform>()->parent.ref();
	if (attached)
	{
		abs_wall_normal = abs_rot * Vec3(0, 0, 1);
		camera->pos += lerped_rot * Vec3(0, 0, 0.5f);
		camera_pos_final += abs_wall_normal * 0.5f;
	}
	else
		abs_wall_normal = camera->rot * Vec3(0, 0, 1);

	Quat rot_inverse = camera->rot.inverse();

	camera->range_center = rot_inverse * (abs_pos - camera->pos);
	camera->range = e->get<Drone>()->range();
	camera->flag(CameraFlagColors, false);
	camera->flag(CameraFlagFog, false);

	Vec3 wall_normal_viewspace = rot_inverse * abs_wall_normal;
	camera->clip_planes[0].redefine(wall_normal_viewspace, camera->range_center + wall_normal_viewspace * -DRONE_RADIUS);
	camera->flag(CameraFlagCullBehindWall, abs_wall_normal.dot(camera_pos_final - abs_pos) < -DRONE_RADIUS + 0.02f); // camera is behind wall; set clip plane to wall
	camera->cull_range = camera->range_center.length();

	if (attached)
		camera->cull_center = Vec3(0, 0, offset);
	else
	{
		// blend cull radius down to zero as we fly away from the wall
		r32 t = Game::time.total - e->get<Drone>()->attach_time;
		const r32 blend_time = 0.1f;
		if (t < blend_time)
		{
			r32 blend = 1.0f - (t / blend_time);
			camera->cull_range *= blend;
			camera->cull_center = Vec3(0, 0, offset);
		}
		else
		{
			camera->cull_range = 0.0f;
			camera->flag(CameraFlagCullBehindWall, false);
		}
	}
}

b8 PlayerHuman::players_on_same_client(const Entity* a, const Entity* b)
{
#if SERVER
	return a->has<PlayerControlHuman>()
		&& b->has<PlayerControlHuman>()
		&& Net::Server::client_id(a->get<PlayerControlHuman>()->player.ref()) == Net::Server::client_id(b->get<PlayerControlHuman>()->player.ref());
#else
	return true;
#endif
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
		if (i.item()->camera.ref() == camera)
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

Vec2 PlayerHuman::camera_topdown_movement(const Update& u, s8 gamepad, Camera* camera)
{
	Vec2 movement(0, 0);

	b8 keyboard = false;

	// buttons/keys
	{
		if ((u.input->get(Controls::Left, gamepad) && !u.last_input->get(Controls::Left, gamepad))
			|| (u.input->get(Controls::Right, gamepad) && !u.last_input->get(Controls::Right, gamepad))
			|| (u.input->get(Controls::Forward, gamepad) && !u.last_input->get(Controls::Forward, gamepad))
			|| (u.input->get(Controls::Backward, gamepad) && !u.last_input->get(Controls::Backward, gamepad)))
		{
			keyboard = true;
			if (u.input->get(Controls::Left, gamepad))
				movement.x -= 1.0f;
			if (u.input->get(Controls::Right, gamepad))
				movement.x += 1.0f;
			if (u.input->get(Controls::Forward, gamepad))
				movement.y -= 1.0f;
			if (u.input->get(Controls::Backward, gamepad))
				movement.y += 1.0f;
		}
	}

	// joysticks
	{
		Vec2 last_joystick(u.last_input->gamepads[gamepad].left_x, u.last_input->gamepads[gamepad].left_y);
		Input::dead_zone(&last_joystick.x, &last_joystick.y, UI_JOYSTICK_DEAD_ZONE);
		Vec2 current_joystick(u.input->gamepads[gamepad].left_x, u.input->gamepads[gamepad].left_y);
		Input::dead_zone(&current_joystick.x, &current_joystick.y, UI_JOYSTICK_DEAD_ZONE);

		if (last_joystick.length_squared() == 0.0f
			&& current_joystick.length_squared() > 0.0f)
			movement += current_joystick;
	}

	r32 movement_amount = movement.length();
	if (movement_amount > 0.0f)
	{
		// transitioning from one zone to another
		movement /= movement_amount; // normalize
		Vec3 movement3d = camera->rot * Vec3(-movement.x, -movement.y, 0);

		// raycast against the +y plane
		Vec3 ray = camera->rot * Vec3(0, 0, 1);
		r32 d = -movement3d.y / ray.y;
		movement3d += ray * d;

		movement = Vec2(movement3d.x, movement3d.z);
		movement.normalize();
		movement *= movement_amount;
	}

	return movement;
}

PlayerHuman::PlayerHuman(b8 local, s8 g)
	: gamepad(g),
	camera(),
	msg_text(),
	msg_timer(0.0f),
	menu(),
	angle_horizontal(),
	angle_vertical(),
	menu_state(),
	kill_cam_rot(),
	rumble(),
	animation_time(),
	upgrade_last_visit_highest_available(Upgrade::None),
	score_summary_scroll(),
	spectate_index(),
	selected_spawn(),
	killed_by(),
	select_spawn_timer(),
	upgrade_menu_open(),
	last_supported(),
	energy_notification_accumulator(),
#if SERVER
	ai_record_id(),
#endif
	local(local)
{
	if (local)
		uuid = Game::session.local_player_uuids[gamepad];
}

void PlayerHuman::awake()
{
	get<PlayerManager>()->spawn.link<PlayerHuman, const SpawnPosition&, &PlayerHuman::spawn>(this);
	get<PlayerManager>()->upgrade_completed.link<PlayerHuman, Upgrade, &PlayerHuman::upgrade_completed>(this);

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
		camera.ref()->team = s8(get<PlayerManager>()->team.ref()->team());
		camera.ref()->mask = 1 << camera.ref()->team;
		camera.ref()->flag(CameraFlagColors, false);

		Quat rot;
		Game::level.map_view.ref()->absolute(&camera.ref()->pos, &rot);
		camera.ref()->rot = kill_cam_rot = Quat::look(rot * Vec3(0, -1, 0));
	}

#if SERVER
	ai_record_id = AI::record_init(Game::level.team_lookup_reverse(get<PlayerManager>()->team.ref()->team()), get<PlayerManager>()->respawns);
#endif
}

PlayerHuman::~PlayerHuman()
{
	if (camera.ref())
	{
		camera.ref()->remove();
		camera = nullptr;
		Audio::listener_disable(gamepad);
	}
#if SERVER
	AI::record_close(ai_record_id);
	ai_record_id = 0;
#endif
}

void PlayerHuman::rumble_add(r32 r)
{
	rumble = vi_max(rumble, r);
}

PlayerHuman::UIMode PlayerHuman::ui_mode() const
{
	if (menu_state != Menu::State::Hidden)
		return UIMode::Pause;
	else if (Team::match_state == Team::MatchState::Done)
		return UIMode::PvpGameOver;
	else
	{
		Entity* entity = get<PlayerManager>()->instance.ref();
		if (entity)
		{
			if (entity->has<Drone>())
			{
				UpgradeStation* station = UpgradeStation::drone_inside(entity->get<Drone>());
				if (station && station->mode != UpgradeStation::Mode::Deactivating)
					return UIMode::PvpUpgrading;
				else
					return UIMode::PvpDefault;
			}
			else
				return UIMode::ParkourDefault;
		}
		else
			return UIMode::Dead;
	}
}

void PlayerHuman::msg(const char* msg, b8 good)
{
	msg_text.text(gamepad, msg);
	msg_text.color = good ? UI::color_accent() : UI::color_alert();
	msg_timer = msg_time;
	msg_good = good;
}

void PlayerHuman::energy_notify(s32 change)
{
	if (msg_timer == 0.0f)
		energy_notification_accumulator = 0;
	energy_notification_accumulator += s16(change);

	{
		char buffer[512];
		sprintf(buffer, _(strings::energy_added), energy_notification_accumulator);
		msg(buffer, true);
	}
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
		angle_horizontal -= r32(u.input->cursor_x) * s;
		angle_vertical += r32(u.input->cursor_y) * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
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

	camera.ref()->rot = Quat::euler(0, angle_horizontal, angle_vertical);
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
	enum class Type : s8
	{
		Dash,
		DashCombo,
		Go,
		Reflect,
		UpgradeStart,
		AbilitySelect,
		count,
	};

	Vec3 pos;
	Quat rot;
	Vec3 dir;
	Vec3 target;
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
		|| msg->type == Message::Type::DashCombo
		|| msg->type == Message::Type::Go
		|| msg->type == Message::Type::Reflect)
	{
		serialize_position(p, &msg->pos, Net::Resolution::High);
		if (!serialize_quat(p, &msg->rot, Net::Resolution::High))
			net_error();
		serialize_r32_range(p, msg->dir.x, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.y, -1.0f, 1.0f, 16);
		serialize_r32_range(p, msg->dir.z, -1.0f, 1.0f, 16);
	}

	if (msg->type == Message::Type::DashCombo)
		serialize_position(p, &msg->target, Net::Resolution::High);
	else
		msg->target = Vec3::zero;

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
	else if (Stream::IsReading)
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
	Entity* instance = get<PlayerManager>()->instance.ref();
	if (instance && !UpgradeStation::drone_inside(instance->get<Drone>()))
	{
		UpgradeStation* station = UpgradeStation::drone_at(instance->get<Drone>());
		if (station)
		{
			instance->get<Drone>()->ability(Ability::None);
			station->drone_enter(instance->get<Drone>());
		}
	}

	if (UpgradeStation::drone_inside(instance->get<Drone>()))
	{
		animation_time = Game::real_time.total;
		menu.animate();
		menu.selected = 0;
		upgrade_menu_open = true;
		upgrade_last_visit_highest_available = get<PlayerManager>()->upgrade_highest_owned_or_available();
	}
}

void PlayerHuman::upgrade_menu_hide()
{
	upgrade_menu_open = false;
	upgrade_station_try_exit();
}

void PlayerHuman::upgrade_station_try_exit()
{
	Entity* instance = get<PlayerManager>()->instance.ref();
	if (instance && get<PlayerManager>()->state() != PlayerManager::State::Upgrading)
	{
		UpgradeStation* station = UpgradeStation::drone_inside(instance->get<Drone>());
		if (station && station->mode != UpgradeStation::Mode::Deactivating)
		{
			station->drone_exit();
			upgrade_last_visit_highest_available = get<PlayerManager>()->upgrade_highest_owned_or_available();
		}
	}
}

void PlayerHuman::upgrade_completed(Upgrade u)
{
	if (upgrade_menu_open && menu.selected > 0)
	{
		// an upgrade was just removed from the menu
		// shift selected menu item up one so the player is not surprised by what they currently have selected
		Upgrade selected = upgrade_selected();
		if (selected == Upgrade::None || s32(selected) > s32(u) + 1)
			menu.selected = vi_max(0, menu.selected - 1);
	}
}

void PlayerHuman::update(const Update& u)
{
	Entity* entity = get<PlayerManager>()->instance.ref();

	// record parkour support
	if (Game::level.local && Game::level.mode == Game::Mode::Parkour && entity)
	{
		btCollisionWorld::ClosestRayResultCallback ray_callback = entity->get<Walker>()->check_support();
		if (ray_callback.hasHit())
		{
			const btRigidBody* bt_support = (const btRigidBody*)(ray_callback.m_collisionObject);
			RigidBody* support = Entity::list.data[bt_support->getUserIndex()].get<RigidBody>();

			Vec3 relative_position = support->get<Transform>()->to_local(entity->get<Transform>()->absolute_pos());
			b8 record_support = false;
			if (last_supported.length == 0)
				record_support = true;
			else
			{
				const SupportEntry& last_entry = last_supported[last_supported.length - 1];
				if (last_entry.support.ref() != support || (last_entry.relative_position - relative_position).length_squared() > 2.0f * 2.0f)
					record_support = true;
			}

			if (record_support)
			{
				if (last_supported.length == last_supported.capacity())
					last_supported.remove_ordered(0);
				SupportEntry* entry = last_supported.add();
				entry->support = support;
				entry->relative_position = relative_position;
				entry->rotation = entity->get<Walker>()->target_rotation;
			}
		}
	}

	if (!local
#if !SERVER
		&& Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying
#endif
		)
		return;

#if !SERVER
	// if anyone hits a button, go back to the main menu
	if (Net::Client::replay_mode() == Net::Client::ReplayMode::Replaying)
	{
		if (Game::scheduled_load_level == AssetNull
			&& ((gamepad == 0 && u.input->keys.any()) || u.input->gamepads[gamepad].btns))
		{
			if (Game::session.type == SessionType::Story)
				Menu::title();
			else
				Game::schedule_load_level(Asset::Level::overworld, Game::Mode::Special);
		}
	}
	else // no rumble when replaying
#endif
	if (rumble > 0.0f)
	{
		u.input->gamepads[gamepad].rumble = Settings::gamepads[gamepad].rumble ? vi_min(1.0f, rumble) : 0.0f;
		rumble = vi_max(0.0f, rumble - u.time.delta);
	}

	// camera stuff
	{
		s32 player_count;
#if DEBUG_AI_CONTROL
		player_count = count_local() + PlayerAI::list.count();
#else
		player_count = count_local();
#endif
		Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
		Camera::ViewportBlueprint* blueprint = &viewports[count_local_before(this)];

		camera.ref()->viewport =
		{
			Vec2(s32(blueprint->x * r32(u.input->width)), s32(blueprint->y * r32(u.input->height))),
			Vec2(s32(blueprint->w * r32(u.input->width)), s32(blueprint->h * r32(u.input->height))),
		};
		camera.ref()->flag(CameraFlagColors, Game::level.mode == Game::Mode::Parkour && !Overworld::modal());

		if (entity)
			camera.ref()->flag(CameraFlagActive, true);
		else
		{
			if (Game::level.mode == Game::Mode::Pvp)
			{
				r32 aspect = camera.ref()->viewport.size.y == 0 ? 1.0f : camera.ref()->viewport.size.x / camera.ref()->viewport.size.y;
				camera.ref()->perspective(fov_map_view, aspect, 1.0f, Game::level.skybox.far_plane);
				camera.ref()->range = 0;
				if (get<PlayerManager>()->spawn_timer == 0.0f)
				{
					camera.ref()->cull_range = 0;
					camera.ref()->flag(CameraFlagCullBehindWall, false);
					camera.ref()->flag(CameraFlagFog, true);
				}
				upgrade_menu_hide();
			}
			else
				camera.ref()->flag(CameraFlagActive, false);
		}
	}

	msg_timer = vi_max(0.0f, msg_timer - Game::real_time.delta);

	// after this point, it's all input-related stuff
	if (Console::visible
		|| Overworld::active()
		|| Game::level.mode == Game::Mode::Special
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

	if (entity)
		select_spawn_timer = vi_max(0.0f, select_spawn_timer - u.time.delta); // for letterbox animation

	switch (ui_mode())
	{
		case UIMode::PvpDefault:
		{
			kill_cam_rot = camera.ref()->rot;
			if (UpgradeStation::drone_at(entity->get<Drone>()) && get<PlayerManager>()->energy > 0)
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
		case UIMode::PvpUpgrading:
		{
			if (upgrade_menu_open)
			{
				// upgrade menu
				if (u.last_input->get(Controls::Cancel, gamepad)
					&& !u.input->get(Controls::Cancel, gamepad)
					&& !Game::cancel_event_eaten[gamepad])
				{
					Game::cancel_event_eaten[gamepad] = true;
					upgrade_menu_hide();
				}
				else if (!UpgradeStation::drone_inside(entity->get<Drone>())) // we got kicked out of the upgrade station; probably by the server
					upgrade_menu_hide();
				else
				{
					b8 upgrade_in_progress = !get<PlayerManager>()->can_transition_state();

					s8 last_selected = menu.selected;

					menu.start(u, gamepad);

					if (menu.item(u, _(strings::close), nullptr, upgrade_in_progress, Asset::Mesh::icon_close))
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
								&& (Game::level.has_feature(Game::FeatureLevel::All) || !get<PlayerManager>()->upgrades) // only allow one ability upgrade in tutorial
								&& (Game::level.has_feature(Game::FeatureLevel::All) || AbilityInfo::list[i].type != AbilityInfo::Type::Other); // don't allow Other ability upgrades in tutorial
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
						animation_time = Game::real_time.total;
				}
			}
			else
			{
				// upgrade menu closed, but we're still in the upgrade station
				if (!u.input->get(Controls::Interact, gamepad) && u.last_input->get(Controls::Interact, gamepad))
					upgrade_menu_show();
				else
					upgrade_station_try_exit();
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

				r32 aspect = camera.ref()->viewport.size.y == 0 ? 1 : camera.ref()->viewport.size.x / camera.ref()->viewport.size.y;
				camera.ref()->perspective(fov_map_view, aspect, 0.02f, Game::level.skybox.far_plane);
				camera.ref()->range = 0;
				camera.ref()->cull_range = 0;

				if (!Console::visible)
				{
					r32 speed = u.input->keys.get(s32(KeyCode::LShift)) ? 24.0f : 4.0f;
					if (u.input->get(Controls::Forward, gamepad))
						camera.ref()->pos += camera.ref()->rot * Vec3(0, 0, 1) * u.time.delta * speed;
					if (u.input->get(Controls::Backward, gamepad))
						camera.ref()->pos += camera.ref()->rot * Vec3(0, 0, -1) * u.time.delta * speed;
					if (u.input->get(Controls::Right, gamepad))
						camera.ref()->pos += camera.ref()->rot * Vec3(-1, 0, 0) * u.time.delta * speed;
					if (u.input->get(Controls::Left, gamepad))
						camera.ref()->pos += camera.ref()->rot * Vec3(1, 0, 0) * u.time.delta * speed;

#if DEBUG
					if (Game::level.local && u.input->keys.get(s32(KeyCode::MouseLeft)) && !u.last_input->keys.get(s32(KeyCode::MouseLeft)))
					{
						Entity* box = World::create<PhysicsEntity>(Asset::Mesh::cube, camera.ref()->pos, camera.ref()->rot, RigidBody::Type::Box, Vec3(0.25f, 0.25f, 0.5f), 1.0f, CollisionDefault, ~CollisionDroneIgnore);
						box->get<RigidBody>()->btBody->setLinearVelocity(camera.ref()->rot * Vec3(0, 0, 15));
						Net::finalize(box);
					}
#endif
				}
			}
			else if (Game::level.mode == Game::Mode::Pvp && get<PlayerManager>()->respawns != 0)
			{
				// we're spawning
				if (!get<PlayerManager>()->can_spawn)
				{
					// player can't spawn yet; needs to solve sudoku
					sudoku.update(u, gamepad, this);
					if (sudoku.complete() && sudoku.timer_animation == 0.0f)
						get<PlayerManager>()->set_can_spawn();
				}
				else if (get<PlayerManager>()->spawn_timer > 0.0f)
				{
					// waiting for spawn timer; if something killed us, show the kill cam
					Entity* k = killed_by.ref();
					if (k)
						kill_cam_rot = Quat::look(Vec3::normalize(k->get<Transform>()->absolute_pos() - camera.ref()->pos));
					if (get<PlayerManager>()->spawn_timer < SPAWN_DELAY - 1.0f)
						camera.ref()->rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), camera.ref()->rot, kill_cam_rot);
				}
				else
				{
					// select a spawn point
					AI::Team my_team = get<PlayerManager>()->team.ref()->team();
					if (select_spawn_timer > 0.0f)
					{
						if (selected_spawn.ref() && selected_spawn.ref()->team == my_team)
						{
							select_spawn_timer = vi_max(0.0f, select_spawn_timer - u.time.delta);
							if (select_spawn_timer == 0.0f)
								get<PlayerManager>()->spawn_select(selected_spawn.ref());
						}
						else
							select_spawn_timer = 0.0f;
					}
					else
					{
						if (!selected_spawn.ref() || selected_spawn.ref()->team != my_team)
							selected_spawn = SpawnPoint::closest(1 << s32(my_team), camera.ref()->pos);

						Vec2 movement = camera_topdown_movement(u, gamepad, camera.ref());
						r32 movement_amount = movement.length();
						if (movement_amount > 0.0f)
						{
							movement /= movement_amount;
							SpawnPoint* closest = nullptr;
							r32 closest_dot = FLT_MAX;

							Vec3 spawn_pos = selected_spawn.ref()->get<Transform>()->absolute_pos();
							for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
							{
								SpawnPoint* candidate = i.item();
								if (candidate == selected_spawn.ref() || candidate->team != my_team)
									continue;

								Vec3 candidate_pos = candidate->get<Transform>()->absolute_pos();
								Vec3 to_candidate = candidate_pos - spawn_pos;
								if (movement.dot(Vec2::normalize(Vec2(to_candidate.x, to_candidate.z))) > 0.707f)
								{
									r32 dot = movement.dot(Vec2(to_candidate.x, to_candidate.z));
									if (dot < closest_dot)
									{
										closest = candidate;
										closest_dot = dot;
									}
								}
							}
							if (closest)
								selected_spawn = closest;
						}

						if (u.input->get(Controls::Interact, gamepad) && !u.last_input->get(Controls::Interact, gamepad))
							select_spawn_timer = 1.0f;
					}

					// move camera to focus on selected spawn point
					{
						Quat target_rot = Quat::look(Game::level.map_view.ref()->absolute_rot() * Vec3(0, -1, 0));
						Vec3 target_pos = selected_spawn.ref()->get<Transform>()->absolute_pos() + target_rot * Vec3(0, 0, Game::level.skybox.far_plane * -0.5f);
						camera.ref()->pos += (target_pos - camera.ref()->pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
						camera.ref()->rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), camera.ref()->rot, target_rot);
					}
				}
			}
			else if (Game::level.mode == Game::Mode::Pvp)
			{
				// we're dead but others still playing; spectate
				update_camera_rotation(u);

				r32 aspect = camera.ref()->viewport.size.y == 0 ? 1 : camera.ref()->viewport.size.x / camera.ref()->viewport.size.y;
				camera.ref()->perspective(fov_default, aspect, 0.02f, Game::level.skybox.far_plane);

				if (PlayerCommon::list.count() > 0)
				{
					spectate_index += UI::input_delta_horizontal(u, gamepad);
					if (spectate_index < 0)
						spectate_index = PlayerCommon::list.count() - 1;
					else if (spectate_index >= PlayerCommon::list.count())
						spectate_index = 0;

					Entity* spectating = live_player_get(spectate_index);

					if (spectating)
						camera_setup_drone(spectating, camera.ref(), 6.0f);
				}
			}
			break;
		}
		case UIMode::PvpGameOver:
		{
			camera.ref()->range = 0;
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
			camera.ref()->rot = Quat::euler(0.0f, PI * 0.25f, PI * 0.25f);
			camera_setup_drone(e, camera.ref(), 6.0f);
		}
	}
	
	if (camera.ref())
		Audio::listener_update(gamepad, camera.ref()->pos, camera.ref()->rot);
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

void get_standing_position(Transform* i, Vec3* pos, r32* angle)
{
	Vec3 i_pos;
	Quat i_rot;
	i->absolute(&i_pos, &i_rot);
	Vec3 dir = i_rot * Vec3(1, 0, 0);
	dir.y = 0.0f;
	dir.normalize();
	if (angle)
		*angle = atan2f(dir.x, dir.z);
	*pos = i_pos;
	pos->y += (WALKER_DEFAULT_CAPSULE_HEIGHT * 0.5f) + WALKER_SUPPORT_HEIGHT;
}

void PlayerHuman::game_mode_transitioning()
{
	if (camera.ref())
	{
		Quat rot;
		Game::level.map_view.ref()->absolute(&camera.ref()->pos, &rot);
		camera.ref()->rot = Quat::look(rot * Vec3(0, -1, 0));
	}
	get<PlayerManager>()->can_spawn = Game::level.mode == Game::Mode::Parkour;
	last_supported.length = 0;
}

void PlayerHuman::spawn(const SpawnPosition& normal_spawn_pos)
{
	Entity* spawned;

	SpawnPosition spawn_pos;

	if (Game::level.mode == Game::Mode::Pvp)
	{
		// spawn drone
		spawned = World::create<DroneEntity>(get<PlayerManager>()->team.ref()->team());
		spawn_pos = normal_spawn_pos;
	}
	else
	{
		// spawn traceur

		b8 spawned_at_last_supported = false;
		if (last_supported.length > 0)
		{
			// restore last supported position
			s32 backtrack = killed_by.ref() ? 10 : 1; // if we were killed by something specific, backtrack farther than if we just fell to our death
			for (s32 i = 0; i < backtrack; i++)
			{
				if (last_supported.length > 1)
					last_supported.remove_ordered(last_supported.length - 1);
				else
					break;
			}
			while (last_supported.length > 0) // try to spawn at last supported location
			{
				SupportEntry entry = last_supported[last_supported.length - 1];
				last_supported.remove_ordered(last_supported.length - 1);
				if (entry.support.ref())
				{
					spawn_pos.pos = entry.support.ref()->get<Transform>()->to_world(entry.relative_position);
					spawn_pos.angle = entry.rotation;
					spawned_at_last_supported = true;
					break;
				}
			}
		}

		if (!spawned_at_last_supported)
		{
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
				if (Game::save.zone_last != AssetNull)
				{
					for (s32 i = 0; i < Game::level.tram_tracks.length; i++)
					{
						const Game::TramTrack& t = Game::level.tram_tracks[i];
						if (t.level == Game::save.zone_last)
						{
							track = s8(i);
							break;
						}
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
		}

		spawned = World::create<Traceur>(spawn_pos.pos, spawn_pos.angle, get<PlayerManager>()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute_pos(spawn_pos.pos);
	PlayerCommon* common = spawned->add<PlayerCommon>(get<PlayerManager>());
	common->angle_horizontal = spawn_pos.angle;

	spawned->add<PlayerControlHuman>(this);

	if (Game::level.mode == Game::Mode::Parkour)
	{
		if (Game::level.post_pvp && !Game::save.zone_current_restore)
		{
			// player is getting out of the terminal
			spawned->get<Animator>()->layers[3].set(Asset::Animation::character_terminal_exit, 0.0f); // bypass animation blending
			spawned->get<PlayerControlHuman>()->anim_base = Game::level.terminal_interactable.ref();
		}

		Game::save.zone_current_restore = false;
		Game::level.post_pvp = false;
	}
	else
		ParticleEffect::spawn(ParticleEffect::Type::SpawnDrone, spawn_pos.pos, Quat::look(Vec3(0, 1, 0)));

	Net::finalize(spawned);

	get<PlayerManager>()->set_instance(spawned);
}

void PlayerHuman::assault_status_display()
{
	AI::Team team = get<PlayerManager>()->team.ref()->team();
	char buffer[512];
	b8 good;
	if (Turret::list.count() > 0)
	{
		good = team == 0;
		sprintf(buffer, _(strings::turrets_remaining), Turret::list.count());
	}
	else
	{
		good = team != 0;
		sprintf(buffer, _(strings::core_modules_remaining), CoreModule::list.count());
	}
	msg(buffer, good);
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

r32 ability_draw(const RenderParams& params, const PlayerManager* manager, const Vec2& pos, s8 gamepad, s32 index, Controls binding)
{
	char string[255];

	Ability ability = manager->abilities[index];

	sprintf(string, "%s", Settings::gamepads[gamepad].bindings[s32(binding)].string(Game::ui_gamepad_types[gamepad]));
	const Vec4* color;
	if (Game::time.total - manager->ability_purchase_times[index] < msg_time)
		color = UI::flash_function(Game::time.total) ? &UI::color_default : &UI::color_background;
	else if (!manager->ability_valid(ability) || !manager->instance.ref()->get<PlayerCommon>()->movement_enabled())
		color = params.sync->input.get(binding, gamepad) ? &UI::color_disabled() : &UI::color_alert();
	else if (manager->instance.ref()->get<Drone>()->current_ability == ability)
		color = &UI::color_default;
	else
		color = &UI::color_accent();
	return draw_icon_text(params, gamepad, pos, AbilityInfo::list[s32(ability)].icon, string, *color);
}

r32 match_timer_width()
{
	return text_size * 2.5f * UI::scale;
}

void match_timer_draw(const RenderParams& params, const Vec2& pos, UIText::Anchor anchor_x)
{
	r32 remaining = vi_max(0.0f, Game::session.config.time_limit() - Team::match_time);

	Vec2 box(match_timer_width(), text_size * UI::scale);
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

	const Vec4* color;
	if (remaining > Game::session.config.time_limit() * 0.5f)
		color = &UI::color_default;
	else if (remaining > Game::session.config.time_limit() * 0.25f)
		color = &UI::color_accent();
	else
		color = &UI::color_alert();

	{
		b8 draw;
		if (remaining > Game::session.config.time_limit() * 0.2f)
			draw = true;
		else if (remaining > 30.0f)
			draw = UI::flash_function_slow(Game::real_time.total);
		else
			draw = UI::flash_function(Game::real_time.total);
		if (draw)
		{
			s32 remaining_minutes = remaining / 60.0f;
			s32 remaining_seconds = remaining - (remaining_minutes * 60.0f);

			UIText text;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Min;
			text.color = *color;
			text.text(0, _(strings::timer), remaining_minutes, remaining_seconds);
			text.draw(params, p);
		}
	}
}

enum class ScoreboardPosition : s8
{
	Center,
	Bottom,
	count,
};

void scoreboard_draw(const RenderParams& params, const PlayerManager* manager, ScoreboardPosition position)
{
	const Rect2& vp = params.camera->viewport;
	Vec2 p;
	switch (position)
	{
		case ScoreboardPosition::Center:
		{
			p = vp.size * Vec2(0.5f, 0.8f);
			break;
		}
		case ScoreboardPosition::Bottom:
		{
			p = vp.size * Vec2(0.5f, 0.3f);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	if (Game::level.mode == Game::Mode::Pvp
		&& Team::match_state == Team::MatchState::Active)
		match_timer_draw(params, p, UIText::Anchor::Center);

	UIText text;
	text.size = text_size;
	r32 width = MENU_ITEM_WIDTH * 1.25f;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Min;
	text.color = UI::color_default;
	p.y += text.bounds().y + MENU_ITEM_PADDING * -3.0f;
	p.x += width * -0.5f;

	// "deploying..."
	if (!manager->instance.ref() && manager->respawns != 0)
	{
		AssetID string;
		if (Team::match_state == Team::MatchState::Active)
		{
			if (Game::session.config.game_type == GameType::Assault)
			{
				if (manager->team.ref()->team() == 0)
					string = strings::deploy_timer_defender;
				else
					string = strings::deploy_timer_attacker;
			}
			else
				string = strings::deploy_timer;
		}
		else
			string = strings::waiting;
		text.text(0, _(string), s32(manager->spawn_timer + 1));
		UI::box(params, Rect2(p, Vec2(width, text.bounds().y)).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	if (Game::session.config.game_type == GameType::Assault)
	{
		// show remaining drones label
		text.text(0, _(strings::drones_remaining));
		text.color = UI::color_accent();
		UI::box(params, Rect2(p, Vec2(width, text.bounds().y)).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
		p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
	}

	// sort by team
	AI::Team team_mine = manager->team.ref()->team();
	AI::Team team = team_mine;
	while (true)
	{
		if (Game::session.config.game_type == GameType::Deathmatch)
		{
			const Team& team_ref = Team::list[team];

			text.anchor_x = UIText::Anchor::Min;
			text.color = Team::ui_color(manager->team.ref()->team(), team);
			PlayerManager* player = nullptr;
			if (team_ref.player_count() == 1)
			{
				// use the only player's username
				for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
				{
					if (i.item()->team.ref()->team() == team)
					{
						player = i.item();
						break;
					}
				}
				text.text_raw(0, player->username);
			}
			else
			{
				// use the team name
				static const AssetID team_names[MAX_TEAMS] = { strings::team_a, strings::team_b, strings::team_c, strings::team_d };
				text.text_raw(0, _(team_names[team]));
			}
			UI::box(params, Rect2(p, Vec2(width, text.bounds().y)).outset(MENU_ITEM_PADDING), UI::color_background);
			text.draw(params, p);

			// ping
			if (!Game::level.local && player && player->has<PlayerHuman>()) // todo: fake ping for ai players
			{
				r32 rtt = Net::rtt(player->get<PlayerHuman>());
				text.anchor_x = UIText::Anchor::Max;
				text.color = UI::color_ping(rtt);
				text.text(0, _(strings::ping), s32(rtt * 1000.0f));
				text.draw(params, p + Vec2(width * 0.75f, 0));
			}

			text.anchor_x = UIText::Anchor::Max;
			text.text(0, "%d", s32(team_ref.kills));
			text.draw(params, p + Vec2(width - MENU_ITEM_PADDING, 0));

			p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
		}
		else
		{
			for (auto i = PlayerManager::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->team.ref()->team() == team)
				{
					text.anchor_x = UIText::Anchor::Min;
					text.color = Team::ui_color(manager->team.ref()->team(), i.item()->team.ref()->team());
					text.text_raw(0, i.item()->username);
					UI::box(params, Rect2(p, Vec2(width, text.bounds().y)).outset(MENU_ITEM_PADDING), UI::color_background);
					text.draw(params, p);

					if (!Game::level.local && i.item()->has<PlayerHuman>()) // todo: fake ping for ai players
					{
						r32 rtt = Net::rtt(i.item()->get<PlayerHuman>());
						text.anchor_x = UIText::Anchor::Min;
						text.color = UI::color_ping(rtt);
						text.text(0, _(strings::ping), s32(rtt * 1000.0f));
						text.draw(params, p + Vec2(width * 0.75f, 0));
					}

					text.anchor_x = UIText::Anchor::Max;
					text.wrap_width = 0;
					text.text(0, "%d", s32(i.item()->respawns) + (i.item()->instance.ref() ? 1 : 0));
					text.draw(params, p + Vec2(width - MENU_ITEM_PADDING, 0));

					p.y -= text.bounds().y + MENU_ITEM_PADDING * 2.0f;
				}
			}
		}
		team = AI::Team((s32(team) + 1) % Team::list.count());
		if (team == team_mine)
			break;
	}
}

Upgrade PlayerHuman::upgrade_selected() const
{
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
	return upgrade;
}

void PlayerHuman::draw_ui(const RenderParams& params) const
{
	if (params.camera != camera.ref()
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
	if (Game::level.has_feature(Game::FeatureLevel::Abilities) && Game::session.config.allow_upgrades)
	{
		if (mode == UIMode::PvpDefault
			&& get<PlayerManager>()->can_transition_state()
			&& UpgradeStation::drone_at(get<PlayerManager>()->instance.ref()->get<Drone>())
			&& get<PlayerManager>()->energy > 0)
		{
			// "upgrade!" prompt
			UIText text;
			text.color = get<PlayerManager>()->upgrade_available() ? UI::color_accent() : UI::color_disabled();
			text.text(gamepad, _(strings::prompt_upgrade));
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.size = text_size;
			Vec2 pos = vp.size * Vec2(0.5f, 0.2f);
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(params, pos);
		}

		if ((mode == UIMode::PvpDefault || mode == UIMode::PvpUpgrading)
			&& get<PlayerManager>()->can_transition_state())
		{
			// draw abilities

			Vec2 pos = ui_anchor;
			// ability 1
			if (get<PlayerManager>()->abilities[0] != Ability::None)
			{
				pos.x += ability_draw(params, get<PlayerManager>(), pos, gamepad, 0, Controls::Ability1);

				// ability 2
				if (get<PlayerManager>()->abilities[1] != Ability::None)
				{
					pos.x += ability_draw(params, get<PlayerManager>(), pos, gamepad, 1, Controls::Ability2);

					// ability 3
					if (get<PlayerManager>()->abilities[2] != Ability::None)
						pos.x += ability_draw(params, get<PlayerManager>(), pos, gamepad, 2, Controls::Ability3);
				}
			}
		}
	}

	if (Game::level.mode == Game::Mode::Pvp
		&& Game::level.has_feature(Game::FeatureLevel::Abilities)
		&& Game::session.config.allow_upgrades
		&& (mode == UIMode::PvpDefault || mode == UIMode::PvpUpgrading))
	{
		// energy
		char buffer[128];
		sprintf(buffer, "%d", get<PlayerManager>()->energy);
		Vec2 p = ui_anchor + Vec2(match_timer_width() + text_size * UI::scale, (text_size + 16.0f) * -UI::scale);
		draw_icon_text(params, gamepad, p, Asset::Mesh::icon_energy, buffer, UI::color_accent(), text_size * 5 * UI::scale);
	}

	if (mode == UIMode::PvpDefault)
	{
		if (params.sync->input.get(Controls::Scoreboard, gamepad))
			scoreboard_draw(params, get<PlayerManager>(), ScoreboardPosition::Center);
	}
	else if (mode == UIMode::PvpUpgrading)
	{
		if (upgrade_menu_open)
		{
			Vec2 upgrade_menu_pos = vp.size * Vec2(0.5f, 0.6f);
			menu.draw_ui(params, upgrade_menu_pos, UIText::Anchor::Center, UIText::Anchor::Center);

			if (menu.selected > 0)
			{
				// show details of currently highlighted upgrade
				Upgrade upgrade = upgrade_selected();
				vi_assert(upgrade != Upgrade::None);

				if (get<PlayerManager>()->current_upgrade == Upgrade::None
					&& get<PlayerManager>()->upgrade_available(upgrade))
				{
					r32 padding = 8.0f * UI::scale;

					const UpgradeInfo& info = UpgradeInfo::list[s32(upgrade)];
					UIText text;
					text.color = UI::color_accent();
					text.size = text_size;
					text.anchor_x = UIText::Anchor::Min;
					text.anchor_y = UIText::Anchor::Max;
					text.wrap_width = MENU_ITEM_WIDTH - padding * 2.0f;
					s16 cost = get<PlayerManager>()->upgrade_cost(upgrade);
					text.text(gamepad, _(strings::upgrade_description), cost, _(info.description));
					UIMenu::text_clip(&text, animation_time, 150.0f);

					Vec2 pos = upgrade_menu_pos + Vec2(MENU_ITEM_WIDTH * -0.5f + padding, menu.height() * -0.5f - padding * 7.0f);
					UI::box(params, text.rect(pos).outset(padding), UI::color_background);
					text.draw(params, pos);
				}
			}
		}
	}
	else if (mode == UIMode::Dead)
	{
		if (Game::level.mode == Game::Mode::Pvp)
		{
			// if we haven't spawned yet, then show the player list
			if (get<PlayerManager>()->respawns != 0)
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
						text.color = UI::color_alert();
						text.text(gamepad, _(strings::alarm));

						Vec2 pos = vp.size * Vec2(0.5f, 0.8f);
						Rect2 rect = text.rect(pos).outset(MENU_ITEM_PADDING * 2.0f);
						UI::box(params, rect, UI::color_background);
						text.draw(params, pos);
						UI::border(params, rect, 4.0f, UI::color_alert());
					}
					sudoku.draw(params, gamepad);
				}
				else
				{
					if (get<PlayerManager>()->spawn_timer > 0.0f)
						scoreboard_draw(params, get<PlayerManager>(), ScoreboardPosition::Bottom);
					else
					{
						if (select_spawn_timer > 0.0f)
						{
							// spawning...
							Menu::progress_infinite(params, _(strings::deploying), vp.size * Vec2(0.5f));
						}
						else
						{
							// select spawn point
							AI::Team my_team = get<PlayerManager>()->team.ref()->team();
							for (auto i = SpawnPoint::list.iterator(); !i.is_last(); i.next())
								UI::indicator(params, i.item()->get<Transform>()->absolute_pos(), Team::ui_color(my_team, i.item()->team), i.item()->team == my_team, 1.0f, PI);

							for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
								UI::indicator(params, i.item()->get<Transform>()->absolute_pos(), Team::ui_color(my_team, i.item()->team), false, 1.0f);

							Vec2 p;
							if (selected_spawn.ref() && UI::project(params, selected_spawn.ref()->get<Transform>()->absolute_pos(), &p))
								UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_accent(), PI);

							if (Game::session.config.game_type == GameType::Assault)
							{
								// attacking/defending
								UIText text;
								text.anchor_x = UIText::Anchor::Center;
								text.anchor_y = UIText::Anchor::Min;
								text.color = UI::color_default;
								text.text(gamepad, _(my_team == 0 ? strings::turrets_remaining_defending : strings::turrets_remaining_attacking), Turret::list.count());
								Vec2 pos = vp.size * Vec2(0.5f, 0.25f);
								UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::color_background);
								text.draw(params, pos);
							}

							{
								// deploy prompt
								UIText text;
								text.anchor_x = UIText::Anchor::Center;
								text.anchor_y = UIText::Anchor::Max;
								text.color = UI::color_accent();
								text.text(gamepad, _(strings::prompt_deploy));
								Vec2 pos = vp.size * Vec2(0.5f, 0.2f);
								UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::color_background);
								text.draw(params, pos);
							}
						}
					}
				}
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
					text.color = UI::color_accent();
					text.text(gamepad, _(strings::spectating));
					pos = vp.size * Vec2(0.5f, 0.1f);
					UI::box(params, text.rect(pos).outset(MENU_ITEM_PADDING), UI::color_background);
					text.draw(params, pos);
				}
			}
		}
	}

	if (mode == UIMode::PvpGameOver && Game::level.mode == Game::Mode::Pvp)
	{
		// show victory/defeat/draw message
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = 32.0f;

		Team* winner = Team::winner.ref();
		if (winner == get<PlayerManager>()->team.ref()) // we won
		{
			text.color = UI::color_accent();
			text.text(gamepad, _(Game::session.type == SessionType::Story ? strings::story_victory : strings::victory));
		}
		else if (!winner) // it's a draw
		{
			text.color = UI::color_alert();
			text.text(gamepad, _(strings::draw));
		}
		else // we lost
		{
			text.color = UI::color_alert();
			text.text(gamepad, _(Game::session.type == SessionType::Story ? strings::story_defeat : strings::defeat));
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
			for (s32 i = score_summary_scroll.top(); i < score_summary_scroll.bottom(Team::score_summary.length); i++)
			{
				const Team::ScoreSummaryItem& item = Team::score_summary[i];
				text.color = item.player.ref() == get<PlayerManager>() ? UI::color_accent() : Team::ui_color(team, item.team);

				UIText amount = text;
				amount.anchor_x = UIText::Anchor::Max;
				amount.wrap_width = 0;

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
			score_summary_scroll.end(params, p + Vec2(0, MENU_ITEM_PADDING));

			// press x to continue
			if (Game::real_time.total - Team::game_over_real_time > SCORE_SUMMARY_DELAY + SCORE_SUMMARY_ACCEPT_DELAY)
			{
				Vec2 p = vp.size * Vec2(0.5f, 0.2f);
				text.wrap_width = 0;
				text.color = UI::color_accent();
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
				UI::border(params, bar, 2, UI::color_accent());
				UI::box(params, { bar.pos, Vec2(bar.size.x * (1.0f - (get<PlayerManager>()->state_timer / total_time)), bar.size.y) }, UI::color_accent());
				text.draw(params, pos);
			}
		}

		if (mode == UIMode::PvpDefault || mode == UIMode::PvpUpgrading) // show game timer
			match_timer_draw(params, ui_anchor + Vec2(0, (text_size + 16.0f) * -UI::scale), UIText::Anchor::Min);
	}

	// network error icon
#if !SERVER
	if (!Game::level.local && Net::Client::lagging())
		UI::mesh(params, Asset::Mesh::icon_network_error, vp.size * Vec2(0.9f, 0.5f), Vec2(text_size * 2.0f * UI::scale), UI::color_alert());
#endif

	// message
	if (msg_timer > 0.0f)
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
				Audio::post_global_event(msg_good ? AK::EVENTS::PLAY_MESSAGE_BEEP_GOOD : AK::EVENTS::PLAY_MESSAGE_BEEP_BAD);
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
	if (Game::level.mode == Game::Mode::Parkour && Overworld::zone_under_attack() != AssetNull)
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Max;
		text.anchor_y = UIText::Anchor::Min;
		text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
		text.color = UI::color_alert();
		r32 timer = Overworld::zone_under_attack_timer();
		text.text(gamepad, _(strings::prompt_zone_defend), Loader::level_name(Overworld::zone_under_attack()), s32(ceilf(timer)));
		UIMenu::text_clip_timer(&text, ZONE_UNDER_ATTACK_THRESHOLD - timer, 80.0f);

		Vec2 p = Vec2(params.camera->viewport.size.x, 0) + Vec2(MENU_ITEM_PADDING * -5.0f, MENU_ITEM_PADDING * 5.0f);
		UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
		text.draw(params, p);
	}

	if (get<PlayerManager>()->instance.ref() && select_spawn_timer > 0.0f)
		Menu::draw_letterbox(params, select_spawn_timer, TRANSITION_TIME);

	if (mode == UIMode::Pause) // pause menu always drawn on top
		menu.draw_ui(params, Vec2(0, params.camera->viewport.size.y * 0.5f), UIText::Anchor::Min, UIText::Anchor::Center);
}

void PlayerHuman::draw_alpha(const RenderParams& params) const
{
	if (ui_mode() == UIMode::Dead
		&& get<PlayerManager>()->can_spawn
		&& get<PlayerManager>()->spawn_timer > 0.0f)
	{
		Entity* k = killed_by.ref();
		if (k)
		{
			RenderSync* sync = params.sync;
			sync->write(RenderOp::DepthFunc);
			sync->write(RenderDepthFunc::Greater);

			{
				RenderParams p = params;
				p.flags |= RenderFlagAlphaOverride;
				if (k->has<View>())
					k->get<View>()->draw(p);
				else if (k->has<SkinnedModel>())
					k->get<SkinnedModel>()->draw(p);
			}

			sync->write(RenderOp::DepthFunc);
			sync->write(RenderDepthFunc::Less);
		}
	}
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
	AI::Team my_team = get<AIAgent>()->team;
	for (auto i = Bolt::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != my_team)
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
			attach_quat = Quat::slerp(vi_min(1.0f, attach_lerp_speed * u.time.delta), attach_quat, rot);
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

	// if we are spawning on to a flat floor, set attach_quat immediately
	// this preserves the camera rotation set by the PlayerSpawn
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
		return look() * Vec3(0, 0, 1);
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
		r32 tolerance_pos;
		c->remote_position(&tolerance_pos);
		if (dist_sq < tolerance_pos * tolerance_pos)
			c->get<Transform>()->absolute(msg.pos, msg.rot);

#if SERVER
		// update RTT based on the sequence number
		c->rtt = Net::Server::rtt(c->player.ref(), seq) + NET_INTERPOLATION_DELAY;
#endif
	}

	switch (msg.type)
	{
		case PlayerControlHumanNet::Message::Type::Dash:
		{
			c->get<Drone>()->current_ability = Ability::None;
			if (c->get<Drone>()->dash_start(msg.dir, c->get<Transform>()->absolute_pos())) // HACK: set target to current position so that it is not used
			{
				c->try_primary = false;
				c->try_secondary = false;
			}
			break;
		}
		case PlayerControlHumanNet::Message::Type::DashCombo:
		{
			c->get<Drone>()->current_ability = Ability::None;
			if (c->get<Drone>()->dash_start(msg.dir, msg.target))
			{
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
					c->try_secondary = false;
				else
				{
					const AbilityInfo& info = AbilityInfo::list[s32(msg.ability)];
					if (!info.rapid_fire)
						c->player.ref()->rumble_add(0.5f);
				}
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
	camera_shake_timer = 0.0f; // stop screen shake
#if SERVER
	if (get<Health>()->can_take_damage())
	{
		AI::RecordedLife::Action action;
		action.type = AI::RecordedLife::Action::TypeMove;
		Quat rot;
		get<Transform>()->absolute(&action.pos, &rot);
		action.normal = rot * Vec3(0, 0, 1);
		AI::record_add(player.ref()->ai_record_id, ai_record_tag, action);
		ai_record_wait_timer = AI_RECORD_WAIT_TIME;
	}
	ai_record_tag.init(player.ref()->get<PlayerManager>());
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

	b8 in_range;

	if (type == PlayerControlHuman::TargetIndicator::Type::DroneOutOfRange)
	{
		in_range = false;
		show = true; // show even out of range
	}
	else
	{
		show = true;

		r32 range = p->get<Drone>()->range();
		in_range = (target->absolute_pos() - me).length_squared() < range * range;
		if (!in_range)
		{
			// out of range; some indicators just disappear; others change
			if (type == PlayerControlHuman::TargetIndicator::Type::Battery)
				type = PlayerControlHuman::TargetIndicator::Type::BatteryOutOfRange;
			else if (type == PlayerControlHuman::TargetIndicator::Type::Turret)
				type = PlayerControlHuman::TargetIndicator::Type::TurretOutOfRange;
			else if (type == PlayerControlHuman::TargetIndicator::Type::CoreModule)
				type = PlayerControlHuman::TargetIndicator::Type::CoreModuleOutOfRange;
			else if (type == PlayerControlHuman::TargetIndicator::Type::TurretFriendly
				|| type == PlayerControlHuman::TargetIndicator::Type::CoreModuleFriendly)
			{
				// show core modules even if out of range
			}
			else
				show = false; // don't show this indicator because it's out of range
		}
	}

	if (show)
	{
		if (in_range)
		{
			// calculate target intersection trajectory
			Vec3 intersection;
			if (p->get<Drone>()->predict_intersection(target, nullptr, &intersection, p->get<Drone>()->target_prediction_speed()))
				p->target_indicators.add({ intersection, target->velocity(), type });
		}
		else // too far away; just show the target's actual position
			p->target_indicators.add({ target->absolute_pos(), target->velocity(), type });
	}
}

// returns the actual detected entity, if any. could be the original player, or something else.
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
				player_add_target_indicator(p, detected_entity->get<Target>(), visible ? PlayerControlHuman::TargetIndicator::Type::DroneVisible : PlayerControlHuman::TargetIndicator::Type::DroneOutOfRange);
		}
	}

	// headshot indicators
	for (auto i = Minion::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->get<AIAgent>()->team != team)
		{
			PlayerControlHuman::TargetIndicator::Type type = i.item()->goal.entity.ref() == p->entity()
				? PlayerControlHuman::TargetIndicator::Type::MinionAttacking
				: PlayerControlHuman::TargetIndicator::Type::Minion;
			player_add_target_indicator(p, i.item()->get<Target>(), type);
		}
	}

	// batteries
	if (Game::level.has_feature(Game::FeatureLevel::Batteries))
	{
		for (auto i = Battery::list.iterator(); !i.is_last(); i.next())
		{
			PlayerControlHuman::TargetIndicator::Type type = i.item()->team == team
				? PlayerControlHuman::TargetIndicator::Type::BatteryFriendly
				: PlayerControlHuman::TargetIndicator::Type::Battery;
			player_add_target_indicator(p, i.item()->get<Target>(), type);
		}
	}

	// sensors
	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team)
			player_add_target_indicator(p, i.item()->get<Target>(), PlayerControlHuman::TargetIndicator::Type::Sensor);
	}

	// turrets and core modules
	if (Game::level.has_feature(Game::FeatureLevel::Turrets))
	{
		if (Turret::list.count() > 0)
		{
			for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
			{
				PlayerControlHuman::TargetIndicator::Type type;
				if (i.item()->target.ref() == p->entity())
					type = PlayerControlHuman::TargetIndicator::Type::TurretAttacking;
				else if (i.item()->team == team)
					type = PlayerControlHuman::TargetIndicator::Type::TurretFriendly;
				else
					type = PlayerControlHuman::TargetIndicator::Type::Turret;
				player_add_target_indicator(p, i.item()->get<Target>(), type);
			}
		}
		else
		{
			for (auto i = CoreModule::list.iterator(); !i.is_last(); i.next())
			{
				PlayerControlHuman::TargetIndicator::Type type;
				if (i.item()->team == team)
					type = PlayerControlHuman::TargetIndicator::Type::CoreModuleFriendly;
				else
					type = PlayerControlHuman::TargetIndicator::Type::CoreModule;
				player_add_target_indicator(p, i.item()->get<Target>(), type);
			}
		}
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
		if (i.item()->team != team && !(i.item()->flags & ForceField::FlagPermanent))
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
				control->get<Transform>()->absolute(&msg.pos, &msg.rot);
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
	camera_shake_timer(),
	target_indicators(),
	last_gamepad_input_time(),
	gamepad_rotation_speed(),
	remote_control(),
	player(p),
	position_history(),
	anim_base(),
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
	rtt = Net::rtt(player.ref()) + NET_INTERPOLATION_DELAY;
#endif

	if (player.ref()->local && !Game::level.local)
	{
		Transform* t = get<Transform>();
		remote_control.pos = t->pos;
		remote_control.rot = t->rot;
		remote_control.parent = t->parent;
	}

	player.ref()->killed_by = nullptr;
	player.ref()->select_spawn_timer = TRANSITION_TIME * 0.5f; // for letterbox animation

	link_arg<const HealthEvent&, &PlayerControlHuman::health_changed>(get<Health>()->changed);
	link_arg<Entity*, &PlayerControlHuman::killed>(get<Health>()->killed);

	if (has<Drone>())
	{
		last_pos = get<Drone>()->center_lerped();
		link<&PlayerControlHuman::drone_detaching>(get<Drone>()->detaching);
		link<&PlayerControlHuman::drone_done_flying_or_dashing>(get<Drone>()->done_flying);
		link<&PlayerControlHuman::drone_done_flying_or_dashing>(get<Drone>()->done_dashing);
		link_arg<const DroneReflectEvent&, &PlayerControlHuman::drone_reflecting>(get<Drone>()->reflecting);
		link_arg<Entity*, &PlayerControlHuman::hit_target>(get<Drone>()->hit);

		if (Team::match_state == Team::MatchState::Done && Game::level.has_feature(Game::FeatureLevel::All))
		{
			if (Game::session.config.game_type == GameType::Assault)
				player.ref()->assault_status_display();
			else if (Game::session.config.game_type == GameType::Deathmatch)
			{
				if (player.ref()->get<PlayerManager>()->deaths == 0)
					player.ref()->msg(_(strings::attack), true);
			}
		}
	}
	else
	{
		last_pos = get<Transform>()->absolute_pos();
		link_arg<r32, &PlayerControlHuman::parkour_landed>(get<Walker>()->land);
		link<&PlayerControlHuman::terminal_enter_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_terminal_enter, 2.5f));
		link<&PlayerControlHuman::interact_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_interact, 3.8f));
		link<&PlayerControlHuman::interact_animation_callback>(get<Animator>()->trigger(Asset::Animation::character_terminal_exit, 4.0f));
		get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_WIND);
		get<Audio>()->param(AK::GAME_PARAMETERS::PARKOUR_WIND, 0.0f);
	}
}

PlayerControlHuman::~PlayerControlHuman()
{
	if (has<Parkour>())
		get<Audio>()->post_event(AK::EVENTS::STOP_PARKOUR_ALL);
	if (player.ref())
	{
		player.ref()->select_spawn_timer = 0.0f;
#if SERVER
		AI::record_close(player.ref()->ai_record_id);
		player.ref()->ai_record_id = AI::record_init(Game::level.team_lookup_reverse(player.ref()->get<PlayerManager>()->team.ref()->team()), player.ref()->get<PlayerManager>()->respawns);
#endif
	}
}

void PlayerControlHuman::health_changed(const HealthEvent& e)
{
	s8 total = e.hp + e.shield;
	if (total < 0)
	{
		if (has<Drone>()) // de-scope when damaged
			try_secondary = false;
		if (has<Drone>() || e.source.ref()) // no rumble if you just fall in parkour mode
			camera_shake(total < -1 ? 1.0f : 0.7f);
	}
}

void PlayerControlHuman::killed(Entity* killed_by)
{
	if (killed_by)
	{
		if (killed_by->has<Bolt>())
			player.ref()->killed_by = killed_by->get<Bolt>()->owner.ref();
		else if (killed_by->has<Grenade>())
			player.ref()->killed_by = killed_by->get<Grenade>()->owner.ref()->instance.ref();
		else
			player.ref()->killed_by = killed_by;
	}
	else
		player.ref()->killed_by = nullptr;
}

void PlayerControlHuman::drone_reflecting(const DroneReflectEvent& e)
{
	// send message if we are a client in a network game.
	if (!Game::level.local)
	{
		PlayerControlHumanNet::Message msg;
		get<Transform>()->absolute(&msg.pos, &msg.rot);
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
	anim_base = nullptr;
}

void PlayerControlHuman::hit_target(Entity* target)
{
	player.ref()->rumble_add(0.5f);
}

void PlayerControlHuman::drone_detaching()
{
	camera_shake_timer = 0.0f; // stop screen shake

#if SERVER
	ai_record_tag.init(player.ref()->get<PlayerManager>());

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
	return !Console::visible
		&& !cinematic_active()
		&& !Overworld::active()
		&& (ui_mode == PlayerHuman::UIMode::PvpDefault || ui_mode == PlayerHuman::UIMode::ParkourDefault)
		&& Team::match_state == Team::MatchState::Active
		&& !Menu::dialog_active(player.ref()->gamepad)
		&& !anim_base.ref();
}

b8 PlayerControlHuman::movement_enabled() const
{
	return input_enabled() && get<PlayerCommon>()->movement_enabled();
}
void PlayerControlHuman::update_camera_input(const Update& u, r32 overall_rotation_multiplier, r32 gamepad_rotation_multiplier)
{
	if (input_enabled())
	{
		s32 gamepad = player.ref()->gamepad;
		if (gamepad == 0)
		{
			r32 s = overall_rotation_multiplier * speed_mouse * Settings::gamepads[gamepad].effective_sensitivity();
			get<PlayerCommon>()->angle_horizontal -= r32(u.input->cursor_x) * s;
			get<PlayerCommon>()->angle_vertical += r32(u.input->cursor_y) * s * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f);
		}

		if (u.input->gamepads[gamepad].type != Gamepad::Type::None)
		{
			Vec2 adjustment = Vec2
			(
				-u.input->gamepads[gamepad].right_x,
				u.input->gamepads[gamepad].right_y * (Settings::gamepads[gamepad].invert_y ? -1.0f : 1.0f)
			);
			Input::dead_zone(&adjustment.x, &adjustment.y);
			adjustment *= overall_rotation_multiplier * speed_joystick * Settings::gamepads[gamepad].effective_sensitivity() * Game::time.delta * gamepad_rotation_multiplier;
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

Vec3 PlayerControlHuman::get_movement(const Update& u, const Quat& rot) const
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
		get<Transform>()->rot = Quat::identity;
		get<Transform>()->parent = remote_control.parent;
		last_pos = get<Transform>()->absolute_pos();
		get<Walker>()->absolute_pos(last_pos); // force rigid body
		{
			Vec3 forward = remote_control.rot * Vec3(0, 0, 1);
			r32 angle = atan2f(forward.x, forward.z);
			if (!std::isnan(angle)) // validate client rotation
				get<Walker>()->target_rotation = angle;
		}
		get<Target>()->net_velocity = get<Target>()->net_velocity * 0.7f + ((last_pos - abs_pos_last) / Net::tick_rate()) * 0.3f;
	}
	else if (input_enabled())
	{
		// if the remote position is close to what we think it is, snap to it
		if (get<Drone>()->state() == Drone::State::Crawl // only if we're crawling
			&& remote_control.parent.ref()) // and only if the remote thinks we're crawling
		{
			Transform* t = get<Transform>();
			Vec3 abs_pos;
			Quat abs_rot;
			t->absolute(&abs_pos, &abs_rot);

			Vec3 remote_abs_pos = remote_control.pos;
			Quat remote_abs_rot = remote_control.rot;
			remote_control.parent.ref()->to_world(&remote_abs_pos, &remote_abs_rot);
			r32 tolerance_pos;
			r32 tolerance_rot;
			remote_position(&tolerance_pos, &tolerance_rot);
			if ((remote_abs_pos - abs_pos).length_squared() < tolerance_pos * tolerance_pos
				&& Quat::angle(remote_abs_rot, abs_rot) < tolerance_rot)
			{
				t->absolute(remote_abs_pos, remote_abs_rot);
			}
		}
	}
}

PlayerControlHuman::RemoteControl PlayerControlHuman::remote_control_get(const Update& u) const
{
	RemoteControl control;
	control.movement = get_movement(u, get<PlayerCommon>()->look());
	Transform* t = get<Transform>();
	control.pos = t->pos;
	if (has<Parkour>())
		control.rot = Quat::euler(0, get<Walker>()->target_rotation, 0);
	else
		control.rot = t->rot;
	control.parent = t->parent;
	return control;
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
			camera->rot = camera->rot * Quat::euler(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 67)) * shake, noise::sample3d(Vec3(offset + 137)) * shake);
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
			vi_assert(Game::save.resources[s32(Resource::AccessKeys)] > 0);
			Interactable* interactable = Interactable::closest(i.item()->get<Transform>()->absolute_pos());
			if (interactable)
			{
				i.item()->anim_base = interactable->entity();

				// skip sudoku
				interactable->interact();
				i.item()->get<Animator>()->layers[3].play(Asset::Animation::character_interact);
				i.item()->get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_INTERACT);
			}
			break;
		}
	}
}

void player_confirm_terminal_interactable(s8 gamepad)
{
	for (auto i = PlayerControlHuman::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->player.ref()->gamepad == gamepad)
		{
			Interactable* interactable = Interactable::closest(i.item()->get<Transform>()->absolute_pos());
			if (interactable && interactable->type == Interactable::Type::Terminal)
			{
				i.item()->anim_base = interactable->entity();
				i.item()->get<Animator>()->layers[3].play(Asset::Animation::character_terminal_enter); // animation will eventually trigger the interactable
			}
			break;
		}
	}
}

void player_cancel_interactable(s8 gamepad)
{
}

const PlayerControlHuman::PositionEntry* PlayerControlHuman::remote_position(r32* tolerance_pos, r32* tolerance_rot) const
{
	r32 timestamp = Game::real_time.total - Net::rtt(player.ref());
	const PositionEntry* position = nullptr;
	r32 tmp_tolerance_pos = 0.0f;
	r32 tmp_tolerance_rot = 0.0f;
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
					tmp_tolerance_pos = vi_max(tmp_tolerance_pos, (position_history[i].pos - position_history[j].pos).length());
					tmp_tolerance_rot = vi_max(tmp_tolerance_rot, Quat::angle(position_history[i].rot, position_history[j].rot));
				}
			}
			tmp_tolerance_pos *= 6.0f;
			tmp_tolerance_rot *= 6.0f;
			break;
		}
	}
	tmp_tolerance_pos += NET_SYNC_TOLERANCE_POS;
	tmp_tolerance_rot += NET_SYNC_TOLERANCE_ROT;
	if (tolerance_pos)
		*tolerance_pos = tmp_tolerance_pos;
	if (tolerance_rot)
		*tolerance_rot = tmp_tolerance_rot;
	return position;
}

// 0 to 1
r32 zoom_amount_get(PlayerControlHuman* player, const Update& u)
{
	s8 gamepad = player->player.ref()->gamepad;
	if (Settings::gamepads[gamepad].zoom_toggle)
		return player->try_secondary ? 1.0f : 0.0f;
	else
	{
		// analog zoom
		if (player->try_secondary)
		{
			const InputBinding& binding = Settings::gamepads[gamepad].bindings[s32(Controls::Zoom)];
			if (u.input->keys.get(s32(binding.key1)) || u.input->keys.get(s32(binding.key2)))
				return 1.0f;

			Gamepad::Btn zoom_btn = binding.btn;
			r32 t;
			if (zoom_btn == Gamepad::Btn::LeftTrigger)
				t = u.input->gamepads[gamepad].left_trigger;
			else if (zoom_btn == Gamepad::Btn::RightTrigger)
				t = u.input->gamepads[gamepad].right_trigger;
			else
				t = 1.0f;

			if (t > 0.95f)
				return 1.0f;
			else if (t > 0.0f)
				return 0.5f;
			else
				return 0.0f;
		}
		else
			return 0.0f;
	}
}

void PlayerControlHuman::update(const Update& u)
{
	s32 gamepad = player.ref()->gamepad;

	if (position_history.length == 0 || Game::real_time.total > position_history[position_history.length - 1].timestamp + Net::tick_rate() * 0.5f)
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

	if (has<Drone>())
	{
		if (local())
		{
			if (!Game::level.local)
			{
				// we are a client and this is a local player
				// make sure we never get too far from where the server says we should be
				// get the position entry at this time in the history

				// make sure we're not too far from it
				r32 tolerance_pos;
				r32 tolerance_rot;
				const PositionEntry* position = remote_position(&tolerance_pos, &tolerance_rot);
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

			Camera* camera = player.ref()->camera.ref();

			r32 zoom_amount = zoom_amount_get(this, u);

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

				r32 fov_target = LMath::lerpf(zoom_amount, fov_default, (get<Drone>()->current_ability == Ability::Sniper ? fov_sniper : fov_zoom));

				if (fov < fov_target)
					fov = vi_min(fov + zoom_speed * sinf(fov) * u.time.delta, fov_target);
				else if (fov > fov_target)
					fov = vi_max(fov - zoom_speed * sinf(fov) * u.time.delta, fov_target);
			}

			// update camera projection
			{
				r32 aspect = camera->viewport.size.y == 0 ? 1 : camera->viewport.size.x / camera->viewport.size.y;
				camera->perspective(fov, aspect, 0.005f, Game::level.skybox.far_plane);
			}

			// collect target indicators
			player_collect_target_indicators(this);

			if (get<Transform>()->parent.ref())
			{
				r32 gamepad_rotation_multiplier = 1.0f;

				r32 look_speed = LMath::lerpf(zoom_amount, 1.0f, get<Drone>()->current_ability == Ability::Sniper ? zoom_speed_multiplier_sniper : zoom_speed_multiplier);

				if (input_enabled() && u.input->gamepads[gamepad].type != Gamepad::Type::None)
				{
					// gamepad aim assist
					Vec3 to_reticle = reticle.pos - camera->pos;
					r32 reticle_distance = to_reticle.length();
					to_reticle /= reticle_distance;
					for (s32 i = 0; i < target_indicators.length; i++)
					{
						const TargetIndicator indicator = target_indicators[i];

						if (indicator.type == TargetIndicator::Type::BatteryOutOfRange
							|| indicator.type == TargetIndicator::Type::BatteryFriendly
							|| indicator.type == TargetIndicator::Type::DroneOutOfRange
							|| indicator.type == TargetIndicator::Type::TurretFriendly
							|| indicator.type == TargetIndicator::Type::CoreModuleFriendly
							|| indicator.type == TargetIndicator::Type::TurretOutOfRange
							|| indicator.type == TargetIndicator::Type::CoreModuleOutOfRange)
							continue;

						Vec3 to_indicator = indicator.pos - camera->pos;
						r32 indicator_distance = to_indicator.length();
						if (indicator_distance > DRONE_THIRD_PERSON_OFFSET + DRONE_SHIELD_RADIUS * 2.0f
							&& indicator_distance < reticle_distance + 2.5f)
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

										if (indicator.velocity.length_squared() > DRONE_DASH_SPEED * 0.5f * DRONE_DASH_SPEED * 0.5f) // enemy moving too fast
											continue;

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

									r32 max_adjustment = look_speed * 0.5f * speed_joystick * u.time.delta;

									if (current_offset.x > 0 == adjustment.x > 0 // only adjust if it's an adjustment toward the target
										&& fabsf(get<PlayerCommon>()->angle_vertical) < PI * 0.4f) // only adjust if we're not looking straight up or down
										get<PlayerCommon>()->angle_horizontal = LMath::angle_range(get<PlayerCommon>()->angle_horizontal + vi_max(-max_adjustment, vi_min(max_adjustment, adjustment.x)));

									if (current_offset.y > 0 == adjustment.y > 0) // only adjust if it's an adjustment toward the target
										get<PlayerCommon>()->angle_vertical = LMath::angle_range(get<PlayerCommon>()->angle_vertical + vi_max(-max_adjustment, vi_min(max_adjustment, adjustment.y)));
								}

								break;
							}
						}
					}
				}

				update_camera_input(u, look_speed, gamepad_rotation_multiplier);
				get<PlayerCommon>()->clamp_rotation(get<PlayerCommon>()->attach_quat * Vec3(0, 0, 1), 0.707f);
				camera->rot = Quat::euler(0, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);

				// crawling
				{
					Vec3 movement = get_movement(u, get<PlayerCommon>()->look());
					get<Drone>()->crawl(movement, u.time.delta);
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
					Vec3 trace_end = trace_start + trace_dir * DRONE_SNIPE_DISTANCE;
					RaycastCallbackExcept ray_callback(trace_start, trace_end, entity());
					for (auto i = UpgradeStation::list.iterator(); !i.is_last(); i.next()) // ignore drones inside upgrade stations
					{
						if (i.item()->drone.ref())
							ray_callback.ignore(i.item()->drone.ref()->entity());
					}
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
							Vec3 hit;
							b8 hit_target;
							if (get<Drone>()->can_shoot(detach_dir, &hit, &hit_target))
							{
								if (hit_target)
									reticle.type = ReticleType::Target;
								else if ((hit - me).length() > distance - DRONE_RADIUS)
									reticle.type = ReticleType::Normal;
							}
							else if (get<Drone>()->direction_is_toward_attached_wall(detach_dir))
							{
								r32 range = get<Drone>()->range();
								if ((Vec3(ray_callback.m_hitPointWorld) - me).length_squared() < range * range)
								{
									if (hit_entity->has<Target>())
										reticle.type = ReticleType::DashTarget;
									else if (!(ray_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & DRONE_INACCESSIBLE_MASK))
										reticle.type = ReticleType::DashCombo;
								}
							}
							else if (hit_entity->has<Target>())
							{
								// when you're aiming at a target that is attached to the same surface you are,
								// sometimes the point you're aiming at is actually away from the wall,
								// so it registers as a shot rather than a dash.
								// and sometimes that shot can't actually be taken.
								// so we need to check for this case and turn it into a dash if we can.

								// check if they're in range and close enough to our wall
								Vec3 to_target = hit_entity->get<Target>()->absolute_pos() - me;
								if (to_target.length_squared() < DRONE_DASH_DISTANCE * DRONE_DASH_DISTANCE
									&& fabsf(to_target.dot(get<Transform>()->absolute_rot() * Vec3(0, 0, 1))) < DRONE_SHIELD_RADIUS)
									reticle.type = ReticleType::Dash;
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
							reticle.type = ReticleType::Target;
						}
					}
				}
				else
				{
					reticle.pos = trace_start + trace_dir * DRONE_SNIPE_DISTANCE;
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
				if (try_primary && camera_shake_timer < 0.1f) // don't go anywhere if the camera is shaking wildly
				{
					PlayerControlHumanNet::Message msg;
					msg.dir = Vec3::normalize(reticle.pos - get<Transform>()->absolute_pos());
					get<Transform>()->absolute(&msg.pos, &msg.rot);
					if (reticle.type == ReticleType::DashCombo || reticle.type == ReticleType::DashTarget)
					{
						msg.type = PlayerControlHumanNet::Message::Type::DashCombo;
						msg.target = reticle.pos;
						PlayerControlHumanNet::send(this, &msg);
					}
					else if (reticle.type == ReticleType::Dash)
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
				rtt = Net::rtt(player.ref()) + NET_INTERPOLATION_DELAY;

			ai_record_wait_timer -= u.time.delta;
			if (ai_record_wait_timer < 0.0f)
			{
				ai_record_wait_timer += AI_RECORD_WAIT_TIME;
				if (get<Health>()->can_take_damage())
				{
					AI::RecordedLife::Action action;
					action.type = AI::RecordedLife::Action::TypeWait;
					AI::record_add(player.ref()->ai_record_id, ai_record_tag, action);
				}
				ai_record_tag.init(player.ref()->get<PlayerManager>());
			}

			get<Drone>()->crawl(remote_control.movement, u.time.delta);
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
			if (input_enabled()
				&& get<Animator>()->layers[3].animation == AssetNull
				&& !u.input->get(Controls::InteractSecondary, gamepad)
				&& u.last_input->get(Controls::InteractSecondary, gamepad))
			{
				Interactable* interactable = Interactable::closest(get<Transform>()->absolute_pos());
				if (interactable)
				{
					switch (interactable->type)
					{
						case Interactable::Type::Terminal:
						{
							switch (Game::save.zones[Game::level.id])
							{
								case ZoneState::PvpHostile:
								{
									if (Game::level.post_pvp || platform::timestamp() - Game::save.zone_lost_times[Game::level.id] < ZONE_LOST_COOLDOWN)
									{
										// terminal is temporarily locked, must leave and come back
										player.ref()->msg(_(strings::terminal_locked), false);
									}
									else if (Game::level.max_teams <= 2 || Game::save.group != Net::Master::Group::None) // if the map requires more than two players, you must be in a group
									{
										if (Game::save.resources[s32(Resource::Drones)] < DEFAULT_ASSAULT_DRONES)
										{
											Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_resource), DEFAULT_ASSAULT_DRONES, _(strings::drones));
										}
										else
											Menu::dialog_with_cancel(gamepad, &player_confirm_terminal_interactable, &player_cancel_interactable, _(strings::confirm_capture), DEFAULT_ASSAULT_DRONES);
									}
									else
									{
										// must be in a group
										player.ref()->msg(_(strings::group_required), false);
									}
									break;
								}
								case ZoneState::ParkourUnlocked:
								{
									vi_assert(false); // shouldn't be any terminals in parkour zones
									break;
								}
								case ZoneState::Locked:
								{
									interactable->interact();
									get<Animator>()->layers[3].play(Asset::Animation::character_interact);
									get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_INTERACT);
									anim_base = interactable->entity();
									break;
								}
								case ZoneState::PvpFriendly:
								{
									// zone is already owned
									player.ref()->msg(_(strings::zone_already_captured), false);
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
							s8 track = s8(interactable->user_data);
							AssetID target_level = Game::level.tram_tracks[track].level;
							Tram* tram = Tram::by_track(track);
							if (tram->doors_open() // if the tram doors are open, we can always close them
								|| (!tram->arrive_only && target_level != AssetNull // if the target zone doesn't exist, or if the tram is for arrivals only, nothing else matters, we can't do anything
									&& (Game::save.zones[target_level] == ZoneState::ParkourUnlocked // if we've already unlocked it, go ahead
									|| (Overworld::zone_is_pvp(target_level) && Game::save.resources[s32(Resource::Drones)] >= DEFAULT_ASSAULT_DRONES)))) // if it's a PvP zone, we need x drones to capture it
							{
								// go right ahead
								interactable->interact();
								get<Animator>()->layers[3].play(Asset::Animation::character_interact);
								get<Audio>()->post_event(AK::EVENTS::PLAY_PARKOUR_INTERACT);
								anim_base = interactable->entity();
							}
							else if (tram->arrive_only || target_level == AssetNull) // can't leave
								player.ref()->msg(_(strings::zone_unavailable), false);
							else if (Overworld::zone_is_pvp(target_level))
								Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_drones), DEFAULT_ASSAULT_DRONES);
							else if (Game::save.resources[s32(Resource::AccessKeys)] > 0) // ask if they want to use a key
								Menu::dialog(gamepad, &player_confirm_tram_interactable, _(strings::confirm_spend), 1, _(strings::access_keys));
							else // not enough
								Menu::dialog(gamepad, &Menu::dialog_no_action, _(strings::insufficient_resource), 1, _(strings::access_keys));
							break;
						}
						case Interactable::Type::Shop:
						{
							Overworld::show(player.ref()->camera.ref(), Overworld::State::StoryModeOverlay, Overworld::StoryTab::Inventory);
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

			if (anim_base.ref())
			{
				// an animation is playing
				// position player where they need to be
				// if anim_base is an interactable, place the player directly in front of it

				if (get<Animator>()->layers[3].animation == AssetNull)
					anim_base = nullptr; // animation done
				else
				{
					// desired rotation / position
					Vec3 target_pos;
					r32 target_angle;
					if (anim_base.ref()->has<Interactable>())
					{
						get_interactable_standing_position(anim_base.ref()->get<Transform>(), &target_pos, &target_angle);

						// lerp to interactable
						r32 angle = fabsf(LMath::angle_to(get<PlayerCommon>()->angle_horizontal, target_angle));
						get<PlayerCommon>()->angle_horizontal = LMath::lerpf(vi_min(1.0f, (INTERACT_LERP_ROTATION_SPEED / angle) * u.time.delta), get<PlayerCommon>()->angle_horizontal, LMath::closest_angle(target_angle, get<PlayerCommon>()->angle_horizontal));
						get<PlayerCommon>()->angle_vertical = LMath::lerpf(vi_min(1.0f, (INTERACT_LERP_ROTATION_SPEED / fabsf(get<PlayerCommon>()->angle_vertical)) * u.time.delta), get<PlayerCommon>()->angle_vertical, -arm_angle_offset);

						Vec3 abs_pos = get<Transform>()->absolute_pos();
						r32 distance = (abs_pos - target_pos).length();
						if (distance > 0.0f)
							get<Walker>()->absolute_pos(Vec3::lerp(vi_min(1.0f, (INTERACT_LERP_TRANSLATION_SPEED / distance) * u.time.delta), abs_pos, target_pos));
					}
					else
					{
						get_standing_position(anim_base.ref()->get<Transform>(), &target_pos, &target_angle);
						// instantly teleport
						get<Walker>()->absolute_pos(target_pos);
						get<PlayerCommon>()->angle_horizontal = target_angle;
						get<PlayerCommon>()->angle_vertical = 0.0f;
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
					Overworld::show(player.ref()->camera.ref(), Overworld::State::StoryMode);
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
				Vec3 wall_normal = get<Parkour>()->last_support.ref()->get<Transform>()->to_world_normal(get<Parkour>()->relative_wall_run_normal);

				Vec3 forward = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Vec3(0, 0, 1);

				if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Forward)
					get<PlayerCommon>()->clamp_rotation(-wall_normal); // make sure we're always facing the wall
				else
				{
					// we're running along the wall
					// make sure we can't look backward
					get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation, 0) * Vec3(0, 0, 1));
					if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Left)
						get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation + PI * -0.5f, 0) * Vec3(0, 0, 1));
					else
						get<PlayerCommon>()->clamp_rotation(Quat::euler(0, get<Walker>()->rotation + PI * 0.5f, 0) * Vec3(0, 0, 1));
				}
			}
			else if (parkour_state == Parkour::State::HardLanding
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

void PlayerControlHuman::cinematic(Entity* basis, AssetID anim)
{
	vi_assert(has<Parkour>());

	get<Animator>()->layers[3].set(anim, 0.0f);

	Vec3 target_pos;
	r32 target_angle;
	if (basis->has<Interactable>())
		get_interactable_standing_position(basis->get<Transform>(), &target_pos, &target_angle);
	else
		get_standing_position(basis->get<Transform>(), &target_pos, &target_angle);

	get<PlayerCommon>()->angle_horizontal = get<Parkour>()->last_angle_horizontal = get<Walker>()->rotation = get<Walker>()->target_rotation = target_angle;
	get<PlayerCommon>()->angle_vertical = 0.0f;
	get<Parkour>()->lean = 0.0f;
	get<Walker>()->absolute_pos(target_pos);

	anim_base = basis;
}

b8 PlayerControlHuman::cinematic_active() const
{
	// cinematic is active if we're playing an animation on layer 3
	// however, the collectible pickup animation also runs on layer 3 and it's not a cinematic
	AssetID anim = get<Animator>()->layers[3].animation;
	return anim != AssetNull && anim != Asset::Animation::character_pickup;
}

void PlayerControlHuman::update_late(const Update& u)
{
	if (has<Parkour>()
		&& !Overworld::modal()
		&& local())
	{
		Camera* camera = player.ref()->camera.ref();

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

		{
			// camera bone affects rotation only
			Quat camera_animation = Quat::euler(PI * -0.5f, 0, 0);
			get<Animator>()->bone_transform(Asset::Bone::character_camera, nullptr, &camera_animation);
			camera->rot = Quat::euler(get<Parkour>()->lean, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical) * Quat::euler(0, PI * 0.5f, 0) * camera_animation * Quat::euler(0, PI * -0.5f, 0);

			camera->pos = Vec3(0, 0, 0.1f);
			Quat q = Quat::identity;
			get<Parkour>()->head_to_object_space(&camera->pos, &q);
			camera->pos = get<Transform>()->to_world(camera->pos);

			// third-person
			//camera->pos += camera->rot * Vec3(0, 0, -2);
		}

		// wind sound and camera shake at high speed
		{
			r32 speed = get<Parkour>()->fsm.current == Parkour::State::Mantle || get<Walker>()->support.ref()
				? 0.0f
				: get<RigidBody>()->btBody->getInterpolationLinearVelocity().length();
			get<Audio>()->param(AK::GAME_PARAMETERS::PARKOUR_WIND, LMath::clampf((speed - 8.0f) / 25.0f, 0, 1));
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
		|| params.camera != player.ref()->camera.ref()
		|| Overworld::active()
		|| Team::match_state == Team::MatchState::Done)
		return;

	const Rect2& viewport = params.camera->viewport;

	r32 range = has<Drone>() ? get<Drone>()->range() : DRONE_MAX_DISTANCE;

	AI::Team team = get<AIAgent>()->team;

#if DEBUG_NET_SYNC
	Vec3 remote_abs_pos = remote_control.pos;
	if (remote_control.parent.ref())
		remote_abs_pos = remote_control.parent.ref()->to_world(remote_abs_pos);
	UI::indicator(params, remote_abs_pos, UI::color_default, false);
#endif

	// target indicators
	for (s32 i = 0; i < target_indicators.length; i++)
	{
		const TargetIndicator& indicator = target_indicators[i];
		switch (indicator.type)
		{
			case TargetIndicator::Type::DroneVisible:
			{
				UI::indicator(params, indicator.pos, UI::color_alert(), false);
				break;
			}
			case TargetIndicator::Type::DroneOutOfRange:
			{
				UI::indicator(params, indicator.pos, UI::color_alert(), true);
				break;
			}
			case TargetIndicator::Type::Battery:
			{
				UI::indicator(params, indicator.pos, UI::color_accent(), true, 1.0f, PI);
				break;
			}
			case TargetIndicator::Type::BatteryFriendly:
			{
				UI::indicator(params, indicator.pos, Team::ui_color_friend, false, 1.0f, PI);
				break;
			}
			case TargetIndicator::Type::BatteryOutOfRange:
			{
				UI::indicator(params, indicator.pos, UI::color_accent(), false, 1.0f, PI);
				break;
			}
			case TargetIndicator::Type::Minion:
			{
				UI::indicator(params, indicator.pos, UI::color_alert(), false);
				break;
			}
			case TargetIndicator::Type::MinionAttacking:
			{
				if (UI::flash_function(Game::time.total))
					UI::indicator(params, indicator.pos, UI::color_alert(), true);
				break;
			}
			case TargetIndicator::Type::Turret:
			case TargetIndicator::Type::CoreModule:
			{
				UI::indicator(params, indicator.pos, Team::ui_color_enemy, true);
				break;
			}
			case TargetIndicator::Type::TurretFriendly:
			case TargetIndicator::Type::CoreModuleFriendly:
			{
				UI::indicator(params, indicator.pos, Team::ui_color_friend, false);
				break;
			}
			case TargetIndicator::Type::TurretOutOfRange:
			case TargetIndicator::Type::CoreModuleOutOfRange:
			{
				UI::indicator(params, indicator.pos, Team::ui_color_enemy, false);
				break;
			}
			case TargetIndicator::Type::TurretAttacking:
			{
				if (UI::flash_function(Game::time.total))
					UI::indicator(params, indicator.pos, UI::color_alert(), true);
				break;
			}
			case TargetIndicator::Type::Sensor:
			case TargetIndicator::Type::ForceField:
			case TargetIndicator::Type::Grenade:
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

		AI::Team my_team = get<AIAgent>()->team;
		if (Turret::list.count() > 0)
		{
			// turret health bars
			if (Game::level.mode == Game::Mode::Pvp && Game::level.has_feature(Game::FeatureLevel::Turrets))
			{
				for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
				{
					Vec3 turret_pos = i.item()->get<Transform>()->absolute_pos();

					if (i.item()->team == my_team || (turret_pos - me).length_squared() < range * range)
					{
						Vec2 p;
						if (UI::project(params, turret_pos, &p))
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
			}
		}

		// force field health bars
		for (auto i = ForceField::list.iterator(); !i.is_last(); i.next())
		{
			if (!(i.item()->flags & ForceField::FlagPermanent))
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
						Health* hp = i.item()->get<Health>();
						UI::box(params, { bar.pos, Vec2(bar.size.x * (r32(hp->hp) / r32(hp->hp_max)), bar.size.y) }, color);
					}
				}
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
			if (i.item()->team != my_team && i.item()->visible())
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
	}

	if (has<Drone>())
	{
		PlayerManager* manager = player.ref()->get<PlayerManager>();

		// highlight upgrade point if there is an upgrade available
		if (Game::level.has_feature(Game::FeatureLevel::Abilities)
			&& Game::session.config.allow_upgrades
			&& (Game::level.has_feature(Game::FeatureLevel::All) || Game::level.feature_level == Game::FeatureLevel::Abilities) // disable prompt in tutorial after ability has been purchased
			&& manager->upgrade_available() && manager->upgrade_highest_owned_or_available() != player.ref()->upgrade_last_visit_highest_available
			&& !UpgradeStation::drone_at(get<Drone>()))
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

		Interactable* closest_interactable = Interactable::closest(get<Transform>()->absolute_pos());

		b8 resource_changed = false;
		for (s32 i = 0; i < s32(Resource::count); i++)
		{
			if (Game::real_time.total - Overworld::resource_change_time(Resource(i)) < 2.0f)
			{
				resource_changed = true;
				break;
			}
		}

		if (closest_interactable || resource_changed)
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
						: (Game::save.resources[i] == 0 ? UI::color_alert() : UI::color_accent());
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
				text.color = UI::color_accent();
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
					if (i.item()->arrive_only)
						continue;

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
									case ZoneState::PvpFriendly:
									{
										text.color = Team::ui_color_friend;
										break;
									}
									case ZoneState::ParkourUnlocked:
									{
										text.color = UI::color_default;
										break;
									}
									case ZoneState::Locked:
									{
										if (Overworld::zone_is_pvp(zone))
											text.color = Game::save.resources[s32(Resource::Drones)] >= DEFAULT_ASSAULT_DRONES ? UI::color_default : UI::color_disabled();
										else
											text.color = Game::save.resources[s32(Resource::AccessKeys)] > 0 ? UI::color_default : UI::color_disabled();
										break;
									}
									case ZoneState::PvpHostile:
									{
										text.color = Team::ui_color_enemy;
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

				// highlight shop
				if (Game::level.shop.ref())
				{
					Vec3 pos = Game::level.shop.ref()->get<Transform>()->absolute_pos();
					Vec3 to_shop = pos - params.camera->pos;
					r32 distance = to_shop.length();
					if (distance > 8.0f)
					{
						to_shop /= distance;
						if (to_shop.dot(look_dir) > 0.92f)
						{
							Vec2 p;
							if (UI::project(params, pos + Vec3(0, 3, 0), &p))
							{
								UIText text;
								text.color = UI::color_default;
								text.text(player.ref()->gamepad, _(strings::shop));
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
					text.color = UI::color_accent();
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

				{
					PlayerManager* other_manager = other_player.item()->manager.ref();
					s32 ability_count = other_manager->ability_count();
					if (ability_count > 0)
					{
						r32 item_size = text_size * UI::scale * 0.75f;
						Vec2 p2 = username_pos + Vec2((ability_count * -0.5f + 0.5f) * item_size + ((ability_count - 1) * HP_BOX_SPACING * -0.5f), (text_size * UI::scale) + item_size);
						UI::box(params, { Vec2(p2.x + item_size * -0.5f - HP_BOX_SPACING, p2.y + item_size * -0.5f - HP_BOX_SPACING), Vec2((ability_count * item_size) + ((ability_count + 1) * HP_BOX_SPACING), item_size + HP_BOX_SPACING * 2.0f) }, UI::color_background);
						for (s32 i = 0; i < ability_count; i++)
						{
							const AbilityInfo& info = AbilityInfo::list[s32(other_manager->abilities[i])];
							UI::mesh(params, info.icon, p2, Vec2(item_size), *color);
							p2.x += item_size + HP_BOX_SPACING;
						}
					}
				}
			}
		}
	}

	const Health* health = get<Health>();

	b8 is_vulnerable = !get<AIAgent>()->stealth && get<Health>()->can_take_damage() && health->hp == 1 && health->shield == 0 && Game::session.config.drone_shield > 0;

	// compass
	{
		Vec2 compass_size = Vec2(vi_min(viewport.size.x, viewport.size.y) * 0.3f);
		if (is_vulnerable && get<PlayerCommon>()->incoming_attacker())
		{
			// we're being attacked; flash the compass
			b8 show = UI::flash_function(Game::real_time.total);
			if (show)
				UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_alert());
			if (show && !UI::flash_function(Game::real_time.total - Game::real_time.delta))
				Audio::post_global_event(AK::EVENTS::PLAY_DANGER_BEEP);
		}
		else if (enemy_visible && !get<AIAgent>()->stealth)
			UI::mesh(params, Asset::Mesh::compass, viewport.size * Vec2(0.5f, 0.5f), compass_size, UI::color_alert());
	}

	{
		// danger indicator

		b8 danger = enemy_visible && (enemy_dangerous_visible || is_vulnerable) && !get<AIAgent>()->stealth;

		if (danger)
		{
			UIText text;
			text.size = 24.0f;
			text.color = UI::color_alert();
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;

			text.text(player.ref()->gamepad, _(strings::danger));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.25f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (is_vulnerable ? UI::flash_function(Game::real_time.total) : UI::flash_function_slow(Game::real_time.total))
				text.draw(params, pos);
		}

		// shield indicator
		if (is_vulnerable)
		{
			UIText text;
			text.color = UI::color_alert();
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.text(player.ref()->gamepad, _(strings::shield_down));

			Vec2 pos = viewport.size * Vec2(0.5f, 0.625f);

			Rect2 box = text.rect(pos).outset(8 * UI::scale);
			UI::box(params, box, UI::color_background);
			if (danger ? UI::flash_function(Game::real_time.total) : UI::flash_function_slow(Game::real_time.total))
				text.draw(params, pos);
		}
	}

	// stealth indicator
	if (get<AIAgent>()->stealth)
	{
		UIText text;
		text.color = UI::color_accent();
		text.text(player.ref()->gamepad, _(strings::stealth));
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Center;
		text.size = text_size;
		Vec2 pos = viewport.size * Vec2(0.5f, 0.675f);
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
					text.color = UI::color_alert();
					text.draw(params, pos);
				}
			}
			else
			{
				// draw bar
				Rect2 box = text.rect(pos).outset(MENU_ITEM_PADDING);
				UI::box(params, box, UI::color_background);
				UI::border(params, box, 2, UI::color_alert());
				UI::box(params, { box.pos, Vec2(box.size.x * detect_danger, box.size.y) }, UI::color_alert());

				text.color = UI::color_background;
				text.draw(params, pos);

				// todo: sound
			}
		}
	}

	// reticle
	if (has<Drone>()
		&& movement_enabled()
#if !SERVER
		&& Net::Client::replay_mode() != Net::Client::ReplayMode::Replaying
#endif
		)
	{
		Vec2 pos = viewport.size * Vec2(0.5f, 0.5f);
		const r32 spoke_length = 12.0f;
		const r32 spoke_width = 3.0f;
		const r32 start_radius = 8.0f + spoke_length * 0.5f;

		b8 cooldown_can_go = get<Drone>()->cooldown_can_shoot();

		b8 reticle_valid;
		const Vec4* color;
		if (reticle.type == ReticleType::Error || reticle.type == ReticleType::DashError)
		{
			color = &UI::color_disabled();
			reticle_valid = false;
		}
		else if (reticle.type != ReticleType::None
			&& cooldown_can_go
			&& (get<Drone>()->current_ability == Ability::None || player.ref()->get<PlayerManager>()->ability_valid(get<Drone>()->current_ability)))
		{
			color = &UI::color_accent();
			reticle_valid = true;
		}
		else
		{
			color = &UI::color_alert();
			reticle_valid = false;
		}

		// cooldown indicator
		{
			s32 charges = get<Drone>()->charges;
			if (charges == 0)
			{
				r32 cooldown_scale = (get<Drone>()->cooldown - Drone::cooldown_thresholds[0]) / (DRONE_COOLDOWN - Drone::cooldown_thresholds[0]);
				UI::triangle_border(params, { pos, Vec2((start_radius + spoke_length) * (2.5f + 5.0f * cooldown_scale) * UI::scale) }, spoke_width, UI::color_alert(), PI);
			}
			else
			{
				const Vec2 box_size = Vec2(10.0f) * UI::scale;
				for (s32 i = 0; i < charges; i++)
				{
					Vec2 p = pos + Vec2(0.0f, -36.0f + i * -16.0f) * UI::scale;
					UI::triangle(params, { p, box_size }, *color, PI);
				}
			}
		}

		// reticle
		{
			const r32 ratio = 0.8660254037844386f;
			if (reticle_valid)
			{
				// triangular crosshair
				UI::centered_box(params, { pos + Vec2(ratio, 0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, *color, PI * 0.5f * 0.33f);
				UI::centered_box(params, { pos + Vec2(-ratio, 0.5f) * UI::scale * start_radius, Vec2(spoke_length, spoke_width) * UI::scale }, *color, PI * 0.5f * -0.33f);
				UI::centered_box(params, { pos + Vec2(0, -1.0f) * UI::scale * start_radius, Vec2(spoke_width, spoke_length) * UI::scale }, *color);
			}
			else
			{
				// crossbars
				UI::centered_box(params, { pos, Vec2(spoke_length * 3.0f, spoke_width) * UI::scale }, *color, PI * 0.25f);
				UI::centered_box(params, { pos, Vec2(spoke_length * 3.0f, spoke_width) * UI::scale }, *color, PI * 0.75f);
			}

			if (get<Drone>()->current_ability != Ability::None)
			{
				Ability a = get<Drone>()->current_ability;
				Vec2 p = pos + Vec2(0, (-128.0f + text_size + 8.0f) * UI::scale);
				UI::centered_box(params, { p, Vec2(34.0f * UI::scale) }, UI::color_background);
				UI::mesh(params, AbilityInfo::list[s32(a)].icon, p, Vec2(18.0f * UI::scale), *color);

				// cancel prompt
				UIText text;
				text.color = UI::color_accent();
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
				p = pos + Vec2(0, -128.0f * UI::scale);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, p);
			}

			if (reticle_valid && (reticle.type == ReticleType::Normal || reticle.type == ReticleType::Target || reticle.type == ReticleType::DashTarget))
			{
				Vec2 a;
				if (UI::project(params, reticle.pos, &a))
					UI::triangle(params, { a, Vec2(10.0f * UI::scale) }, reticle.type == ReticleType::Target || reticle.type == ReticleType::DashTarget ? UI::color_alert() : *color, PI);
			}
		}
	}
}


}