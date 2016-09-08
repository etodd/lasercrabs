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

#define DEPLOY_TIME_LOCAL 3.0f

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

struct ZoneNode
{
	AssetID id;
	Ref<Transform> pos;
	u8 size;
	u8 max_teams;
};

enum class State
{
	SplitscreenSelectTeams,
	SplitscreenSelectLevel,
	SplitscreenDeploying,
	StoryMode,
	Deploying,
};

// story index describes the place in the story we are about to experience
// so if the story index is "CoraIntro", we're ready to do Cora's intro

AssetID cora_entry_points[] =
{
	AssetNull, // tutorial hasn't been completed
	strings::intro,
	AssetNull,
	AssetNull,
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

	struct StoryMode
	{
		Tab tab = Tab::Map;
		Tab tab_previous = Tab::Messages;
		r32 tab_timer;
		r32 mode_transition_time;
		r32 hack_timer;
		Messages messages;
		Ref<Transform> camera_messages;
		Ref<Transform> camera_inventory;
	};

	Camera* camera;
	Array<ZoneNode> zones;
	AssetID last_level = AssetNull;
	AssetID next_level = AssetNull;
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
	strings::tip_6,
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

void message_add(AssetID contact, AssetID text, r64 timestamp = -1.0)
{
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
	Game::Message* msg = Game::save.messages_scheduled.add();
	msg->contact = contact;
	msg->text = text;
	msg->timestamp = platform::time() + delay;
	msg->read = false;
}

void splitscreen_select_teams_update(const Update& u)
{
	if (UIMenu::active[0])
		return;

	if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
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

			if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
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
		data.state = State::SplitscreenSelectLevel;
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

	r32 scale = (UI::scale < 1.0f ? 0.5f : 1.0f) * UI::scale;

	UIText text;
	text.anchor_x = UIText::Anchor::Center;
	text.anchor_y = UIText::Anchor::Max;
	text.color = UI::accent_color;
	text.wrap_width = main_view_size.x - 48.0f * scale;
	Vec2 pos = center + Vec2(0, main_view_size.y * 0.5f - (48.0f * scale));

	// prompt
	if (splitscreen_teams_are_valid())
	{
		text.text(_(strings::prompt_splitscreen_ready));
		text.draw(params, pos);
	}

	pos.y -= 48.0f * scale;

	// draw team labels
	const r32 team_offset = 128.0f * scale;
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
		pos.y -= 64.0f * scale;

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

		Vec2 icon_pos = pos + Vec2(x_offset, 0);
		UI::mesh(params, Asset::Mesh::icon_gamepad, icon_pos, Vec2(48.0f * scale), *color);
		text.text("%d", i + 1);
		text.draw(params, icon_pos);
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
		if (data.zones[i].id == data.next_level)
			return &data.zones[i];
	}
	return nullptr;
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

	const ZoneNode* zone = get_zone_node(data.next_level);
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
			r32 closest_dot = 8.0f;
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
				data.next_level = closest->id;
		}
	}

	focus_camera(u, *zone);
}

Vec3 zone_color(const ZoneNode& zone)
{
	if (zone.id == AssetNull)
		return Vec3(0.0f);
	else if (Game::session.local_multiplayer)
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
				return Vec3(0.0f);
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
	if (zone.id == AssetNull)
		return UI::disabled_color;
	else if (Game::session.local_multiplayer)
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
		zones.push(data.zones[i]);

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
	// highlight level locations
	const ZoneNode* zone = get_zone_node(data.next_level);

	// draw current zone name
	if (zone)
	{
		Vec2 p;
		if (UI::project(params, zone->pos.ref()->absolute_pos(), &p))
		{
			UIText text;
			text.color = zone_ui_color(*zone);
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.text_raw(AssetLookup::Level::names[zone->id]);
			UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
			text.draw(params, p);
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
		text.text(_(strings::prompt_deploy));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
}

void deploy_update(const Update& u)
{
	const ZoneNode& level_node = *get_zone_node(data.next_level);
	focus_camera(u, level_node);

	data.deploy_timer -= Game::real_time.delta;
	data.camera->active = data.deploy_timer > 0.5f;
	if (data.deploy_timer < 0.0f)
		go(data.next_level);
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
	const ZoneNode* current_level = zones_draw(params);

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

Game::Message* message_get(AssetID msg)
{
	for (s32 i = 0; i < Game::save.messages.length; i++)
	{
		if (Game::save.messages[i].text == msg)
			return &Game::save.messages[i];
	}
	return nullptr;
}

void message_read(Game::Message* msg)
{
	msg->read = true;
	if (msg->text == strings::msg_albert_intro_2)
		message_schedule(strings::contact_cora, strings::msg_cora_intro, 2.0);
}

void conversation_finished()
{
	Game::save.story_index++;
	messages_transition(Data::Messages::Mode::Messages);
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
		if ((messages->mode == Data::Messages::Mode::Messages || messages->mode == Data::Messages::Mode::Message)
			&& messages->contact_selected == strings::contact_cora
			&& u.last_input->get(Controls::Call, 0) && !u.input->get(Controls::Call, 0))
		{
			messages_transition(Data::Messages::Mode::Cora);
			if (cora_entry_points[Game::save.story_index] == AssetNull)
				messages->cora_timer = 30.0f;
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
								messages_transition(Data::Messages::Mode::Messages);
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

b8 spend_resource(Game::Resource res, u16 amount)
{
	if (Game::save.resources[(s32)res] >= amount)
	{
		Game::save.resources[(s32)res] -= amount;
		return true;
	}
	return false;
}

void tab_map_update(const Update& u)
{
	if (data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME)
	{
		select_zone_update(u, data.story.hack_timer == 0.0f); // disable movement if we're hacking

		if (data.story.hack_timer > 0.0f)
		{
			data.story.hack_timer = vi_max(0.0f, data.story.hack_timer - u.time.delta);
			if (data.story.hack_timer == 0.0f)
			{
				// hack complete
				if (Game::save.zones[data.next_level] == Game::ZoneState::Locked
					&& spend_resource(Game::Resource::HackKits, 1))
				{
					if (data.next_level == Asset::Level::Safe_Zone)
						Game::save.zones[data.next_level] = Game::ZoneState::Owned;
					else
						Game::save.zones[data.next_level] = Game::ZoneState::Hostile;

					// make adjacent zones accessible (but still locked)
					const ZoneNode* zone = get_zone_node(data.next_level);
					Vec3 zone_pos = zone->pos.ref()->absolute_pos();
					zone_pos.y = 0.0f;
					for (s32 i = 0; i < data.zones.length; i++)
					{
						ZoneNode* neighbor_zone = &data.zones[i];
						if (neighbor_zone != zone)
						{
							Vec3 neighbor_pos = neighbor_zone->pos.ref()->absolute_pos();
							neighbor_pos.y = 0.0f;
							if ((neighbor_pos - zone_pos).length_squared() < 4.0f * 4.0f)
								Game::save.zones[neighbor_zone->id] = Game::ZoneState::Locked;
						}
					}
				}
			}
		}
		else
		{
			// interact button
			if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
			{
				Game::ZoneState zone_state = Game::save.zones[data.next_level];
				if (zone_state == Game::ZoneState::Locked)
				{
					if (Game::save.resources[(s32)Game::Resource::HackKits] > 0)
						data.story.hack_timer = 2.0f; // start hacking
				}
				else if (zone_state == Game::ZoneState::Hostile)
					deploy_start();
				else if (data.next_level == Asset::Level::Safe_Zone && zone_state == Game::ZoneState::Owned)
					deploy_start();
			}
		}
	}
	else
		data.story.hack_timer = 0.0f;
}

void tab_inventory_update(const Update& u)
{
	if (data.story.tab == Tab::Inventory && data.story.tab_timer > TAB_ANIMATION_TIME)
		focus_camera(u, data.story.camera_inventory.ref());
}

#define STORY_MODE_INIT_TIME 2.0f

b8 can_switch_tab(const Data::StoryMode& story)
{
	return story.hack_timer == 0.0f
		&& (story.messages.mode != Data::Messages::Mode::Cora || story.messages.cora_timer > 0.0f);
}

void story_mode_update(const Update& u)
{
	if (UIMenu::active[0] || Game::time.total < STORY_MODE_INIT_TIME)
		return;

	data.story.tab_timer += u.time.delta;

	// start the mode transition animation when we first open any tab
	if (data.story.tab_timer > TAB_ANIMATION_TIME && data.story.tab_timer - Game::real_time.delta <= TAB_ANIMATION_TIME)
		data.story.mode_transition_time = Game::real_time.total;

	if (can_switch_tab(data.story))
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

	// pause
	if (!Game::cancel_event_eaten[0]
		&& ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			|| (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))))
	{
		Game::cancel_event_eaten[0] = true;
		Menu::show();
	}
}

// the lower left corner of the tab box starts at `pos`
Rect2 tab_draw(const RenderParams& p, const Data::StoryMode& data, Tab tab, const char* label, Vec2* pos, b8 flash = false)
{
	b8 draw = true;

	const Vec4* color;
	if (data.tab == tab)
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

void contacts_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect, const Vec2& panel_size, const StaticArray<ContactDetails, MAX_CONTACTS>& contacts)
{
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
				UI::border(p, Rect2(pos, panel_size).outset(-2.0f * UI::scale), 2.0f, UI::accent_color);

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
				text.text(buffer);
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
	Vec2 panel_size(rect.size.x, PADDING * 2.0f + TEXT_SIZE * UI::scale);

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
					contacts_draw(p, data, rect, panel_size, contacts);
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
							text.text(buffer);
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
		contacts_draw(p, data, rect, panel_size, contacts);
	}
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& story, const Rect2& rect)
{
	if (story.tab == Tab::Map)
	{
		zones_draw(p);

		// deploy prompt
		{
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;

			AssetID prompt;
			switch (Game::save.zones[data.next_level])
			{
				case Game::ZoneState::Friendly:
				{
					prompt = strings::prompt_inspect;
					text.color = UI::accent_color;
					break;
				}
				case Game::ZoneState::Hostile:
				{
					prompt = strings::prompt_deploy;
					text.color = UI::accent_color;
					break;
				}
				case Game::ZoneState::Locked:
				{
					prompt = strings::prompt_hack;
					if (Game::save.resources[(s32)Game::Resource::HackKits] > 0)
						text.color = UI::accent_color;
					else
						text.color = UI::disabled_color;
					break;
				}
				case Game::ZoneState::Owned:
				{
					if (data.next_level == Asset::Level::Safe_Zone)
						prompt = strings::prompt_deploy;
					else
						prompt = strings::prompt_inspect;
					text.color = UI::accent_color;
					break;
				}
				case Game::ZoneState::Inaccessible:
				{
					prompt = strings::prompt_hack;
					text.color = UI::disabled_color;
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			text.text(_(prompt));

			Vec2 pos = p.camera->viewport.size * Vec2(0.5f, 0.2f);

			UI::box(p, text.rect(pos).outset(8 * UI::scale), UI::background_color);

			text.draw(p, pos);
		}

		if (story.hack_timer > 0.0f)
			progress_draw(p, _(strings::hacking), p.camera->viewport.size * Vec2(0.5f, 0.2f));
	}
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	if (data.tab == Tab::Inventory)
	{
	}
	else
	{
	}
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
	if (can_switch_tab(data.story))
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
	return data.state == State::SplitscreenSelectLevel
		|| data.state == State::SplitscreenDeploying
		|| data.state == State::Deploying
		|| (data.state == State::StoryMode && data.story.tab == Tab::Map && data.story.tab_timer > TAB_ANIMATION_TIME);
}

void splitscreen_select_zone_update(const Update& u)
{
	// cancel
	if (Game::session.local_multiplayer
		&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
	{
		data.state = State::SplitscreenSelectTeams;
		return;
	}

	select_zone_update(u, true);

	// deploy button
	if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
	{
		const ZoneNode* zone = get_zone_node(data.next_level);
		if (splitscreen_team_count() <= zone->max_teams)
			deploy_start();
	}
}

void update(const Update& u)
{
	if (data.last_level == Asset::Level::terminal && Game::session.level != Asset::Level::terminal)
	{
		// cleanup
		data.~Data();
		data = Data();
		data.last_level = Asset::Level::terminal;
	}

	if (Game::session.level == Asset::Level::terminal && !Console::visible)
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
			case State::SplitscreenSelectLevel:
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
	}
	data.last_level = Game::session.level;
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
		case State::SplitscreenSelectLevel:
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

void show()
{
	Game::schedule_load_level(Asset::Level::terminal, Game::Mode::Special);
}

void init(const Update& u, const EntityFinder& entities)
{
	if (Game::session.level != Asset::Level::terminal)
		return;

	data.tip_index = mersenne::rand() % tip_count;
	data.next_level = Game::session.local_multiplayer ? Asset::Level::Medias_Res : Asset::Level::Safe_Zone;
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
			data.state = State::SplitscreenSelectLevel; // already selected teams, go straight to level select; the player can always go back
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
			Game::save.zones[Asset::Level::Safe_Zone] = Game::ZoneState::Locked;
		}
	}

	Cora::init();
	Cora::conversation_finished().link(&conversation_finished);
}

}

}
