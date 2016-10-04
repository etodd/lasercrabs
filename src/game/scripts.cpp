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

namespace scene
{
	struct Data
	{
		UIText text;
		Camera* camera;
	};
	
	static Data* data;

	void cleanup()
	{
		data->camera->remove();
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		if (Game::session.mode == Game::Mode::Special)
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
		PlayerManager* manager = LocalPlayer::list.iterator().item()->manager.ref();
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
		Entity* entity = AIPlayerControl::list.iterator().item()->entity();
		entity->get<Transform>()->absolute_pos(data->test_dummy_location.ref()->absolute_pos());
		LinkArg<Entity*>* link = &entity->get<Health>()->killed;
		link->link(&player_or_ai_killed);
	}

	void player_spawned()
	{
		LocalPlayerControl::list.iterator().item()->get<Health>()->hp = 2;
		LocalPlayerControl::list.iterator().item()->get<Health>()->killed.link(&player_or_ai_killed);
	}

	void health_spotted(Entity* player)
	{
		if (player->has<LocalPlayerControl>() && data->state == TutorialState::Start)
		{
			data->state = TutorialState::Health;
			Cora::text_clear();
			Cora::text_schedule(0.25f, _(strings::tut_health));
		}
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

	void update(const Update& u)
	{
		if (data->state == TutorialState::Upgrade)
		{
			PlayerManager* manager = LocalPlayer::list.iterator().item()->manager.ref();
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

		data = new Data();
		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
		Game::draws.add(&draw);

		data->minion_location = entities.find("minion")->get<Transform>();
		data->test_dummy_location = entities.find("test_dummy")->get<Transform>();

		entities.find("health_trigger")->get<PlayerTrigger>()->entered.link(&health_spotted);
		entities.find("health")->get<Target>()->target_hit.link(&health_got);

		PlayerManager* ai_manager = PlayerManager::list.add();
		new (ai_manager) PlayerManager(&Team::list[1]);

		utf8cpy(ai_manager->username, _(strings::dummy));

		AIPlayer* ai_player = AIPlayer::list.add();
		new (ai_player) AIPlayer(ai_manager, AIPlayer::generate_config());

		AIPlayer::Config* config = &ai_player->config;
		config->high_level = AIPlayer::HighLevelLoop::Noop;
		config->low_level = AIPlayer::LowLevelLoop::Noop;

		PlayerManager* player_manager = LocalPlayer::list.iterator().item()->manager.ref();
		player_manager->spawn.link(&player_spawned);
		player_manager->ability_spawned.link(&ability_spawned);
		ai_manager->spawn.link(&ai_spawned);
		data->test_dummy = ai_manager;

		Cora::init(); // have to init manually since Cora normally isn't loaded in PvP mode
		Cora::text_schedule(PLAYER_SPAWN_DELAY + 1.0f, _(strings::tut_start));
	}
}

Script Script::all[] =
{
	{ "scene", scene::init },
	{ "tutorial", tutorial::init },
	{ 0, 0, },
};

}
