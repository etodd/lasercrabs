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
#include "utf8/utf8.h"
#include "sha1/sha1.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "cora.h"
#include "data/priority_queue.h"

namespace VI
{


namespace Terminal
{

#define DEPLOY_TIME_MIN 4.0f
#define DEPLOY_TIME_RANGE 3.0f
#define DEPLOY_TIME_OFFLINE 3.0f

#define PADDING (16.0f * UI::scale)
#define TAB_SIZE Vec2(160.0f * UI::scale, UI_TEXT_SIZE_DEFAULT * UI::scale + PADDING * 2.0f)
#define MAIN_VIEW_SIZE (Vec2(768.0f, 512.0f) * ((UI::scale < 1.0f ? 0.5f : 1.0f) * UI::scale))
#define BORDER 2.0f
#define OPACITY 0.8f

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
enum class StoryIndex
{
	IntroAlbert,
	Tutorial,
	IntroCora,
	IntroSissyFoos,
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
	struct StoryMode
	{
		Tab tab;
		Tab tab_previous;
		r32 tab_timer;
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

void init(const Update& u, const EntityFinder& entities)
{
	if (Game::session.level != Asset::Level::terminal)
		return;

	data.tip_index = mersenne::rand() % tip_count;
	data.next_level = Game::session.local_multiplayer ? Asset::Level::Medias_Res : Asset::Level::Soteria;
	data.camera = Camera::add();

	{
		Entity* map_view_entity = entities.find("map_view");
		Transform* map_view = map_view_entity->get<Transform>();
		data.camera_offset = map_view;
		data.camera->pos = map_view->absolute_pos();
		data.camera->rot = Quat::look(map_view->absolute_rot() * Vec3(0, -1, 0));

		data.story.camera_messages = entities.find("camera_messages")->get<Transform>();
		data.story.camera_inventory = entities.find("camera_inventory")->get<Transform>();
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
				if (level_id != Asset::Level::Soteria || !Game::session.local_multiplayer) // only show tutorial level in story mode
				{
					ZoneNode* node = data.zones.add();
					*node =
					{
						level_id,
						view->get<Transform>(),
						(u8)Json::get_s32(entry.properties, "size", 1),
						(u8)Json::get_s32(entry.properties, "max_teams", 2),
					};
					if (level_id == data.next_level)
						data.camera->pos = view->get<Transform>()->absolute_pos() + data.camera_offset.ref()->absolute_pos();
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
		// story mode
		// todo: figure out how to trigger Cora
		//const char* entry_point_str = Loader::level_name(Game::levels[Game::save.level_index]);
		const char* entry_point_str = "cora_intro";
		Cora::init();
		Cora::activate(strings_get(entry_point_str));
		data.state = State::StoryMode;
	}
}

void splitscreen_select_teams_update(const Update& u)
{
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

void tab_draw_common(const RenderParams& p, const char* label, const Vec4& color, const Vec4& background_color, const Vec2& pos, r32 width)
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
		text.color = UI::background_color;
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
		tab_draw_common(params, _(strings::splitscreen_prompt), UI::accent_color, background_color, bottom_left, main_view_size.x);
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
		text.text(_(strings::splitscreen_prompt_ready));
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
	Game::save = Game::Save();
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

void select_zone_update(const Update& u)
{
	const ZoneNode* zone = get_zone_node(data.next_level);
	if (!zone)
		return;

	if (!UIMenu::active[0])
	{
		// cancel
		if (Game::session.local_multiplayer
			&& u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
		{
			data.state = State::SplitscreenSelectTeams;
			return;
		}

		// movement
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

		// deploy button
		if (u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0)
			&& (!Game::session.local_multiplayer || splitscreen_team_count() <= zone->max_teams)) // if we're in splitscreen, make sure we don't have too many teams
		{
			data.state = Game::session.local_multiplayer ? State::SplitscreenDeploying : State::Deploying;
			data.deploy_timer = DEPLOY_TIME_OFFLINE;
			data.tip_time = Game::real_time.total;
		}
	}

	focus_camera(u, *zone);
}

Vec3 zone_color(const ZoneNode& zone)
{
	if (zone.id == AssetNull)
		return Vec3(0.3f);
	else if (zone.id == data.next_level)
		return Vec3(1);
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
				return Vec3(0.3f);
			}
			case Game::ZoneState::Friendly:
			{
				return Team::color_friend.xyz();
			}
			case Game::ZoneState::Hostile:
			{
				return Team::color_enemy.xyz();
			}
			case Game::ZoneState::Owned:
			{
				return UI::accent_color.xyz();
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
				return UI::disabled_color;
			}
			case Game::ZoneState::Friendly:
			{
				return Team::ui_color_friend;
			}
			case Game::ZoneState::Hostile:
			{
				return Team::ui_color_enemy;
			}
			case Game::ZoneState::Owned:
			{
				return UI::accent_color;
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

void select_zone_draw(const RenderParams& params)
{
	const ZoneNode* zone = zones_draw(params);

	// press [x] to deploy
	{
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = zone_ui_color(*zone);
		text.text(_(strings::deploy_prompt));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}

	// TODO: show alert box saying the previous match was forfeit
	/*
	if (Game::session.forfeit != Game::Forfeit::None)
	{
		// the previous match was forfeit; let the player know
		UIText text;
		text.anchor_x = text.anchor_y = UIText::Anchor::Center;
		text.color = UI::accent_color;
		text.text(_(Game::session.forfeit == Game::Forfeit::NetworkError ? strings::forfeit_network_error : strings::forfeit_opponent_quit));

		Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.8f);

		UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

		text.draw(params, pos);
	}
	*/
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

void progress_draw(const RenderParams& params, const char* label)
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.color = UI::accent_color;
	text.text(label);
	Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

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
	progress_draw(params, _(Game::session.local_multiplayer ? strings::loading_offline : strings::connecting));

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

void tab_messages_update(const Update& u)
{
	focus_camera(u, data.story.camera_messages.ref());
}


void tab_map_update(const Update& u)
{
	select_zone_update(u);
}

void tab_inventory_update(const Update& u)
{
	focus_camera(u, data.story.camera_inventory.ref());
}

#define STORY_MODE_INIT_TIME 2.0f

void story_mode_update(const Update& u)
{
	// pause
	if (!Game::cancel_event_eaten[0]
		&& ((u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
			|| (u.last_input->get(Controls::Pause, 0) && !u.input->get(Controls::Pause, 0))))
	{
		Game::cancel_event_eaten[0] = true;
		Menu::show();
	}

	if (Menu::main_menu_state != Menu::State::Hidden || Game::time.total < STORY_MODE_INIT_TIME)
		return;

	data.story.tab_timer += u.time.delta;
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

	if (data.story.tab_timer > TAB_ANIMATION_TIME)
	{
		switch (data.story.tab)
		{
			case Tab::Messages:
			{
				tab_messages_update(u);
				break;
			}
			case Tab::Map:
			{
				tab_map_update(u);
				break;
			}
			case Tab::Inventory:
			{
				tab_inventory_update(u);
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

// the lower left corner of the tab box starts at `pos`
Rect2 tab_draw(const RenderParams& p, const Data::StoryMode& data, Tab tab, const char* label, Vec2* pos)
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
		tab_draw_common(p, label, *color, background_color, *pos, width);
	}

	Rect2 result = { *pos, { width, main_view_size.y } };

	pos->x += width + PADDING;

	return result;
}

void tab_messages_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
}

void tab_map_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
	if (data.tab == Tab::Map)
		select_zone_draw(p);
}

void tab_inventory_draw(const RenderParams& p, const Data::StoryMode& data, const Rect2& rect)
{
}

void story_mode_draw(const RenderParams& p)
{
	if (Menu::main_menu_state != Menu::State::Hidden)
		return;

	if (Game::time.total < STORY_MODE_INIT_TIME)
	{
		progress_draw(p, _(strings::connecting));
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
		Rect2 rect = tab_draw(p, data.story, Tab::Messages, _(strings::tab_messages), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_messages_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Map, _(strings::tab_map), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_map_draw(p, data.story, rect);
	}
	{
		Rect2 rect = tab_draw(p, data.story, Tab::Inventory, _(strings::tab_inventory), &pos);
		if (data.story.tab_timer > TAB_ANIMATION_TIME)
			tab_inventory_draw(p, data.story, rect);
	}

	// left/right tab control prompt
	{
		UIText text;
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
				select_zone_update(u);
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
			select_zone_draw(params);
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

}

}
