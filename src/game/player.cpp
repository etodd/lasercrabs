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

namespace VI
{

#define fov_initial (80.0f * PI * 0.5f / 180.0f)
#define zoom_ratio 0.5f
#define zoom_speed (1.0f / 0.1f)
#define speed_mouse 0.0025f
#define fov_zoom (fov_initial * zoom_ratio)
#define speed_mouse_zoom (speed_mouse * zoom_ratio)
#define speed_joystick 3.0f
#define speed_joystick_zoom (speed_joystick * zoom_ratio)
#define joystick_dead_zone 0.1f
#define attach_speed 5.0f
#define max_attach_time 0.35f
#define rotation_speed 20.0f
#define msg_time 0.75f

#define credits_initial 50
#define credits_pickup 25

#if DEBUG
#define PLAYER_SPAWN_DELAY 1.0f
#define MINION_SPAWN_INITIAL_DELAY 2.0f
#else
#define PLAYER_SPAWN_DELAY 5.0f
#define MINION_SPAWN_INITIAL_DELAY 10.0f
#endif
#define MINION_SPAWN_INTERVAL 30.0f
#define MINION_SPAWN_GROUP_INTERVAL 2.0f

b8 flash_function(r32 time)
{
	return (b8)((s32)(time * 16.0f) % 2);
}

void draw_indicator(const RenderParams& params, const Vec3& pos, const Vec4& color, b8 flash = false)
{
	b8 show;
	if (flash)
		show = flash_function(params.sync->time.total);
	else
		show = true;
	if (show)
	{
		Vec2 screen_pos;
		b8 on_screen = UI::project(params, pos, &screen_pos);
		Vec2 center = params.camera->viewport.size * 0.5f;
		Vec2 offset = screen_pos - center;
		if (!on_screen)
			offset *= -1.0f;

		r32 radius = fmin(params.camera->viewport.size.x, params.camera->viewport.size.y) * 0.95f * 0.5f;

		r32 offset_length = offset.length();
		if (offset_length > radius || (offset_length > 0.0f && !on_screen))
		{
			offset *= radius / offset_length;
			UI::triangle(params, { center + offset, Vec2(32) * UI::scale }, color, atan2f(offset.y, offset.x) + PI * -0.5f);
		}
		else
			UI::centered_border(params, { center + offset, Vec2(32) * UI::scale }, 4, color, PI * 0.25f);
	}
}

StaticArray<Team, (s32)AI::Team::count> Team::list;

Team::Team()
	: minion_spawn_timer(MINION_SPAWN_INITIAL_DELAY)
{
}

r32 minion_spawn_delay(Team::MinionSpawnState state)
{
	if (state == Team::MinionSpawnState::One)
		return MINION_SPAWN_INTERVAL;
	else
		return MINION_SPAWN_GROUP_INTERVAL;
}

void Team::update(const Update& u)
{
	if (minion_spawns.length > 0)
	{
		minion_spawn_timer -= u.time.delta;
		if (minion_spawn_timer < 0)
		{
			for (int i = 0; i < minion_spawns.length; i++)
			{
				Vec3 pos;
				Quat rot;
				minion_spawns[i].ref()->absolute(&pos, &rot);
				pos += rot * Vec3(0, 0, 1); // spawn it around the edges
				Entity* entity = World::create<Minion>(pos, rot, team());
			}
			minion_spawn_state = (MinionSpawnState)(((s32)minion_spawn_state + 1) % (s32)MinionSpawnState::count);
			minion_spawn_timer = minion_spawn_delay(minion_spawn_state);
		}
	}
}

PinArray<PlayerManager, MAX_PLAYERS> PlayerManager::list;

PlayerManager::PlayerManager(Team* team)
	: spawn_timer(PLAYER_SPAWN_DELAY),
	team(team),
	credits(credits_initial)
{
}

void PlayerManager::update(const Update& u)
{
	if (team.ref()->player_spawn.ref() && !entity.ref())
	{
		spawn_timer -= u.time.delta;
		if (spawn_timer < 0 || Game::data.mode == Game::Mode::Parkour)
		{
			spawn.fire();
			spawn_timer = PLAYER_SPAWN_DELAY;
		}
	}
}

PinArray<LocalPlayer, MAX_PLAYERS> LocalPlayer::list;

LocalPlayer::LocalPlayer(PlayerManager* m, u8 g)
	: gamepad(g),
	manager(m),
	pause(),
	camera(),
	credits_text(),
	msg_text(),
	msg_timer(msg_time),
	menu(),
	revision(),
	options_menu()
{
	sprintf(manager.ref()->username, "Player %d", gamepad);

	msg_text.font = Asset::Font::lowpoly;
	msg_text.size = 18.0f;
	msg_text.color = UI::default_color;
	msg_text.anchor_x = UIText::Anchor::Center;
	msg_text.anchor_y = UIText::Anchor::Center;

	credits_text.font = Asset::Font::lowpoly;
	credits_text.size = 18.0f;
	credits_text.color = UI::default_color;
	credits_text.anchor_x = UIText::Anchor::Center;

	m->spawn.link<LocalPlayer, &LocalPlayer::spawn>(this);
}

LocalPlayer::UIMode LocalPlayer::ui_mode() const
{
	if (Menu::is_special_level(Game::data.level))
		return UIMode::Default;
	else if (pause)
		return UIMode::Pause;
	else if (manager.ref()->entity.ref() || NoclipControl::list.count() > 0)
		return UIMode::Default;
	else
		return UIMode::Spawning;
}

void LocalPlayer::msg(const char* msg)
{
	msg_text.text(msg);
	msg_timer = 0.0f;
}

void LocalPlayer::ensure_camera(const Update& u, b8 active)
{
	if (active && !camera && !manager.ref()->entity.ref())
	{
		camera = Camera::add();
		camera->fog = Game::data.mode == Game::Mode::Parkour;
		camera->mask = 1 << (s32)manager.ref()->team.ref()->team();
		s32 player_count = list.count();
		Camera::ViewportBlueprs32* viewports = Camera::viewport_blueprs32s[player_count - 1];
		Camera::ViewportBlueprs32* blueprs32 = &viewports[gamepad];

		camera->viewport =
		{
			Vec2((s32)(blueprs32->x * (r32)u.input->width), (s32)(blueprs32->y * (r32)u.input->height)),
			Vec2((s32)(blueprs32->w * (r32)u.input->width), (s32)(blueprs32->h * (r32)u.input->height)),
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

void LocalPlayer::update(const Update& u)
{
	if (Console::visible)
		return;

	credits_text.text("+%d", manager.ref()->credits);

	if (msg_timer < msg_time)
		msg_timer += u.time.delta;

	if (((u.last_input->gamepads[gamepad].btns & Gamepad::Btn::Start) && !(u.input->gamepads[gamepad].btns & Gamepad::Btn::Start))
		|| (gamepad == 0 && u.last_input->keys[(s32)KeyCode::Escape] && !u.input->keys[(s32)KeyCode::Escape]))
	{
		if (options_menu)
			options_menu = false;
		else
			pause = !pause;
	}

	switch (ui_mode())
	{
		case UIMode::Default:
		{
			// Nothing going on
			ensure_camera(u, false);
			if (manager.ref()->entity.ref())
				manager.ref()->entity.ref()->get<LocalPlayerControl>()->enable_input = true;
			break;
		}
		case UIMode::Pause:
		{
			// Paused

			ensure_camera(u, true);

			if (manager.ref()->entity.ref())
				manager.ref()->entity.ref()->get<LocalPlayerControl>()->enable_input = false;

			Rect2& viewport = camera ? camera->viewport : manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera->viewport;
			menu.start(u, gamepad);

			if (options_menu)
			{
				Vec2 pos = viewport.pos + Vec2(0, viewport.size.y * 0.5f + Menu::options_height() * 0.5f);
				if (!Menu::options(u, gamepad, &menu, &pos))
					options_menu = false;
			}
			else
			{
				Vec2 pos = viewport.pos + Vec2(0, viewport.size.y * 0.5f + UIMenu::height(3) * 0.5f);
				if (menu.item(u, gamepad, &pos, "Resume"))
					pause = false;
				if (menu.item(u, gamepad, &pos, "Options"))
					options_menu = true;
				if (menu.item(u, gamepad, &pos, "Main menu"))
					Menu::title();
			}

			menu.end();

			break;
		}
		case UIMode::Spawning:
		{
			// Player is currently dead
			ensure_camera(u, true);

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

	if (Game::data.mode == Game::Mode::Multiplayer)
	{
		// Spawn AWK
		pos += Quat::euler(0, angle + (gamepad * PI * 0.5f), 0) * Vec3(0, 0, 1); // spawn it around the edges
		spawned = World::create<AwkEntity>(manager.ref()->team.ref()->team());
	}
	else // parkour mode
	{
		// Spawn traceur
		spawned = World::create<Traceur>(pos, Quat::euler(0, angle, 0), manager.ref()->team.ref()->team());
	}

	spawned->get<Transform>()->absolute(pos, rot);
	spawned->add<PlayerCommon>(manager.ref()->username);
	manager.ref()->entity = spawned;

	LocalPlayerControl* control = spawned->add<LocalPlayerControl>(gamepad);
	control->player = this;
	control->angle_horizontal = angle;
}

void LocalPlayer::draw_alpha(const RenderParams& params) const
{
	if (params.camera != camera && (!manager.ref()->entity.ref() || params.camera != manager.ref()->entity.ref()->get<LocalPlayerControl>()->camera))
		return;

	const r32 line_thickness = 2.0f * UI::scale;
	const r32 padding = 6.0f * UI::scale;

	const Rect2& vp = params.camera->viewport;

	Entity* player = manager.ref()->entity.ref();
	Vec3 player_pos;
	if (player)
		player_pos = player->get<Transform>()->absolute_pos();

	// draw health bars
	const r32 far_plane = Skybox::far_plane * Skybox::far_plane;
	for (auto i = Health::list.iterator(); !i.is_last(); i.next())
	{
		const Health* health = i.item();
		if (health->entity() == manager.ref()->entity.ref()) // don't draw our own health bar right now
			continue;

		b8 visible;
		Vec3 enemy_pos = health->get<Transform>()->absolute_pos();
		if (health->get<AIAgent>()->team == manager.ref()->team.ref()->team())
			visible = true;
		else if (player && health->has<PlayerCommon>())
			visible = PlayerCommon::visibility[PlayerCommon::visibility_hash(health->get<PlayerCommon>(), player->get<PlayerCommon>())];
		else
		{
			Vec3 diff = enemy_pos - player_pos;
			if (diff.length_squared() < far_plane)
			{
				if (btVector3(diff).fuzzyZero())
					visible = true;
				else
				{
					btCollisionWorld::ClosestRayResultCallback ray_callback(player_pos, enemy_pos);
					Physics::raycast(&ray_callback);
					visible = !ray_callback.hasHit() || ray_callback.m_collisionObject->getUserIndex() == health->entity_id;
				}
			}
			else
				visible = false;
		}

		if (visible)
		{
			enemy_pos.y += 0.75f;
			const Vec4& color = AI::colors[(s32)health->get<AIAgent>()->team];
			Vec2 pos;
			if (UI::project(params, enemy_pos, &pos))
			{
				pos.y += 24.0f * UI::scale;
				const Vec2 size = Vec2(32.0f, 2.0f) * UI::scale;
				UI::box(params, { pos - size * 0.5f, size }, UI::background_color);
				UI::box(params, { pos - size * 0.5f, size * Vec2((r32)health->hp / (r32)health->total, 1.0f) }, color);
				UI::border(params, { pos - size * 0.5f, size }, 2, color);
			}
		}
	}

	// Draw message
	if (msg_timer < msg_time)
	{
		r32 last_timer = msg_timer;
		b8 flash = flash_function(msg_timer);
		b8 last_flash = flash_function(msg_timer - params.sync->time.delta);
		if (flash)
		{
			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.6f);
			Rect2 box = msg_text.rect(pos).outset(6 * UI::scale);
			UI::box(params, box, UI::background_color);
			msg_text.draw(params, pos);
			if (!last_flash)
				Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
		}
	}

	if (Game::data.mode == Game::Mode::Multiplayer)
	{
		Vec2 pos = params.camera->viewport.size * 0.1f;
		UI::box(params, credits_text.rect(pos).outset(6 * UI::scale), UI::background_color);
		credits_text.draw(params, pos);
	}

	switch (ui_mode())
	{
		case UIMode::Default:
		{
			break;
		}
		case UIMode::Pause:
		{
			UIText text;
			text.font = Asset::Font::lowpoly;

			menu.draw_alpha(params);
			
			break;
		}
		case UIMode::Spawning:
		{
			// Player is currently dead
			UIText text;
			text.font = Asset::Font::lowpoly;

			text.wrap_width = MENU_ITEM_WIDTH;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.text("Spawning... %d", (s32)manager.ref()->spawn_timer + 1);
			Vec2 p = vp.pos + vp.size * Vec2(0.5f);
			UI::box(params, text.rect(p).outset(8.0f), UI::background_color);
			text.draw(params, p);

			break;
		}
	}
}

b8 PlayerCommon::visibility[] = {};
s32 PlayerCommon::visibility_count = 0;

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

PlayerCommon::PlayerCommon(const char* username)
	: cooldown(),
	username_text(),
	visibility_index()
{
	username_text.font = Asset::Font::lowpoly;
	username_text.size = 18.0f;
	username_text.anchor_x = UIText::Anchor::Center;
	username_text.text(username);
}

void PlayerCommon::awake()
{
	get<Health>()->total = AWK_HEALTH;
	username_text.color = AI::colors[(s32)get<AIAgent>()->team];
}

void PlayerCommon::update_visibility()
{
	PlayerCommon* players[MAX_PLAYERS];

	s32 player_count = PlayerCommon::list.count();
	vi_assert(player_count <= MAX_PLAYERS);

	visibility_count = combination(player_count, 2);

	s32 index = 0;
	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		players[index] = i.item()->get<PlayerCommon>();
		i.item()->visibility_index = index;
		index++;
	}

	index = 0;
	for (s32 i = 0; i < player_count; i++)
	{
		for (s32 j = i + 1; j < player_count; j++)
		{
			Vec3 start = players[i]->has<Parkour>() ? players[i]->get<Parkour>()->head_pos() : players[i]->get<Awk>()->center();
			Vec3 end = players[j]->has<Parkour>() ? players[j]->get<Parkour>()->head_pos() : players[j]->get<Awk>()->center();
			if (btVector3(end - start).fuzzyZero())
				visibility[index] = true;
			else
			{
				btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
				Physics::raycast(&ray_callback);
				visibility[index] = !ray_callback.hasHit();
			}
			index++;
		}
	}
}

s32 PlayerCommon::visibility_hash(const PlayerCommon* awk_a, const PlayerCommon* awk_b)
{
	b8 a_index_lower = awk_a->visibility_index < awk_b->visibility_index;
	s32 a = a_index_lower ? awk_a->visibility_index : awk_b->visibility_index;
	s32 b = a_index_lower ? awk_b->visibility_index : awk_a->visibility_index;
	s32 a_index = visibility_count - combination(visibility_count - a, 2);
	s32 hash = a_index + (b - a) - 1;
	s32 max_hash = visibility_count - 1;
	return hash > 0 ? (hash < max_hash ? hash : max_hash) : 0;
}

void PlayerCommon::draw_alpha(const RenderParams& params) const
{
	// See if we need to render our username
	LocalPlayerControl* player = LocalPlayerControl::player_for_camera(params.camera);

	if (player && player->entity() != entity())
	{
		if (player->get<AIAgent>()->team == get<AIAgent>()->team
			|| PlayerCommon::visibility[PlayerCommon::visibility_hash(this, player->get<PlayerCommon>())])
		{
			Vec3 pos = get<Transform>()->absolute_pos();
			pos.y += 1.0f;
			Vec2 screen_pos;
			if (UI::project(params, pos, &screen_pos))
			{
				screen_pos.y += 32.0f * UI::scale;
				username_text.draw(params, screen_pos + Vec2(0, 32 * UI::scale));
			}
		}
	}
}

void PlayerCommon::update(const Update& u)
{
	if (has<Parkour>() || get<Transform>()->parent.ref())
	{
		// Either we are a Minion, or we're an Awk and we're attached to a surface
		// Either way, we need to decrement the cooldown timer
		cooldown = fmax(0.0f, cooldown - u.time.delta);
	}
}

void LocalPlayerControl::awk_bounce(const Vec3& new_velocity)
{
	Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
	attach_quat = Quat::look(direction);
}

void LocalPlayerControl::awk_attached()
{
	Quat absolute_rot = get<Transform>()->absolute_rot();
	Vec3 wall_normal = absolute_rot * Vec3(0, 0, 1);
	Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
	Vec3 up = Vec3::normalize(wall_normal.cross(direction));
	Vec3 right = direction.cross(up);
	if (right.dot(wall_normal) < 0.0f)
		right *= -1.0f;

	b8 vertical_surface = fabs(direction.y) < 0.99f;
	if (vertical_surface)
	{
		angle_horizontal = atan2f(direction.x, direction.z);
		angle_vertical = -asinf(direction.y);
	}

	// If we are spawning on to a flat floor, set attach_quat immediately
	// This preserves the camera rotation set by the PlayerSpawn
	if (!vertical_surface && direction.y == -1.0f)
		attach_quat = absolute_rot;
	else
		attach_quat = Quat::look(right);

	get<Audio>()->post_event(AK::EVENTS::STOP_FLY);
}

LocalPlayerControl::LocalPlayerControl(u8 gamepad)
	: angle_horizontal(),
	angle_vertical(),
	attach_quat(Quat::identity),
	gamepad(gamepad),
	fov_blend(),
	allow_zoom(true),
	try_jump(),
	try_parkour(),
	enable_input(),
	lean()
{
	camera = Camera::add();
	camera->fog = Game::data.mode == Game::Mode::Parkour;
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
		link_arg<const Vec3&, &LocalPlayerControl::awk_bounce>(get<Awk>()->bounce);
		link_arg<Entity*, &LocalPlayerControl::hit_target>(get<Awk>()->hit);
	}

	camera->mask = 1 << (s32)get<AIAgent>()->team;
}

void LocalPlayerControl::hit_target(Entity* target)
{
	if (target->has<CreditsPickup>())
	{
		player.ref()->manager.ref()->credits += credits_pickup;
		char m[255];
		sprintf(m, "+%d credits", credits_pickup);
		player.ref()->msg(m);
	}
	else
		player.ref()->msg("Target hit");
}

r32 dead_zone(r32 x)
{
	if (fabs(x) < joystick_dead_zone)
		return 0.0f;
	return (x > 0.0f ? x - joystick_dead_zone : x + joystick_dead_zone) * (1.0f / (1.0f - joystick_dead_zone));
}

b8 LocalPlayerControl::input_enabled() const
{
	return !Console::visible && enable_input;
}

void LocalPlayerControl::update_camera_input(const Update& u)
{
	const Settings& settings = Loader::settings();
	if (input_enabled())
	{
		if (gamepad == 0)
		{
			r32 s = LMath::lerpf(fov_blend, speed_mouse, speed_mouse_zoom);
			angle_horizontal -= s * (r32)u.input->cursor_x;
			angle_vertical += s * (r32)u.input->cursor_y;
		}

		if (u.input->gamepads[gamepad].active)
		{
			r32 s = LMath::lerpf(fov_blend, speed_joystick, speed_joystick_zoom);
			angle_horizontal -= s * u.time.delta * dead_zone(u.input->gamepads[gamepad].right_x);
			angle_vertical += s * u.time.delta * dead_zone(u.input->gamepads[gamepad].right_y);
		}

		angle_vertical = LMath::clampf(angle_vertical, PI * -0.495f, PI * 0.495f);
	}
}

Vec3 LocalPlayerControl::get_movement(const Update& u, const Quat& rot)
{
	const Settings& settings = Loader::settings();
	Vec3 movement = Vec3::zero;
	if (input_enabled())
	{
		if (u.input->get(settings.bindings.forward, gamepad))
			movement += rot * Vec3(0, 0, 1);
		if (u.input->get(settings.bindings.backward, gamepad))
			movement += rot * Vec3(0, 0, -1);
		if (u.input->get(settings.bindings.right, gamepad))
			movement += rot * Vec3(-1, 0, 0);
		if (u.input->get(settings.bindings.left, gamepad))
			movement += rot * Vec3(1, 0, 0);
		if (u.input->get(settings.bindings.jump, gamepad))
			movement.y += 1;

		if (u.input->gamepads[gamepad].active)
		{
			movement += rot * Vec3(-dead_zone(u.input->gamepads[gamepad].left_x), 0, 0);
			movement += rot * Vec3(0, 0, -dead_zone(u.input->gamepads[gamepad].left_y));
		}
	}
	return movement;
}

void PlayerCommon::transfer_to(Entity* new_entity)
{
	Entity* old_entity = entity();

	s32 old_health = get<Health>()->hp;

	old_entity->detach<PlayerCommon>(); // this

	LocalPlayerControl* local_control = nullptr;
	if (old_entity->has<LocalPlayerControl>())
	{
		local_control = old_entity->get<LocalPlayerControl>();
		local_control->lean = 0.0f;
		old_entity->detach<LocalPlayerControl>();
	}

	AIPlayerControl* ai_control = nullptr;
	if (old_entity->has<AIPlayerControl>())
	{
		ai_control = old_entity->get<AIPlayerControl>();
		old_entity->detach<AIPlayerControl>();
	}

	World::remove(old_entity);

	new_entity->get<Health>()->hp = old_health;

	new_entity->attach(this);
	awake();

	if (local_control)
	{
		new_entity->attach(local_control);
		local_control->player.ref()->manager.ref()->entity = new_entity;
		local_control->awake();
	}

	if (ai_control)
	{
		new_entity->attach(ai_control);
		ai_control->awake();
	}
}

void LocalPlayerControl::update(const Update& u)
{
	const Settings& settings = Loader::settings();

	{
		// Zoom
		r32 fov_blend_target = 0.0f;
		if (has<Awk>() && input_enabled())
		{
			if (u.input->get(settings.bindings.secondary, gamepad))
			{
				if (allow_zoom)
				{
					fov_blend_target = 1.0f;
					if (!u.last_input->get(settings.bindings.secondary, gamepad))
						get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_IN);
				}
			}
			else
			{
				if (allow_zoom && u.last_input->get(settings.bindings.secondary, gamepad))
					get<Audio>()->post_event(AK::EVENTS::PLAY_ZOOM_OUT);
				allow_zoom = true;
			}
		}

		if (fov_blend < fov_blend_target)
			fov_blend = fmin(fov_blend + u.time.delta * zoom_speed, fov_blend_target);
		else if (fov_blend > fov_blend_target)
			fov_blend = fmax(fov_blend - u.time.delta * zoom_speed, fov_blend_target);
	}

	Vec3 camera_pos;
	Quat look_quat;

	if (has<Awk>())
	{
		// Awk control code
		if (get<Transform>()->parent.ref())
		{
			Quat rot = get<Transform>()->absolute_rot();

			r32 angle = Quat::angle(attach_quat, rot);
			if (angle > 0)
				attach_quat = Quat::slerp(fmin(1.0f, rotation_speed * u.time.delta), attach_quat, rot);

			// Look
			{
				Vec3 attach_normal = attach_quat * Vec3(0, 0, 1);

				update_camera_input(u);

				if (Game::data.mode == Game::Mode::Parkour && attach_normal.y > 0.7f && u.input->get(settings.bindings.parkour, gamepad) && !u.last_input->get(settings.bindings.parkour, gamepad))
				{
					// Switch to sentinel
					Vec3 pos = get<Awk>()->center() + Vec3(0, 1.0f, 0);
					AI::Team old_team = get<AIAgent>()->team;
					Entity* new_entity = World::create<Minion>(pos, Quat::euler(0, angle_horizontal, 0), old_team);
					get<PlayerCommon>()->transfer_to(new_entity);
					return;
				}

				look_quat = Quat::euler(lean, angle_horizontal, angle_vertical);

				Vec3 forward = look_quat * Vec3(0, 0, 1);
				r32 dot = forward.dot(attach_normal);
				if (dot < -0.5f)
				{
					forward = Vec3::normalize(forward - (dot + 0.5f) * attach_normal);
					angle_horizontal = atan2f(forward.x, forward.z);
					angle_vertical = -asinf(forward.y);
					look_quat = Quat::euler(lean, angle_horizontal, angle_vertical);
				}
			}
		}
		else
		{
			Vec3 direction = Vec3::normalize(get<Awk>()->velocity);
			Quat rot = Quat::look(direction);
			r32 angle = Quat::angle(attach_quat, rot);
			if (angle > 0)
				attach_quat = Quat::slerp(fmin(1.0f, rotation_speed * u.time.delta), attach_quat, rot);
			look_quat = attach_quat;
		}

		camera_pos = get<Awk>()->center();
	}
	else
	{
		// Minion control code
		update_camera_input(u);
		look_quat = Quat::euler(lean, angle_horizontal, angle_vertical);
		{
			camera_pos = Vec3(0, 0, 0.05f);
			Quat q = Quat::identity;
			get<Parkour>()->head_to_object_space(&camera_pos, &q);
			camera_pos = get<Transform>()->to_world(camera_pos);
		}

		Vec3 movement = get_movement(u, Quat::euler(0, angle_horizontal, 0));
		Vec2 dir = Vec2(movement.x, movement.z);
		get<Walker>()->dir = dir;

		// parkour button
		b8 parkour_pressed = input_enabled() && u.input->get(settings.bindings.parkour, gamepad);

		if (get<Parkour>()->fsm.current == Parkour::State::WallRun && !parkour_pressed)
			get<Parkour>()->fsm.transition(Parkour::State::Normal);

		if (parkour_pressed && !u.last_input->get(settings.bindings.parkour, gamepad))
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
		b8 jump_pressed = input_enabled() && u.input->get(settings.bindings.jump, gamepad);
		if (jump_pressed && !u.last_input->get(settings.bindings.jump, gamepad))
			try_jump = true;
		else if (!jump_pressed)
			try_jump = false;

		if (try_jump)
		{
			if (get<Parkour>()->try_jump(angle_horizontal))
			{
				try_parkour = false;
				try_jump = false;
				try_slide = false;
			}
		}
		
		// slide button
		b8 slide_pressed = input_enabled() && u.input->get(settings.bindings.slide, gamepad);

		if (get<Parkour>()->fsm.current == Parkour::State::Slide && !slide_pressed)
			get<Parkour>()->fsm.transition(Parkour::State::Normal);

		if (slide_pressed && !u.last_input->get(settings.bindings.slide, gamepad))
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
			r32 arm_angle = LMath::clampf(angle_vertical * 0.5f, -PI * 0.15f, PI * 0.15f);
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_L, Vec3::zero, Quat::euler(arm_angle, 0, 0));
			get<Animator>()->override_bone(Asset::Bone::character_upper_arm_R, Vec3::zero, Quat::euler(-arm_angle, 0, 0));
		}

		r32 lean_target = 0.0f;

		if (get<Parkour>()->fsm.current == Parkour::State::WallRun)
		{
			Parkour::WallRunState state = get<Parkour>()->wall_run_state;
			const r32 wall_run_lean = 8.0f * PI / 180.0f;
			if (state == Parkour::WallRunState::Left)
				lean_target = -wall_run_lean;
			else if (state == Parkour::WallRunState::Right)
				lean_target = wall_run_lean;

			Vec3 wall_normal = get<Parkour>()->last_support.ref()->get<Transform>()->to_world_normal(get<Parkour>()->relative_wall_run_normal);

			Vec3 forward = look_quat * Vec3(0, 0, 1);

			if (get<Parkour>()->wall_run_state == Parkour::WallRunState::Forward)
				wall_normal *= -1.0f; // Make sure we're always facing the wall
			else
			{
				// We're running along the wall
				// Make sure we can't look backward
				clamp_rotation(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1));
			}

			clamp_rotation(wall_normal);
		}
		else if (get<Parkour>()->fsm.current == Parkour::State::Slide)
		{
			lean_target = (PI / 180.0f) * 15.0f;
			clamp_rotation(Quat::euler(0, get<Walker>()->target_rotation, 0) * Vec3(0, 0, 1));
		}
		else
		{
			lean_target = get<Walker>()->net_speed * (LMath::closest_angle(last_angle_horizontal, angle_horizontal) - angle_horizontal) * fmin((1.0f / 180.0f) / u.time.delta, 1.0f);

			get<Walker>()->target_rotation = angle_horizontal;

			// make sure our body is facing within 90 degrees of our target rotation
			r32 delta = LMath::angle_to(get<Walker>()->rotation, angle_horizontal);
			if (delta > PI * 0.5f)
				get<Walker>()->rotation = LMath::angle_range(get<Walker>()->rotation + delta - PI * 0.5f);
			else if (delta < PI * -0.5f)
				get<Walker>()->rotation = LMath::angle_range(get<Walker>()->rotation + delta + PI * 0.5f);
		}

		lean += (lean_target - lean) * fmin(1.0f, 20.0f * u.time.delta);
		look_quat = Quat::euler(lean, angle_horizontal, angle_vertical);
	}
	
	s32 player_count = LocalPlayer::list.count();
	Camera::ViewportBlueprs32* viewports = Camera::viewport_blueprs32s[player_count - 1];
	Camera::ViewportBlueprs32* blueprs32 = &viewports[gamepad];

	if (has<Awk>())
		camera->range = AWK_MAX_DISTANCE;
	camera->viewport =
	{
		Vec2((s32)(blueprs32->x * (r32)u.input->width), (s32)(blueprs32->y * (r32)u.input->height)),
		Vec2((s32)(blueprs32->w * (r32)u.input->width), (s32)(blueprs32->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective(LMath::lerpf(fov_blend, fov_initial, fov_zoom), aspect, 0.02f, Skybox::far_plane);

	// Camera matrix
	camera->pos = camera_pos + (Game::data.third_person ? look_quat * Vec3(0, 0, -2) : Vec3::zero);
	camera->rot = look_quat;
	if (has<Awk>())
		camera->wall_normal = look_quat.inverse() * (get<Transform>()->absolute_rot() * Vec3(0, 0, 1));
	else
		camera->wall_normal = Vec3(0, 0, 1);

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
			rayCallback.m_collisionFilterMask = rayCallback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;

			Physics::btWorld->rayTest(trace_start, trace_end, rayCallback);

			if (rayCallback.hasHit())
			{
				tracer.type = rayCallback.hit_target ? TraceType::Target : TraceType::Normal;
				tracer.pos = rayCallback.m_hitPointWorld;

				short group = rayCallback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
				if (group & (CollisionWalker | CollisionInaccessible))
					tracer.type = TraceType::None; // We can't go there
			}
		}

		if (tracer.type != TraceType::None)
		{
			if (tracer.type == TraceType::Normal)
			{
				AI::Team team = get<AIAgent>()->team;
				for (auto i = AIAgent::list.iterator(); !i.is_last(); i.next())
				{
					r32 view_range = i.item()->has<MinionAI>() ? MINION_VIEW_RANGE : TURRET_VIEW_RANGE;
					if (i.item()->team != team && (i.item()->get<Transform>()->absolute_pos() - tracer.pos).length_squared() < view_range * view_range)
					{
						tracer.type = TraceType::Danger;
						break;
					}
				}
			}

			if (input_enabled())
			{
				b8 go = u.input->get(settings.bindings.primary, gamepad) && !u.last_input->get(settings.bindings.primary, gamepad);

				if (go && get<Awk>()->detach(u, look_quat * Vec3(0, 0, 1)))
				{
					allow_zoom = false;
					attach_quat = look_quat;
					get<Audio>()->post_event(AK::EVENTS::PLAY_FLY);
				}
			}
		}
	}

	Audio::listener_update(gamepad, camera_pos, look_quat);

	last_angle_horizontal = angle_horizontal;
}

void LocalPlayerControl::clamp_rotation(const Vec3& direction)
{
	Quat look_quat = Quat::euler(lean, angle_horizontal, angle_vertical);
	Vec3 forward = look_quat * Vec3(0, 0, 1);

	r32 dot = forward.dot(direction);
	if (dot < 0.0f)
	{
		forward = Vec3::normalize(forward - dot * direction);
		angle_horizontal = atan2f(forward.x, forward.z);
		angle_vertical = -asinf(forward.y);
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
	if (params.technique != RenderTechnique::Default)
		return;

	if (params.camera == camera)
	{
		static const Vec3 scale = Vec3(0.05f);
		static const r32 s32erval = 1.0f;

		AI::Team team = get<AIAgent>()->team;
		Vec4 color = AI::colors[(s32)team];

		const Rect2& viewport = params.camera->viewport;

		// Draw reticle
		if (has<Awk>() && get<Transform>()->parent.ref())
		{
			r32 cooldown = get<PlayerCommon>()->cooldown;
			r32 radius = cooldown == 0.0f ? 0.0f : fmax(0.0f, 32.0f * (get<PlayerCommon>()->cooldown / AWK_MAX_DISTANCE_COOLDOWN));
			if (radius > 0 || tracer.type == TraceType::None)
				UI::centered_border(params, { viewport.size * Vec2(0.5f, 0.5f), Vec2(3.0f + radius) * UI::scale }, 2, UI::default_color, PI * 0.25f);
			else
			{
				// Draw tracer
				Vec2 a;
				if (UI::project(params, tracer.pos, &a))
				{
					Vec4 color;
					if (tracer.type == TraceType::Target)
						color = Vec4(0, 1, 0, 1);
					else if (tracer.type == TraceType::Danger)
						color = UI::alert_color;
					else
						color = UI::default_color;
					Vec2 size = Vec2(7) * UI::scale;
					UI::centered_box(params, { a, size }, color, PI * 0.25f);
				}
			}
		}

		Audio* audio = get<Audio>();

		b8 alert = false;
		Vec3 pos = get<Transform>()->absolute_pos();

		for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
		{
			if (i.item()->get<AIAgent>()->team != team)
			{
				if ((i.item()->get<Transform>()->absolute_pos() - pos).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
				{
					alert = true;
					break;
				}
			}
		}

		Transform* parent = get<Transform>()->parent.ref();
		if (parent && parent->has<Socket>())
		{
			Vec3 wall_normal = get<Transform>()->absolute_rot() * Vec3(0, 0, 1);
			Socket* socket = parent->get<Socket>();
			for (auto i = Socket::list.iterator(); !i.is_last(); i.next())
			{
				if (i.item() != socket)
				{
					Vec3 socket_pos = i.item()->get<Transform>()->absolute_pos();
					Vec3 to_socket = socket_pos - pos;
					r32 distance = to_socket.length();
					if (distance < AWK_MAX_DISTANCE && wall_normal.dot(to_socket / distance) > 0.0f)
						draw_indicator(params, socket_pos, UI::default_color);
				}
			}
		}

		AssetID secondary_icon = AssetNull;

		Vec2 compass_size = Vec2(fmin(viewport.size.x, viewport.size.y) * 0.3f);

		Vec4 compass_color = alert ? UI::alert_color : Vec4(1, 1, 1, 1);

		UI::mesh(params, Asset::Mesh::compass_inner, viewport.pos + viewport.size * Vec2(0.5f, 0.5f), compass_size, compass_color, angle_vertical);
		UI::mesh(params, Asset::Mesh::compass_outer, viewport.pos + viewport.size * Vec2(0.5f, 0.5f), compass_size, Vec4(compass_color.xyz(), 1.0f), -angle_horizontal);

		// health bar
		{
			const Health* health = get<Health>();
			if (health->hp < health->total)
			{
				const Vec2 size = Vec2(128.0f, 16.0f) * UI::scale;
				const Vec2 pos = viewport.pos + viewport.size * Vec2(0.5f, 0.1f);
				const Vec4& color = AI::colors[(s32)team];
				UI::box(params, { pos - size * 0.5f, size }, UI::background_color);
				UI::box(params, { pos - size * 0.5f, size * Vec2((r32)health->hp / (r32)health->total, 1.0f) }, color);
				UI::border(params, { pos - size * 0.5f, size }, 2, color);
			}
		}
	}
}

}
