#include "scripts.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "strings.h"
#include "utf8/utf8.h"
#include "ai_player.h"
#include "console.h"
#include <unordered_map>
#include <string>
#include "minion.h"
#include "net.h"
#include "team.h"
#include "asset/level.h"
#include "asset/armature.h"
#include "asset/Wwise_IDs.h"
#include "asset/font.h"
#include "asset/animation.h"
#include "data/animator.h"
#include "overworld.h"
#include "player.h"
#include "audio.h"
#include "load.h"

namespace VI
{

namespace Actor
{

// animated, voiced, scripted character

typedef void (*Callback)();

struct Cue
{
	Callback callback;
	r32 delay;
	AkUniqueID sound = AK_InvalidID;
	AssetID animation = AssetNull;
	AssetID text = AssetNull;
	b8 loop;
};

struct Data
{
	StaticArray<Cue, 16> cues;
	r32 last_cue_real_time;
	AssetID text = AssetNull;
	AssetID head_bone;
	Ref<Animator> model;
	b8 highlight;
	b8 sound_done;
	AssetID text_tut = AssetNull;
	r32 text_tut_real_time;
};

static Data* data;

void cleanup()
{
	delete data;
	data = nullptr;

	Audio::post_global_event(AK::EVENTS::STOP_DIALOGUE);
	Audio::dialogue_done = false;
}

void tut_clear()
{
	data->text_tut = AssetNull;
}

void tut(AssetID text, r32 delay = 1.0f)
{
	tut_clear();
	data->text_tut = text;
	data->text_tut_real_time = Game::real_time.total + delay;
}

void update(const Update& u)
{
	if (!data)
		return;

	if (Audio::dialogue_done)
	{
		data->sound_done = true;
		data->text = AssetNull;
		Audio::dialogue_done = false;
	}

	if (data->model.ref())
	{
		Animator::Layer* layer = &data->model.ref()->layers[0];
		if ((layer->time == Loader::animation(layer->animation)->duration || layer->behavior == Animator::Behavior::Loop)
			&& data->sound_done
			&& data->cues.length > 0)
		{
			Cue* cue = &data->cues[0];
			if (cue->delay > 0.0f)
				cue->delay -= u.time.delta;
			else
			{
				if (cue->callback)
					cue->callback();
				else
				{
					data->last_cue_real_time = Game::real_time.total;
					data->text = cue->text;
					layer->behavior = cue->loop ? Animator::Behavior::Loop : Animator::Behavior::Freeze;
					layer->play(cue->animation);

					if (cue->sound == AK_InvalidID)
						data->sound_done = true;
					else
					{
						data->model.ref()->get<Audio>()->post_dialogue_event(cue->sound);
						data->sound_done = false;
					}
				}

				if (data) // callback might have called cleanup()
					data->cues.remove_ordered(0);
			}
		}
	}
}

void draw(const RenderParams& params)
{
	if (!data)
		return;

	if (data->highlight && !Overworld::active())
	{
		// direct the player toward the actor only if they're looking the wrong way
		Vec3 head_pos = Vec3::zero;
		data->model.ref()->to_world(data->head_bone, &head_pos);
		Vec2 p;
		Vec2 offset;
		if (!UI::is_onscreen(params, head_pos, &p, &offset))
			UI::triangle(params, { p, Vec2(24 * UI::scale) }, UI::color_default, atan2f(offset.y, offset.x) + PI * -0.5f);

		if (data->text != AssetNull)
		{
			UIText text;
			text.font = Asset::Font::pt_sans;
			text.size = 18.0f;
			text.wrap_width = MENU_ITEM_WIDTH;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::color_default;
			text.text(_(data->text));
			UIMenu::text_clip(&text, data->last_cue_real_time, 80.0f);

			{
				Vec2 p = params.camera->viewport.size * Vec2(0.5f, 0.2f);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);
			}
		}
	}

	if (data->text_tut != AssetNull && Game::real_time.total > data->text_tut_real_time)
	{
		UIText text;
		text.wrap_width = MENU_ITEM_WIDTH;
		text.anchor_x = UIText::Anchor::Center;
		text.anchor_y = UIText::Anchor::Max;
		text.color = UI::color_accent;
		text.text(_(data->text_tut));
		UIMenu::text_clip(&text, data->text_tut_real_time, 80.0f);

		{
			Vec2 p = params.camera->viewport.size * Vec2(0.5f, 0.9f);
			UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
			text.draw(params, p);
		}
	}
}

void init(Entity* model = nullptr, AssetID head_bone = AssetNull)
{
	Audio::dialogue_done = false;

	vi_assert(!data);
	data = new Data();
	if (model)
	{
		model->add<Audio>();
		data->model = model->get<Animator>();
	}
	data->head_bone = head_bone;
	data->sound_done = true;

	b8 already_registered = false;
	for (s32 i = 0; i < Game::cleanups.length; i++)
	{
		if (Game::cleanups[i] == cleanup)
		{
			already_registered = true;
			break;
		}
	}

	if (!already_registered)
	{
		Game::cleanups.add(cleanup);
		Game::updates.add(update);
		Game::draws.add(draw);
	}
}

void cue(AkUniqueID sound, AssetID animation, AssetID text = AssetNull, b8 loop = true, r32 delay = 0.3f)
{
	Cue* c = data->cues.add();
	new (c) Cue();
	c->sound = sound;
	c->animation = animation;
	c->delay = delay;
	c->loop = loop;
	c->text = text;
}

void cue(Callback callback, r32 delay = 0.3f)
{
	Cue* c = data->cues.add();
	new (c) Cue();
	c->callback = callback;
	c->delay = delay;
}

void highlight(b8 a)
{
	data->highlight = a;
}

void done()
{
	highlight(false);
}

}

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
			r32 aspect = data->camera->viewport.size.y == 0 ? 1 : r32(data->camera->viewport.size.x) / r32(data->camera->viewport.size.y);
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
		SailorSpotted,
		SailorTalking,
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
			|| data->state == TutorialState::SailorSpotted
			|| data->state == TutorialState::SailorTalking
			|| data->state == TutorialState::Climb)
		{
			Actor::tut_clear();
			data->state = TutorialState::ClimbDone;
		}
	}

	void wallrun_start(Entity*)
	{
		if (data->state == TutorialState::ClimbDone)
		{
			Actor::tut(strings::tut_wallrun);
			data->state = TutorialState::WallRun;
		}
	}

	void wallrun_success()
	{
		if (data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
		{
			Actor::tut_clear();
			data->state = TutorialState::WallRunDone;
		}
	}

	void sailor_spotted(Entity*)
	{
		if (data->state == TutorialState::Start)
		{
			data->state = TutorialState::SailorSpotted;
			Actor::cue(AK::EVENTS::PLAY_SAILOR_COME_HERE, Asset::Animation::sailor_wait, strings::sailor_come_here);
		}
	}

	void sailor_done()
	{
		if (data->state == TutorialState::SailorTalking)
		{
			data->state = TutorialState::Climb;
			Actor::tut(strings::tut_climb_jump);
		}
	}

	void sailor_talk(Entity*)
	{
		if (data->state == TutorialState::SailorSpotted)
		{
			data->state = TutorialState::SailorTalking;
			Actor::cue(AK::EVENTS::PLAY_SAILOR_SPEECH_1, Asset::Animation::sailor_talk, strings::sailor_speech_1);
			Actor::cue(AK::EVENTS::PLAY_SAILOR_SPEECH_2, Asset::Animation::sailor_talk, strings::sailor_speech_2);
			Actor::cue(AK_InvalidID, Asset::Animation::sailor_close_door, AssetNull, false);
			Actor::cue(&sailor_done);
			Actor::cue(&Actor::done, 0.0f);
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
				r32 aspect = data->camera->viewport.size.y == 0 ? 1 : r32(data->camera->viewport.size.x) / r32(data->camera->viewport.size.y);
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
					for (s32 j = 0; j < s32(KeyCode::Count); j++)
					{
						if (u.last_input->keys[j] && !u.input->keys[j])
						{
							show = true;
							break;
						}
					}
					for (s32 i = 0; i < MAX_GAMEPADS; i++)
					{
						if (u.last_input->gamepads[i].btns && !u.input->gamepads[i].btns)
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

		if ((data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
			&& !data->target_hack_kits.ref())
		{
			// player got the hack kits; done with this bit
			wallrun_success();
		}
	}

	void draw(const RenderParams& p)
	{
		if (Game::level.mode == Game::Mode::Special && Menu::main_menu_state == Menu::State::Hidden && Game::scheduled_load_level == AssetNull && data->transition_timer == 0.0f)
		{
			UIText text;
			text.color = UI::color_accent;
			text.text("[{{Start}}]");
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			Vec2 pos = p.camera->viewport.size * Vec2(0.5f, 0.1f);
			UI::box(p, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.draw(p, pos);
		}

		if (!Overworld::active())
		{
			if (data->state == TutorialState::Climb)
				UI::indicator(p, data->target_climb.ref()->absolute_pos(), UI::color_accent, true);
			else if (data->state == TutorialState::ClimbDone || data->state == TutorialState::WallRun)
				UI::indicator(p, data->target_hack_kits.ref()->absolute_pos(), UI::color_accent, true);
		}

		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(cleanup);

		if (Game::level.mode == Game::Mode::Special)
		{
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

			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->camera->active = false;
		}
		else
			World::remove(entities.find("character"));

		if ((Game::save.zone_last == AssetNull || Game::save.zone_last == Asset::Level::Dock)
			&& entities.find("hack_kits"))
		{
			data->target_climb = entities.find("target_climb")->get<Transform>();
			data->target_hack_kits = entities.find("hack_kits")->get<Transform>();
			data->target_hack_kits.ref()->get<Collectible>()->collected.link(&wallrun_success);
			entities.find("climb_trigger1")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("climb_trigger2")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("wallrun_trigger")->get<PlayerTrigger>()->entered.link(&wallrun_start);
			entities.find("sailor_spotted_trigger")->get<PlayerTrigger>()->entered.link(&sailor_spotted);
			entities.find("sailor_talk_trigger")->get<PlayerTrigger>()->entered.link(&sailor_talk);

			Game::updates.add(update);
			Game::draws.add(draw);

			Actor::init(entities.find("sailor"), Asset::Bone::sailor_head);
			if (Game::level.mode != Game::Mode::Special)
				Actor::highlight(true);
		}
		else
		{
			Animator* sailor = entities.find("sailor")->get<Animator>();
			sailor->layers[0].behavior = Animator::Behavior::Freeze;
			sailor->layers[0].play(Asset::Animation::sailor_close_door);
		}
	}

	void play()
	{
		Game::save.reset();
		Game::session.reset();
		data->transition_timer = total_transition;
		Actor::highlight(true);
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
		Ref<Transform> sparks;
	};

	Data* data;

	void health_got(const TargetEvent& e)
	{
		if (data->state == TutorialState::Start)
		{
			data->state = TutorialState::Upgrade;
			Actor::tut(strings::tut_upgrade);

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
			Game::level.feature_level = Game::FeatureLevel::TutorialAll;
			Actor::tut(strings::tut_capture);
		}
	}

	void player_spawned()
	{
		Entity* player = PlayerControlHuman::list.iterator().item()->entity();
		if (player->has<Awk>())
		{
			player->get<Awk>()->ability_spawned.link(&ability_spawned);

			if (s32(data->state) <= s32(TutorialState::Start))
			{
				data->state = TutorialState::Start;
				Actor::tut(strings::tut_start);
				Game::level.feature_level = Game::FeatureLevel::EnergyPickups;
			}
		}
	}

	void update(const Update& u)
	{
		// sparks on broken door
		if (mersenne::randf_co() < u.time.delta / 0.5f)
			spawn_sparks(data->sparks.ref()->to_world(Vec3(-1.5f + mersenne::randf_co() * 3.0f, 0, 0)), Quat::look(Vec3(0, -1, 0)));

		if (data->state != TutorialState::Done && Team::game_over)
		{
			data->state = TutorialState::Done;
			Actor::tut_clear();
		}
		else if (data->state == TutorialState::Upgrade)
		{
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			for (s32 i = 0; i < s32(Upgrade::count); i++)
			{
				if (manager->has_upgrade(Upgrade(i)))
				{
					data->state = TutorialState::Ability;
					Actor::tut(strings::tut_ability);
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

	void slide_trigger(Entity* p)
	{
		if (Game::level.mode == Game::Mode::Parkour && data->state == TutorialState::ParkourStart)
		{
			data->state = TutorialState::ParkourSlide;
			Actor::tut(strings::tut_slide, 0.0f);
		}
	}

	void slide_success(Entity*)
	{
		if (Game::level.mode == Game::Mode::Parkour)
		{
			if (data->state == TutorialState::ParkourSlide)
				Actor::tut_clear();
			data->state = TutorialState::ParkourDone;
		}
	}

	void init(const EntityFinder& entities)
	{
		Actor::init();

		data = new Data();

		entities.find("health")->get<Target>()->target_hit.link(&health_got);
		PlayerManager* player_manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
		player_manager->spawn.link(&player_spawned);

		entities.find("slide_trigger")->get<PlayerTrigger>()->entered.link(&slide_trigger);
		entities.find("slide_success")->get<PlayerTrigger>()->entered.link(&slide_success);
		data->sparks = entities.find("sparks")->get<Transform>();

		if (Game::level.mode == Game::Mode::Pvp)
		{
			Game::level.feature_level = Game::FeatureLevel::EnergyPickups;
			data->state = TutorialState::Start;
		}
		else
			data->state = TutorialState::ParkourStart;

		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
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
