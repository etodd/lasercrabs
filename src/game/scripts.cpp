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

	void init(const Update& u, const EntityFinder& entities)
	{
		if (Game::level.mode == Game::Mode::Special)
		{
			data = new Data();

			data->camera = Camera::add();

			data->camera->viewport =
			{
				Vec2(0, 0),
				Vec2(u.input->width, u.input->height),
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

	struct Data
	{
		Camera* camera;
		Vec3 camera_start_pos;
		r32 transition_timer;
		Ref<Animator> character;
	};
	
	static Data* data;

	void cleanup()
	{
		if (data->camera)
			data->camera->remove();
		delete data;
		data = nullptr;
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
			if (Game::level.mode == Game::Mode::Special && Menu::main_menu_state == Menu::State::Hidden)
			{
				b8 show = false;
				if (first_show) // wait for the user to hit a button before showing the menu
				{
					for (s32 i = 0; i < MAX_GAMEPADS; i++)
					{
						if (i == 0)
						{
							for (s32 j = 0; j < (s32)KeyCode::Count; j++)
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
	}

	void draw(const RenderParams& p)
	{
		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		if (Game::level.mode == Game::Mode::Special)
		{
			data = new Data();

			data->camera = Camera::add();

			data->camera->viewport =
			{
				Vec2(0, 0),
				Vec2(u.input->width, u.input->height),
			};
			r32 aspect = data->camera->viewport.size.y == 0 ? 1 : (r32)data->camera->viewport.size.x / (r32)data->camera->viewport.size.y;
			data->camera->perspective(start_fov, aspect, 0.1f, Game::level.skybox.far_plane);

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&data->camera_start_pos, &rot);
			data->camera->pos = data->camera_start_pos;
			data->camera->rot = Quat::look(rot * Vec3(0, -1, 0));

			data->character = entities.find("character")->get<Animator>();

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
		Game::save = Game::Save();
		Game::session.reset();
		data->transition_timer = total_transition;

		Cora::text_schedule(total_transition + 1.0f, _(strings::tut_start));
	}
}

namespace tutorial
{
	enum class TutorialState
	{
		Start, Health, Minion, Upgrade, Ability, KillPlayer, Done,
	};

	struct Data
	{
		TutorialState state;
		Ref<Transform> minion_location;
		Ref<Transform> test_dummy_location;
		Ref<PlayerManager> test_dummy;
		r32 field_spawn_timer;
	};

	Data* data;

	void minion_killed(Entity*)
	{
		data->state = TutorialState::Upgrade;
		Cora::text_clear();
		Cora::text_schedule(1.0f, _(strings::tut_upgrade));

		Game::level.feature_level = Game::FeatureLevel::Abilities;
		PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
		manager->credits = UpgradeInfo::list[(s32)Upgrade::Sensor].cost + AbilityInfo::list[(s32)Ability::Sensor].spawn_cost * 2;
	}

	void health_got(const TargetEvent& e)
	{
		if (data->state == TutorialState::Health)
		{
			// spawn minion
			Vec3 pos = data->minion_location.ref()->absolute_pos();
			Entity* minion = World::create<Minion>(pos, Quat::identity, data->test_dummy.ref()->team.ref()->team(), data->test_dummy.ref());
			minion->get<Health>()->killed.link(&minion_killed);
			Net::finalize(minion);

			data->state = TutorialState::Minion;
			Cora::text_clear();
			Cora::text_schedule(0.25f, _(strings::tut_minion));
		}
	}

	void player_or_ai_killed(Entity*)
	{
		data->state = TutorialState::Done;
		Cora::clear();
	}

	void ai_spawned()
	{
		Entity* entity = PlayerControlAI::list.iterator().item()->entity();
		entity->get<Transform>()->absolute_pos(data->test_dummy_location.ref()->absolute_pos());
		LinkArg<Entity*>* link = &entity->get<Health>()->killed;
		link->link(&player_or_ai_killed);
	}

	void ability_spawned(Ability)
	{
		if (data->state == TutorialState::Ability)
		{
			data->state = TutorialState::KillPlayer;
			Cora::text_clear();
			Cora::text_schedule(1.0f, _(strings::tut_kill_player));
		}
	}

	void player_spawned()
	{
		Entity* player = PlayerControlHuman::list.iterator().item()->entity();
		if (player->has<Awk>())
		{
			player->get<Health>()->killed.link(&player_or_ai_killed);
			player->get<Awk>()->ability_spawned.link(&ability_spawned);
		}
	}

	void health_spotted(Entity* player)
	{
		if (player->has<PlayerControlHuman>() && data->state == TutorialState::Start)
		{
			data->state = TutorialState::Health;
			Cora::text_clear();
			Cora::text_schedule(0.25f, _(strings::tut_health));
		}
	}

	void update(const Update& u)
	{
		if (data->state == TutorialState::Upgrade)
		{
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			for (s32 i = 0; i < (s32)Upgrade::count; i++)
			{
				if (manager->has_upgrade((Upgrade)i))
				{
					data->state = TutorialState::Ability;
					Cora::text_clear();
					Cora::text_schedule(1.0f, _(strings::tut_ability));
					break;
				}
			}
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

	void init(const Update& u, const EntityFinder& entities)
	{
		Game::level.feature_level = Game::FeatureLevel::EnergyPickups;
		Game::level.kill_limit = 1;

		data = new Data();
		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
		Game::draws.add(&draw);

		data->minion_location = entities.find("minion")->get<Transform>();
		data->test_dummy_location = entities.find("test_dummy")->get<Transform>();

		entities.find("health_trigger")->get<PlayerTrigger>()->entered.link(&health_spotted);
		entities.find("health")->get<Target>()->target_hit.link(&health_got);

		Entity* e = World::create<ContainerEntity>();
		PlayerManager* ai_manager = e->add<PlayerManager>(&Team::list[1]);
		utf8cpy(ai_manager->username, _(strings::dummy));
		Net::finalize(e);

		PlayerAI* ai_player = PlayerAI::list.add();
		new (ai_player) PlayerAI(ai_manager, PlayerAI::generate_config(1, 0.0f));

		AI::Config* config = &ai_player->config;
		config->high_level = AI::HighLevelLoop::Noop;
		config->low_level = AI::LowLevelLoop::Noop;

		PlayerManager* player_manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
		player_manager->spawn.link(&player_spawned);
		ai_manager->spawn.link(&ai_spawned);
		data->test_dummy = ai_manager;

		Cora::text_schedule(PLAYER_SPAWN_DELAY + 1.0f, _(strings::tut_start));
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
