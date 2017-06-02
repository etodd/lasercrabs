#include "scripts.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "strings.h"
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
#include "asset/mesh.h"
#include "data/animator.h"
#include "overworld.h"
#include "player.h"
#include "audio.h"
#include "load.h"
#include "data/components.h"
#include "drone.h"

namespace VI
{

namespace Actor
{

// animated, voiced, scripted character

struct Instance;

typedef void (*Callback)(Instance*);

struct Cue
{
	Callback callback;
	r32 delay;
	AkUniqueID sound = AK_InvalidID;
	AssetID animation = AssetNull;
	AssetID text = AssetNull;
	b8 loop;
};

enum class Behavior : s8
{
	WaitForIdleAnimation,
	InterruptIdleAnimation,
	count,
};

struct Instance
{
	static PinArray<Instance, 8> list;

	StaticArray<Cue, 32> cues;
	r32 last_cue_real_time;
	Behavior behavior;
	Ref<Entity> model;
	Ref<Transform> collision;
	AssetID text;
	AssetID head_bone;
	Revision revision;
	b8 highlight;
	b8 sound_done;
	b8 interrupt_idle_animation;

	inline ID id() const
	{
		return this - &list[0];
	}

	Instance()
		: cues(),
		last_cue_real_time(),
		behavior(),
		model(),
		collision(),
		text(AssetNull),
		head_bone(AssetNull),
		highlight(),
		sound_done(),
		interrupt_idle_animation()
	{
	}

	void cue(AkUniqueID sound, AssetID animation, AssetID text = AssetNull, b8 loop = true, r32 delay = 0.3f)
	{
		if (behavior == Behavior::InterruptIdleAnimation && model.ref()->has<Animator>() && cues.length == 0)
			interrupt_idle_animation = true;

		Cue* c = cues.add();
		new (c) Cue();
		c->sound = sound;
		c->animation = animation;
		c->delay = delay;
		c->loop = loop;
		c->text = text;
	}

	void cue(Callback callback, r32 delay = 0.3f)
	{
		if (behavior == Behavior::InterruptIdleAnimation && model.ref()->has<Animator>() && cues.length == 0)
			interrupt_idle_animation = true;

		Cue* c = cues.add();
		new (c) Cue();
		c->callback = callback;
		c->delay = delay;
	}

	Vec3 collision_offset() const
	{
		if (model.ref()->has<Animator>())
		{
			Vec3 pos(0.0f);
			model.ref()->get<Animator>()->to_local(head_bone, &pos);
			pos.y -= 1.5f;
			return pos;
		}
		else
			return Vec3::zero;
	}
};

PinArray<Instance, 8> Instance::list;

struct Data
{
	r32 text_tut_real_time;
	AssetID text_tut = AssetNull;
};

static Data* data;

void cleanup()
{
	delete data;
	data = nullptr;
	Instance::list.clear();

	Audio::post_global_event(AK::EVENTS::STOP_DIALOGUE);
	Audio::dialogue_callbacks.length = 0;
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

void done(Instance* i)
{
	i->highlight = false;
}

void remove(Instance*);

void update(const Update& u)
{
	if (!data)
		return;

	for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
	{
		Instance* instance = i.item();

		if (instance->model.ref())
		{
			if (!instance->sound_done)
			{
				for (s32 j = 0; j < Audio::dialogue_callbacks.length; j++)
				{
					ID callback_entity_id = Audio::dialogue_callbacks[j];
					if (instance->model.id == callback_entity_id)
					{
						instance->sound_done = true;
						instance->text = AssetNull;
					}
				}
			}

			Animator::Layer* layer;
			if (instance->model.ref()->has<Animator>())
				layer = &instance->model.ref()->get<Animator>()->layers[0];
			else
				layer = nullptr;

			if ((!layer || layer->animation == AssetNull || layer->behavior == Animator::Behavior::Loop || layer->time == Loader::animation(layer->animation)->duration || instance->interrupt_idle_animation)
				&& instance->sound_done
				&& instance->cues.length > 0)
			{
				Cue* cue = &instance->cues[0];
				if (cue->delay > 0.0f)
					cue->delay -= u.time.delta;
				else
				{
					instance->interrupt_idle_animation = false;
					if (cue->callback)
						cue->callback(instance);
					else
					{
						instance->last_cue_real_time = Game::real_time.total;
						instance->text = cue->text;
						if (layer)
						{
							layer->behavior = cue->loop ? Animator::Behavior::Loop : Animator::Behavior::Freeze;
							layer->play(cue->animation);
						}
						else
							vi_assert(cue->animation == AssetNull);

						if (cue->sound == AK_InvalidID)
							instance->sound_done = true;
						else
						{
							instance->sound_done = false;
							instance->model.ref()->get<Audio>()->post_dialogue_event(cue->sound);
						}
					}

					if (data) // callback might have called cleanup()
						instance->cues.remove_ordered(0);
				}
			}

			if (layer)
				instance->collision.ref()->pos = instance->collision_offset();
		}
		else
		{
			// model has been removed
			remove(instance);
		}
	}

	Audio::dialogue_callbacks.length = 0;
}

void draw(const RenderParams& params)
{
	if (!data)
		return;

	if (!Overworld::active())
	{
		for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
		{
			const Instance& instance = *i.item();

			if (!instance.model.ref())
				continue;

			Vec3 actor_pos = Vec3::zero;
			if (instance.head_bone == AssetNull)
				actor_pos = instance.model.ref()->get<Transform>()->absolute_pos();
			else
				instance.model.ref()->get<Animator>()->to_world(instance.head_bone, &actor_pos);

			if (instance.highlight)
				UI::indicator(params, actor_pos + Vec3(0, -0.4f, 0), UI::color_accent, true);

			if (instance.text != AssetNull
				&& (instance.highlight || (actor_pos - params.camera->pos).length_squared() < 8.0f * 8.0f))
			{
				UIText text;
				text.font = Asset::Font::pt_sans;
				text.size = 18.0f;
				text.wrap_width = MENU_ITEM_WIDTH;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.color = UI::color_default;
				text.text(params.camera->gamepad, _(instance.text));
				UIMenu::text_clip(&text, instance.last_cue_real_time, 80.0f);

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
			text.text(params.camera->gamepad, _(data->text_tut));
			UIMenu::text_clip(&text, data->text_tut_real_time, 80.0f);

			{
				Vec2 p = params.camera->viewport.size * Vec2(0.5f, 0.8f);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);
			}
		}
	}
}

void init()
{
	if (!data)
	{
		Audio::dialogue_callbacks.length = 0;
		data = new Data();
		Game::cleanups.add(cleanup);
		Game::updates.add(update);
		Game::draws.add(draw);
	}
}

Instance* add(Entity* model, Behavior behavior = Behavior::WaitForIdleAnimation, AssetID head_bone = AssetNull)
{
	init();

	Instance* i = Instance::list.add();
	new (i) Instance();

	i->revision++;
	i->behavior = behavior;
	i->model = model;
	i->head_bone = head_bone;
	i->sound_done = true;

	if (Game::level.local)
	{
		if (!model->has<Audio>())
			model->add<Audio>();

		// animated actors have collision volumes that are synced up with their bodies
		if (model->has<Animator>())
		{
			Entity* collision = World::create<StaticGeom>(Asset::Mesh::actor_collision, model->get<Transform>()->absolute_pos() + i->collision_offset(), Quat::identity, CollisionInaccessible, ~CollisionParkour & ~CollisionInaccessible & ~CollisionElectric);
			collision->get<Transform>()->reparent(model->get<Transform>());
			collision->get<View>()->mask = 0;
			i->collision = collision->get<Transform>();
			Net::finalize(collision);
		}
	}
	else
	{
		// we're a client
		// find the collision body assigned by the server, if any
		if (model->has<Animator>())
		{
			for (auto j = Transform::list.iterator(); !j.is_last(); j.next())
			{
				if (j.item()->parent.ref() == model->get<Transform>())
				{
					i->collision = j.item();
					break;
				}
			}
		}
	}

	return i;
}

void remove(Instance* i)
{
	if (Game::level.local)
	{
		if (i->model.ref())
			World::remove(i->model.ref());
		if (i->collision.ref())
			World::remove(i->collision.ref()->entity());
	}
	i->~Instance();
	i->revision++;
	Instance::list.remove(i->id());
}

}

AssetID Script::find(const char* name)
{
	AssetID i = 0;
	while (true)
	{
		if (!Script::list[i].name)
			break;

		if (strcmp(Script::list[i].name, name) == 0)
			return i;

		i++;
	}
	return AssetNull;
}

namespace Scripts
{


namespace scene
{
	void init(const EntityFinder& entities)
	{
		for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
		{
			i.item()->get<PlayerManager>()->can_spawn = false;

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&i.item()->camera->pos, &rot);
			i.item()->camera->rot = Quat::look(rot * Vec3(0, -1, 0));
		}
	}
}

namespace title
{
	b8 first_show = true;
	const r32 start_fov = 40.0f * PI * 0.5f / 180.0f;
	const r32 end_fov = 70.0f * PI * 0.5f / 180.0f;
	const r32 total_transition = TRANSITION_TIME + 0.5f;

	enum class TutorialState : s8
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
		Actor::Instance* sailor;
		Vec3 camera_start_pos;
		r32 transition_timer;
		r32 wallrun_footstep_timer;
		s32 wallrun_footstep_index;
		Ref<Animator> character;
		Ref<Transform> target_climb;
		Ref<Transform> target_hack_kits;
		Ref<Transform> target_wall_run;
		Ref<Transform> ivory_ad_text;
		Ref<Actor::Instance> ivory_ad_actor;
		Ref<Actor::Instance> hobo_actor;
		Ref<Entity> hobo;
		TutorialState state;
		b8 sailor_talked;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps;
	};
	
	static Data* data;

	void cleanup()
	{
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
			data->sailor->cue(AK::EVENTS::PLAY_SAILOR_COME_HERE, Asset::Animation::sailor_wait, strings::sailor_come_here);
		}
	}

	void sailor_done(Actor::Instance*)
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
			data->state = TutorialState::SailorTalking;

		if (!data->sailor_talked)
		{
			data->sailor_talked = true;
			data->sailor->highlight = false;
			data->sailor->cue(AK::EVENTS::PLAY_SAILOR_SPEECH_1, Asset::Animation::sailor_talk, strings::sailor_speech_1);
			data->sailor->cue(AK::EVENTS::PLAY_SAILOR_SPEECH_2, Asset::Animation::sailor_talk, strings::sailor_speech_2);
			data->sailor->cue(AK_InvalidID, Asset::Animation::sailor_close_door, AssetNull, false);
			data->sailor->cue(&sailor_done);
			data->sailor->cue(&Actor::done, 0.0f);
		}
	}

	void hobo_done(Actor::Instance* hobo)
	{
		hobo->cue(AK::EVENTS::PLAY_HOBO01, Asset::Animation::hobo_idle, strings::hobo01);
		hobo->cue(AK::EVENTS::PLAY_HOBO02, Asset::Animation::hobo_idle, strings::hobo02);
		hobo->cue(AK::EVENTS::PLAY_HOBO03, Asset::Animation::hobo_idle, strings::hobo03);
		hobo->cue(AK::EVENTS::PLAY_HOBO04, Asset::Animation::hobo_idle, strings::hobo04);
		hobo->cue(AK::EVENTS::PLAY_HOBO05, Asset::Animation::hobo_idle, strings::hobo05);
		hobo->cue(AK::EVENTS::PLAY_HOBO06, Asset::Animation::hobo_idle, strings::hobo06);
		hobo->cue(AK::EVENTS::PLAY_HOBO07, Asset::Animation::hobo_idle, strings::hobo07);
		hobo->cue(AK::EVENTS::PLAY_HOBO08, Asset::Animation::hobo_idle, strings::hobo08);
		hobo->cue(AK::EVENTS::PLAY_HOBO09, Asset::Animation::hobo_idle, strings::hobo09);
		hobo->cue(AK::EVENTS::PLAY_HOBO10, Asset::Animation::hobo_idle, strings::hobo10);
		hobo->cue(&hobo_done, 4.0f);
	}

	void hobo_talk(Entity*)
	{
		Actor::Instance* hobo = data->hobo_actor.ref();
		if (hobo->cues.length == 0)
			hobo_done(hobo);
	}

	void ivory_ad_play(Entity*)
	{
		if (data->ivory_ad_actor.ref()->cues.length == 0)
		{
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD1, AssetNull, strings::ivory_ad1);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD2, AssetNull, strings::ivory_ad2);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD3, AssetNull, strings::ivory_ad3);
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
				Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_IN);
				data->camera->remove();
				data->camera = nullptr;
				World::remove(data->character.ref()->entity());
				Game::level.mode = Game::Mode::Parkour;
				data->sailor->highlight = true;
				for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				{
					i.item()->get<PlayerManager>()->can_spawn = true;
					i.item()->camera->flag(CameraFlagActive, true);
				}
			}
		}
		else
		{
			if (Game::level.mode == Game::Mode::Special && Menu::main_menu_state == Menu::State::Hidden && Game::scheduled_load_level == AssetNull)
			{
				b8 show = false;
				if (first_show) // wait for the user to hit a button before showing the menu
				{
					if (u.last_input->keys.any() && !u.input->keys.any())
						show = true;
					else
					{
						for (s32 i = 0; i < MAX_GAMEPADS; i++)
						{
							if (u.last_input->gamepads[i].btns && !u.input->gamepads[i].btns)
							{
								show = true;
								break;
							}
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
			wallrun_success(); // player got the hack kits; done with this bit

		if (data->state == TutorialState::WallRun)
		{
			// spawn wallrun footstep shockwave effects
			data->wallrun_footstep_timer -= u.time.delta;
			if (data->wallrun_footstep_timer < 0.0f)
			{
				data->wallrun_footstep_timer += 2.0f / data->wallrun_footsteps.length;
				data->wallrun_footstep_index = (data->wallrun_footstep_index + 1) % data->wallrun_footsteps.length;
				EffectLight::add(data->wallrun_footsteps[data->wallrun_footstep_index].ref()->absolute_pos(), 1.0f, 1.0f, EffectLight::Type::Shockwave);
			}
		}

		// ivory ad text
		data->ivory_ad_text.ref()->rot *= Quat::euler(0, u.time.delta * 0.2f, 0);
	}

	void draw(const RenderParams& p)
	{
		if (Game::level.mode == Game::Mode::Special && Menu::main_menu_state == Menu::State::Hidden && Game::scheduled_load_level == AssetNull && data->transition_timer == 0.0f)
		{
			UIText text;
			text.color = UI::color_accent;
			text.text(0, "[{{Start}}]");
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
			else if (data->state == TutorialState::ClimbDone)
				UI::indicator(p, data->target_wall_run.ref()->absolute_pos(), UI::color_accent, true);
			else if (data->state == TutorialState::WallRun)
				UI::indicator(p, data->target_hack_kits.ref()->absolute_pos(), UI::color_accent, true);
		}

		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void init(const EntityFinder& entities)
	{
		vi_assert(!data);
		data = new Data();
		Game::cleanups.add(cleanup);

		if (Game::level.mode == Game::Mode::Special)
		{
			data->camera = Camera::add(0);

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
				i.item()->camera->flag(CameraFlagActive, false);
		}
		else if (Game::level.local)
			World::remove(entities.find("character"));

		if ((Game::save.zone_last == AssetNull || Game::save.zone_last == Asset::Level::Tier_0)
			&& entities.find("hack_kits"))
		{
			data->target_climb = entities.find("target_climb")->get<Transform>();
			data->target_hack_kits = entities.find("hack_kits")->get<Transform>();
			data->target_hack_kits.ref()->get<Collectible>()->collected.link(&wallrun_success);
			data->target_wall_run = entities.find("wallrun_target")->get<Transform>();
			entities.find("climb_trigger1")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("climb_trigger2")->get<PlayerTrigger>()->entered.link(&climb_success);
			entities.find("wallrun_trigger")->get<PlayerTrigger>()->entered.link(&wallrun_start);
			entities.find("sailor_spotted_trigger")->get<PlayerTrigger>()->entered.link(&sailor_spotted);
			entities.find("sailor_talk_trigger")->get<PlayerTrigger>()->entered.link(&sailor_talk);

			data->wallrun_footsteps.add(entities.find("wallrun_footstep")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.001")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.002")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.003")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.004")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.005")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.006")->get<Transform>());
			data->wallrun_footsteps.add(entities.find("wallrun_footstep.007")->get<Transform>());

			Game::updates.add(update);
			Game::draws.add(draw);

			data->sailor = Actor::add(entities.find("sailor"), Actor::Behavior::WaitForIdleAnimation, Asset::Bone::sailor_head);
			Loader::animation(Asset::Animation::sailor_close_door);
			Loader::animation(Asset::Animation::sailor_wait);
			Loader::animation(Asset::Animation::sailor_talk);
			if (Game::level.mode != Game::Mode::Special)
				data->sailor->highlight = true;
		}
		else
		{
			Animator* sailor = entities.find("sailor")->get<Animator>();
			sailor->layers[0].behavior = Animator::Behavior::Freeze;
			sailor->layers[0].play(Asset::Animation::sailor_close_door);
		}

		entities.find("ivory_ad_trigger")->get<PlayerTrigger>()->entered.link(&ivory_ad_play);
		data->ivory_ad_text = entities.find("ivory_ad_text")->get<Transform>();
		data->hobo = entities.find("hobo");

		Actor::init();
		Loader::animation(Asset::Animation::hobo_idle);
		data->hobo_actor = Actor::add(data->hobo.ref(), Actor::Behavior::WaitForIdleAnimation, Asset::Bone::hobo_head);
		entities.find("hobo_trigger")->get<PlayerTrigger>()->entered.link(&hobo_talk);

		data->ivory_ad_actor = Actor::add(entities.find("ivory_ad"));
	}

	void play()
	{
		Game::save.reset();
		Game::session.reset();
		data->transition_timer = total_transition;
		Audio::post_global_event(AK::EVENTS::PLAY_TRANSITION_OUT);
	}
}

namespace tutorial
{
	enum class TutorialState : s8
	{
		ParkourStart,
		ParkourSlide,
		ParkourDone,
		Start,
		Upgrade,
		Ability,
		Turrets,
		Capture,
		ZoneCaptured,
		Overworld,
		Return,
		Done,
		count,
	};

	struct Data
	{
		TutorialState state;
		Ref<Entity> player;
		Ref<Entity> battery;
	};

	Data* data;

	void drone_target_hit(Entity* e)
	{
		if (data->state == TutorialState::Start && e == data->battery.ref())
		{
			data->state = TutorialState::Upgrade;
			Actor::tut(strings::tut_upgrade);

			Game::level.feature_level = Game::FeatureLevel::Abilities;
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			manager->energy = UpgradeInfo::list[s32(Upgrade::Sensor)].cost + AbilityInfo::list[s32(Ability::Sensor)].spawn_cost * 2;
		}
	}

	void ability_spawned(Ability)
	{
		if (data->state == TutorialState::Ability)
		{
			data->state = TutorialState::Turrets;
			Game::level.feature_level = Game::FeatureLevel::Turrets;
			Actor::tut(strings::tut_turrets);
		}
	}

	void update(const Update& u)
	{
		// check if the player has spawned
		if (!data->player.ref() && PlayerControlHuman::list.count() > 0)
		{
			Entity* player = PlayerControlHuman::list.iterator().item()->entity();
			if (player->has<Drone>())
			{
				data->player = player;
				player->get<Drone>()->ability_spawned.link(&ability_spawned);
				player->get<Drone>()->hit.link(&drone_target_hit);

				if (s32(data->state) <= s32(TutorialState::Start))
				{
					data->state = TutorialState::Start;
					Actor::tut(strings::tut_start);
					Game::level.feature_level = Game::FeatureLevel::Batteries;
				}
			}
			else
			{
				// parkour mode
				if (data->state == TutorialState::ZoneCaptured)
				{
					data->state = TutorialState::Overworld;
					Actor::tut(strings::tut_overworld, 4.5f);
				}
			}
		}

		if (data->state == TutorialState::Overworld && Overworld::active())
		{
			data->state = TutorialState::Return;
			Actor::tut_clear();
		}
		else if (data->state == TutorialState::Return && !Overworld::active())
		{
			data->state = TutorialState::Done;
			Actor::tut_clear();
			Actor::tut(strings::tut_done);
		}

		if (data->state != TutorialState::ZoneCaptured && Team::game_over && Game::level.mode == Game::Mode::Pvp)
		{
			data->state = TutorialState::ZoneCaptured;
			Actor::tut_clear();
		}
		else if (data->state == TutorialState::Upgrade)
		{
			if (PlayerHuman::list.count() > 0)
			{
				PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
				for (s32 i = 0; i < s32(Upgrade::count); i++)
				{
					if (manager->has_upgrade(Upgrade(i)))
					{
						data->state = TutorialState::Ability;
						Actor::tut(strings::tut_ability);
						Game::level.feature_level = Game::FeatureLevel::Abilities;
						break;
					}
				}
			}
		}
		else if (data->state == TutorialState::Turrets)
		{
			if (Turret::list.count() == 0)
			{
				data->state = TutorialState::Capture;
				Actor::tut(strings::tut_capture);
			}
		}
	}

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void init(const EntityFinder& entities)
	{
		Actor::init();

		vi_assert(!data);
		data = new Data();

		data->battery = entities.find("battery");

		if (Game::level.mode == Game::Mode::Pvp)
			data->state = TutorialState::Start;
		else
			data->state = TutorialState::ParkourStart;

		Game::level.feature_level = Game::FeatureLevel::Batteries;

		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
	}
}

namespace locke
{
	struct Data
	{
		Actor::Instance* locke;
		b8 spoken;
	};

	Data* data;
	
	void update(const Update& u)
	{
		if (data->locke->cues.length == 0)
		{
			Animator::Layer* layer0 = &data->locke->model.ref()->get<Animator>()->layers[0];
			if (layer0->animation == AssetNull || layer0->time == Loader::animation(layer0->animation)->duration)
			{
				const s32 idle_anim_count = 1;
				const AssetID idle_anims[idle_anim_count] =
				{
					Asset::Animation::locke_shift_weight,
				};
				if (mersenne::rand() % 4 == 0)
					layer0->play(idle_anims[mersenne::rand() % idle_anim_count]);
				else
					layer0->play(Asset::Animation::locke_idle);
			}
		}
	}

	void trigger(Entity*)
	{
		if (Game::level.mode == Game::Mode::Parkour
			&& !data->spoken)
		{
			if (!Game::save.locke_spoken)
			{
				Game::save.locke_index++; // locke_index starts at -1
				if (Game::save.locke_index == 7)
					Game::save.locke_index = 1; // skip the first one, which is the intro to Locke
			}
			Game::save.locke_spoken = true;
			data->spoken = true;
			switch (Game::save.locke_index)
			{
				case 0:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE1A, Asset::Animation::locke_gesture_one_hand_short, strings::locke1a, false);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE1B, Asset::Animation::locke_shift_weight, strings::locke1b, false);
					break;
				}
				case 1:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE2A, Asset::Animation::locke_idle, strings::locke2a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE2B, Asset::Animation::locke_idle, strings::locke2b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE2C, Asset::Animation::locke_idle, strings::locke2c);
					break;
				}
				case 2:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE3A, Asset::Animation::locke_idle, strings::locke3a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE3B, Asset::Animation::locke_idle, strings::locke3b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE3C, Asset::Animation::locke_idle, strings::locke3c);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE3D, Asset::Animation::locke_idle, strings::locke3d);
					break;
				}
				case 3:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE4A, Asset::Animation::locke_idle, strings::locke4a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE4B, Asset::Animation::locke_idle, strings::locke4b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE4C, Asset::Animation::locke_idle, strings::locke4c);
					break;
				}
				case 4:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE5A, Asset::Animation::locke_idle, strings::locke5a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE5B, Asset::Animation::locke_idle, strings::locke5b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE5C, Asset::Animation::locke_idle, strings::locke5c);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE5D, Asset::Animation::locke_idle, strings::locke5d);
					break;
				}
				case 5:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE6A, Asset::Animation::locke_idle, strings::locke6a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE6B, Asset::Animation::locke_idle, strings::locke6b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE6C, Asset::Animation::locke_idle, strings::locke6c);
					break;
				}
				case 6:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE7A, Asset::Animation::locke_idle, strings::locke7a);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE7B, Asset::Animation::locke_idle, strings::locke7b);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE7C, Asset::Animation::locke_idle, strings::locke7c);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE7D, Asset::Animation::locke_idle, strings::locke7d);
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

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void init(const EntityFinder& entities)
	{
		vi_assert(!data);
		data = new Data();
		Entity* locke = entities.find("locke");
		locke->get<PlayerTrigger>()->entered.link(&trigger);
		data->locke = Actor::add(locke, Actor::Behavior::InterruptIdleAnimation, Asset::Bone::locke_head);
		Loader::animation(Asset::Animation::locke_gesture_both_arms);
		Loader::animation(Asset::Animation::locke_gesture_one_hand);
		Loader::animation(Asset::Animation::locke_gesture_one_hand_short);
		Loader::animation(Asset::Animation::locke_idle);
		Loader::animation(Asset::Animation::locke_shift_weight);
		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
	}
}


}

Script Script::list[] =
{
	{ "scene", Scripts::scene::init },
	{ "tutorial", Scripts::tutorial::init },
	{ "title", Scripts::title::init },
	{ "locke", Scripts::locke::init },
	{ 0, 0, },
};
s32 Script::count; // set in Game::init

}