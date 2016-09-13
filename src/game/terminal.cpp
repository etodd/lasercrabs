#include "terminal.h"
#include "console.h"
#include "game.h"
#include "mersenne/mersenne-twister.h"
#include "audio.h"
#include "menu.h"
#include "asset/level.h"
#include "asset/Wwise_IDs.h"
#include "asset/mesh.h"
#include "asset/font.h"
#include "asset/dialogue.h"
#include "asset/texture.h"
#include "asset/lookup.h"
#include "asset/shader.h"
#include "strings.h"
#include "load.h"
#include "entities.h"
#include "render/particles.h"
#include "data/import_common.h"
#include "sha1/sha1.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "cora.h"
#include "data/priority_queue.h"
#include "platform/util.h"
#include "utf8/utf8.h"

namespace VI
{


namespace Terminal
{

#define DEPLOY_TIME_LOCAL 2.0f

#define SCALE_MULTIPLIER (UI::scale < 1.0f ? 0.5f : 1.0f)
#define PADDING (16.0f * UI::scale * SCALE_MULTIPLIER)
#define TAB_SIZE Vec2(160.0f * UI::scale, UI_TEXT_SIZE_DEFAULT * UI::scale + PADDING * 2.0f)
#define MAIN_VIEW_SIZE (Vec2(768.0f, 512.0f) * (SCALE_MULTIPLIER * UI::scale))
#define TEXT_SIZE (UI_TEXT_SIZE_DEFAULT * SCALE_MULTIPLIER)
#define MESSAGE_TRUNCATE_LONG (72 * SCALE_MULTIPLIER)
#define MESSAGE_TRUNCATE_SHORT (48 * SCALE_MULTIPLIER)
#define BORDER 2.0f
#define OPACITY 0.8f
#define STRING_BUFFER_SIZE 256
#define HACK_TIME 2.0f
#define BUY_TIME 1.0f
#define DEPLOY_COST_DRONES 1
#define CREDITS_INITIAL 40
#define ENERGY_GENERATION_PER_CELL 10

struct ZoneNode
{
	AssetID id;
	Ref<Transform> pos;
	u16 rewards[(s32)Game::Resource::count];
	u8 size;
	u8 max_teams;
};

enum class State
{
	SplitscreenSelectTeams,
	SplitscreenSelectZone,
	SplitscreenDeploying,
	StoryMode,
	Deploying,
};

AssetID cora_entry_points[] =
{
	AssetNull, // 0 - safe zone yet to be completed
	AssetNull, // 1 - medias res yet to be played
	strings::intro, // 2 - cora's finally ready talk
	AssetNull, // 3 - you've joined sissy foos
	AssetNull,
	AssetNull,
	AssetNull,
	AssetNull,
};

enum class Tab
{
	Messages,
	Map,
	Inventory,
	count,
};

typedef void(*DialogCallback)();

struct Data
{
	struct Messages
	{
		enum class Mode
		{
			Contacts,
			Messages,
			Message,
			Cora,
		};

		Mode mode;
		UIScroll contact_scroll;
		UIScroll message_scroll;
		AssetID contact_selected = AssetNull;
		AssetID message_selected = AssetNull;
		r32 cora_timer;
	};

	struct Inventory
	{
		r32 resources_blink_timer[(s32)Game::Resource::count];
		r32 buy_timer;
		Game::Resource resource_selected;
		u16 resources_last[(s32)Game::Resource::count];
	};

	struct Map
	{
		r32 hack_timer;
		r32 capture_timer;
		b8 in_group_queue;
	};

	struct StoryMode
	{
		DialogCallback dialog_callback;
		Tab tab = Tab::Map;
		Tab tab_previous = Tab::Messages;
		r32 tab_timer;
		r32 mode_transition_time;
		r32 dialog_time;
		Messages messages;
		Inventory inventory;
		Map map;
		Ref<Transform> camera_messages;
		Ref<Transform> camera_inventory;
		char dialog[255];
		r32 dialog_time_limit;
	};

	Camera* camera;
	Array<ZoneNode> zones;
	AssetID zone_last = AssetNull;
	AssetID zone_selected = AssetNull;
	State state;
	s32 tip_index;
	r32 tip_time;
	r32 deploy_timer;
	StoryMode story;
	Ref<Transform> camera_offset;
};

Data data = Data();

const s32 tip_count = 13;
const AssetID tips[tip_count] =
{
	strings::tip_0,
	strings::tip_1,
	strings::tip_2,
	strings::tip_3,
	strings::tip_4,
	strings::tip_5,
	strings::tut_minion,
	strings::tip_7,
	strings::tip_8,
	strings::tip_9,
	strings::tip_10,
	strings::tip_11,
	strings::tip_12,
};

s32 splitscreen_team_count()
{
	s32 player_count = 0;
	s32 team_counts[MAX_PLAYERS] = {};
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team team = Game::session.local_player_config[i];
		if (team != AI::TeamNone)
			team_counts[(s32)team]++;
	}
	s32 teams_with_players = 0;
	for (s32 i = 0; i < MAX_PLAYERS; i++)
	{
		if (team_counts[i] > 0)
			teams_with_players++;
	}
	return teams_with_players;
}

b8 splitscreen_teams_are_valid()
{
	return splitscreen_team_count() > 1;
}

Game::Message* message_get(AssetID msg)
{
	for (s32 i = 0; i < Game::save.messages.length; i++)
	{
		if (Game::save.messages[i].text == msg)
			return &Game::save.messages[i];
	}
	return nullptr;
}

void message_add(AssetID contact, AssetID text, r64 timestamp = -1.0)
{
	if (message_get(text))
		return; // already sent

	Game::Message* msg = Game::save.messages.insert(0);
	msg->contact = contact;
	msg->text = text;
	if (timestamp == -1.0)
		msg->timestamp = platform::time();
	else
		msg->timestamp = timestamp;
	msg->read = false;
}

void message_schedule(AssetID contact, AssetID text, r64 delay)
{
	if (message_get(text))
		return; // already sent
	for (s32 i = 0; i < Game::save.messages_scheduled.length; i++)
	{
		if (Game::save.messages_scheduled[i].text == text)
			return; // already scheduled
	}

	Game::Message* msg = Game::save.messages_scheduled.add();
	msg->contact = contact;
	msg->text = text;
	msg->timestamp = platform::time() + delay;
	msg->read = false;
}

// default callback
void dialog_no_action()
{
}

void dialog(DialogCallback callback, const char* format, ...)
{
	va_list args;
	va_start(args, format);

	if (!format)
		format = "";

#if defined(_WIN32)
	vsprintf_s(data.story.dialog, 254, format, args);
#else
	vsnprintf(data.story.dialog, 254, format, args);
#endif

	va_end(args);

	data.story.dialog_callback = callback;
	data.story.dialog_time = Game::real_time.total;
	data.story.dialog_time_limit = 0.0f;
}

void splitscreen_select_teams_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0])
	{
		Menu::title();
		return;
	}

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team* team = &Game::session.local_player_config[i];
		if (u.input->gamepads[i].active || i == 0) // player is active
		{
			// handle D-pad
			b8 left = u.input->get(Controls::Left, i) && !u.last_input->get(Controls::Left, i);
			b8 right = u.input->get(Controls::Right, i) && !u.last_input->get(Controls::Right, i);

			// handle joysticks
			if (u.input->gamepads[i].active)
			{
				r32 x_last = Input::dead_zone(u.last_input->gamepads[i].left_x);
				if (x_last == 0.0f)
				{
					r32 x_current = Input::dead_zone(u.input->gamepads[i].left_x);
					if (x_current < 0.0f)
						left = true;
					else if (x_current > 0.0f)
						right = true;
				}
			}

			if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i) && !Game::cancel_event_eaten[i])
			{
				if (i > 0) // player 0 must stay in
				{
					*team = AI::TeamNone;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
			else if (left)
			{
				if (*team == AI::TeamNone)
				{
					// we're already all the way to the left
				}
				else if (*team == 0)
				{
					if (i > 0) // player 0 must stay in
					{
						*team = AI::TeamNone;
						Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
					}
				}
				else
				{
					(*team) -= 1;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
			else if (right)
			{
				if (*team == AI::TeamNone)
				{
					*team = 0;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
				else if (*team < MAX_PLAYERS - 1)
				{
					*team += 1;
					Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
				}
			}
		}
		else // controller is gone
			*team = AI::TeamNone;
	}

	if (u.last_input->get(Controls::Interact, 0)
		&& !u.input->get(Controls::Interact, 0)
		&& splitscreen_teams_are_valid())
	{
		data.state = State::SplitscreenSelectZone;
	}
}

void tab_draw_common(const RenderParams& p, const char* label, const Vec2& pos, r32 width, const Vec4& color, const Vec4& background_color, const Vec4& text_color = UI::background_color)
{
	// body
	Vec2 size(width, MAIN_VIEW_SIZE.y);
	{
		if (background_color.w > 0.0f)
			UI::box(p, { pos, size }, background_color);
		UI::border(p, { pos, size }, BORDER, color);
	}

	// label
	{
		Vec2 tab_pos = pos + Vec2(0, size.y);
		UI::box(p, { tab_pos, TAB_SIZE }, color);

		UIText text;
		text.anchor_x = UIText::Anchor::Min;
		text.anchor_y = UIText::Anchor::Min;
		text.color = text_color;
		text.text(label);
		text.draw(p, tab_pos + Vec2(PADDING));
	}
}

void draw_gamepad_icon(const RenderParams& p, const Vec2& pos, s32 index, const Vec4& color, r32 scale = 1.0f)
{
	UI::mesh(p, Asset::Mesh::icon_gamepad, pos, Vec2(48.0f * scale * UI::scale), color);
	UIText text;
	text.size *= scale;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;
	text.color = UI::background_color;
	text.text("%d", index + 1);
	text.draw(p, pos);
}

void splitscreen_select_teams_draw(const RenderParams& params)
{
	const Rect2& vp = params.camera->viewport;
	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	Vec2 center = vp.size * 0.5f;
	{
		Vec2 bottom_left = center + (main_view_size * -0.5f);
		Vec4 background_color = Vec4(UI::background_color.xyz(), OPACITY);
		tab_draw_common(params, _(strings::prompt_splitscreen), bottom_left, main_view_size.x, UI::accent_color, background_color);
	}

	UIText text;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::accent_color;
	text.wrap_width = main_view_size.x - 48.0f * UI::scale * SCALE_MULTIPLIER;
	Vec2 pos = center + Vec2(0, main_view_size.y * 0.5f - (48.0f * UI::scale * SCALE_MULTIPLIER));

	// prompt
	if (splitscreen_teams_are_valid())
	{
		text.text(_(strings::prompt_splitscreen_ready));
		text.draw(params, pos);
	}

	pos.y -= 48.0f * UI::scale * SCALE_MULTIPLIER;

	// draw team labels
	const r32 team_offset = 128.0f * UI::scale * SCALE_MULTIPLIER;
	text.wrap_width = 0;
	text.text(_(strings::team_a));
	text.draw(params, pos + Vec2(team_offset * -1.0f, 0));
	text.text(_(strings::team_b));
	text.draw(params, pos + Vec2(0, 0));
	text.text(_(strings::team_c));
	text.draw(params, pos + Vec2(team_offset * 1.0f, 0));
	text.text(_(strings::team_d));
	text.draw(params, pos + Vec2(team_offset * 2.0f, 0));

	// set up text for gamepad number labels
	text.color = UI::background_color;
	text.wrap_width = 0;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;

	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		pos.y -= 64.0f * UI::scale * SCALE_MULTIPLIER;

		AI::Team team = Game::session.local_player_config[i];

		const Vec4* color;
		r32 x_offset;
		if (i > 0 && !params.sync->input.gamepads[i].active)
		{
			color = &UI::disabled_color;
			x_offset = team_offset * -2.0f;
		}
		else if (team == AI::TeamNone)
		{
			color = &UI::default_color;
			x_offset = team_offset * -2.0f;
		}
		else
		{
			color = &UI::accent_color;
			x_offset = ((r32)team - 1.0f) * team_offset;
		}

		draw_gamepad_icon(params, pos + Vec2(x_offset, 0), i, *color, SCALE_MULTIPLIER);
	}
}

void go(AssetID zone)
{
	Game::schedule_load_level(zone, Game::Mode::Pvp);
}

void focus_camera(const Update& u, const Vec3& target_pos, const Quat& target_rot)
{
	data.camera->pos += (target_pos - data.camera->pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
	data.camera->rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), data.camera->rot, target_rot);
}

void focus_camera(const Update& u, const Transform* target)
{
	focus_camera(u, target->absolute_pos(), Quat::look(target->absolute_rot() * Vec3(0, -1, 0)));
}

void focus_camera(const Update& u, const ZoneNode& zone)
{
	Vec3 target_pos = zone.pos.ref()->absolute_pos() + data.camera_offset.ref()->absolute_pos();
	Quat target_rot = Quat::look(data.camera_offset.ref()->absolute_rot() * Vec3(0, -1, 0));
	focus_camera(u, target_pos, target_rot);
}

const ZoneNode* get_zone_node(AssetID id)
{
	for (s32 i = 0; i < data.zones.length; i++)
	{
		if (data.zones[i].id == data.zone_selected)
			return &data.zones[i];
	}
	return nullptr;
}

b8 resource_spend(Game::Resource res, u16 amount)
{
	if (Game::save.resources[(s32)res] >= amount)
	{
		Game::save.resources[(s32)res] -= amount;
		return true;
	}
	return false;
}

void deploy_start()
{
	data.state = Game::session.local_multiplayer ? State::SplitscreenDeploying : State::Deploying;
	data.deploy_timer = DEPLOY_TIME_LOCAL;
	data.tip_time = Game::real_time.total;
}

void select_zone_update(const Update& u, b8 enable_movement)
{
	if (UIMenu::active[0])
		return;

	const ZoneNode* zone = get_zone_node(data.zone_selected);
	if (!zone)
		return;

	// movement
	if (enable_movement)
	{
		Vec2 movement(0, 0);

		// buttons/keys
		{
			if (u.input->get(Controls::Left, 0) && !u.last_input->get(Controls::Left, 0))
				movement.x -= 1.0f;
			if (u.input->get(Controls::Right, 0) && !u.last_input->get(Controls::Right, 0))
				movement.x += 1.0f;
			if (u.input->get(Controls::Forward, 0) && !u.last_input->get(Controls::Forward, 0))
				movement.y -= 1.0f;
			if (u.input->get(Controls::Backward, 0) && !u.last_input->get(Controls::Backward, 0))
				movement.y += 1.0f;
		}

		// joysticks
		{
			Vec2 last_joystick(u.last_input->gamepads[0].left_x, u.last_input->gamepads[0].left_y);
			Input::dead_zone(&last_joystick.x, &last_joystick.y, UI_JOYSTICK_DEAD_ZONE);
			Vec2 current_joystick(u.input->gamepads[0].left_x, u.input->gamepads[0].left_y);
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
			Vec3 movement3d = data.camera->rot * Vec3(-movement.x, 0, -movement.y);
			movement = Vec2(movement3d.x, movement3d.z);
			Vec3 zone_pos = zone->pos.ref()->absolute_pos();
			const ZoneNode* closest = nullptr;
			r32 closest_dot = FLT_MAX;
			for (s32 i = 0; i < data.zones.length; i++)
			{
				const ZoneNode& candidate = data.zones[i];
				if (candidate.id != AssetNull)
				{
					Vec3 candidate_pos = candidate.pos.ref()->absolute_pos();
					Vec3 to_candidate = (candidate_pos - zone_pos);
					r32 dot = movement.dot(Vec2(to_candidate.x, to_candidate.z));
					r32 normalized_dot = movement.dot(Vec2::normalize(Vec2(to_candidate.x, to_candidate.z)));
					if (dot < closest_dot && normalized_dot > 0.7f)
					{
						closest = &candidate;
						closest_dot = dot;
					}
				}
			}
			if (closest)
				data.zone_selected = closest->id;
		}
	}

	focus_camera(u, *zone);
}

Vec3 zone_color(const ZoneNode& zone)
{
	if (Game::session.local_multiplayer)
	{
		if (splitscreen_team_count() <= zone.max_teams)
			return Team::color_friend.xyz();
		else
			return Vec3(0.3f);
	}
	else
	{
		Game::ZoneState zone_state = Game::save.zones[zone.id];
		switch (zone_state)
		{
			case Game::ZoneState::Locked:
			{
				return Vec3(1.0f);
			}
			case Game::ZoneState::Friendly:
			{
				return UI::accent_color.xyz();
			}
			case Game::ZoneState::Hostile:
			{
				return Team::color_enemy.xyz();
			}
			case Game::ZoneState::Owned:
			{
				return Team::color_friend.xyz();
			}
			case Game::ZoneState::Inaccessible:
			{
				return Vec3(0.3f);
			}
			default:
			{
				vi_assert(false);
				return Vec3::zero;
			}
		}
	}
}

const Vec4& zone_ui_color(const ZoneNode& zone)
{
	if (Game::session.local_multiplayer)
	{
		if (splitscreen_team_count() <= zone.max_teams)
			return Team::ui_color_friend;
		else
			return UI::disabled_color;
	}
	else
	{
		Game::ZoneState zone_state = Game::save.zones[zone.id];
		switch (zone_state)
		{
			case Game::ZoneState::Locked:
			{
				return UI::default_color;
			}
			case Game::ZoneState::Friendly:
			{
				return UI::accent_color;
			}
			case Game::ZoneState::Hostile:
			{
				return Team::ui_color_enemy;
			}
			case Game::ZoneState::Owned:
			{
				return Team::ui_color_friend;
			}
			case Game::ZoneState::Inaccessible:
			{
				return UI::disabled_color;
			}
			default:
			{
				vi_assert(false);
				return UI::default_color;
			}
		}
	}
}

#define DEFAULT_ZONE_COLOR Vec3(0.2f)
void zones_draw_override(const RenderParams& params)
{
	struct SortKey
	{
		r32 priority(const ZoneNode& n)
		{
			// sort farthest zones first
			Vec3 camera_forward = data.camera->rot * Vec3(0, 0, 1);
			return camera_forward.dot(n.pos.ref()->absolute_pos() - data.camera->pos);
		}
	};

	SortKey key;
	PriorityQueue<ZoneNode, SortKey> zones(&key);

	for (s32 i = 0; i < data.zones.length; i++)
	{
		if (data.zones[i].id != AssetNull)
			zones.push(data.zones[i]);
	}

	RenderSync* sync = params.sync;

	sync->write(RenderOp::DepthTest);
	sync->write<b8>(true);

	while (zones.size() > 0)
	{
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);
		const ZoneNode& zone = zones.pop();
		View* view = zone.pos.ref()->get<View>();
		view->color = Vec4(zone_color(zone), 1.0f);
		view->draw(params);
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);
		view->color = Vec4(DEFAULT_ZONE_COLOR, 1.0f);
		view->draw(params);
	}
}

// returns current zone node
const ZoneNode* zones_draw(const RenderParams& params)
{
	// highlight zone locations
	const ZoneNode* zone = get_zone_node(data.zone_selected);

	// draw current zone name
	if (zone && data.deploy_timer == 0.0f)
	{
		Vec2 p;
		if (UI::project(params, zone->pos.ref()->absolute_pos(), &p))
		{
			if (Game::session.local_multiplayer)
			{
				UIText text;
				text.color = zone_ui_color(*zone);
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.text_raw(AssetLookup::Level::names[zone->id]);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
			else
				UI::triangle_border(params, { p, Vec2(32.0f * UI::scale) }, BORDER * 2.0f, UI::accent_color);
		}
	}

	return zone;
}

void splitscreen_select_zone_draw(const RenderParams& params)
{
	const ZoneNode* zone = zones_draw(params);

	// press [x] to deploy
	{
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = zone_ui_color(*zone);
		text.text("%s\n%s", _(strings::prompt_deploy), _(strings::prompt_back));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.25f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}

	// draw teams
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::accent_color;
		text.size *= SCALE_MULTIPLIER;

		s32 player_count = Game::session.local_player_count();
		const r32 gamepad_spacing = 128.0f * UI::scale * SCALE_MULTIPLIER;
		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.1f) + Vec2(gamepad_spacing * (player_count - 1) * -0.5f, 0);

		AssetID team_labels[MAX_PLAYERS] =
		{
			strings::team_a,
			strings::team_b,
			strings::team_c,
			strings::team_d,
		};

		for (s32 i = 0; i < MAX_GAMEPADS; i++)
		{
			AI::Team team = Game::session.local_player_config[i];
			if (team != AI::TeamNone)
			{
				text.text(_(team_labels[(s32)team]));
				text.draw(params, pos + Vec2(0, 32.0f * UI::scale * SCALE_MULTIPLIER));
				draw_gamepad_icon(params, pos, i, UI::accent_color, SCALE_MULTIPLIER);
				pos.x += gamepad_spacing;
			}
		}
	}
}

void deploy_update(const Update& u)
{
	const ZoneNode& zone = *get_zone_node(data.zone_selected);
	focus_camera(u, zone);

	data.deploy_timer -= Game::real_time.delta;
	data.camera->active = data.deploy_timer > 0.5f;
	if (data.deploy_timer < 0.0f)
		go(data.zone_selected);
}

void bar_draw(const RenderParams& params, const char* label, r32 percentage, const Vec2& pos)
{
	Vec2 bar_size(180.0f * UI::scale, 32.0f * UI::scale);
	Rect2 bar = { pos + bar_size * -0.5f, bar_size };
	UI::box(params, bar, UI::background_color);
	UI::border(params, bar, 2, UI::accent_color);
	UI::box(params, { bar.pos, Vec2(bar.size.x * percentage, bar.size.y) }, UI::accent_color);

	UIText text;
	text.size = 18.0f;
	text.color = UI::background_color;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Center;
	text.text(label);
	text.draw(params, bar.pos + bar.size * 0.5f);
}

void progress_draw(const RenderParams& params, const char* label, const Vec2& pos_overall)
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.color = UI::accent_color;
	text.text(label);

	Vec2 pos = pos_overall + Vec2(24 * UI::scale, 0);

	UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::background_color);

	text.draw(params, pos);

	Vec2 triangle_pos = Vec2
	(
		pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
		pos.y
	);
	UI::triangle_border(params, { triangle_pos, Vec2(20 * UI::scale) }, 9, UI::accent_color, Game::real_time.total * -12.0f);
}

void deploy_draw(const RenderParams& params)
{
	const ZoneNode* current_zone = zones_draw(params);

	// show "loading..."
	progress_draw(params, _(Game::session.local_multiplayer ? strings::loading_offline : strings::connecting), params.camera->viewport.size * Vec2(0.5f, 0.2f));

	{
		// show a tip
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::accent_color;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.text(_(strings::tip), _(tips[data.tip_index]));
		UIMenu::text_clip(&text, data.tip_time, 80.0f);

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f) + Vec2(0, 48.0f * UI::scale);

		UI::box(params, text.rect(pos).outset(12 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
}

b8 can_switch_tab()
{
	const Data::StoryMode& story = data.story;
	return story.map.hack_timer == 0.0f
		&& story.map.capture_timer == 0.0f
		&& story.inventory.buy_timer == 0.0f
		&& !story.dialog_callback
		&& (story.messages.mode != Data::Messages::Mode::Cora || story.messages.cora_timer > 0.0f);
}

#define TAB_ANIMATION_TIME 0.3f

#define MAX_CONTACTS 16

struct ContactDetails
{
	r64 last_message_timestamp;
	AssetID last_message_text;
	AssetID name;
	s32 unread;
};

void collect_contacts(StaticArray<ContactDetails, MAX_CONTACTS>* contacts)
{
	for (s32 i = 0; i < Game::save.messages.length; i++)
	{
		const Game::Message& msg = Game::save.messages[i];
		ContactDetails* existing_contact = nullptr;
		for (s32 j = 0; j < contacts->length; j++)
		{
			if (msg.contact == (*contacts)[j].name)
			{
				existing_contact = &((*contacts)[j]);
				break;
			}
		}

		if (!existing_contact)
		{
			existing_contact = contacts->add();
			existing_contact->name = msg.contact;
			existing_contact->last_message_timestamp = msg.timestamp;
			existing_contact->last_message_text = msg.text;
		}

		if (!msg.read)
			existing_contact->unread++;
	}
}

void collect_messages(Array<Game::Message>* messages, AssetID contact)
{
	for (s32 i = 0; i < Game::save.messages.length; i++)
	{
		const Game::Message& msg = Game::save.messages[i];
		if (msg.contact == contact)
			messages->add(msg);
	}
}

void messages_transition(Data::Messages::Mode mode)
{
	if (data.story.messages.mode != mode)
	{
		data.story.messages.mode = mode;
		data.story.mode_transition_time = Game::real_time.total;
	}
}

void message_statistics(s32* unread_count, r64* most_recent)
{
	*unread_count = 0;
	*most_recent = 0;
	for (s32 i = 0; i < Game::save.messages.length; i++)
	{
		const Game::Message& msg = Game::save.messages[i];
		if (!msg.read)
			(*unread_count)++;
		if (msg.timestamp > *most_recent)
			*most_recent = msg.timestamp;
	}
}

void message_read(Game::Message* msg)
{
	msg->read = true;
	if (msg->text == strings::msg_albert_intro_2)
		message_schedule(strings::contact_cora, strings::msg_cora_intro, 2.0);
}

#define ZONE_MAX_CHILDREN 12
void get_zone_children(const ZoneNode& node, StaticArray<Ref<Transform>, ZONE_MAX_CHILDREN>* children)
{
	children->length = 0;
	for (auto i = Transform::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->parent.ref() == node.pos.ref())
			children->add(i.item());
	}
}

// make sure inaccessible/locked zones are correct
void update_zone_states()
{
	// reset
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& z = data.zones[i];
		if (Game::save.zones[z.id] == Game::ZoneState::Locked && z.id != Asset::Level::Safe_Zone)
			Game::save.zones[z.id] = Game::ZoneState::Inaccessible;
	}

	// make zones adjacent to owned/friendly zones accessible (but still locked)
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& zone = data.zones[i];
		Game::ZoneState zone_state = Game::save.zones[zone.id];
		if (zone_state == Game::ZoneState::Friendly || zone_state == Game::ZoneState::Owned)
		{
			StaticArray<Ref<Transform>, ZONE_MAX_CHILDREN> children;
			get_zone_children(zone, &children);
			children.add(zone.pos.ref());
			for (s32 j = 0; j < children.length; j++)
			{
				Vec3 zone_pos = children[j].ref()->absolute_pos();
				zone_pos.y = 0.0f;
				for (s32 k = 0; k < data.zones.length; k++)
				{
					const ZoneNode& neighbor_zone = data.zones[k];
					if (Game::save.zones[neighbor_zone.id] == Game::ZoneState::Inaccessible)
					{
						Vec3 neighbor_pos = neighbor_zone.pos.ref()->absolute_pos();
						neighbor_pos.y = 0.0f;
						if ((neighbor_pos - zone_pos).length_squared() < 4.0f * 4.0f)
							Game::save.zones[neighbor_zone.id] = Game::ZoneState::Locked;
					}
				}
			}
		}
	}
}


void join_group(Game::Group g)
{
	Game::save.group = g;
	for (s32 i = 0; i < data.zones.length; i++)
	{
		const ZoneNode& zone = data.zones[i];
		if (zone.id != AssetNull)
		{
			if (g == Game::Group::None)
			{
				Game::save.zones[zone.id] = zone.id == Asset::Level::Safe_Zone || zone.id == Asset::Level::Medias_Res
					? (mersenne::randf_cc() > 0.5f ? Game::ZoneState::Owned : Game::ZoneState::Hostile) : Game::ZoneState::Locked;
			}
			else
			{
				if (zone.max_teams < MAX_PLAYERS)
					Game::save.zones[zone.id] = Game::ZoneState::Inaccessible;
				else
					Game::save.zones[zone.id] = mersenne::randf_cc() > 0.5f ? Game::ZoneState::Friendly : Game::ZoneState::Hostile;
			}
		}
	}
	update_zone_states();
	data.story.tab = Tab::Map;
}

void conversation_finished()
{
	Game::save.story_index++;
	messages_transition(Data::Messages::Mode::Messages);
	if (Game::save.story_index == 3)
		join_group(Game::Group::SissyFoos); // you are now part of sissy foos
}

void tab_messages_update(const Update& u)
{
	Data::StoryMode* story = &data.story;
	Data::Messages* messages = &story->messages;

	{
		r64 time = platform::time();
		for (s32 i = 0; i < Game::save.messages_scheduled.length; i++)
		{
			const Game::Message& schedule = Game::save.messages_scheduled[i];
			if (time > schedule.timestamp)
			{
				message_add(schedule.contact, schedule.text, schedule.timestamp);
				Game::save.messages_scheduled.remove(i);
				i--;
			}
		}
	}

	if (story->tab == Tab::Messages && story->tab_timer > TAB_ANIMATION_TIME)
	{
		focus_camera(u, story->camera_messages.ref());

		// call cora
		if (messages->mode != Data::Messages::Mode::Cora
			&& messages->contact_selected == strings::contact_cora
			&& u.last_input->get(Controls::InteractSecondary, 0) && !u.input->get(Controls::InteractSecondary, 0))
		{
			messages_transition(Data::Messages::Mode::Cora);
			Game::save.cora_called = true;
			if (cora_entry_points[Game::save.story_index] == AssetNull)
				messages->cora_timer = 10.0f;
			else
				messages->cora_timer = 3.0f;
		}

		switch (messages->mode)
		{
			case Data::Messages::Mode::Contacts:
			{
				StaticArray<ContactDetails, MAX_CONTACTS> contacts;
				collect_contacts(&contacts);

				messages->contact_scroll.update_menu(contacts.length);
				if (contacts.length == 0)
					messages->contact_selected = AssetNull;
				else
				{
					s32 contact_index = 0;
					for (s32 i = 0; i < contacts.length; i++)
					{
						if (contacts[i].name == messages->contact_selected)
						{
							contact_index = i;
							break;
						}
					}
					contact_index += UI::vertical_input_delta(u, 0);
					contact_index = vi_max(0, vi_min(contact_index, (s32)contacts.length - 1));
					messages->contact_selected = contacts[contact_index].name;
					messages->contact_scroll.scroll_into_view(contact_index);
				}

				if (messages->contact_selected != AssetNull && u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				{
					messages->message_selected = AssetNull;
					messages_transition(Data::Messages::Mode::Messages);
				}
				break;
			}
			case Data::Messages::Mode::Messages:
			{
				Array<Game::Message> msg_list;
				collect_messages(&msg_list, messages->contact_selected);
				messages->message_scroll.update_menu(msg_list.length);
				if (msg_list.length == 0)
					messages->message_selected = AssetNull;
				else
				{
					s32 msg_index = 0;
					for (s32 i = 0; i < msg_list.length; i++)
					{
						if (msg_list[i].text == messages->message_selected)
						{
							msg_index = i;
							break;
						}
					}
					msg_index += UI::vertical_input_delta(u, 0);
					msg_index = vi_max(0, vi_min(msg_index, (s32)msg_list.length - 1));
					messages->message_selected = msg_list[msg_index].text;
					messages->message_scroll.scroll_into_view(msg_index);

					Game::Message* message_selected = message_get(messages->message_selected);
					if (!message_selected->read && utf8len(_(message_selected->text)) < MESSAGE_TRUNCATE_LONG)
						message_read(message_selected); // automatically mark read

					if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
					{
						if (!message_selected->read)
							message_read(message_selected);
						messages_transition(Data::Messages::Mode::Message);
					}
				}

				if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
				{
					messages->message_selected = AssetNull;
					messages_transition(Data::Messages::Mode::Contacts);
					Game::cancel_event_eaten[0] = true;
				}
				break;
			}
			case Data::Messages::Mode::Message:
			{
				if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
				{
					messages_transition(Data::Messages::Mode::Messages);
					Game::cancel_event_eaten[0] = true;
				}
				break;
			}
			case Data::Messages::Mode::Cora:
			{
				if (messages->cora_timer > 0.0f)
				{
					if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
					{
						// cancel
						messages_transition(Data::Messages::Mode::Messages);
						Game::cancel_event_eaten[0] = true;
						messages->cora_timer = 0.0f;
					}
					else
					{
						messages->cora_timer = vi_max(0.0f, messages->cora_timer - Game::real_time.delta);
						if (messages->cora_timer == 0.0f)
						{
							AssetID entry_point = cora_entry_points[Game::save.story_index];
							if (entry_point == AssetNull)
							{
								messages_transition(Data::Messages::Mode::Messages);
								dialog(&dialog_no_action, _(strings::call_timed_out));
							}
							else
								Cora::activate(entry_point);
						}
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
	else
	{
		// minimized view; reset scroll
		messages->contact_scroll.pos = 0;
		if (messages->mode == Data::Messages::Mode::Cora)
		{
			// cancel cora call
			messages->cora_timer = 0.0f;
			messages->mode = Data::Messages::Mode::Messages;
		}
	}
}

void hack_start()
{
	data.story.map.hack_timer = HACK_TIME;
}

void capture_start()
{
	if (resource_spend(Game::Resource::Drones, 1))
	{
		if (data.zone_selected == Asset::Level::Safe_Zone)
			data.story.map.capture_timer = 3.0f;
		else
			data.story.map.capture_timer = 3.0f + mersenne::randf_cc() * 20.0f;
	}
}

void auto_capture_fail_manual_deploy()
{
	if (resource_spend(Game::Resource::Energy, CREDITS_INITIAL))
		deploy_start();
}

void story_zone_done(AssetID zone, Game::MatchResult result)
{
	b8 captured = false;
	if (result == Game::MatchResult::Victory)
	{
		dialog(&dialog_no_action, _(strings::victory));
		captured = true;
	}
	else if (result == Game::MatchResult::NetworkError)
	{
		dialog(&dialog_no_action, _(strings::forfeit_network_error));
		captured = true;
	}
	else if (result == Game::MatchResult::OpponentQuit)
	{
		dialog(&dialog_no_action, _(strings::forfeit_opponent_quit));
		captured = true;
	}

	if (captured)
	{
		const ZoneNode* z = get_zone_node(zone);
		for (s32 i = 0; i < (s32)Game::Resource::count; i++)
			Game::save.resources[i] += z->rewards[i];

		if (Game::save.group == Game::Group::None)
			Game::save.zones[zone] = Game::ZoneState::Owned;
		else
			Game::save.zones[zone] = Game::ZoneState::Friendly;
		update_zone_states();
	}

	if (Game::save.story_index == 0 && zone == Asset::Level::Safe_Zone && captured)
	{
		Game::save.story_index++;
		if (Game::save.cora_called)
			message_schedule(strings::contact_albert, strings::msg_albert_keep_trying, 3.0f);
	}
	else if (Game::save.story_index == 1 && zone == Asset::Level::Medias_Res)
	{
		Game::save.story_index++; // cora's ready to talk
		if (Game::save.cora_called)
			message_schedule(strings::contact_albert, strings::msg_albert_keep_trying, 3.0f);
	}
}

void tab_map_update(const Update& u)
{
	if (data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME)
	{
		b8 enable_input = can_switch_tab();
		select_zone_update(u, enable_input); // only enable movement if can_switch_tab()

		if (data.story.map.hack_timer > 0.0f)
		{
			data.story.map.hack_timer = vi_max(0.0f, data.story.map.hack_timer - u.time.delta);
			if (data.story.map.hack_timer == 0.0f)
			{
				// hack complete
				const ZoneNode* zone = get_zone_node(data.zone_selected);
				if (Game::save.zones[data.zone_selected] == Game::ZoneState::Locked
					&& resource_spend(Game::Resource::HackKits, zone->size))
				{
					Game::save.zones[data.zone_selected] = Game::ZoneState::Hostile;
					update_zone_states();
				}
			}
		}
		else if (data.story.map.capture_timer > 0.0f)
		{
			data.story.map.capture_timer = vi_max(0.0f, data.story.map.capture_timer - u.time.delta);
			if (data.story.map.capture_timer == 0.0f)
			{
				// capture complete
				b8 auto_capture_succeed;
				if (data.zone_selected == Asset::Level::Safe_Zone)
					auto_capture_succeed = false;
				else
					auto_capture_succeed = mersenne::randf_cc() > 0.5f;

				if (auto_capture_succeed)
					story_zone_done(data.zone_selected, Game::MatchResult::Victory);
				else
				{
					if (Game::save.resources[(s32)Game::Resource::Energy] < CREDITS_INITIAL)
						dialog(&dialog_no_action, _(strings::auto_capture_fail_insufficient_resource), CREDITS_INITIAL, _(strings::energy));
					else
					{
						dialog(&auto_capture_fail_manual_deploy, _(strings::auto_capture_fail_prompt), CREDITS_INITIAL);
						data.story.dialog_time_limit = 10.0f;
					}
				}
			}
		}
		else
		{
			// interact button
			if (enable_input && u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				Game::ZoneState zone_state = Game::save.zones[data.zone_selected];
				if (zone_state == Game::ZoneState::Locked)
				{
					u16 cost = get_zone_node(data.zone_selected)->size;
					if (Game::save.resources[(s32)Game::Resource::HackKits] < cost)
						dialog(&dialog_no_action, _(strings::insufficient_resource), cost, _(strings::hack_kits));
					else
						dialog(&hack_start, _(strings::confirm_hack), cost);
				}
				else
				{
					if (zone_state == Game::ZoneState::Hostile)
					{
						if (Game::save.resources[(s32)Game::Resource::Drones] < DEPLOY_COST_DRONES)
							dialog(&dialog_no_action, _(strings::insufficient_resource), DEPLOY_COST_DRONES, _(strings::drones));
						else
							dialog(&capture_start, _(strings::confirm_capture), DEPLOY_COST_DRONES);
					}
				}
			}
		}
	}
	else
		data.story.map.hack_timer = 0.0f;
}

struct ResourceInfo
{
	AssetID icon;
	AssetID description;
	u16 cost;
};

ResourceInfo resource_info[(s32)Game::Resource::count] =
{
	{
		Asset::Mesh::icon_energy,
		strings::energy,
		-1,
	},
	{
		Asset::Mesh::icon_hack_kit,
		strings::hack_kits,
		200,
	},
	{
		Asset::Mesh::icon_drone,
		strings::drones,
		50,
	},
};

void resource_buy()
{
	data.story.inventory.buy_timer = BUY_TIME;
}

void tab_inventory_update(const Update& u)
{
	if (data.story.tab == Tab::Inventory && data.story.tab_timer > TAB_ANIMATION_TIME)
		focus_camera(u, data.story.camera_inventory.ref());

	Data::Inventory* inventory = &data.story.inventory;

	if (data.story.inventory.buy_timer > 0.0f)
	{
		data.story.inventory.buy_timer = vi_max(0.0f, data.story.inventory.buy_timer - u.time.delta);
		if (data.story.inventory.buy_timer == 0.0f)
		{
			Game::Resource resource = data.story.inventory.resource_selected;
			const ResourceInfo& info = resource_info[(s32)resource];
			if (resource_spend(Game::Resource::Energy, info.cost))
				Game::save.resources[(s32)resource] += 1;
		}
	}

	if (data.story.tab == Tab::Inventory && can_switch_tab())
	{
		// handle input
		s32 selected = (s32)inventory->resource_selected;
		selected += UI::vertical_input_delta(u, 0);
		if (selected < 0)
			selected = (s32)Game::Resource::count - 1;
		else if (selected >= (s32)Game::Resource::count)
			selected = 0;
		inventory->resource_selected = (Game::Resource)selected;

		if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
		{
			const ResourceInfo& info = resource_info[(s32)inventory->resource_selected];
			if (info.cost != (u16)-1)
			{
				if (Game::save.resources[(s32)Game::Resource::Energy] >= info.cost)
					dialog(&resource_buy, _(strings::prompt_buy), info.cost, _(info.description));
				else
					dialog(&dialog_no_action, _(strings::insufficient_resource), info.cost, _(strings::energy));
			}
		}
	}

	for (s32 i = 0; i < (s32)Game::Resource::count; i++)
	{
		if (Game::save.resources[i] != inventory->resources_last[i])
			inventory->resources_blink_timer[i] = 0.5f;
		else if (data.story.tab_timer > TAB_ANIMATION_TIME)
			inventory->resources_blink_timer[i] = vi_max(0.0f, inventory->resources_blink_timer[i] - u.time.delta);
		inventory->resources_last[i] = Game::save.resources[i];
	}
}

#define STORY_MODE_INIT_TIME 2.0f

void story_mode_update(const Update& u)
{
	if (UIMenu::active[0] || Game::time.total < STORY_MODE_INIT_TIME)
		return;

	data.story.tab_timer += u.time.delta;

	// start the mode transition animation when we first open any tab
	if (data.story.tab_timer > TAB_ANIMATION_TIME && data.story.tab_timer - Game::real_time.delta <= TAB_ANIMATION_TIME)
		data.story.mode_transition_time = Game::real_time.total;

	if (can_switch_tab())
	{
		if (u.last_input->get(Controls::TabLeft, 0) && !u.input->get(Controls::TabLeft, 0))
		{
			data.story.tab_previous = data.story.tab;
			data.story.tab = (Tab)(vi_max(0, (s32)data.story.tab - 1));
			if (data.story.tab != data.story.tab_previous)
				data.story.tab_timer = 0.0f;
		}
		if (u.last_input->get(Controls::TabRight, 0) && !u.input->get(Controls::TabRight, 0))
		{
			data.story.tab_previous = data.story.tab;
			data.story.tab = (Tab)(vi_min((s32)Tab::count - 1, (s32)data.story.tab + 1));
			if (data.story.tab != data.story.tab_previous)
				data.story.tab_timer = 0.0f;
		}
	}

	tab_messages_update(u);
	tab_map_update(u);
	tab_inventory_update(u);
}

Vec2 get_panel_size(const Rect2& rect)
{
	return Vec2(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_draw(const RenderParams& p, const Data::StoryMode& data, Tab tab, const char* label, Vec2* pos, b8 flash = false)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab && !data.dialog_callback)
	{
		// flash the tab when it is selected
		if (data.tab_timer < TAB_ANIMATION_TIME)
		{
			if (UI::flash_function(Game::real_time.total))
				color = &UI::default_color;
			else
				draw = false; // don't draw the tab at all
		}
		else
			color = &UI::accent_color;
	}
	else
		color = &UI::disabled_color;

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	// calculate the width of the tab right now and the width of the tab before the last transition
	r32 current_width = data.tab == tab ? main_view_size.x : tab_size.x;
	r32 previous_width = data.tab_previous == tab ? main_view_size.x : tab_size.x;
	// then blend between the two widths for a nice animation
	r32 blend = Ease::cubic_out(vi_min(1.0f, data.tab_timer / TAB_ANIMATION_TIME), 0.0f, 1.0f);
	r32 width = LMath::lerpf(blend, previous_width, current_width);

	if (draw)
	{
		// if we're minimized, fill in the background
		const Vec4& background_color = data.tab == tab ? Vec4(0, 0, 0, 0) : Vec4(UI::background_color.xyz(), OPACITY);
		tab_draw_common(p, label, *pos, width, *color, background_color, flash && !UI::flash_function(Game::real_time.total) ? Vec4(0, 0, 0, 0) : UI::background_color);
	}

	Rect2 result = { *pos, { width, main_view_size.y } };

	pos->x += width + PADDING;

	return result;
}

void timestamp_string(r64 timestamp, char* str)
{
	r64 diff = platform::time() - timestamp;
	if (diff < 60.0f)
		sprintf(str, _(strings::now));
	else if (diff < 3600.0f)
		sprintf(str, "%d%s", (s32)(diff / 60.0f), _(strings::minute));
	else if (diff < 86400)
		sprintf(str, "%d%s", (s32)(diff / 3600.0f), _(strings::hour));
	else
		sprintf(str, "%d%s", (s32)(diff / 86400), _(strings::day));
}

void contacts_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect, const StaticArray<ContactDetails, MAX_CONTACTS>& contacts)
{
	Vec2 panel_size = get_panel_size(rect);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);
	r64 time = platform::time();
	data.messages.contact_scroll.start(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
	for (s32 i = 0; i < contacts.length; i++)
	{
		if (!data.messages.contact_scroll.item(i))
			continue;

		const ContactDetails& contact = contacts[i];
		b8 selected = data.tab == Tab::Messages && contact.name == data.messages.contact_selected;

		UI::box(p, { pos, panel_size }, UI::background_color);

		if (time - contact.last_message_timestamp > 0.5f || UI::flash_function(Game::real_time.total)) // flash new messages
		{
			if (selected)
				UI::border(p, Rect2(pos, panel_size).outset(-BORDER * UI::scale), BORDER, UI::accent_color);

			UIText text;
			text.size = TEXT_SIZE * (data.tab == Tab::Messages ? 1.0f : 0.75f);
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			if (data.tab == Tab::Messages)
			{
				text.color = selected ? UI::accent_color : Team::ui_color_friend;
				UIMenu::text_clip(&text, data.mode_transition_time + (i - data.messages.contact_scroll.pos) * 0.05f, 100.0f);
			}
			else
				text.color = UI::default_color;
			text.text("%s (%d)", _(contact.name), contact.unread);
			text.draw(p, pos + Vec2(PADDING, panel_size.y * 0.5f));

			if (data.tab == Tab::Messages)
			{
				text.color = selected ? UI::accent_color : UI::default_color;
				char buffer[STRING_BUFFER_SIZE];
				{
					// truncate if necessary
					const char* msg_text = _(contact.last_message_text);
					memory_index len = utf8len(msg_text);
					const s32 truncate = MESSAGE_TRUNCATE_SHORT;
					if (len > truncate)
					{
						char buffer2[STRING_BUFFER_SIZE];
						utf8ncpy(buffer2, msg_text, truncate - 3);
						buffer2[truncate - 3] = '\0';
						sprintf(buffer, "%s...", buffer2);
					}
					else
						utf8cpy(buffer, msg_text);
				}
				text.font = Asset::Font::pt_sans;
				text.text_raw(buffer, UITextFlagSingleLine);
				text.draw(p, pos + Vec2(panel_size.x * 0.35f, panel_size.y * 0.5f));

				text.color = selected ? UI::accent_color : UI::alert_color;
				text.font = Asset::Font::lowpoly;
				timestamp_string(contact.last_message_timestamp, buffer);
				text.anchor_x = UIText::Anchor::Max;
				text.text(buffer);
				text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));
			}
		}
			
		pos.y -= panel_size.y;
	}
	data.messages.contact_scroll.end(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
}

void tab_messages_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	Vec2 panel_size = get_panel_size(rect);

	Vec2 top_bar_size(rect.size.x, panel_size.y * 1.5f);

	StaticArray<ContactDetails, MAX_CONTACTS> contacts;
	collect_contacts(&contacts);

	if (data.tab == Tab::Messages)
	{
		// full view
		if (contacts.length > 0)
		{
			switch (data.messages.mode)
			{
				case Data::Messages::Mode::Contacts:
				{
					contacts_draw(p, data, rect, contacts);
					break;
				}
				case Data::Messages::Mode::Messages:
				{
					Array<Game::Message> msg_list;
					collect_messages(&msg_list, data.messages.contact_selected);

					r64 time = platform::time();
					Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

					// top bar
					{
						UI::box(p, { pos, top_bar_size }, UI::background_color);

						UIText text;
						text.size = TEXT_SIZE;
						text.anchor_x = UIText::Anchor::Min;
						text.anchor_y = UIText::Anchor::Center;
						text.color = Team::ui_color_friend;
						UIMenu::text_clip(&text, data.mode_transition_time, 80.0f);
						text.text(_(data.messages.contact_selected));
						text.draw(p, pos + Vec2(PADDING, top_bar_size.y * 0.5f));

						if (data.messages.contact_selected == strings::contact_cora)
							text.text("%s    %s", _(strings::prompt_call), _(strings::prompt_back));
						else
							text.text(_(strings::prompt_back));
						text.color = UI::default_color;
						text.anchor_x = UIText::Anchor::Max;
						text.draw(p, pos + Vec2(top_bar_size.x - PADDING, top_bar_size.y * 0.5f));

						pos.y -= panel_size.y + PADDING * 2.0f;
					}

					data.messages.message_scroll.start(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
					for (s32 i = 0; i < msg_list.length; i++)
					{
						if (!data.messages.message_scroll.item(i))
							continue;

						const Game::Message& msg = msg_list[i];
						b8 selected = msg.text == data.messages.message_selected;

						UI::box(p, { pos, panel_size }, UI::background_color);

						if (time - msg.timestamp > 0.5f || UI::flash_function(Game::real_time.total)) // flash new messages
						{
							if (selected)
								UI::border(p, Rect2(pos, panel_size).outset(-2.0f * UI::scale), 2.0f, UI::accent_color);

							UIText text;
							text.size = TEXT_SIZE;
							text.anchor_x = UIText::Anchor::Min;
							text.anchor_y = UIText::Anchor::Center;
							text.color = selected ? UI::accent_color : UI::alert_color;
							UIMenu::text_clip(&text, data.mode_transition_time + (i - data.messages.message_scroll.pos) * 0.05f, 100.0f);

							if (!msg.read)
								UI::triangle(p, { pos + Vec2(panel_size.x * 0.05f, panel_size.y * 0.5f), Vec2(12.0f * UI::scale) }, UI::alert_color, PI * -0.5f);

							char buffer[STRING_BUFFER_SIZE];
							{
								// truncate if necessary
								const char* msg_text = _(msg.text);
								memory_index len = utf8len(msg_text);
								const s32 truncate = MESSAGE_TRUNCATE_LONG;
								if (len > truncate)
								{
									char buffer2[STRING_BUFFER_SIZE];
									utf8ncpy(buffer2, msg_text, truncate - 3);
									buffer2[truncate - 3] = '\0';
									sprintf(buffer, "%s...", buffer2);
								}
								else
									utf8cpy(buffer, msg_text);
							}
							text.font = Asset::Font::pt_sans;
							text.text_raw(buffer, UITextFlagSingleLine);
							text.color = selected ? UI::accent_color : UI::default_color;
							text.draw(p, pos + Vec2(panel_size.x * 0.1f, panel_size.y * 0.5f));

							timestamp_string(msg.timestamp, buffer);
							text.font = Asset::Font::lowpoly;
							text.color = UI::alert_color;
							text.text(buffer);
							text.anchor_x = UIText::Anchor::Max;
							text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));
						}

						pos.y -= panel_size.y;
					}
					data.messages.message_scroll.end(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
					break;
				}
				case Data::Messages::Mode::Message:
				{
					Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

					// top bar
					UI::box(p, { pos, top_bar_size }, UI::background_color);

					Game::Message* msg;
					for (s32 i = 0; i < Game::save.messages.length; i++)
					{
						if (Game::save.messages[i].text == data.messages.message_selected)
						{
							msg = &Game::save.messages[i];
							break;
						}
					}

					UIText text;
					text.size = TEXT_SIZE;
					text.anchor_x = UIText::Anchor::Min;
					text.anchor_y = UIText::Anchor::Center;
					text.color = Team::ui_color_friend;
					UIMenu::text_clip(&text, data.mode_transition_time, 80.0f);
					text.text(_(data.messages.contact_selected));
					text.draw(p, pos + Vec2(PADDING, top_bar_size.y * 0.5f));

					char buffer[64];
					timestamp_string(msg->timestamp, buffer);
					text.text(buffer);
					text.color = UI::alert_color;
					text.draw(p, pos + Vec2(top_bar_size.x * 0.5f, top_bar_size.y * 0.5f));

					if (data.messages.contact_selected == strings::contact_cora)
						text.text("%s    %s", _(strings::prompt_call), _(strings::prompt_back));
					else
						text.text(_(strings::prompt_back));
					text.color = UI::default_color;
					text.anchor_x = UIText::Anchor::Max;
					text.draw(p, pos + Vec2(top_bar_size.x - PADDING, top_bar_size.y * 0.5f));

					pos.y -= PADDING;

					// main body

					text.anchor_y = UIText::Anchor::Max;
					text.anchor_x = UIText::Anchor::Min;
					text.wrap_width = panel_size.x + PADDING * -2.0f;
					text.font = Asset::Font::pt_sans;
					UIMenu::text_clip(&text, data.mode_transition_time, 150.0f);
					text.text(_(msg->text));
					Vec2 text_pos = pos + Vec2(PADDING, -PADDING);
					UI::box(p, text.rect(text_pos).outset(PADDING), UI::background_color);
					text.draw(p, text_pos);

					break;
				}
				case Data::Messages::Mode::Cora:
				{
					if (data.messages.cora_timer > 0.0f)
					{
						progress_draw(p, _(strings::calling), rect.pos + rect.size * 0.5f);

						// cancel prompt
						UIText text;
						text.anchor_x = text.anchor_y = UIText::Anchor::Center;
						text.color = UI::accent_color;
						text.text(_(strings::prompt_cancel));

						Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

						UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::background_color);
						text.draw(p, pos);
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
	else
	{
		// minimized view
		contacts_draw(p, data, rect, contacts);
	}
}

void zone_stat_draw(const RenderParams& p, const Rect2& rect, s32 index, const char* label, const Vec4& color)
{
	UIText text;
	text.anchor_x = UIText::Anchor::Min;
	text.anchor_y = UIText::Anchor::Max;
	text.size = TEXT_SIZE * SCALE_MULTIPLIER * 0.75f;
	text.color = color;
	text.wrap_width = MENU_ITEM_WIDTH * 0.5f + (PADDING * -2.0f);
	Vec2 pos = rect.pos + Vec2(PADDING, rect.size.y - PADDING + index * (text.size * -UI::scale - PADDING));
	text.text_raw(label);
	UI::box(p, text.rect(pos).outset(PADDING), UI::background_color);
	text.draw(p, pos);
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& story, const Rect2& rect)
{
	if (story.tab == Tab::Map)
	{
		zones_draw(p);

		// show selected zone info
		Game::ZoneState zone_state = Game::save.zones[data.zone_selected];
		if (zone_state == Game::ZoneState::Hostile || zone_state == Game::ZoneState::Friendly || zone_state == Game::ZoneState::Owned)
		{
			// show stats
			const ZoneNode* zone = get_zone_node(data.zone_selected);
			zone_stat_draw(p, rect, 0, Loader::level_name(data.zone_selected), zone_ui_color(*zone));
			char buffer[255];
			sprintf(buffer, _(strings::energy_generation), zone->size * ENERGY_GENERATION_PER_CELL);
			zone_stat_draw(p, rect, 1, buffer, UI::default_color);

			if (zone_state == Game::ZoneState::Hostile)
			{
				// show potential rewards
				b8 has_rewards = false;
				for (s32 i = 0; i < (s32)Game::Resource::count; i++)
				{
					if (zone->rewards[i] > 0)
					{
						has_rewards = true;
						break;
					}
				}

				if (has_rewards)
				{
					zone_stat_draw(p, rect, 2, _(strings::capture_bonus), UI::accent_color);
					s32 index = 3;
					for (s32 i = 0; i < (s32)Game::Resource::count; i++)
					{
						if (zone->rewards[i] > 0)
						{
							sprintf(buffer, "%d %s", zone->rewards[i], _(resource_info[i].description));
							zone_stat_draw(p, rect, index, buffer, UI::default_color);
							index++;
						}
					}
				}
			}
		}
		else if (zone_state == Game::ZoneState::Locked)
			zone_stat_draw(p, rect, 0, _(strings::unknown), Team::ui_color_enemy);

		if (can_switch_tab())
		{
			// action prompt
			AssetID prompt;
			if (zone_state == Game::ZoneState::Hostile)
				prompt = strings::prompt_capture;
			else if (zone_state == Game::ZoneState::Locked)
				prompt = strings::prompt_hack;
			else
				prompt = AssetNull;

			if (prompt != AssetNull)
			{
				UIText text;
				text.anchor_x = text.anchor_y = UIText::Anchor::Center;
				text.color = UI::accent_color;
				text.text(_(prompt));

				Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

				UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::background_color);

				text.draw(p, pos);
			}
		}

		if (story.map.hack_timer > 0.0f)
			bar_draw(p, _(strings::hacking), 1.0f - (story.map.hack_timer / HACK_TIME), p.camera->viewport.size * Vec2(0.5f, 0.2f));
		else if (story.map.capture_timer > 0.0f)
			progress_draw(p, _(strings::capturing), p.camera->viewport.size * Vec2(0.5f, 0.2f));
	}
}

void inventory_items_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	Vec2 panel_size = get_panel_size(rect);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);
	for (s32 i = 0; i < (s32)Game::Resource::count; i++)
	{
		b8 selected = data.tab == Tab::Inventory && data.inventory.resource_selected == (Game::Resource)i;

		UI::box(p, { pos, panel_size }, UI::background_color);
		if (selected)
			UI::border(p, Rect2(pos, panel_size).outset(BORDER * -UI::scale), BORDER, UI::accent_color);

		r32 icon_size = 18.0f * SCALE_MULTIPLIER * UI::scale;

		b8 flash = data.inventory.resources_blink_timer[i] > 0.0f;
		b8 draw = !flash || UI::flash_function(Game::real_time.total);

		const Vec4* color;
		if (flash)
			color = &UI::accent_color;
		else if (selected)
			color = &UI::accent_color;
		else if (Game::save.resources[i] == 0)
			color = &UI::alert_color;
		else
			color = &UI::default_color;
		
		const ResourceInfo& info = resource_info[i];

		if (draw)
			UI::mesh(p, info.icon, pos + Vec2(PADDING + icon_size * 0.5f, panel_size.y * 0.5f), Vec2(icon_size), *color);

		UIText text;
		text.anchor_x = UIText::Anchor::Max;
		text.anchor_y = UIText::Anchor::Center;
		text.color = *color;
		text.size = TEXT_SIZE * (data.tab == Tab::Inventory ? 1.0f : 0.75f);
		text.text("%d", Game::save.resources[i]);
		if (draw)
			text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));

		if (draw && data.tab == Tab::Inventory)
		{
			text.anchor_x = UIText::Anchor::Min;
			text.text(_(info.description));
			text.draw(p, pos + Vec2(icon_size * 2.0f + PADDING * 2.0f, panel_size.y * 0.5f));
		}

		pos.y -= panel_size.y;
	}
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	inventory_items_draw(p, data, rect);

	if (data.inventory.buy_timer > 0.0f)
		bar_draw(p, _(strings::buying), 1.0f - (data.inventory.buy_timer / BUY_TIME), p.camera->viewport.size * Vec2(0.5f, 0.2f));
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	if (Game::time.total < STORY_MODE_INIT_TIME)
	{
		progress_draw(p, _(strings::connecting), p.camera->viewport.size * Vec2(0.5f, 0.2f));
		return;
	}

	const Rect2& vp = p.camera->viewport;

	const Vec2 main_view_size = MAIN_VIEW_SIZE;
	const Vec2 tab_size = TAB_SIZE;

	Vec2 center = vp.size * 0.5f;
	Vec2 total_size = Vec2(main_view_size.x + (tab_size.x + PADDING) * 2.0f, main_view_size.y);
	Vec2 bottom_left = center + total_size * -0.5f + Vec2(0, -tab_size.y);

	Vec2 pos = bottom_left;
	{
		s32 unread_count;
		r64 most_recent_message;
		message_statistics(&unread_count, &most_recent_message);
		b8 flash = platform::time() - most_recent_message < 0.5f;
		char message_label[64];
		sprintf(message_label, _(strings::tab_messages), unread_count);
		Rect2 rect = tab_draw(p, data.story, Tab::Messages, message_label, &pos, flash).outset(-PADDING);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
		{
			tab_messages_draw(p, data.story, rect);
			Cora::draw(p, rect.pos + rect.size * 0.5f);
		}
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Map, _(strings::tab_map), &pos).outset(-PADDING);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_map_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Inventory, _(strings::tab_inventory), &pos).outset(-PADDING);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_inventory_draw(p, data.story, rect);
	}

	// left/right tab control prompt
	if (can_switch_tab())
	{
		UIText text;
		text.size = TEXT_SIZE;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::default_color;
		text.text("[{{TabLeft}}]");

		Vec2 pos = bottom_left + Vec2(tab_size.x * 0.5f, main_view_size.y + tab_size.y * 1.5f);
		UI::box(p, text.rect(pos).outset(PADDING), UI::background_color);
		text.draw(p, pos);

		pos.x += total_size.x - tab_size.x;
		text.text("[{{TabRight}}]");
		UI::box(p, text.rect(pos).outset(PADDING), UI::background_color);
		text.draw(p, pos);
	}
}

b8 should_draw_zones()
{
	return data.state == State::SplitscreenSelectZone
		|| data.state == State::SplitscreenDeploying
		|| data.state == State::Deploying
		|| (data.state == State::StoryMode && data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME);
}

void splitscreen_select_zone_update(const Update& u)
{
	// cancel
	if (Game::session.local_multiplayer
		&& !Game::cancel_event_eaten[0]
		&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
	{
		data.state = State::SplitscreenSelectTeams;
		Game::cancel_event_eaten[0] = true;
		return;
	}

	select_zone_update(u, true);

	// deploy button
	if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
	{
		const ZoneNode* zone = get_zone_node(data.zone_selected);
		if (splitscreen_team_count() <= zone->max_teams)
			deploy_start();
	}
}

void update(const Update& u)
{
	if (data.zone_last == Asset::Level::terminal && Game::session.level != Asset::Level::terminal)
	{
		// cleanup
		data.~Data();
		data = Data();
		data.zone_last = Asset::Level::terminal;
	}

	if (Game::session.level == Asset::Level::terminal && !Console::visible)
	{
		DialogCallback dialog_callback_old = data.story.dialog_callback;

		switch (data.state)
		{
			case State::StoryMode:
			{
				story_mode_update(u);
				break;
			}
			case State::SplitscreenSelectTeams:
			{
				splitscreen_select_teams_update(u);
				break;
			}
			case State::SplitscreenSelectZone:
			{
				splitscreen_select_zone_update(u);
				break;
			}
			case State::Deploying:
			case State::SplitscreenDeploying:
			{
				deploy_update(u);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		// dialog
		if (data.story.dialog_time_limit > 0.0f)
		{
			data.story.dialog_time_limit = vi_max(0.0f, data.story.dialog_time_limit - u.time.delta);
			if (data.story.dialog_time_limit == 0.0f)
				data.story.dialog_callback = nullptr; // cancel
		}

		// dialog buttons
		if (data.story.dialog_callback && dialog_callback_old // make sure we don't trigger the button on the first frame the dialog is shown
			&& (Game::session.local_multiplayer || Game::time.total > STORY_MODE_INIT_TIME)) // don't show dialog until story mode is initialized
		{
			if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				// accept
				DialogCallback callback = data.story.dialog_callback;
				data.story.dialog_callback = nullptr;
				callback();
			}
			else if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			{
				// cancel
				data.story.dialog_callback = nullptr;
				Game::cancel_event_eaten[0] = true;
			}
		}

		// pause
		if (!Game::cancel_event_eaten[0]
			&& ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
				|| (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))))
		{
			Game::cancel_event_eaten[0] = true;
			Menu::show();
		}
	}
	data.zone_last = Game::session.level;
}

void draw_override(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default || Game::session.level != Asset::Level::terminal)
		return;

	if (should_draw_zones())
		zones_draw_override(params);
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default || Game::session.level != Asset::Level::terminal)
		return;

	switch (data.state)
	{
		case State::SplitscreenSelectTeams:
		{
			splitscreen_select_teams_draw(params);
			break;
		}
		case State::StoryMode:
		{
			story_mode_draw(params);
			break;
		}
		case State::SplitscreenSelectZone:
		{
			splitscreen_select_zone_draw(params);
			break;
		}
		case State::Deploying:
		case State::SplitscreenDeploying:
		{
			deploy_draw(params);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}

	// draw dialog box
	if (data.story.dialog_callback
		&& (Game::session.local_multiplayer || Game::time.total > STORY_MODE_INIT_TIME)) // don't show dialog until story mode is initialized
	{
		const r32 padding = 16.0f * UI::scale;
		UIText text;
		text.color = UI::default_color;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.text(data.story.dialog);
		UIMenu::text_clip(&text, data.story.dialog_time, 150.0f);
		Vec2 pos = params.camera->viewport.size * 0.5f;
		Rect2 text_rect = text.rect(pos).outset(padding);
		UI::box(params, text_rect, UI::background_color);
		text.draw(params, pos);

		// accept
		text.wrap_width = 0;
		text.anchor_y = UIText::Anchor::Max;
		text.anchor_x = UIText::Anchor::Min;
		text.color = UI::accent_color;
		text.clip = 0;
		text.text(data.story.dialog_time_limit > 0.0f ? "%s (%d)" : "%s", _(strings::prompt_accept), (s32)(data.story.dialog_time_limit) + 1);
		Vec2 prompt_pos = text_rect.pos + Vec2(padding, 0);
		Rect2 prompt_rect = text.rect(prompt_pos).outset(padding);
		prompt_rect.size.x = text_rect.size.x;
		UI::box(params, prompt_rect, UI::background_color);
		text.draw(params, prompt_pos);

		if (data.story.dialog_callback != &dialog_no_action)
		{
			// cancel
			text.anchor_x = UIText::Anchor::Max;
			text.color = UI::alert_color;
			text.clip = 0;
			text.text(_(strings::prompt_cancel));
			text.draw(params, prompt_pos + Vec2(text_rect.size.x + padding * -2.0f, 0));
		}

		UI::border(params, { prompt_rect.pos, prompt_rect.size + Vec2(0, text_rect.size.y - padding) }, BORDER, UI::accent_color);
	}
}

void show()
{
	Game::schedule_load_level(Asset::Level::terminal, Game::Mode::Special);
}

void init(const Update& u, const EntityFinder& entities)
{
	if (Game::session.level != Asset::Level::terminal)
		return;

	data.tip_index = mersenne::rand() % tip_count;
	if (data.zone_last == Asset::Level::title)
		data.zone_last = Asset::Level::terminal;
	if (data.zone_last == Asset::Level::terminal)
		data.zone_selected = Game::session.local_multiplayer ? Asset::Level::Medias_Res : Asset::Level::Safe_Zone;
	else
		data.zone_selected = data.zone_last;
	data.camera = Camera::add();

	{
		Entity* map_view_entity = entities.find("map_view");
		Transform* map_view = map_view_entity->get<Transform>();
		data.camera_offset = map_view;

		data.story.camera_messages = entities.find("camera_messages")->get<Transform>();
		data.story.camera_inventory = entities.find("camera_inventory")->get<Transform>();

		data.camera->pos = data.story.camera_messages.ref()->absolute_pos();
		data.camera->rot = Quat::look(data.story.camera_messages.ref()->absolute_rot() * Vec3(0, -1, 0));
	}

	{
		for (s32 i = 0; i < entities.map.length; i++)
		{
			const EntityFinder::NameEntry& entry = entities.map[i];
			const char* zone_name = Json::get_string(entry.properties, "Zone");
			if (zone_name)
			{
				View* view = entry.entity.ref()->get<View>();
				view->mask = 0; // don't render this; we will render it manually in zones_draw_override()
				view->shader = Asset::Shader::flat;

				AssetID level_id = Loader::find_level(zone_name);
				if (level_id != Asset::Level::Safe_Zone || !Game::session.local_multiplayer) // only show tutorial level in story mode
				{
					ZoneNode* node = data.zones.add();
					*node =
					{
						level_id,
						view->get<Transform>(),
						{
							(u16)Json::get_s32(entry.properties, "energy", 0),
							(u16)Json::get_s32(entry.properties, "hack_kits", 0),
							(u16)Json::get_s32(entry.properties, "drones", 0),
						},
						(u8)Json::get_s32(entry.properties, "size", 1),
						(u8)Json::get_s32(entry.properties, "max_teams", 2),
					};
				}
			}
		}
	}

	// set up camera
	data.camera->viewport =
	{
		Vec2(0, 0),
		Vec2(u.input->width, u.input->height),
	};
	r32 aspect = data.camera->viewport.size.y == 0 ? 1 : (r32)data.camera->viewport.size.x / (r32)data.camera->viewport.size.y;
	data.camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);

	if (Game::session.local_multiplayer)
	{
		if (Game::session.local_player_count() <= 1) // haven't selected teams yet
			data.state = State::SplitscreenSelectTeams;
		else
			data.state = State::SplitscreenSelectZone; // already selected teams, go straight to level select; the player can always go back
	}
	else
	{
		data.state = State::StoryMode;

		if (Game::save.messages.length == 0) // initial messages
		{
			message_add(strings::contact_ivory_corp, strings::msg_ivory_corp_intro, platform::time() - (86400.0 * 1.9));
			message_add(strings::contact_albert, strings::msg_albert_intro, platform::time() - (86400.0 * 1.6));
			message_add(strings::contact_albert, strings::msg_albert_intro_2, platform::time() - (86400.0 * 1.5));
			Game::save.resources[(s32)Game::Resource::HackKits] = 1;
			Game::save.resources[(s32)Game::Resource::Drones] = 4;
			Game::save.resources[(s32)Game::Resource::Energy] = (u16)(CREDITS_INITIAL * 3.5f);
			Game::save.zones[Asset::Level::Safe_Zone] = Game::ZoneState::Locked;
		}

		if (data.zone_last != Asset::Level::terminal)
			story_zone_done(data.zone_last, Game::session.last_match);
	}

	Cora::init();
	Cora::conversation_finished().link(&conversation_finished);
}

}

}
