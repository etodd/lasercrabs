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
#include "settings.h"
#include "asset/texture.h"
#include "render/skinned_model.h"
#include "data/animator.h"
#include "input.h"

namespace VI
{

namespace Actor
{

// animated, voiced, scripted character

struct Instance;

typedef void (*Callback)(Instance*);

enum class Loop : s8
{
	No,
	Yes,
	count,
};

enum class Overlap : s8
{
	No,
	Yes,
	count,
};

struct Cue
{
	Callback callback;
	r32 delay;
	AkUniqueID sound = AK_InvalidID;
	AssetID animation = AssetNull;
	AssetID text = AssetNull;
	Loop loop;
	Overlap overlap;
};

enum class IdleBehavior : s8
{
	Wait,
	Interrupt,
	count,
};

struct Instance
{
	static PinArray<Instance, 8> list;

	StaticArray<Cue, 32> cues;
	r32 last_cue_real_time;
	r32 dialogue_radius;
	IdleBehavior idle_behavior;
	Ref<Entity> model;
	Ref<Transform> collision;
	AssetID text;
	AssetID head_bone;
	Revision revision;
	b8 highlight;
	b8 sound_done;
	b8 overlap_animation;

	inline ID id() const
	{
		return this - &list[0];
	}

	Instance()
		: cues(),
		last_cue_real_time(),
		idle_behavior(),
		dialogue_radius(8.0f),
		model(),
		collision(),
		text(AssetNull),
		head_bone(AssetNull),
		highlight(),
		sound_done(),
		overlap_animation()
	{
	}

	void cue(AkUniqueID sound, AssetID animation, AssetID text = AssetNull, Loop loop = Loop::Yes, Overlap overlap = Overlap::No, r32 delay = 0.3f)
	{
		if (idle_behavior == IdleBehavior::Interrupt && model.ref()->has<Animator>() && cues.length == 0)
			overlap_animation = true;

		Cue* c = cues.add();
		new (c) Cue();
		c->sound = sound;
		c->animation = animation;
		c->delay = delay;
		c->loop = loop;
		c->overlap = overlap;
		c->text = text;
	}

	void cue(Callback callback, r32 delay = 0.3f)
	{
		if (idle_behavior == IdleBehavior::Interrupt && model.ref()->has<Animator>() && cues.length == 0)
			overlap_animation = true;

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

			if ((!layer
				|| layer->animation == AssetNull
				|| layer->behavior == Animator::Behavior::Loop
				|| layer->time == Loader::animation(layer->animation)->duration
				|| instance->overlap_animation)
				&& instance->sound_done
				&& instance->cues.length > 0)
			{
				Cue* cue = &instance->cues[0];
				if (cue->delay > 0.0f)
					cue->delay -= u.time.delta;
				else
				{
					if (cue->callback)
						cue->callback(instance);
					else
					{
						instance->last_cue_real_time = Game::real_time.total;
						instance->text = cue->text;
						if (layer)
						{
							if (cue->animation != AssetNull)
							{
								instance->overlap_animation = cue->overlap == Overlap::Yes ? true : false;
								layer->behavior = cue->loop == Loop::Yes ? Animator::Behavior::Loop : Animator::Behavior::Freeze;
								layer->play(cue->animation);
							}
						}
						else
							vi_assert(cue->animation == AssetNull);

						if (cue->sound == AK_InvalidID)
							instance->sound_done = true;
						else
						{
							instance->sound_done = false;
							instance->model.ref()->get<Audio>()->post_dialogue(cue->sound);
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

void draw_ui(const RenderParams& params)
{
	if (!data)
		return;

	if (!Overworld::active())
	{
		for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
		{
			const Instance& instance = *i.item();

			Vec3 actor_pos = Vec3::zero;
			if (instance.model.ref())
			{
				if (instance.head_bone == AssetNull)
					actor_pos = instance.model.ref()->get<Transform>()->absolute_pos();
				else
					instance.model.ref()->get<Animator>()->to_world(instance.head_bone, &actor_pos);

				if (Settings::waypoints && instance.highlight)
					UI::indicator(params, actor_pos + Vec3(0, -0.4f, 0), UI::color_accent(), true);
			}

			if (Settings::subtitles
				&& instance.text != AssetNull
				&& (instance.highlight || instance.dialogue_radius == 0.0f || (actor_pos - params.camera->pos).length_squared() < instance.dialogue_radius * instance.dialogue_radius))
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
			text.color = UI::color_accent();
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
		Game::draws.add(draw_ui);
	}
}

Instance* add(Entity* model, AssetID head_bone = AssetNull, IdleBehavior idle_behavior = IdleBehavior::Wait)
{
	init();

	Instance* i = Instance::list.add();
	new (i) Instance();

	i->revision++;
	i->idle_behavior = idle_behavior;
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

b8 any_input(const Update& u)
{
	if (u.last_input->keys.any() && !u.input->keys.any())
		return true;
	else if (u.last_input->gamepads[0].btns && !u.input->gamepads[0].btns)
		return true;
	return false;
}

namespace splash
{

	struct Data
	{
		r32 timer = 2.0f;
		Ref<Camera> camera;
	};
	Data* data;

	void update(const Update& u)
	{
		data->timer -= u.real_time.delta;

		if (Game::schedule_timer == 0.0f
			&& (data->timer < 0.0f || any_input(u)))
			Menu::title();

		data->camera.ref()->viewport =
		{
			Vec2(0, 0),
			Vec2(Settings::display().width, Settings::display().height),
		};
		const r32 fov = 40.0f * PI * 0.5f / 180.0f;
		data->camera.ref()->perspective(fov, 0.1f, Game::level.skybox.far_plane);
	}

	void draw_ui(const RenderParams& params)
	{
		if (params.camera == data->camera.ref())
		{
			const Rect2& vp = params.camera->viewport;
			UI::mesh(params, Asset::Mesh::helvetica_scenario, vp.size * 0.5f, Vec2(vp.size.x * 0.25f));

			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.text(0, _(strings::presents));
			text.draw(params, vp.size * Vec2(0.5f, 0.4f));
		}
	}

	void cleanup()
	{
		data->camera.ref()->remove();
		delete data;
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();
		data->camera = Camera::add(0);
		Game::draws.add(draw_ui);
		Game::updates.add(update);
	}
}

namespace Docks
{
	const r32 start_fov = 40.0f * PI * 0.5f / 180.0f;
	const r32 end_fov = 70.0f * PI * 0.5f / 180.0f;
	const r32 total_transition = TRANSITION_TIME + 0.5f;

	enum class TutorialState : s8
	{
		Start,
		DadaSpotted,
		DadaTalking,
		Jump,
		Climb,
		Done,
		count,
	};

	struct Data
	{
		Ref<Camera> camera;
		Actor::Instance* dada;
		Quat barge_base_rot;
		Vec3 barge_base_pos;
		Vec3 camera_start_pos;
		r32 transition_timer;
		Ref<Animator> character;
		Ref<Transform> ivory_ad_text;
		Ref<Actor::Instance> ivory_ad_actor;
		Ref<Actor::Instance> hobo_actor;
		Ref<Entity> hobo;
		Ref<Transform> barge;
		TutorialState state;
		b8 dada_talked;
	};
	
	static Data* data;

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void jump_trigger(Entity*)
	{
		if (data->state == TutorialState::Start
			|| data->state == TutorialState::DadaSpotted
			|| data->state == TutorialState::DadaTalking)
		{
			data->state = TutorialState::Jump;
			Actor::tut(strings::tut_jump, 0.0f);
		}
	}

	void climb_trigger(Entity*)
	{
		if (data->state == TutorialState::Jump)
		{
			data->state = TutorialState::Climb;
			Actor::tut(strings::tut_climb, 0.0f);
		}
	}

	void climb_success_trigger(Entity*)
	{
		if (data->state == TutorialState::Climb)
		{
			data->state = TutorialState::Done;
			Actor::tut_clear();
		}
	}

	void dada_spotted(Entity*)
	{
		if (data->state == TutorialState::Start)
		{
			data->state = TutorialState::DadaSpotted;
			data->dada->cue(AK::EVENTS::PLAY_DADA01, Asset::Animation::dada_wait, strings::dada01);
		}
	}

	void dada_talk(Entity*)
	{
		if (data->state == TutorialState::DadaSpotted)
			data->state = TutorialState::DadaTalking;

		if (!data->dada_talked)
		{
			data->dada_talked = true;
			data->dada->highlight = false;
			data->dada->cue(AK::EVENTS::PLAY_DADA02, Asset::Animation::dada_talk, strings::dada02);
			data->dada->cue(AK::EVENTS::PLAY_DADA03, Asset::Animation::dada_talk, strings::dada03);
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_close_door, AssetNull, Actor::Loop::No);
			data->dada->cue(&Actor::done, 0.0f);
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
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD01, AssetNull, strings::ivory_ad01);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD02, AssetNull, strings::ivory_ad02);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD03, AssetNull, strings::ivory_ad03);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD04, AssetNull, strings::ivory_ad04);
			data->ivory_ad_actor.ref()->cue(AK::EVENTS::PLAY_IVORY_AD05, AssetNull, strings::ivory_ad05);
		}
	}

	void update_title(const Update& u)
	{
		if (data->camera.ref())
		{
			Vec3 head_pos = Vec3::zero;
			data->character.ref()->to_world(Asset::Bone::character_head, &head_pos);
			r32 blend = data->transition_timer > 0.0f ? vi_min(1.0f, total_transition - data->transition_timer) : 0.0f;
			data->camera.ref()->pos = Vec3::lerp(blend, data->camera_start_pos, head_pos);

			data->camera.ref()->viewport =
			{
				Vec2(0, 0),
				Vec2(Settings::display().width, Settings::display().height),
			};
			data->camera.ref()->perspective(LMath::lerpf(blend * 0.5f, start_fov, end_fov), 0.1f, Game::level.skybox.far_plane);

			if (Game::level.mode == Game::Mode::Special && !Overworld::active() && !Overworld::transitioning())
				Menu::title_menu(u, data->camera.ref());
		}

		if (data->transition_timer > 0.0f)
		{
			r32 old_timer = data->transition_timer;
			data->transition_timer = vi_max(0.0f, data->transition_timer - Game::real_time.delta);
			if (data->transition_timer < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
			{
				Audio::post_global(AK::EVENTS::PLAY_TRANSITION_IN);
				data->camera.ref()->remove();
				data->camera = nullptr;
				World::remove(data->character.ref()->entity());
				Game::level.mode = Game::Mode::Parkour;
				data->dada->highlight = true;
				for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				{
					i.item()->get<PlayerManager>()->can_spawn = true;
					i.item()->camera.ref()->flag(CameraFlagActive, true);
				}
			}
		}
		else
		{
			if (Game::level.mode == Game::Mode::Special
				&& Menu::main_menu_state == Menu::State::Hidden
				&& Game::scheduled_load_level == AssetNull
				&& !Overworld::active() && !Overworld::transitioning())
			{
				if (Game::session.type == SessionType::Multiplayer)
				{
					Overworld::show(data->camera.ref(), Overworld::State::Multiplayer);
					Overworld::skip_transition();
				}
				else if (any_input(u))
					Menu::show();
			}
		}
	}

	void draw_ui(const RenderParams& p)
	{
		Loader::texture(Asset::Texture::logo);

		if (Game::level.mode == Game::Mode::Special
			&& Game::scheduled_load_level == AssetNull
			&& data->transition_timer == 0.0f
			&& !Overworld::active()
			&& Game::session.type == SessionType::Story
			&& !Menu::dialog_active(0))
		{
			Rect2 logo_rect;
			if (Menu::main_menu_state == Menu::State::Hidden)
			{
				UIText text;
				text.color = UI::color_accent();
				text.text(0, "[{{Start}}]");
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Center;
				Vec2 pos = p.camera->viewport.size * Vec2(0.5f, 0.1f);
				UI::box(p, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
				text.draw(p, pos);

				Vec2 size(p.camera->viewport.size.x * 0.25f);
				logo_rect = { p.camera->viewport.size * 0.5f + size * Vec2(-0.5f, -0.5f), size };
			}
			else
			{
				Vec2 menu_pos(p.camera->viewport.size.x * 0.5f, p.camera->viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f);
				Vec2 size((MENU_ITEM_WIDTH + MENU_ITEM_PADDING * -2.0f) * 0.3f);
				logo_rect = { menu_pos + size * Vec2(-0.5f, 0.0f) + Vec2(0.0f, MENU_ITEM_PADDING * 3.0f), size };
			}
			UI::sprite(p, Asset::Texture::logo, { logo_rect.pos + logo_rect.size * 0.5f, logo_rect.size });
		}

		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void update(const Update& u)
	{
		// bob the barge
		r32 t = u.time.total * (1.0f / 5.0f);
		data->barge.ref()->absolute_pos(data->barge_base_pos + Vec3(0, sinf(t) * 0.2f, 0));
		data->barge.ref()->absolute_rot(data->barge_base_rot * Quat::euler(cosf(t) * 0.02f, 0, 0));

		// ivory ad text
		data->ivory_ad_text.ref()->rot *= Quat::euler(0, u.time.delta * 0.2f, 0);
	}

	void init(const EntityFinder& entities)
	{
		vi_assert(!data);
		data = new Data();
		Game::cleanups.add(cleanup);

#if !SERVER
		if (!Game::user_key.id)
			Net::Client::master_send_auth();
#endif

		if (Game::level.mode == Game::Mode::Special)
		{
			data->camera = Camera::add(0);

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&data->camera_start_pos, &rot);
			data->camera.ref()->pos = data->camera_start_pos;
			data->camera.ref()->rot = Quat::look(rot * Vec3(0, -1, 0));

			data->character = entities.find("character")->get<Animator>();

			for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
				i.item()->camera.ref()->flag(CameraFlagActive, false);
		}
		else if (Game::level.local)
			World::remove(entities.find("character"));

		if ((Game::save.zone_last == AssetNull || Game::save.zone_last == Asset::Level::Docks)
			&& entities.find("drones"))
		{
			entities.find("jump_trigger")->get<PlayerTrigger>()->entered.link(&jump_trigger);
			entities.find("climb_trigger")->get<PlayerTrigger>()->entered.link(&climb_trigger);
			entities.find("climb_trigger2")->get<PlayerTrigger>()->entered.link(&climb_trigger);
			entities.find("climb_success_trigger")->get<PlayerTrigger>()->entered.link(&climb_success_trigger);
			entities.find("climb_success_trigger2")->get<PlayerTrigger>()->entered.link(&climb_success_trigger);
			entities.find("dada_spotted_trigger")->get<PlayerTrigger>()->entered.link(&dada_spotted);
			entities.find("dada_talk_trigger")->get<PlayerTrigger>()->entered.link(&dada_talk);

			Game::updates.add(update_title);
			Game::draws.add(draw_ui);

			data->dada = Actor::add(entities.find("dada"), Asset::Bone::dada_head);
			Loader::animation(Asset::Animation::dada_close_door);
			Loader::animation(Asset::Animation::dada_wait);
			Loader::animation(Asset::Animation::dada_talk);
			if (Game::level.mode != Game::Mode::Special)
				data->dada->highlight = true;
		}
		else
		{
			Animator* dada = entities.find("dada")->get<Animator>();
			dada->layers[0].behavior = Animator::Behavior::Freeze;
			dada->layers[0].play(Asset::Animation::dada_close_door);
		}

		entities.find("ivory_ad_trigger")->get<PlayerTrigger>()->entered.link(&ivory_ad_play);
		data->ivory_ad_text = entities.find("ivory_ad_text")->get<Transform>();
		data->hobo = entities.find("hobo");

		Actor::init();
		Loader::animation(Asset::Animation::hobo_idle);
		data->hobo_actor = Actor::add(data->hobo.ref(), Asset::Bone::hobo_head);
		entities.find("hobo_trigger")->get<PlayerTrigger>()->entered.link(&hobo_talk);

		data->ivory_ad_actor = Actor::add(entities.find("ivory_ad"));
		data->ivory_ad_actor.ref()->dialogue_radius = 0.0f;

		data->barge = entities.find("barge")->get<Transform>();
		data->barge.ref()->absolute(&data->barge_base_pos, &data->barge_base_rot);

		Game::updates.add(update);
	}

	void play()
	{
		Game::save.reset();
		Game::session.reset(SessionType::Story);
		data->transition_timer = total_transition;
		Audio::post_global(AK::EVENTS::PLAY_TRANSITION_OUT);
	}
}

namespace tutorial
{
	enum class TutorialState : s8
	{
		None,
		WallRun,
		WallRunDone,
		Crawl,
		Battery,
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
		r32 wallrun_footstep_timer;
		s32 wallrun_footstep_index;
		TutorialState state;
		Ref<Entity> player;
		Ref<Entity> battery;
		Ref<Transform> crawl_target;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps;
	};

	Data* data;

	void wallrun_start(Entity*)
	{
		if (Game::level.mode == Game::Mode::Parkour && data->state == TutorialState::None)
		{
			Actor::tut(strings::tut_wallrun, 0.0f);
			data->state = TutorialState::WallRun;
		}
	}

	void wallrun_success(Entity*)
	{
		if (data->state == TutorialState::WallRun)
		{
			Actor::tut_clear();
			data->state = TutorialState::WallRunDone;
		}
	}

	void drone_target_hit(Entity* e)
	{
		if ((data->state == TutorialState::Crawl || data->state == TutorialState::Battery) && e == data->battery.ref())
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

	void crawl_complete(Entity*)
	{
		if (data->state == TutorialState::Crawl)
		{
			data->state = TutorialState::Battery;
			Actor::tut(strings::tut_battery);
			Game::level.feature_level = Game::FeatureLevel::Batteries;
		}
	}

	void draw_ui(const RenderParams& params)
	{
		if (data->state == TutorialState::Crawl)
			UI::indicator(params, data->crawl_target.ref()->absolute_pos(), UI::color_accent(), true);
	}

	void update(const Update& u)
	{
		// check if the player has spawned
		if (!data->player.ref() && PlayerControlHuman::list.count() > 0)
		{
			Entity* player = PlayerControlHuman::list.iterator().item()->entity();
			data->player = player;
			if (player->has<Drone>())
			{
				player->get<Drone>()->ability_spawned.link(&ability_spawned);
				player->get<Drone>()->hit.link(&drone_target_hit);

				if (s32(data->state) <= s32(TutorialState::Crawl))
				{
					data->state = TutorialState::Crawl;
					Actor::tut(strings::tut_crawl);
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
		else if (data->state == TutorialState::Overworld && Overworld::active())
		{
			data->state = TutorialState::Return;
			Actor::tut_clear();
		}
		else if (data->state == TutorialState::Return && !Overworld::active())
		{
			data->state = TutorialState::Done;
			Actor::tut(strings::tut_done);
		}

		if (data->state != TutorialState::ZoneCaptured && Team::match_state == Team::MatchState::Done && Game::level.mode == Game::Mode::Pvp)
		{
			data->state = TutorialState::ZoneCaptured;
			Actor::tut_clear();
		}
		else if (data->state == TutorialState::Upgrade)
		{
			if (PlayerHuman::list.count() > 0)
			{
				PlayerHuman* human = PlayerHuman::list.iterator().item();
				PlayerManager* manager = human->get<PlayerManager>();
				if (manager->upgrades)
				{
					Actor::tut_clear();
					if (human->ui_mode() != PlayerHuman::UIMode::PvpUpgrading)
					{
						data->state = TutorialState::Ability;
						Actor::tut(strings::tut_ability, 0.5f);
						Game::level.feature_level = Game::FeatureLevel::Abilities;
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

		data->wallrun_footsteps.add(entities.find("wallrun_footstep")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.001")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.002")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.003")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.004")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.005")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.006")->get<Transform>());
		data->wallrun_footsteps.add(entities.find("wallrun_footstep.007")->get<Transform>());

		data->crawl_target = entities.find("crawl_target")->get<Transform>();
		entities.find("crawl_trigger")->get<PlayerTrigger>()->entered.link(&crawl_complete);
		entities.find("wallrun_trigger")->get<PlayerTrigger>()->entered.link(&wallrun_start);
		entities.find("wallrun_success")->get<PlayerTrigger>()->entered.link(&wallrun_success);

		Game::level.feature_level = Game::FeatureLevel::Base;

		Game::updates.add(&update);
		Game::draws.insert(0, &draw_ui);
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
				if (Game::save.locke_index == 8)
					Game::save.locke_index = 2; // skip the first two
			}
			Game::save.locke_spoken = true;
			data->spoken = true;
			switch (Game::save.locke_index)
			{
				case 0:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_A01, Asset::Animation::locke_gesture_one_hand_short, strings::locke_a01, Actor::Loop::No);
					break;
				}
				case 1:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_B01, Asset::Animation::locke_gesture_one_hand_short, strings::locke_b01, Actor::Loop::No);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_B02, Asset::Animation::locke_shift_weight, strings::locke_b02, Actor::Loop::No);
					break;
				}
				case 2:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_C01, Asset::Animation::locke_idle, strings::locke_c01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_C02, Asset::Animation::locke_idle, strings::locke_c02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_C03, Asset::Animation::locke_idle, strings::locke_c03);
					break;
				}
				case 3:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_D01, Asset::Animation::locke_idle, strings::locke_d01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_D02, Asset::Animation::locke_idle, strings::locke_d02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_D03, Asset::Animation::locke_idle, strings::locke_d03);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_D04, Asset::Animation::locke_idle, strings::locke_d04);
					break;
				}
				case 4:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_E01, Asset::Animation::locke_idle, strings::locke_e01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_E02, Asset::Animation::locke_idle, strings::locke_e02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_E03, Asset::Animation::locke_idle, strings::locke_e03);
					break;
				}
				case 5:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_F01, Asset::Animation::locke_idle, strings::locke_f01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_F02, Asset::Animation::locke_idle, strings::locke_f02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_F03, Asset::Animation::locke_idle, strings::locke_f03);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_F04, Asset::Animation::locke_idle, strings::locke_f04);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_F05, Asset::Animation::locke_idle, strings::locke_f05);
					break;
				}
				case 6:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_G01, Asset::Animation::locke_idle, strings::locke_g01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_G02, Asset::Animation::locke_idle, strings::locke_g02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_G03, Asset::Animation::locke_idle, strings::locke_g03);
					break;
				}
				case 7:
				{
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_H01, Asset::Animation::locke_idle, strings::locke_h01);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_H02, Asset::Animation::locke_idle, strings::locke_h02);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_H03, Asset::Animation::locke_idle, strings::locke_h03);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_H04, Asset::Animation::locke_idle, strings::locke_h04);
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
		data->locke = Actor::add(locke, Asset::Bone::locke_head, Actor::IdleBehavior::Interrupt);
		Loader::animation(Asset::Animation::locke_gesture_both_arms);
		Loader::animation(Asset::Animation::locke_gesture_one_hand);
		Loader::animation(Asset::Animation::locke_gesture_one_hand_short);
		Loader::animation(Asset::Animation::locke_idle);
		Loader::animation(Asset::Animation::locke_shift_weight);
		Game::updates.add(&update);
		Game::cleanups.add(&cleanup);
	}
}

namespace tier_1
{
	struct Data
	{
	};
	Data* data;

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(&cleanup);
	}
}

namespace tier_2
{
	struct Data
	{
		Actor::Instance* meursault;
		Ref<Entity> anim_base;
		Ref<Entity> hobo;
		Ref<Entity> terminal;
		Ref<Entity> drone;
		b8 anim_played;
		b8 drones_given;
	};
	Data* data;

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	b8 net_msg(Net::StreamRead* p, Net::MessageSource src)
	{
		using Stream = Net::StreamRead;
		if (!data->drones_given)
		{
			if (Game::level.local)
				Overworld::resource_change(Resource::Drones, 2);
			data->drones_given = true;
		}
		return true;
	}

	b8 give_drones()
	{
		using Stream = Net::StreamWrite;
		Stream* p = Script::net_msg_new(net_msg);
		Net::msg_finalize(p);
		return true;
	}

	void give_drones_animation_callback(Actor::Instance*)
	{
		give_drones();
	}

	void trigger(Entity* player)
	{
#if !SERVER
		if (!data->anim_played)
		{
			data->anim_played = true;

			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_ARROW, Asset::Animation::meursault_intro, strings::meursault_arrow, Actor::Loop::No, Actor::Overlap::Yes, 0.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A01, AssetNull, strings::meursault_a01, Actor::Loop::No, Actor::Overlap::No, 3.6f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A02, AssetNull, strings::meursault_a02, Actor::Loop::No, Actor::Overlap::No, 3.6f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A03, AssetNull, strings::meursault_a03, Actor::Loop::No, Actor::Overlap::No, 5.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A04, AssetNull, strings::meursault_a04, Actor::Loop::No, Actor::Overlap::No, 4.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A05, AssetNull, strings::meursault_a05, Actor::Loop::No, Actor::Overlap::No, 2.5f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A06, AssetNull, strings::meursault_a06, Actor::Loop::No, Actor::Overlap::No, 1.5f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A07, AssetNull, strings::meursault_a07, Actor::Loop::No, Actor::Overlap::No, 2.8f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A08, AssetNull, strings::meursault_a08, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A09, AssetNull, strings::meursault_a09, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A10, AssetNull, strings::meursault_a10, Actor::Loop::No, Actor::Overlap::No, 5.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A11, AssetNull, strings::meursault_a11, Actor::Loop::No, Actor::Overlap::No, 2.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A12, AssetNull, strings::meursault_a12, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A13, AssetNull, strings::meursault_a13, Actor::Loop::No, Actor::Overlap::No, 3.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A14, AssetNull, strings::meursault_a14, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(&give_drones_animation_callback, 0.0f);
			data->meursault->cue(AK::EVENTS::PLAY_MEURSAULT_A15, AssetNull, strings::meursault_a15, Actor::Loop::No, Actor::Overlap::No, 2.0f);

			player->get<PlayerControlHuman>()->cinematic(data->anim_base.ref(), Asset::Animation::character_meursault_intro);
		}
#endif
	}

	void update(const Update& u)
	{
		if (u.input->keys.get(s32(KeyCode::D1)) && !u.last_input->keys.get(s32(KeyCode::D1)))
			data->hobo.ref()->get<Animator>()->layers[0].play(Asset::Animation::hobo_trailer1);
		if (u.input->keys.get(s32(KeyCode::D2)) && !u.last_input->keys.get(s32(KeyCode::D2)))
			data->hobo.ref()->get<Animator>()->layers[0].play(Asset::Animation::hobo_trailer2);
		if (u.input->keys.get(s32(KeyCode::D3)) && !u.last_input->keys.get(s32(KeyCode::D3)))
		{
			data->hobo.ref()->get<Animator>()->layers[0].play(Asset::Animation::hobo_trailer3);
			data->terminal.ref()->get<Animator>()->layers[0].play(Asset::Animation::terminal_trailer3_terminal);
		}
		if (u.input->keys.get(s32(KeyCode::D4)) && !u.last_input->keys.get(s32(KeyCode::D4)))
		{
			data->hobo.ref()->get<Animator>()->layers[0].play(Asset::Animation::hobo_trailer4);
			data->drone.ref()->get<Animator>()->layers[0].play(Asset::Animation::drone_trailer4_drone);
		}
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(&cleanup);
		Game::updates.add(&update);

		data->meursault = Actor::add(entities.find("meursault"), Asset::Bone::meursault_head, Actor::IdleBehavior::Interrupt);

		entities.find("trigger")->get<PlayerTrigger>()->entered.link(&trigger);
		data->anim_base = entities.find("player_anim");

		data->hobo = World::create<Prop>(Asset::Mesh::hobo, Asset::Armature::hobo);
		data->hobo.ref()->get<SkinnedModel>()->radius = 1000.0f;
		data->hobo.ref()->get<Animator>()->layers[0].blend_time = 0.0f;
		data->hobo.ref()->get<Animator>()->layers[0].behavior = Animator::Behavior::Default;

		data->terminal = World::create<Prop>(Asset::Mesh::terminal, Asset::Armature::terminal);
		data->terminal.ref()->get<SkinnedModel>()->radius = 1000.0f;
		data->terminal.ref()->get<Animator>()->layers[0].blend_time = 0.0f;
		data->terminal.ref()->get<Animator>()->layers[0].behavior = Animator::Behavior::Default;

		data->drone = World::create<Prop>(Asset::Mesh::drone, Asset::Armature::drone);
		data->drone.ref()->get<SkinnedModel>()->radius = 1000.0f;
		data->drone.ref()->get<Animator>()->layers[0].blend_time = 0.0f;
		data->drone.ref()->get<Animator>()->layers[0].behavior = Animator::Behavior::Default;

		Loader::animation(Asset::Animation::character_meursault_intro);
		Loader::animation(Asset::Animation::meursault_intro);
	}
}


}

Script Script::list[] =
{
	{ "splash", Scripts::splash::init, nullptr, },
	{ "tutorial", Scripts::tutorial::init, nullptr, },
	{ "Docks", Scripts::Docks::init, nullptr, },
	{ "locke", Scripts::locke::init, nullptr, },
	{ "tier_1", Scripts::tier_1::init, nullptr },
	{ "tier_2", Scripts::tier_2::init, Scripts::tier_2::net_msg, },
	{ 0, 0, },
};
s32 Script::count; // set in Game::init

b8 net_msg_init(Net::StreamWrite* p, s32 script_id)
{
	using Stream = Net::StreamWrite;
	serialize_int(p, s32, script_id, 0, Script::count);
	return true;
}

Net::StreamWrite* Script::net_msg_new(NetMsgFunction callback)
{
	using Stream = Net::StreamWrite;

	s32 script_id = -1;
	for (s32 i = 0; i < count; i++)
	{
		if (list[i].net_callback == callback)
		{
			script_id = i;
			break;
		}
	}
	vi_assert(script_id != -1);

	Stream* p = Net::msg_new(Net::MessageType::Script);
	net_msg_init(p, script_id);
	return p;
}

b8 Script::net_msg(Net::StreamRead* p, Net::MessageSource src)
{
	using Stream = Net::StreamRead;
	s32 script_id;
	serialize_int(p, s32, script_id, 0, count);
	const Script& script = list[script_id];
	if (!script.net_callback || !script.net_callback(p, src))
		net_error();
	return true;
}

}
