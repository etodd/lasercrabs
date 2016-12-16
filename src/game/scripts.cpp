#include "scripts.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "strings.h"
#include "utf8/utf8.h"
#include "ai_player.h"
#include "console.h"
#include <unordered_map>
#include <string>
#include "cora.h"
#include "minion.h"
#include "net.h"
#include "team.h"
#include "asset/level.h"
#include "asset/armature.h"
#include "data/animator.h"
#include "overworld.h"

namespace VI
{

Script* Script::find(const char* name)
{
	s32 i = 0;
	while (true)
	{
		if (!Script::all[i].name)
			break;

		if (utf8cmp(Script::all[i].name, name) == 0)
			return &Script::all[i];

		i++;
	}
	return nullptr;
}

namespace Scripts
{


namespace scene
{
	struct Data
	{
		Camera* camera;
	};
	
	static Data* data;

	void cleanup()
	{
		data->camera->remove();
		delete data;
		data = nullptr;
	}

	void init(const EntityFinder& entities)
	{
		if (Game::level.mode == Game::Mode::Special)
		{
			data = new Data();

			data->camera = Camera::add();

			data->camera->viewport =
			{
				Vec2(0, 0),
				Vec2(Game::width, Game::height),
			};
			r32 aspect = data->camera->viewport.size.y == 0 ? 1 : (r32)data->camera->viewport.size.x / (r32)data->camera->viewport.size.y;
			data->camera->perspective((40.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&data->camera->pos, &rot);
			data->camera->rot = Quat::look(rot * Vec3(0, -1, 0));

			Game::cleanups.add(cleanup);
		}
	}
}

namespace title
{
	b8 first_show = true;
	const r32 start_fov = 40.0f * PI * 0.5f / 180.0f;
	const r32 end_fov = 70.0f * PI * 0.5f / 180.0f;
	const r32 total_transition = TRANSITION_TIME + 0.5f;

	enum class TutorialState
	{
		Start,
		Message,
		Climb,
		ClimbDone,
		WallRun,
		WallRunDone,
		count,
	};

	struct Data
	{
		Camera* camera;
		Vec3 camera_start_pos;
		r32 transition_timer;
		Ref<Animator> character;
		Ref<Transform> target_climb;
		Ref<Transform> target_hack_kits;
		TutorialState state;
	};
	
	static Data* data;

	void cleanup()
	{
		if (data->camera)
			data->camera->remove();
		delete data;
		data = nullptr;
	}

	void climb_success(Entity*)
	{
		if (data->state == TutorialState::Start
			|| data->state == TutorialState::Message
			|| data->state == TutorialState::Climb)
		{
			Cora::text_clear();
			data->state = TutorialState::ClimbDone;
		}
	}

	void wallrun_start(Entity*)
	{
		if (data->state == TutorialState::ClimbDone)
		{
			Cora::text_clear();
			Cora::text_schedule(0.0f, _(strings::tut_wallrun));
			data->state = TutorialState::WallRun;
		}
	}

	void wallrun_success()
	{
		if (data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
		{
			Cora::text_clear();
			data->state = TutorialState::WallRunDone;
		}
	}

	void message_success()
	{
		if (data->state == TutorialState::Message)
		{
			data->state = TutorialState::Climb;
			Cora::text_clear();
			Cora::text_schedule(0.0f, _(strings::tut_climb_jump));
		}
	}

	void update(const Update& u)
	{
		if (data->transition_timer > 0.0f)
		{
			if (data->camera)
			{
				Vec3 head_pos = Vec3::zero;
				data->character.ref()->to_world(Asset::Bone::character_head, &head_pos);
				r32 blend = vi_min(1.0f, total_transition - data->transition_timer);
				data->camera->pos = Vec3::lerp(blend, data->camera_start_pos, head_pos);
				r32 aspect = data->camera->viewport.size.y == 0 ? 1 : (r32)data->camera->viewport.size.x / (r32)data->camera->viewport.size.y;
				data->camera->perspective(LMath::lerpf(blend * 0.5f, start_fov, end_fov), aspect, 0.1f, Game::level.skybox.far_plane);
			}
			r32 old_timer = data->transition_timer;
			data->transition_timer = vi_max(0.0f, data->transition_timer - Game::real_time.delta);
			if (data->transition_timer < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
			{
				data->camera->remove();
				data->camera = nullptr;
				World::remove(data->character.ref()->entity());
				Game::level.mode = Game::Mode::Parkour;
				for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
					i.item()->camera->active = true;
			}
		}
		else
		{
			if (Game::level.mode == Game::Mode::Special && Menu::main_menu_state == Menu::State::Hidden && Game::scheduled_load_level == AssetNull)
			{
				b8 show = false;
				if (first_show) // wait for the user to hit a button before showing the menu
				{
					for (s32 i = 0; i < MAX_GAMEPADS; i++)
					{
						if (i == 0)
						{
							for (s32 j = 0; j < s32(KeyCode::Count); j++)
							{
								if (u.last_input->keys[j] && !u.input->keys[j])
								{
									show = true;
									break;
								}
							}
						}
						if (u.input->gamepads[i].btns)
						{
							show = true;
							break;
						}
					}
				}
				else // we've seen this title screen before; show the menu right away
					show = true;

				if (show)
				{
					first_show = false;
					Menu::show();
				}
			}
		}

		if (data->state == TutorialState::Start && Game::save.messages_unseen)
			data->state = TutorialState::Message;
		else if (data->state == TutorialState::Message && !Game::save.messages_unseen)
			message_success();
		else if ((data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
			&& !data->target_hack_kits.ref())
		{
			// player got the hack kits; done with this bit
			wallrun_success();
		}
	}

	void draw(const RenderParams& p)
	{
		if (!Overworld::active())
		{
			if (data->state == TutorialState::Climb)
				UI::indicator(p, data->target_climb.ref()->absolute_pos(), UI::color_accent, true);
			else if (data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
				UI::indicator(p, data->target_hack_kits.ref()->absolute_pos(), UI::color_accent, true);
			Cora::draw(p, p.camera->viewport.size * Vec2(0.5f, 0.9f));
		}

		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void init(const EntityFinder& entities)
	{
		if (Game::level.mode == Game::Mode::Special)
		{
			data = new Data();

			data->camera = Camera::add();

			data->camera->viewport =
			{
				Vec2(0, 0),
				Vec2(Game::width, Game::height),
			};
			r32 aspect = data->camera->viewport.size.y == 0 ? 1 : data->camera->viewport.size.x / data->camera->viewport.size.y;
			data->camera->perspective(start_fov, aspect, 0.1f, Game::level.skybox.far_plane);

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&data->camera_start_pos, &rot);
			data->camera->pos = data->camera_start_pos;
			data->camera->rot = Quat::look(rot * Vec3(0, -1, 0));

			data->character = entities.find("character")->get<Animator>();

			data->target_climb = entities.find("target_climb")->get<Transform>();
			data->target_hack_kits = entities.find("hack_kits")->get<Transform>();
			data->target_hack_kits.ref()->get<Collectible>()->collected.link(&wallrun_success);
			entities.find("climb_trigger1")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("climb_trigger2")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("wallrun_trigger")->get<PlayerTrigger>()->entered.link(&wallrun_start);

			Game::updates.add(update);
			Game::draws.add(draw);
			Game::cleanups.add(cleanup);

			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->camera->active = false;
		}
		else
			World::remove(entities.find("character"));
	}

	void play()
	{
		Game::save.reset();
		Game::session.reset();
		data->transition_timer = total_transition;
		Overworld::message_schedule(strings::contact_meursault, strings::msg_meursault_intro, 7.0f);
	}
}

namespace tutorial
{
	enum class TutorialState
	{
		ParkourStart, ParkourSlide, ParkourDone, Start, Upgrade, Ability, Capture, Done,
	};

	struct Data
	{
		TutorialState state;
	};

	Data* data;

	void health_got(const TargetEvent& e)
	{
		if (data->state == TutorialState::Start)
		{
			data->state = TutorialState::Upgrade;
			Cora::text_clear();
			Cora::text_schedule(1.0f, _(strings::tut_upgrade));

			Game::level.feature_level = Game::FeatureLevel::Abilities;
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			manager->credits = UpgradeInfo::list[s32(Upgrade::Sensor)].cost + AbilityInfo::list[s32(Ability::Sensor)].spawn_cost * 2;
	}	
	}

	void ability_spawned(Ability)
	{
		if (data->state == TutorialState::Ability)
		{
			data->state = TutorialState::Capture;
			Cora::text_clear();
			Cora::text_schedule(1.0f, _(strings::tut_capture));
		}
	}

	void player_spawned()
	{
		Entity* player = PlayerControlHuman::list.iterator().item()->entity();
		if (player->has<Awk>())
		{
			player->get<Awk>()->ability_spawned.link(&ability_spawned);

			if (data->state == TutorialState::Start)
			{
				Cora::text_clear();
				Cora::text_schedule(1.0f, _(strings::tut_start));
			}
		}
	}

	void update(const Update& u)
	{
		if (data->state == TutorialState::Upgrade)
		{
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			for (s32 i = 0; i < s32(Upgrade::count); i++)
			{
				if (manager->has_upgrade(Upgrade(i)))
				{
					data->state = TutorialState::Ability;
					Cora::text_clear();
					Cora::text_schedule(1.0f, _(strings::tut_ability));
					break;
				}
			}
		}
		else if (data->state != TutorialState::Done && Team::game_over)
		{
			data->state = TutorialState::Done;
			Cora::text_clear();
		}
	}

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void draw(const RenderParams& p)
	{
		Cora::draw(p, p.camera->viewport.size * Vec2(0.5f, 0.9f));
	}

	void slide_trigger(Entity* p)
	{
		if (Game::level.mode == Game::Mode::Parkour && data->state == TutorialState::ParkourStart)
		{
			data->state = TutorialState::ParkourSlide;
			Cora::text_clear();
			Cora::text_schedule(0.0f, _(strings::tut_slide));
		}
	}

	void slide_success(Entity*)
	{
		if (Game::level.mode == Game::Mode::Parkour)
		{
			if (data->state == TutorialState::ParkourSlide)
				Cora::text_clear();
			data->state = TutorialState::ParkourDone;
		}
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();

		Game::level.feature_level = Game::FeatureLevel::EnergyPickups;

		entities.find("health")->get<Target>()->target_hit.link(&health_got);
		PlayerManager* player_manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
		player_manager->spawn.link(&player_spawned);

		entities.find("slide_trigger")->get<PlayerTrigger>()->entered.link(&slide_trigger);
		entities.find("slide_success")->get<PlayerTrigger>()->entered.link(&slide_success);
		data->state = Game::level.mode == Game::Mode::Parkour ? TutorialState::ParkourStart : TutorialState::Start;

		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
		Game::draws.add(&draw);
	}
}


}

Script Script::all[] =
{
	{ "scene", Scripts::scene::init },
	{ "tutorial", Scripts::tutorial::init },
	{ "title", Scripts::title::init },
	{ 0, 0, },
};

}
