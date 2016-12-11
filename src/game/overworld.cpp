#include "overworld.h"
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
#include "noise.h"
#include "team.h"

namespace VI
{


namespace Overworld
{

#define DEPLOY_TIME_LOCAL 1.0f

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
#define AUTO_CAPTURE_TIME 30.0f
#define ZONE_MAX_CHILDREN 12
#define EVENT_INTERVAL_PER_ZONE (60.0f * 5.0f)
#define EVENT_ODDS_PER_ZONE (1.0f / EVENT_INTERVAL_PER_ZONE) // an event will happen on average every X minutes per zone you own

struct ZoneNode
{
	StaticArray<Vec3, ZONE_MAX_CHILDREN> children;
	AssetID id;
	AssetID mesh;
	s16 rewards[s32(Resource::count)];
	s8 size;
	s8 max_teams;

	inline Vec3 pos() const
	{
		return children[children.length - 1];
	}
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
	AssetNull, // 3 - you've joined a group
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

#define DEPLOY_ANIMATION_TIME 1.0f

struct PropEntry
{
	Quat rot;
	Vec3 pos;
	AssetID mesh;
};

struct WaterEntry
{
	Quat rot;
	Vec3 pos;
	Water::Config config;
};

struct DataGlobal
{
	StaticArray<ZoneNode, MAX_ZONES> zones;
	Array<PropEntry> props;
	Array<WaterEntry> waters;
	Vec3 camera_offset_pos;
	Quat camera_offset_rot;
	Vec3 camera_messages_pos;
	Quat camera_messages_rot;
	Vec3 camera_inventory_pos;
	Quat camera_inventory_rot;
};
DataGlobal global;

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
		r32 timer_cora;
	};

	struct Inventory
	{
		enum class Mode
		{
			Normal,
			Buy,
		};

		r32 resources_blink_timer[s32(Resource::count)];
		r32 timer_buy;
		Resource resource_selected;
		Mode mode;
		s16 buy_quantity;
		s16 resources_last[s32(Resource::count)];
	};

	struct Map
	{
		r32 zones_change_time[MAX_ZONES];
	};

	struct StoryMode
	{
		r64 timestamp_last;
		Tab tab = Tab::Map;
		Tab tab_previous = Tab::Messages;
		r32 tab_timer;
		r32 mode_transition_time;
		Messages messages;
		Inventory inventory;
		Map map;
	};

	Camera* camera;
	Camera camera_restore_data;
	Quat camera_rot;
	Vec3 camera_pos;
	r32 timer_deploy;
	r32 timer_transition;
	State state;
	StoryMode story;
	AssetID zone_selected = AssetNull;
	b8 active;
	b8 active_next;
};

Data data = Data();

s32 splitscreen_team_count()
{
	s32 player_count = 0;
	s32 team_counts[MAX_PLAYERS] = {};
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team team = Game::session.local_player_config[i];
		if (team != AI::TeamNone)
			team_counts[s32(team)]++;
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

void message_add(AssetID contact, AssetID text, r64 timestamp)
{
	if (message_get(text))
		return; // already sent

	Game::Message* msg = Game::save.messages.insert(0);
	msg->contact = contact;
	msg->text = text;
	if (timestamp == -1.0)
		msg->timestamp = platform::timestamp();
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
	msg->timestamp = platform::timestamp() + delay;
	msg->read = false;
}

void splitscreen_select_teams_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0)
		&& !Game::cancel_event_eaten[0] && Game::scheduled_load_level == AssetNull)
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

void tab_draw_common(const RenderParams& p, const char* label, const Vec2& pos, r32 width, const Vec4& color, const Vec4& background_color, const Vec4& text_color = UI::color_background)
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
	text.color = UI::color_background;
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
		Vec4 background_color = Vec4(UI::color_background.xyz(), OPACITY);
		tab_draw_common(params, _(strings::prompt_splitscreen), bottom_left, main_view_size.x, UI::color_accent, background_color);
	}

	UIText text;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::color_accent;
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
	text.color = UI::color_background;
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
			color = &UI::color_disabled;
			x_offset = team_offset * -2.0f;
		}
		else if (team == AI::TeamNone)
		{
			color = &UI::color_default;
			x_offset = team_offset * -2.0f;
		}
		else
		{
			color = &UI::color_accent;
			x_offset = ((r32)team - 1.0f) * team_offset;
		}

		draw_gamepad_icon(params, pos + Vec2(x_offset, 0), i, *color, SCALE_MULTIPLIER);
	}
}

void go(AssetID zone)
{
	vi_assert(Game::level.local);
	if (Game::session.story_mode)
	{
		PlayerControlHuman* player = PlayerControlHuman::list.iterator().item();
		Game::save.zone_current_restore = true;
		Game::save.zone_current_restore_position = player->get<Transform>()->absolute_pos();
		Game::save.zone_current_restore_rotation = player->get<PlayerCommon>()->angle_horizontal;
	}
	Game::schedule_load_level(zone, Game::Mode::Pvp);
}

void focus_camera(const Update& u, const Vec3& target_pos, const Quat& target_rot)
{
	data.camera_pos += (target_pos - data.camera_pos) * vi_min(1.0f, 5.0f * Game::real_time.delta);
	data.camera_rot = Quat::slerp(vi_min(1.0f, 5.0f * Game::real_time.delta), data.camera_rot, target_rot);
}

void focus_camera(const Update& u, const ZoneNode& zone)
{
	Vec3 target_pos = zone.pos() + global.camera_offset_pos;
	focus_camera(u, target_pos, global.camera_offset_rot);
}

const ZoneNode* zone_node_get(AssetID id)
{
	for (s32 i = 0; i < global.zones.length; i++)
	{
		if (global.zones[i].id == id)
			return &global.zones[i];
	}
	return nullptr;
}

b8 resource_spend(Resource res, s16 amount)
{
	if (Game::save.resources[s32(res)] >= amount)
	{
		Game::save.resources[s32(res)] -= amount;
		return true;
	}
	return false;
}

void deploy_start()
{
	data.state = Game::session.story_mode ? State::Deploying : State::SplitscreenDeploying;
	data.timer_deploy = DEPLOY_TIME_LOCAL;
}

s16 energy_increment_zone(const ZoneNode& zone)
{
	return zone.size * (zone.max_teams == MAX_PLAYERS ? 200 : 10);
}

s16 energy_increment_total()
{
	s16 result = 0;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		ZoneState zone_state = Game::save.zones[zone.id];
		if (zone_state == ZoneState::Friendly)
			result += energy_increment_zone(zone);
	}
	if (Game::save.group != Game::Group::None)
		result = result / 8;
	return result;
}

b8 zone_can_select(AssetID z)
{
	if (Game::session.story_mode)
		return true;
	else
	{
		const ZoneNode* zone = zone_node_get(z);
		return zone && splitscreen_team_count() <= zone->max_teams;
	}
}

void select_zone_update(const Update& u, b8 enable_movement)
{
	if (UIMenu::active[0])
		return;

	const ZoneNode* zone = zone_node_get(data.zone_selected);
	if (!zone)
		return;

	// movement
	if (enable_movement)
	{
		Vec2 movement(0, 0);

		b8 keyboard = false;

		// buttons/keys
		{
			if (u.input->get(Controls::Left, 0) && !u.last_input->get(Controls::Left, 0))
			{
				movement.x -= 1.0f;
				keyboard = true;
			}
			if (u.input->get(Controls::Right, 0) && !u.last_input->get(Controls::Right, 0))
			{
				movement.x += 1.0f;
				keyboard = true;
			}
			if (u.input->get(Controls::Forward, 0) && !u.last_input->get(Controls::Forward, 0))
			{
				movement.y -= 1.0f;
				keyboard = true;
			}
			if (u.input->get(Controls::Backward, 0) && !u.last_input->get(Controls::Backward, 0))
			{
				movement.y += 1.0f;
				keyboard = true;
			}
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
			Vec3 movement3d = data.camera_rot * Vec3(-movement.x, 0, -movement.y);
			movement = Vec2(movement3d.x, movement3d.z);
			const ZoneNode* closest = nullptr;
			r32 closest_dot = FLT_MAX;
			r32 closest_normalized_dot = 0.6f;

			for (s32 i = 0; i < zone->children.length; i++)
			{
				const Vec3& zone_pos = zone->children[i];
				for (s32 j = 0; j < global.zones.length; j++)
				{
					const ZoneNode& candidate = global.zones[j];
					if (&candidate == zone || !zone_can_select(candidate.id))
						continue;

					for (s32 k = 0; k < candidate.children.length; k++)
					{
						const Vec3& candidate_pos = candidate.children[k];
						Vec3 to_candidate = (candidate_pos - zone_pos);
						r32 dot = movement.dot(Vec2(to_candidate.x, to_candidate.z));
						r32 normalized_dot = movement.dot(Vec2::normalize(Vec2(to_candidate.x, to_candidate.z)));
						r32 mixed_dot = dot * vi_max(normalized_dot, 0.9f);
						if (mixed_dot < closest_dot && normalized_dot > closest_normalized_dot)
						{
							closest = &candidate;
							closest_normalized_dot = vi_min(0.8f, normalized_dot);
							closest_dot = vi_max(2.0f, mixed_dot);
						}
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
	if (!Game::session.story_mode)
	{
		if (splitscreen_team_count() <= zone.max_teams)
			return Team::color_friend.xyz();
		else
			return Vec3(0.25f);
	}
	else
	{
		ZoneState zone_state = Game::save.zones[zone.id];
		switch (zone_state)
		{
			case ZoneState::Locked:
			{
				return Vec3(0.25f);
			}
			case ZoneState::Friendly:
			{
				return Team::color_friend.xyz();
			}
			case ZoneState::Hostile:
			{
				return Team::color_enemy.xyz();
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
	if (Game::session.story_mode)
	{
		ZoneState zone_state = Game::save.zones[zone.id];
		switch (zone_state)
		{
			case ZoneState::Locked:
			{
				return UI::color_disabled;
			}
			case ZoneState::Friendly:
			{
				return Team::ui_color_friend;
			}
			case ZoneState::Hostile:
			{
				return Team::ui_color_enemy;
			}
			default:
			{
				vi_assert(false);
				return UI::color_default;
			}
		}
	}
	else
	{
		if (splitscreen_team_count() <= zone.max_teams)
			return Team::ui_color_friend;
		else
			return UI::color_disabled;
	}
}

void zone_draw_mesh(const RenderParams& params, AssetID mesh, const Vec3& pos, const Vec4& color)
{
	const Mesh* mesh_data = Loader::mesh_permanent(mesh);

	Loader::shader(Asset::Shader::standard_flat);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_flat);
	sync->write(params.technique);

	Mat4 m;
	m.make_translate(pos);
	Mat4 mvp = m * params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mvp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(mvp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::mv);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(m * params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(color);

	sync->write(RenderOp::Mesh);
	sync->write(RenderPrimitiveMode::Triangles);
	sync->write(mesh);
}

#define BACKGROUND_COLOR Vec4(0.8f, 0.8f, 0.8f, 1)
void zones_draw_override(const RenderParams& params)
{
	struct SortKey
	{
		r32 priority(const ZoneNode& n)
		{
			// sort farthest zones first
			Vec3 camera_forward = data.camera_rot * Vec3(0, 0, 1);
			return camera_forward.dot(n.pos() - data.camera_pos);
		}
	};

	SortKey key;
	PriorityQueue<ZoneNode, SortKey> zones(&key);

	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		// flash if necessary
		if (!Game::session.story_mode
			|| Game::real_time.total > data.story.map.zones_change_time[zone.id] + 0.5f
			|| UI::flash_function(Game::real_time.total))
		{
			zones.push(zone);
		}
	}

	RenderSync* sync = params.sync;

	sync->write(RenderOp::DepthTest);
	sync->write<b8>(true);

	while (zones.size() > 0)
	{
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Back);
		const ZoneNode& zone = zones.pop();
		zone_draw_mesh(params, zone.mesh, zone.pos(), Vec4(zone_color(zone), 1.0f));
		sync->write<RenderOp>(RenderOp::CullMode);
		sync->write<RenderCullMode>(RenderCullMode::Front);
		zone_draw_mesh(params, zone.mesh, zone.pos(), BACKGROUND_COLOR);
	}
}

// returns current zone node
const ZoneNode* zones_draw(const RenderParams& params)
{
	if (data.timer_deploy > 0.0f || Game::scheduled_load_level != AssetNull)
		return nullptr;

	// highlight zone locations
	const ZoneNode* selected_zone = zone_node_get(data.zone_selected);

	if (Game::session.story_mode)
	{
		// "you are here"
		const ZoneNode* current_zone = zone_node_get(Game::level.id);
		Vec2 p;
		if (current_zone && UI::project(params, current_zone->pos(), &p))
			UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_accent, PI);

		// highlight selected zone
		if (selected_zone)
		{
			Vec2 p;
			if (UI::project(params, selected_zone->pos(), &p))
				UI::triangle_border(params, { p, Vec2(48.0f * UI::scale) }, BORDER * 2.0f, UI::color_accent, PI);
		}

		// zone under attack
		const ZoneNode* under_attack = zone_node_get(zone_under_attack());
		if (under_attack)
		{
			Vec2 p;
			if (UI::is_onscreen(params, under_attack->pos(), &p))
			{
				if (UI::flash_function(Game::real_time.total))
					UI::triangle(params, { p, Vec2(24.0f * UI::scale) }, UI::color_alert, PI);
			}
			else
			{
				if (UI::flash_function(Game::real_time.total))
					UI::indicator(params, under_attack->pos(), UI::color_alert, true, 1.0f, PI);
			}

			UIText text;
			text.color = UI::color_alert;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			r32 time = zone_under_attack_timer();
			if (time > 0.0f)
				text.text("%d", s32(ceilf(time)));
			else
				text.text(_(strings::zone_defense_expired));

			{
				Vec2 text_pos = p;
				text_pos.y += 32.0f * UI::scale;
				UI::box(params, text.rect(text_pos).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, text_pos);
			}
		}
	}
	else
	{
		// not story mode

		// draw selected zone name
		if (selected_zone)
		{
			Vec2 p;
			if (UI::project(params, selected_zone->pos(), &p))
			{
				UIText text;
				text.color = zone_ui_color(*selected_zone);
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.text_raw(AssetLookup::Level::names[selected_zone->id]);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::color_background);
				text.draw(params, p);
			}
		}
	}

	return selected_zone;
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

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::color_background);

		text.draw(params, pos);
	}

	// draw teams
	{
		UIText text;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Min;
		text.color = UI::color_accent;
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
				text.text(_(team_labels[s32(team)]));
				text.draw(params, pos + Vec2(0, 32.0f * UI::scale * SCALE_MULTIPLIER));
				draw_gamepad_icon(params, pos, i, UI::color_accent, SCALE_MULTIPLIER);
				pos.x += gamepad_spacing;
			}
		}
	}
}

b8 zone_can_capture(AssetID zone_id)
{
	if (zone_id == Game::level.id)
		return false;

	switch (Game::save.zones[zone_id])
	{
		case ZoneState::Friendly:
		{
#if SERVER
			return zone_id == zone_under_attack();
#else
			return zone_under_attack_timer() > 0.0f && zone_id == zone_under_attack();
#endif
		}
		case ZoneState::Hostile:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

namespace OverworldNet
{
	enum class Message
	{
		CaptureOrDefend,
		ZoneUnderAttack,
		ZoneChange,
		count,
	};

	b8 capture_or_defend(AssetID zone)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::CaptureOrDefend;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		Net::msg_finalize(p);
		return true;
	}

	b8 zone_under_attack(AssetID zone)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::ZoneUnderAttack;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		Net::msg_finalize(p);
		return true;
	}

	b8 zone_change(AssetID zone, ZoneState state)
	{
		using Stream = Net::StreamWrite;
		Stream* p = Net::msg_new(Net::MessageType::Overworld);
		Message m = Message::ZoneChange;
		serialize_enum(p, Message, m);
		serialize_s16(p, zone);
		serialize_enum(p, ZoneState, state);
		Net::msg_finalize(p);
		return true;
	}
}

AssetID zone_under_attack()
{
	return Game::session.zone_under_attack;
}

r32 zone_under_attack_timer()
{
	return vi_max(0.0f, Game::session.zone_under_attack_timer - (ZONE_UNDER_ATTACK_TIME - ZONE_UNDER_ATTACK_THRESHOLD));
}

b8 net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;

	OverworldNet::Message type;
	serialize_enum(p, OverworldNet::Message, type);

	AssetID zone;
	serialize_s16(p, zone);

	switch (type)
	{
		case OverworldNet::Message::CaptureOrDefend:
		{
			if (Game::level.local)
			{
				if (zone_node_get(zone) && zone_can_capture(zone))
					go(zone);
			}
			break;
		}
		case OverworldNet::Message::ZoneUnderAttack:
		{
			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				Game::session.zone_under_attack = zone;
				Game::session.zone_under_attack_timer = ZONE_UNDER_ATTACK_TIME;
			}
			break;
		}
		case OverworldNet::Message::ZoneChange:
		{
			ZoneState state;
			serialize_enum(p, ZoneState, state);
			// server does not accept ZoneChange messages from client
			if (Game::level.local == (src == Net::MessageSource::Loopback))
			{
				Game::save.zones[zone] = state;
				data.story.map.zones_change_time[zone] = Game::real_time.total;
			}
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

void deploy_update(const Update& u)
{
	const ZoneNode& zone = *zone_node_get(data.zone_selected);
	focus_camera(u, zone);

	// must use Game::real_time because Game::time is paused when overworld is active

	r32 old_timer = data.timer_deploy;
	data.timer_deploy = vi_max(0.0f, data.timer_deploy - Game::real_time.delta);

	if (data.timer_deploy > 0.5f)
	{
		// particles
		r32 t = old_timer;
		const r32 particle_interval = 0.015f;
		const ZoneNode* zone = zone_node_get(data.zone_selected);
		while (s32(t / particle_interval) > s32(data.timer_deploy / particle_interval))
		{
			r32 particle_blend = (t - 0.5f) / 0.5f;
			Particles::tracers.add
			(
				zone->pos() + Vec3(0, -2.0f + particle_blend * 12.0f, 0),
				Vec3::zero,
				0
			);
			t -= particle_interval;
		}
	}
	else
	{
		// screen shake
		r32 shake = (data.timer_deploy / 0.5f) * 0.05f;
		r32 offset = Game::real_time.total * 20.0f;
		data.camera_pos += Vec3(noise::sample3d(Vec3(offset)) * shake, noise::sample3d(Vec3(offset + 64)) * shake, noise::sample3d(Vec3(offset + 128)) * shake);
	}

	if (data.timer_deploy == 0.0f && old_timer > 0.0f)
	{
		if (Game::level.local)
			go(data.zone_selected);
		else if (Game::session.story_mode)
			OverworldNet::capture_or_defend(data.zone_selected);
		else
			vi_assert(false); // todo: networked pvp matches
	}
}

void deploy_draw(const RenderParams& params)
{
	const ZoneNode* current_zone = zones_draw(params);

	// show "loading..."
	Menu::progress_infinite(params, _(strings::deploying), params.camera->viewport.size * Vec2(0.5f, 0.2f));
}

b8 can_switch_tab()
{
	const Data::StoryMode& story = data.story;
	return data.active
		&& data.timer_transition == 0.0f
		&& data.timer_deploy == 0.0f
		&& story.inventory.timer_buy == 0.0f
		&& !Menu::dialog_callback[0]
		&& (story.messages.mode != Data::Messages::Mode::Cora || story.messages.timer_cora > 0.0f);
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
	if (msg->text == strings::msg_aldus_intro)
		message_schedule(strings::contact_cora, strings::msg_cora_intro, 1.0);
}

void group_join(Game::Group g)
{
	// todo: redo this whole thing
	Game::save.group = g;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (zone.max_teams > 2)
		{
			if (g == Game::Group::None)
				Overworld::zone_change(zone.id, ZoneState::Locked);
			else
				Overworld::zone_change(zone.id, mersenne::randf_cc() > 0.7f ? ZoneState::Friendly : ZoneState::Hostile);
		}
	}
}

void conversation_finished()
{
	Game::save.story_index++;
	messages_transition(Data::Messages::Mode::Messages);
	if (Game::save.story_index == 3)
		group_join(Game::Group::Futifs);
}

void tab_messages_update(const Update& u)
{
	Data::StoryMode* story = &data.story;
	Data::Messages* messages = &story->messages;

	{
		r64 time = platform::timestamp();
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

	if (story->tab == Tab::Messages && story->tab_timer > TAB_ANIMATION_TIME && can_switch_tab())
	{
		focus_camera(u, global.camera_messages_pos, global.camera_messages_rot);

		// call cora
		if (messages->mode != Data::Messages::Mode::Cora
			&& messages->contact_selected == strings::contact_cora
			&& u.last_input->get(Controls::InteractSecondary, 0) && !u.input->get(Controls::InteractSecondary, 0))
		{
			messages_transition(Data::Messages::Mode::Cora);
			Game::save.cora_called = true;
			if (cora_entry_points[Game::save.story_index] == AssetNull)
				messages->timer_cora = 10.0f;
			else
				messages->timer_cora = 3.0f;
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
					contact_index += UI::input_delta_vertical(u, 0);
					contact_index = vi_max(0, vi_min(contact_index, s32(contacts.length) - 1));
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
					msg_index += UI::input_delta_vertical(u, 0);
					msg_index = vi_max(0, vi_min(msg_index, s32(msg_list.length) - 1));
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
				if (messages->timer_cora > 0.0f)
				{
					if (!Game::cancel_event_eaten[0] && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
					{
						// cancel
						messages_transition(Data::Messages::Mode::Messages);
						Game::cancel_event_eaten[0] = true;
						messages->timer_cora = 0.0f;
					}
					else
					{
						messages->timer_cora = vi_max(0.0f, messages->timer_cora - Game::real_time.delta);
						if (messages->timer_cora == 0.0f)
						{
							AssetID entry_point = cora_entry_points[Game::save.story_index];
							if (entry_point == AssetNull)
							{
								messages_transition(Data::Messages::Mode::Messages);
								Menu::dialog(0, &Menu::dialog_no_action, _(strings::call_timed_out));
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

	if (story->tab != Tab::Messages)
	{
		// minimized view; reset scroll
		messages->contact_scroll.pos = 0;
		if (messages->mode == Data::Messages::Mode::Cora)
		{
			// cancel cora call
			messages->timer_cora = 0.0f;
			messages_transition(Data::Messages::Mode::Messages);
		}
	}
}

void capture_start(s8 gamepad)
{
	if (zone_can_capture(data.zone_selected) && resource_spend(Resource::Drones, 1))
		deploy_start();
}

void zone_done(AssetID zone)
{
	b8 captured = Game::save.zones[zone] == ZoneState::Friendly;

	if (captured)
	{
		const ZoneNode* z = zone_node_get(zone);
		for (s32 i = 0; i < s32(Resource::count); i++)
			Game::save.resources[i] += z->rewards[i];
	}

	if (Game::save.story_index == 0 && zone == Asset::Level::Safe_Zone && captured)
	{
		Game::save.story_index++;
		if (Game::save.cora_called)
			message_schedule(strings::contact_aldus, strings::msg_aldus_keep_trying, 3.0);
	}
	else if (Game::save.story_index == 1 && zone == Asset::Level::Medias_Res)
	{
		Game::save.story_index++; // cora's ready to talk
		if (Game::save.cora_called)
			message_schedule(strings::contact_aldus, strings::msg_aldus_keep_trying, 3.0);
	}
}

void zone_change(AssetID zone, ZoneState state)
{
	vi_assert(Game::level.local);
	OverworldNet::zone_change(zone, state);
}

b8 zone_filter_default(AssetID zone_id)
{
	return true;
}

b8 zone_filter_can_change(AssetID zone_id)
{
	if (zone_id == Asset::Level::Safe_Zone || zone_id == Asset::Level::Dock)
		return false;
	else if (zone_id == Game::level.id || (Game::session.story_mode && zone_id == Game::save.zone_current))
		return false;

	return true;
}

b8 zone_filter_captured(AssetID zone_id)
{
	return Game::save.zones[zone_id] == ZoneState::Friendly;
}

void zone_statistics(s32* captured, s32* hostile, s32* locked, b8 (*filter)(AssetID) = &zone_filter_default)
{
	*captured = 0;
	*hostile = 0;
	*locked = 0;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (filter(zone.id))
		{
			ZoneState state = Game::save.zones[zone.id];
			if (state == ZoneState::Friendly)
				(*captured)++;
			else if (state == ZoneState::Hostile)
				(*hostile)++;
			else if (state == ZoneState::Locked)
				(*locked)++;
		}
	}
}

void tab_map_update(const Update& u)
{
	if (data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME)
	{
		b8 enable_input = can_switch_tab();
		select_zone_update(u, enable_input); // only enable movement if can_switch_tab()

		// capture button
		if (enable_input
			&& u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0)
			&& can_switch_tab()
			&& zone_can_capture(data.zone_selected))
		{
			if (Game::save.resources[s32(Resource::Drones)] < DEPLOY_COST_DRONES)
				Menu::dialog(0, &Menu::dialog_no_action, _(strings::insufficient_resource), DEPLOY_COST_DRONES, _(strings::drones));
			else
				Menu::dialog(0, &capture_start, _(strings::confirm_capture), DEPLOY_COST_DRONES);
		}
	}
}

ResourceInfo resource_info[s32(Resource::count)] =
{
	{
		Asset::Mesh::icon_energy,
		strings::energy,
		0,
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

void resource_buy(s8 gamepad)
{
	data.story.inventory.timer_buy = BUY_TIME;
}

void tab_inventory_update(const Update& u)
{
	if (data.story.tab == Tab::Inventory && data.story.tab_timer > TAB_ANIMATION_TIME)
		focus_camera(u, global.camera_inventory_pos, global.camera_inventory_rot);

	Data::Inventory* inventory = &data.story.inventory;

	if (data.story.inventory.timer_buy > 0.0f)
	{
		data.story.inventory.timer_buy = vi_max(0.0f, data.story.inventory.timer_buy - u.time.delta);
		if (data.story.inventory.timer_buy == 0.0f)
		{
			Resource resource = data.story.inventory.resource_selected;
			const ResourceInfo& info = resource_info[s32(resource)];
			if (resource_spend(Resource::Energy, info.cost * data.story.inventory.buy_quantity))
				Game::save.resources[s32(resource)] += data.story.inventory.buy_quantity;
			data.story.inventory.mode = Data::Inventory::Mode::Normal;
			data.story.inventory.buy_quantity = 1;
		}
	}

	if (data.story.tab == Tab::Inventory && can_switch_tab())
	{
		// handle input
		switch (inventory->mode)
		{
			case Data::Inventory::Mode::Normal:
			{
				s32 selected = s32(inventory->resource_selected);
				selected = vi_max(0, vi_min(s32(Resource::count) - 1, selected + UI::input_delta_vertical(u, 0)));
				inventory->resource_selected = (Resource)selected;

				if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				{
					const ResourceInfo& info = resource_info[s32(inventory->resource_selected)];
					if (info.cost > 0)
					{
						inventory->mode = Data::Inventory::Mode::Buy;
						inventory->buy_quantity = 1;
					}
				}
				break;
			}
			case Data::Inventory::Mode::Buy:
			{
				inventory->buy_quantity = vi_max(1, vi_min(8, inventory->buy_quantity + UI::input_delta_horizontal(u, 0)));
				if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
				{
					const ResourceInfo& info = resource_info[s32(inventory->resource_selected)];
					if (Game::save.resources[s32(Resource::Energy)] >= info.cost * inventory->buy_quantity)
						Menu::dialog(0, &resource_buy, _(strings::prompt_buy), inventory->buy_quantity * info.cost, inventory->buy_quantity, _(info.description));
					else
						Menu::dialog(0, &Menu::dialog_no_action, _(strings::insufficient_resource), info.cost * inventory->buy_quantity, _(strings::energy));
				}
				else if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0])
				{
					inventory->mode = Data::Inventory::Mode::Normal;
					Game::cancel_event_eaten[0] = true;
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

	if (data.story.tab != Tab::Inventory)
	{
		// minimized view; reset
		inventory->mode = Data::Inventory::Mode::Normal;
		inventory->buy_quantity = 1;
	}

	for (s32 i = 0; i < s32(Resource::count); i++)
	{
		if (Game::save.resources[i] != inventory->resources_last[i])
			inventory->resources_blink_timer[i] = 0.5f;
		else if (data.story.tab_timer > TAB_ANIMATION_TIME)
			inventory->resources_blink_timer[i] = vi_max(0.0f, inventory->resources_blink_timer[i] - u.time.delta);
		inventory->resources_last[i] = Game::save.resources[i];
	}
}

AssetID zone_random(b8(*filter1)(AssetID), b8(*filter2)(AssetID) = &zone_filter_default)
{
	StaticArray<AssetID, MAX_ZONES> zones;
	for (s32 i = 0; i < global.zones.length; i++)
	{
		const ZoneNode& zone = global.zones[i];
		if (filter1(zone.id) && filter2(zone.id))
			zones.add(zone.id);
	}
	if (zones.length > 0)
		return zones[mersenne::randf_co() * zones.length];
	else
		return AssetNull;
}

void zone_random_attack(r32 elapsed_time)
{
	if (zone_under_attack() == AssetNull)
	{
		s32 captured;
		s32 hostile;
		s32 locked;
		zone_statistics(&captured, &hostile, &locked, &zone_filter_can_change);

		r32 event_odds = elapsed_time * EVENT_ODDS_PER_ZONE * captured;

		while (mersenne::randf_co() < event_odds)
		{
			AssetID z = zone_random(&zone_filter_captured, &zone_filter_can_change); // live incoming attack
			if (z != AssetNull)
				OverworldNet::zone_under_attack(z);
			event_odds -= 1.0f;
		}
	}
}

void story_mode_update(const Update& u)
{
	// energy increment
	{
		r64 t = platform::timestamp();
		if (s32(t / ENERGY_INCREMENT_INTERVAL) > s32(data.story.timestamp_last / ENERGY_INCREMENT_INTERVAL))
			Game::save.resources[s32(Resource::Energy)] += energy_increment_total();
		data.story.timestamp_last = t;
	}

	if (UIMenu::active[0])
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
			data.story.tab = (Tab)(vi_max(0, s32(data.story.tab) - 1));
			if (data.story.tab != data.story.tab_previous)
				data.story.tab_timer = 0.0f;
		}
		if (u.last_input->get(Controls::TabRight, 0) && !u.input->get(Controls::TabRight, 0))
		{
			data.story.tab_previous = data.story.tab;
			data.story.tab = (Tab)(vi_min(s32(Tab::count) - 1, s32(data.story.tab) + 1));
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
	if (data.tab == tab && !Menu::dialog_callback[0])
	{
		// flash the tab when it is selected
		if (data.tab_timer < TAB_ANIMATION_TIME)
		{
			if (UI::flash_function(Game::real_time.total))
				color = &UI::color_default;
			else
				draw = false; // don't draw the tab at all
		}
		else
			color = &UI::color_accent;
	}
	else
		color = &UI::color_disabled;

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
		const Vec4& background_color = data.tab == tab ? Vec4(0, 0, 0, 0) : Vec4(UI::color_background.xyz(), OPACITY);
		tab_draw_common(p, label, *pos, width, *color, background_color, flash && !UI::flash_function(Game::real_time.total) ? Vec4(0, 0, 0, 0) : UI::color_background);
	}

	Rect2 result = { *pos, { width, main_view_size.y } };

	pos->x += width + PADDING;

	return result;
}

void timestamp_string(r64 timestamp, char* str)
{
	r64 diff = platform::timestamp() - timestamp;
	if (diff < 60.0f)
		sprintf(str, "%s", _(strings::now));
	else if (diff < 3600.0f)
		sprintf(str, "%d%s", s32(diff / 60.0f), _(strings::minute));
	else if (diff < 86400)
		sprintf(str, "%d%s", s32(diff / 3600.0f), _(strings::hour));
	else
		sprintf(str, "%d%s", s32(diff / 86400), _(strings::day));
}

void contacts_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect, const StaticArray<ContactDetails, MAX_CONTACTS>& contacts)
{
	Vec2 panel_size = get_panel_size(rect);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);
	r64 time = platform::timestamp();
	data.messages.contact_scroll.start(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y));
	for (s32 i = 0; i < contacts.length; i++)
	{
		if (!data.messages.contact_scroll.item(i))
			continue;

		const ContactDetails& contact = contacts[i];
		b8 selected = data.tab == Tab::Messages && contact.name == data.messages.contact_selected && !Menu::dialog_callback[0];

		UI::box(p, { pos, panel_size }, UI::color_background);

		if (time - contact.last_message_timestamp > 0.5f || UI::flash_function(Game::real_time.total)) // flash new messages
		{
			if (selected)
				UI::border(p, Rect2(pos, panel_size).outset(-BORDER * UI::scale), BORDER, UI::color_accent);

			UIText text;
			text.size = TEXT_SIZE * (data.tab == Tab::Messages ? 1.0f : 0.75f);
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Center;
			if (data.tab == Tab::Messages)
			{
				text.color = selected ? UI::color_accent : Team::ui_color_friend;
				UIMenu::text_clip(&text, data.mode_transition_time + (i - data.messages.contact_scroll.pos) * 0.05f, 100.0f);
			}
			else
				text.color = UI::color_default;
			text.text("%s (%d)", _(contact.name), contact.unread);
			text.draw(p, pos + Vec2(PADDING, panel_size.y * 0.5f));

			if (data.tab == Tab::Messages)
			{
				text.color = selected ? UI::color_accent : UI::color_default;
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

				text.color = selected ? UI::color_accent : UI::color_alert;
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

					r64 time = platform::timestamp();
					Vec2 pos = rect.pos + Vec2(0, rect.size.y - top_bar_size.y);

					// top bar
					{
						UI::box(p, { pos, top_bar_size }, UI::color_background);

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
						text.color = UI::color_default;
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
						b8 selected = msg.text == data.messages.message_selected && !Menu::dialog_callback[0];

						UI::box(p, { pos, panel_size }, UI::color_background);

						if (time - msg.timestamp > 0.5f || UI::flash_function(Game::real_time.total)) // flash new messages
						{
							if (selected)
								UI::border(p, Rect2(pos, panel_size).outset(-2.0f * UI::scale), 2.0f, UI::color_accent);

							UIText text;
							text.size = TEXT_SIZE;
							text.anchor_x = UIText::Anchor::Min;
							text.anchor_y = UIText::Anchor::Center;
							text.color = selected ? UI::color_accent : UI::color_alert;
							UIMenu::text_clip(&text, data.mode_transition_time + (i - data.messages.message_scroll.pos) * 0.05f, 100.0f);

							if (!msg.read)
								UI::triangle(p, { pos + Vec2(panel_size.x * 0.05f, panel_size.y * 0.5f), Vec2(12.0f * UI::scale) }, UI::color_alert, PI * -0.5f);

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
							text.color = selected ? UI::color_accent : UI::color_default;
							text.draw(p, pos + Vec2(panel_size.x * 0.1f, panel_size.y * 0.5f));

							timestamp_string(msg.timestamp, buffer);
							text.font = Asset::Font::lowpoly;
							text.color = UI::color_alert;
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
					UI::box(p, { pos, top_bar_size }, UI::color_background);

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
					text.color = UI::color_alert;
					text.draw(p, pos + Vec2(top_bar_size.x * 0.5f, top_bar_size.y * 0.5f));

					if (data.messages.contact_selected == strings::contact_cora)
						text.text("%s    %s", _(strings::prompt_call), _(strings::prompt_back));
					else
						text.text(_(strings::prompt_back));
					text.color = UI::color_default;
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
					UI::box(p, text.rect(text_pos).outset(PADDING), UI::color_background);
					text.draw(p, text_pos);

					break;
				}
				case Data::Messages::Mode::Cora:
				{
					if (data.messages.timer_cora > 0.0f)
					{
						Menu::progress_infinite(p, _(strings::calling), rect.pos + rect.size * 0.5f);

						// cancel prompt
						UIText text;
						text.anchor_x = text.anchor_y = UIText::Anchor::Center;
						text.color = UI::color_accent;
						text.text(_(strings::prompt_cancel));

						Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

						UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::color_background);
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

AssetID group_name[s32(Game::Group::count)] =
{
	strings::none,
	strings::futifs,
	strings::zodiak,
};

Rect2 zone_stat_draw(const RenderParams& p, const Rect2& rect, UIText::Anchor anchor_x, s32 index, const char* label, const Vec4& color, b8 draw_text = true)
{
	UIText text;
	text.anchor_x = anchor_x;
	text.anchor_y = UIText::Anchor::Max;
	text.size = TEXT_SIZE * SCALE_MULTIPLIER * 0.75f;
	text.color = color;
	text.wrap_width = vi_min(MENU_ITEM_WIDTH * 0.5f * SCALE_MULTIPLIER, rect.size.x) + (PADDING * -2.0f);
	Vec2 pos = rect.pos;
	switch (anchor_x)
	{
		case UIText::Anchor::Min:
		{
			pos.x += PADDING;
			break;
		}
		case UIText::Anchor::Center:
		{
			pos.x += rect.size.x * 0.5f;
			break;
		}
		case UIText::Anchor::Max:
		{
			pos.x += rect.size.x - PADDING;
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	pos.y += rect.size.y - PADDING + index * (text.size * -UI::scale - PADDING);
	text.text_raw(label);
	Rect2 text_rect = text.rect(pos);
	UI::box(p, text_rect.outset(PADDING), UI::color_background);
	if (draw_text)
		text.draw(p, pos);
	return text_rect;
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& story, const Rect2& rect)
{
	{
		// draw stats

		char buffer[255];

		s32 index = 0;

		// total energy increment
		{
			const char* label;
			if (story.tab == Tab::Map)
				label = _(Game::save.group == Game::Group::None ? strings::energy_generation_total : strings::energy_generation_group);
			else
				label = "+%d";
			sprintf(buffer, label, s32(energy_increment_total()));
			Rect2 zone_stat_rect = zone_stat_draw(p, rect, UIText::Anchor::Max, index++, buffer, UI::color_default);

			// energy increment timer
			r32 icon_size = TEXT_SIZE * 1.5f * UI::scale * (story.tab == Tab::Map ? 1.0f : 0.75f);
			r64 t = platform::timestamp();
			UI::triangle_percentage
			(
				p,
				{ zone_stat_rect.pos + Vec2(zone_stat_rect.size.x - icon_size * 0.5f, zone_stat_rect.size.y * 0.5f), Vec2(icon_size) },
				r32(fmod(t, ENERGY_INCREMENT_INTERVAL)) / ENERGY_INCREMENT_INTERVAL,
				UI::color_default,
				PI
			);
		}

		// member of group "x"
		sprintf(buffer, _(strings::member_of_group), _(group_name[s32(Game::save.group)]));
		zone_stat_draw(p, rect, UIText::Anchor::Max, index++, buffer, UI::color_accent);

		if (story.tab != Tab::Map)
		{
			// statistics
			s32 captured;
			s32 hostile;
			s32 locked;
			zone_statistics(&captured, &hostile, &locked);

			sprintf(buffer, _(strings::zones_captured), captured);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, Game::save.group == Game::Group::None ? Team::ui_color_friend : UI::color_accent);

			sprintf(buffer, _(strings::zones_hostile), hostile);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_alert);

			sprintf(buffer, _(strings::zones_locked), locked);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_default);
		}

		// zones under attack
		if (zone_under_attack() != AssetNull)
		{
			sprintf(buffer, _(strings::zones_under_attack), 1);
			zone_stat_draw(p, rect, UIText::Anchor::Min, index++, buffer, UI::color_alert, UI::flash_function_slow(Game::real_time.total)); // flash text
		}
	}

	if (story.tab == Tab::Map)
	{
		zones_draw(p);

		// show selected zone info
		ZoneState zone_state = Game::save.zones[data.zone_selected];

		// show stats
		const ZoneNode* zone = zone_node_get(data.zone_selected);
		zone_stat_draw(p, rect, UIText::Anchor::Min, 0, Loader::level_name(data.zone_selected), zone_ui_color(*zone));
		char buffer[255];
		sprintf(buffer, _(strings::energy_generation), s32(energy_increment_zone(*zone)));
		zone_stat_draw(p, rect, UIText::Anchor::Min, 1, buffer, UI::color_default);

		if (zone_state == ZoneState::Hostile)
		{
			// show potential rewards
			b8 has_rewards = false;
			for (s32 i = 0; i < s32(Resource::count); i++)
			{
				if (zone->rewards[i] > 0)
				{
					has_rewards = true;
					break;
				}
			}

			if (has_rewards)
			{
				zone_stat_draw(p, rect, UIText::Anchor::Min, 2, _(strings::capture_bonus), UI::color_accent);
				s32 index = 3;
				for (s32 i = 0; i < s32(Resource::count); i++)
				{
					if (zone->rewards[i] > 0)
					{
						sprintf(buffer, "%d %s", zone->rewards[i], _(resource_info[i].description));
						zone_stat_draw(p, rect, UIText::Anchor::Min, index, buffer, UI::color_default);
						index++;
					}
				}
			}
		}

		// capture prompt
		if (can_switch_tab() && zone_can_capture(data.zone_selected))
		{
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.color = UI::color_accent;
			text.text(_(Game::save.zones[data.zone_selected] == ZoneState::Friendly ? strings::prompt_defend : strings::prompt_capture));

			Vec2 pos = rect.pos + rect.size * Vec2(0.5f, 0.2f);

			UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::color_background);

			text.draw(p, pos);
		}
	}
}

void inventory_items_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	Vec2 panel_size = get_panel_size(rect);
	Vec2 pos = rect.pos + Vec2(0, rect.size.y - panel_size.y);
	for (s32 i = 0; i < s32(Resource::count); i++)
	{
		b8 selected = data.tab == Tab::Inventory && data.inventory.resource_selected == (Resource)i && !Menu::dialog_callback[0];

		UI::box(p, { pos, panel_size }, UI::color_background);
		if (selected)
			UI::border(p, Rect2(pos, panel_size).outset(BORDER * -UI::scale), BORDER, UI::color_accent);

		r32 icon_size = 18.0f * SCALE_MULTIPLIER * UI::scale;

		b8 flash = data.inventory.resources_blink_timer[i] > 0.0f;
		b8 draw = !flash || UI::flash_function(Game::real_time.total);
		
		const ResourceInfo& info = resource_info[i];

		const Vec4* color;
		if (flash)
			color = &UI::color_accent;
		else if (selected && data.inventory.mode == Data::Inventory::Mode::Buy && Game::save.resources[s32(Resource::Energy)] < data.inventory.buy_quantity * info.cost)
			color = &UI::color_alert; // not enough energy to buy
		else if (selected)
			color = &UI::color_accent;
		else if (Game::save.resources[i] == 0)
			color = &UI::color_alert;
		else
			color = &UI::color_default;

		if (draw)
			UI::mesh(p, info.icon, pos + Vec2(PADDING + icon_size * 0.5f, panel_size.y * 0.5f), Vec2(icon_size), *color);

		UIText text;
		text.anchor_y = UIText::Anchor::Center;
		text.color = *color;
		text.size = TEXT_SIZE * (data.tab == Tab::Inventory ? 1.0f : 0.75f);
		if (draw)
		{
			// current amount
			text.anchor_x = UIText::Anchor::Max;
			text.text("%d", Game::save.resources[i]);
			text.draw(p, pos + Vec2(panel_size.x - PADDING, panel_size.y * 0.5f));

			if (data.tab == Tab::Inventory)
			{
				text.anchor_x = UIText::Anchor::Min;
				text.text(_(info.description));
				text.draw(p, pos + Vec2(icon_size * 2.0f + PADDING * 2.0f, panel_size.y * 0.5f));

				if (selected)
				{
					if (data.inventory.mode == Data::Inventory::Mode::Buy)
					{
						// buy interface
						text.anchor_x = UIText::Anchor::Center;
						text.text("+%d", data.inventory.buy_quantity);

						const r32 buy_quantity_spacing = 32.0f * UI::scale * SCALE_MULTIPLIER;
						Vec2 buy_quantity_pos = pos + Vec2(panel_size.x * 0.4f, panel_size.y * 0.5f);
						text.draw(p, buy_quantity_pos);

						UI::triangle(p, { buy_quantity_pos + Vec2(-buy_quantity_spacing, 0), Vec2(text.size * UI::scale) }, *color, PI * 0.5f);
						UI::triangle(p, { buy_quantity_pos + Vec2(buy_quantity_spacing, 0), Vec2(text.size * UI::scale) }, *color, PI * -0.5f);

						// cost
						text.anchor_x = UIText::Anchor::Min;
						text.text(_(strings::ability_spawn_cost), s32(info.cost * data.inventory.buy_quantity));
						text.draw(p, pos + Vec2(panel_size.x * 0.6f, panel_size.y * 0.5f));
					}
					else
					{
						// normal mode
						if (info.cost > 0)
						{
							// "buy more!"
							text.anchor_x = UIText::Anchor::Center;
							text.text(_(strings::prompt_buy_more));
							text.draw(p, pos + Vec2(panel_size.x * 0.5f, panel_size.y * 0.5f));
						}
					}
				}
			}
		}

		pos.y -= panel_size.y;
	}
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	inventory_items_draw(p, data, rect);

	if (data.tab == Tab::Inventory && data.tab_timer > TAB_ANIMATION_TIME)
	{
		if (data.inventory.timer_buy > 0.0f)
			Menu::progress_bar(p, _(strings::buying), 1.0f - (data.inventory.timer_buy / BUY_TIME), p.camera->viewport.size * Vec2(0.5f, 0.2f));
	}
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

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
		b8 flash = platform::timestamp() - most_recent_message < 0.5f;
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
		text.color = UI::color_default;
		text.text("[{{TabLeft}}]");

		Vec2 pos = bottom_left + Vec2(tab_size.x * 0.5f, main_view_size.y + tab_size.y * 1.5f);
		UI::box(p, text.rect(pos).outset(PADDING), UI::color_background);
		text.draw(p, pos);

		pos.x += total_size.x - tab_size.x;
		text.text("[{{TabRight}}]");
		UI::box(p, text.rect(pos).outset(PADDING), UI::color_background);
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
	if (!Game::session.story_mode
		&& !Game::cancel_event_eaten[0]
		&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
	{
		data.state = State::SplitscreenSelectTeams;
		Game::cancel_event_eaten[0] = true;
		return;
	}

	select_zone_update(u, true);

	// deploy button
	if (Menu::main_menu_state == Menu::State::Hidden && u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
		deploy_start();
}

void hide()
{
	if (data.timer_transition == 0.0f)
	{
		data.timer_transition = TRANSITION_TIME;
		data.active_next = false;
	}
}

void hide_complete()
{
	Particles::clear();
	if (data.camera)
	{
		memcpy(data.camera, &data.camera_restore_data, sizeof(Camera));
		data.camera = nullptr;
	}
	data.active = data.active_next = false;
}

void show_complete()
{
	Particles::clear();
	{
		Camera* c = data.camera;
		r32 t = data.timer_transition;
		data.~Data();
		new (&data) Data();
		data.camera = c;
		data.timer_transition = t;
	}
	data.active = data.active_next = true;
	if (Game::session.story_mode)
	{
		if (Game::session.zone_under_attack == AssetNull)
			data.zone_selected = Game::level.id;
		else
			data.zone_selected = Game::session.zone_under_attack;
	}
	else
	{
		if (zone_can_select(Game::save.zone_last))
			data.zone_selected = Game::save.zone_last;
		else
			data.zone_selected = Asset::Level::Medias_Res;
	}
	data.camera_restore_data = *data.camera;
	data.camera->colors = false;
	data.camera->mask = 0;
	{
		const ZoneNode* zone = zone_node_get(Game::save.zone_overworld);
		if (!zone)
			zone = zone_node_get(Game::save.zone_last);
		if (zone)
		{
			data.camera_pos = global.camera_offset_pos + zone->pos();
			data.camera_rot = global.camera_offset_rot;
		}
		else
		{
			data.camera_pos = global.camera_messages_pos;
			data.camera_rot = global.camera_messages_rot;
		}
	}
	Game::save.zone_last = Game::level.id;
	if (Game::session.story_mode)
		data.state = State::StoryMode;
	else if (Game::session.local_player_count() > 1)
		data.state = State::SplitscreenSelectZone;
	else
		data.state = State::SplitscreenSelectTeams;
}

s32 zone_player_count(AssetID z)
{
	return zone_node_get(z)->max_teams;
}

r32 particle_accumulator = 0.0f;
void update(const Update& u)
{
	if (Game::level.id == Asset::Level::overworld && Game::scheduled_load_level == AssetNull && !data.active && !data.active_next)
	{
		Camera* c = Camera::add();
		c->viewport =
		{
			Vec2(0, 0),
			Vec2(u.input->width, u.input->height),
		};
		r32 aspect = c->viewport.size.y == 0 ? 1 : (r32)c->viewport.size.x / (r32)c->viewport.size.y;
		c->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);
		data.timer_transition = 0.0f;
		show(c);
		show_complete();
		data.timer_transition = 0.0f;
	}

	if (data.timer_transition > 0.0f)
	{
		r32 old_timer = data.timer_transition;
		data.timer_transition = vi_max(0.0f, data.timer_transition - u.time.delta);
		if (data.timer_transition < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
		{
			if (data.active_next)
				show_complete();
			else
				hide_complete();
		}
	}

	if (Game::session.story_mode)
	{
		if (Game::session.zone_under_attack_timer > 0.0f)
		{
			Game::session.zone_under_attack_timer = vi_max(0.0f, Game::session.zone_under_attack_timer - Game::real_time.delta);
			if (Game::session.zone_under_attack_timer == 0.0f)
			{
				if (Game::level.local
					&& Game::level.id != Game::session.zone_under_attack
					&& data.timer_deploy == 0.0f
					&& Game::scheduled_load_level == AssetNull) // if the player is deploying to this zone to contest it, don't set it to hostile; wait for the match outcome
					zone_change(Game::session.zone_under_attack, ZoneState::Hostile);
				Game::session.zone_under_attack = AssetNull;
			}
		}

		if (Game::level.local && Game::level.mode == Game::Mode::Pvp)
			zone_random_attack(Game::real_time.delta);
	}

	if (data.active && !Console::visible)
	{
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

		data.camera->pos = data.camera_pos;
		data.camera->rot = data.camera_rot;

		// pause
		if (!Game::cancel_event_eaten[0])
		{
			if (Game::session.story_mode && ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
				|| (u.input->get(Controls::Scoreboard, 0) && !u.last_input->get(Controls::Scoreboard, 0))))
			{
				Game::cancel_event_eaten[0] = true;
				hide();
			}
			else if (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))
			{
				Game::cancel_event_eaten[0] = true;
				Menu::show();
			}
		}
	}
}

void draw_opaque(const RenderParams& params)
{
	if (data.active)
	{
		for (s32 i = 0; i < global.props.length; i++)
		{
			const PropEntry& entry = global.props[i];
			Mat4 m;
			m.make_transform(entry.pos, Vec3(1), entry.rot);
			View::draw_mesh(params, entry.mesh, Asset::Shader::standard, AssetNull, m, BACKGROUND_COLOR);
		}

		for (s32 i = 0; i < global.waters.length; i++)
		{
			const WaterEntry& entry = global.waters[i];
			Water::draw_opaque(params, entry.config, entry.pos, entry.rot);
		}
	}
}

void draw_hollow(const RenderParams& params)
{
	if (data.active)
	{
		for (s32 i = 0; i < global.waters.length; i++)
		{
			const WaterEntry& entry = global.waters[i];
			Water::draw_hollow(params, entry.config, entry.pos, entry.rot);
		}
	}
}

void draw_override(const RenderParams& params)
{
	if (data.active && should_draw_zones())
		zones_draw_override(params);
}

void draw_ui(const RenderParams& params)
{
	if (data.active && params.camera == data.camera)
	{
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
	}

	// letterbox transition effect
	if (data.timer_transition > 0.0f)
		Menu::draw_letterbox(params, data.timer_transition, TRANSITION_TIME);
}

void show(Camera* camera)
{
	if (data.timer_transition == 0.0f)
	{
		data.camera = camera;
		data.timer_transition = TRANSITION_TIME;
		data.active_next = true;
	}
}

b8 active()
{
	return data.active;
}

void clear()
{
	hide_complete();
	data.timer_transition = 0.0f;
}

void execute(const char* cmd)
{
	if (utf8cmp(cmd, "capture") == 0)
	{
		Overworld::zone_change(data.zone_selected, ZoneState::Friendly);
		zone_done(data.zone_selected);
	}
	else if (strstr(cmd, "join ") == cmd)
	{
		const char* delimiter = strchr(cmd, ' ');
		const char* group_string = delimiter + 1;
		for (s32 i = 0; i < s32(Game::Group::count); i++)
		{
			if (utf8cmp(group_string, _(group_name[i])) == 0)
			{
				group_join((Game::Group)i);
				break;
			}
		}
	}
}

cJSON* find_entity(cJSON* level, const char* name)
{
	cJSON* element = level->child;
	while (element)
	{
		const char* element_name = Json::get_string(element, "name");
		if (element_name && strcmp(element_name, name) == 0)
			return element;
		element = element->next;
	}
	return nullptr;
}

void init(cJSON* level)
{
	{
		cJSON* t = find_entity(level, "map_view");
		global.camera_offset_pos = Json::get_vec3(t, "pos");
		global.camera_offset_rot = Quat::look(Json::get_quat(t, "rot") * Vec3(0, -1, 0));

		t = find_entity(level, "camera_messages");
		global.camera_messages_pos = Json::get_vec3(t, "pos");
		global.camera_messages_rot = Quat::look(Json::get_quat(t, "rot") * Vec3(0, -1, 0));

		t = find_entity(level, "camera_inventory");
		global.camera_inventory_pos = Json::get_vec3(t, "pos");
		global.camera_inventory_rot = Quat::look(Json::get_quat(t, "rot") * Vec3(0, -1, 0));
	}

	{
		cJSON* element = level->child;
		s32 element_id = 0;
		while (element)
		{
			const char* zone_name = Json::get_string(element, "Zone");
			if (zone_name)
			{
				AssetID level_id = Loader::find_level(zone_name);
				if (level_id != AssetNull)
				{
					ZoneNode* node = global.zones.add();

					Vec3 zone_pos = Json::get_vec3(element, "pos");
					{
						cJSON* element2 = level->child;
						while (element2)
						{
							if (Json::get_s32(element2, "parent") == element_id)
								node->children.add(zone_pos + Json::get_vec3(element2, "pos"));
							element2 = element2->next;
						}
					}
					node->children.add(zone_pos);

					node->id = level_id;

					cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
					cJSON* mesh_json = meshes->child;
					const char* mesh_ref = mesh_json->valuestring;
					node->mesh = Loader::find_mesh(mesh_ref);

					node->rewards[0] = (s16)Json::get_s32(element, "energy", 0);
					node->rewards[1] = (s16)Json::get_s32(element, "hack_kits", 0);
					node->rewards[2] = (s16)Json::get_s32(element, "drones", 0);
					node->size = (s8)Json::get_s32(element, "size", 1);
					node->max_teams = (s8)Json::get_s32(element, "max_teams", 2);
				}
			}
			else if (cJSON_HasObjectItem(element, "Prop"))
			{
				cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
				cJSON* json_mesh = meshes->child;

				Vec3 pos = Json::get_vec3(element, "pos");
				Quat rot = Json::get_quat(element, "rot");

				while (json_mesh)
				{
					const char* mesh_ref = json_mesh->valuestring;

					PropEntry* entry = global.props.add();
					entry->mesh = Loader::find_mesh(mesh_ref);
					Loader::mesh_permanent(entry->mesh);
					entry->pos = pos;
					entry->rot = rot;

					json_mesh = json_mesh->next;
				}
			}
			else if (cJSON_HasObjectItem(element, "Water"))
			{
				cJSON* meshes = cJSON_GetObjectItem(element, "meshes");
				cJSON* mesh_json = meshes->child;
				const char* mesh_ref = mesh_json->valuestring;

				WaterEntry* water = global.waters.add();
				new (&water->config) Water::Config();
				water->config.mesh = Loader::find_mesh(mesh_ref);
				vi_assert(water->config.mesh != AssetNull);
				Loader::mesh_permanent(water->config.mesh);
				water->config.texture = Loader::find(Json::get_string(element, "texture", "water_normal"), AssetLookup::Texture::names);
				water->config.displacement_horizontal = Json::get_r32(element, "displacement_horizontal", 2.0f);
				water->config.displacement_vertical = Json::get_r32(element, "displacement_vertical", 0.75f);
				water->config.color = Vec4(0, 0, 0, 1);
				water->pos = Json::get_vec3(element, "pos");
				water->rot = Json::get_quat(element, "rot");
			}

			element = element->next;
			element_id++;
		}
	}

	if (Game::session.story_mode)
	{
		data.state = State::StoryMode;

		r64 t = platform::timestamp();
		r64 elapsed_time = t - Game::save.timestamp;
		data.story.timestamp_last = t;

		if (Game::level.local)
		{
			// energy increment
			// this must be done before story_zone_done changes the energy increment amount
			Game::save.resources[s32(Resource::Energy)] += vi_min(s32(4 * 60 * 60 / ENERGY_INCREMENT_INTERVAL), s32(elapsed_time / (r64)ENERGY_INCREMENT_INTERVAL)) * energy_increment_total();
		}
	}
	else
	{
		if (Game::session.local_player_count() <= 1) // haven't selected teams yet
			data.state = State::SplitscreenSelectTeams;
		else
			data.state = State::SplitscreenSelectZone; // already selected teams, go straight to level select; the player can always go back
	}
}

}

}
