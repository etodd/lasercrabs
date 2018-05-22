#include "localization.h"

#include "scripts.h"
#include "net.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "console.h"
#include <unordered_map>
#include <string>
#include "minion.h"
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
#include "render/particles.h"
#include "data/animator.h"
#include "input.h"
#include "ease.h"
#include "parkour.h"
#include "noise.h"
#include "data/unicode.h"

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

	Callback interacted;
	StaticArray<Cue, 32> cues;
	r32 last_cue_real_time;
	r32 dialogue_radius;
	IdleBehavior idle_behavior;
	Ref<Entity> model;
	Ref<Transform> collision;
	AssetID text;
	AssetID head_bone;
	AkUniqueID sound_current;
	Revision revision;
	b8 highlight;
	b8 sound_done;
	b8 overlap_animation;

	inline ID id() const
	{
		return ID(this - &list[0]);
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
	
	Vec3 pos() const
	{
		if (head_bone == AssetNull)
			return model.ref()->get<Transform>()->absolute_pos();
		else
		{
			Vec3 p;
			model.ref()->get<Animator>()->to_world(head_bone, &p);
			return p;
		}
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

	void stop()
	{
		if (model.ref())
			model.ref()->get<Audio>()->stop(AK::EVENTS::STOP_DIALOGUE);
		else
			Audio::post_global(AK::EVENTS::STOP_DIALOGUE);
		sound_done = true;
		overlap_animation = true;
		cues.length = 0;
	}
};

PinArray<Instance, 8> Instance::list;

enum class TutMode : s8
{
	Left,
	Center,
	count,
};

struct Data
{
	r32 text_tut_real_time;
	AssetID text_tut = AssetNull;
	TutMode tut_mode;
};

static Data* data;

void cleanup()
{
	delete data;
	data = nullptr;
	Instance::list.clear();
}

void tut_clear()
{
	data->text_tut = AssetNull;
}

void tut(AssetID text, TutMode mode = TutMode::Left, r32 delay = 1.0f)
{
	tut_clear();
	data->text_tut = text;
	data->text_tut_real_time = Game::real_time.total + delay;
	data->tut_mode = mode;
}

b8 tut_active()
{
	return data->text_tut != AssetNull;
}

void done(Instance* i)
{
	i->highlight = false;
}

void remove(Instance*);

const r32 interact_distance = 3.5f;

void update(const Update& u)
{
	if (!data)
		return;

	for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
	{
		Instance* instance = i.item();

		if (!instance->sound_done)
		{
			if (instance->sound_current == AK_InvalidID)
			{
				// time to show dialogue depends on length of text
				if (instance->text == AssetNull)
					instance->sound_done = true;
				else
				{
					const char* text = _(instance->text);
					s32 length = Unicode::codepoint_count(text);
					instance->sound_done = Game::real_time.total - instance->last_cue_real_time > length * 0.075f;
				}
			}
			else
			{
				// time to show dialogue depends on audio
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
		}

		Animator::Layer* layer;
		if (instance->model.ref() && instance->model.ref()->has<Animator>())
			layer = &instance->model.ref()->get<Animator>()->layers[0];
		else
			layer = nullptr;

		if ((!layer
			|| layer->animation == AssetNull
			|| layer->behavior == Animator::Behavior::Loop
			|| layer->time == Loader::animation(layer->animation)->duration
			|| instance->overlap_animation)
			&& instance->sound_done)
		{
			if (instance->cues.length > 0)
			{
				Cue* cue = &instance->cues[0];
				if (cue->delay > 0.0f)
					cue->delay -= u.time.delta;
				else
				{
					instance->last_cue_real_time = Game::real_time.total;
					if (cue->callback)
						cue->callback(instance);
					else
					{
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

						instance->sound_done = false;
						instance->sound_current = cue->sound;
						if (cue->sound != AK_InvalidID)
						{
							if (instance->model.ref())
								instance->model.ref()->get<Audio>()->post_dialogue(cue->sound);
							else
								Audio::post_global_dialogue(cue->sound);
						}
					}

					if (data && instance->cues.length > 0) // callback might have called cleanup()
						instance->cues.remove_ordered(0);
				}
			}
			else // no cues left
				instance->text = AssetNull;
		}

		if (layer)
			instance->collision.ref()->pos = instance->collision_offset();

		if (instance->interacted)
		{
			if (u.last_input->get(Controls::InteractSecondary, 0) && !u.input->get(Controls::InteractSecondary, 0))
			{
				Vec3 camera_pos = PlayerHuman::list.iterator().item()->camera.ref()->pos;
				Vec3 actor_pos = instance->pos();
				r32 dist_sq = (actor_pos - camera_pos).length_squared();
				if (dist_sq < interact_distance * interact_distance)
				{
					Callback c = instance->interacted;
					instance->interacted = nullptr;
					c(instance);
				}
			}
		}
	}

	Audio::dialogue_callbacks.length = 0;
}

void draw_ui(const RenderParams& params)
{
	if (!data)
		return;

	for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
	{
		const Instance& instance = *i.item();

		Vec3 actor_pos = Vec3::zero;
		r32 dist_sq = 0.0f;
		if (instance.model.ref())
		{
			actor_pos = instance.pos();

			if (Settings::waypoints && instance.highlight)
				UI::indicator(params, actor_pos + Vec3(0, -0.4f, 0), UI::color_accent(), true);

			dist_sq = (actor_pos - params.camera->pos).length_squared();
			if (instance.interacted && dist_sq < interact_distance * interact_distance)
				UI::prompt_interact(params);
		}

		if (Settings::subtitles
			&& instance.text != AssetNull
			&& (instance.highlight || instance.dialogue_radius == 0.0f || dist_sq < instance.dialogue_radius * instance.dialogue_radius))
		{
			UIText text;
			text.font = Asset::Font::pt_sans;
			text.size = 18.0f;
			text.wrap_width = MENU_ITEM_WIDTH;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Min;
			text.color = UI::color_default;
			text.text(params.camera->gamepad, _(instance.text));
			UIMenu::text_clip(&text, params.camera->gamepad, instance.last_cue_real_time, 80.0f);

			{
				Vec2 p = params.camera->viewport.size * Vec2(0.5f, 0.25f);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
				text.draw(params, p);
			}
		}
	}

	if (!Overworld::active())
	{
		if (data->text_tut != AssetNull && Game::real_time.total > data->text_tut_real_time)
		{
			UIText text;
			text.color = UI::color_accent();

			Vec2 p;
			switch (data->tut_mode)
			{
				case TutMode::Center:
				{
					text.wrap_width = MENU_ITEM_WIDTH;
					text.anchor_x = UIText::Anchor::Center;
					text.anchor_y = UIText::Anchor::Max;
					p = params.camera->viewport.size * Vec2(0.5f, 0.8f);
					break;
				}
				case TutMode::Left:
				{
					text.wrap_width = MENU_ITEM_WIDTH * 0.5f;
					text.anchor_x = UIText::Anchor::Max;
					text.anchor_y = UIText::Anchor::Center;
					p = params.camera->viewport.size * Vec2(0.35f, 0.5f);
					break;
				}
				default:
				{
					p = Vec2::zero;
					vi_assert(false);
					break;
				}
			}

			text.text(params.camera->gamepad, _(data->text_tut));
			UIMenu::text_clip(&text, params.camera->gamepad, data->text_tut_real_time, 80.0f);

			UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::color_background);
			text.draw(params, p);
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

Instance* add(Entity* model = nullptr, AssetID head_bone = AssetNull, IdleBehavior idle_behavior = IdleBehavior::Wait)
{
	vi_assert(Game::level.local);
	init();

	Instance* i = Instance::list.add();
	new (i) Instance();

	i->revision++;
	i->idle_behavior = idle_behavior;
	i->model = model;
	i->head_bone = head_bone;
	i->sound_done = true;

	if (model)
	{
		if (!model->has<Audio>())
		{
			Audio* audio = model->add<Audio>();
			audio->entry()->flag(AudioEntry::FlagEnableObstructionOcclusion | AudioEntry::FlagEnableForceFieldObstruction, false);
		}

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
	const r32 splash_time = 2.0f;

	struct Data
	{
		r32 timer = splash_time;
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
			Vec2(r32(Settings::display().width), r32(Settings::display().height)),
		};
		const r32 fov = 40.0f * PI * 0.5f / 180.0f;
		data->camera.ref()->perspective(fov, 0.1f, Game::level.far_plane_get());
		data->camera.ref()->rot *= Quat::euler(0, Game::real_time.delta * -0.05f, 0);
	}

	void draw_ui(const RenderParams& params)
	{
		if (params.camera == data->camera.ref())
		{
			const Rect2& vp = params.camera->viewport;
			UI::mesh(params, Asset::Mesh::helvetica_scenario, vp.size * 0.5f, Vec2(vp.size.x * 0.25f * (1.0f - ((data->timer / splash_time) - 0.5f) * 0.2f)));

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
		DadaDone,
		Jump,
		Climb,
		Shop,
		WallRun,
		Done,
		count,
	};

	struct PhysicsSoundEntity
	{
		Ref<Entity> entity;
		r32 last_sound;
	};

	struct Data
	{
		Actor::Instance* dada;
		Actor::Instance* rex;
		Array<PhysicsSoundEntity> carts;
		r32 wallrun_footstep_timer;
		s32 wallrun_footstep_index;
		Vec3 camera_start_pos;
		Vec3 fire_start_pos;
		r32 transition_timer;
		r32 fire_accumulator;
		Ref<Camera> camera;
		Ref<Parkour> player;
		Ref<Animator> character;
		Ref<Transform> ivory_ad_text;
		Ref<Transform> fire;
		Ref<PlayerTrigger> wallrun_trigger_1;
		Ref<PlayerTrigger> wallrun_trigger_2;
		Ref<Animator> cutscene_parkour;
		Ref<Entity> energy;
		Ref<Transform> rex_cart;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps1;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps2;
		TutorialState state;
		b8 dada_talked;
		b8 rex_cart_gone;
		b8 demo_notified;
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
			|| data->state == TutorialState::DadaTalking
			|| data->state == TutorialState::DadaDone)
		{
			data->dada->highlight = false;
			data->state = TutorialState::Jump;
			Actor::tut(strings::tut_jump, Actor::TutMode::Left, 0.0f);
		}
	}

	void dada_spotted(Entity*)
	{
		if (data->state == TutorialState::Start)
		{
			data->state = TutorialState::DadaSpotted;
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_wait, strings::dada01);
		}
	}

	void dada_done(Actor::Instance*)
	{
		if (data->state == TutorialState::DadaTalking)
			data->state = TutorialState::DadaDone;
	}

	void dada_talk(Entity*)
	{
		if (data->state == TutorialState::DadaSpotted)
			data->state = TutorialState::DadaTalking;

		if (!data->dada_talked)
		{
			data->dada_talked = true;
			data->dada->highlight = false;
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_talk, strings::dada02);
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_talk, strings::dada03);
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_close_door, AssetNull, Actor::Loop::No);
			data->dada->cue(&dada_done, 0.0f);
			data->dada->cue(&Actor::done, 0.0f);
		}
	}

	void rex_speak2(Actor::Instance* rex)
	{
		if (!data->rex_cart_gone)
			rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_b01);
	}

	void rex_speak(Actor::Instance* rex)
	{
		rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_a01);
		rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_a02);
		rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_a03);
		rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_a04);
		rex->cue(rex_speak2);
	}

	void ivory_ad_play(Actor::Instance* ad)
	{
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad01);
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad02);
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad03);
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad04);
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad05);
		ad->cue(AK_InvalidID, AssetNull, strings::ivory_ad06);
		ad->cue(&ivory_ad_play, 4.0f);
	}

#if !RELEASE_BUILD
	void cutscene_update(const Update& u)
	{
		Camera* camera = Camera::list.iterator().item();
		camera->perspective(PI * 0.5f * 0.33f, 0.1f, Game::level.far_plane_get());
		if (data->cutscene_parkour.ref())
		{
			camera->pos = Vec3(0.15f, 0, 0);
			camera->rot = Quat::euler(PI * -0.5f, 0, 0);
			data->cutscene_parkour.ref()->to_world(Asset::Bone::parkour_head, &camera->pos, &camera->rot);
		}
	}

	void cutscene_init_common(AssetID rex_anim, AssetID parkour_anim)
	{
		Entity* rex = World::create<Prop>(Asset::Mesh::hobo, Asset::Armature::hobo, rex_anim);
		rex->get<Animator>()->layers[0].blend_time = 0.0f;

		if (parkour_anim != AssetNull)
		{
			Entity* parkour = World::create<Prop>(Asset::Mesh::parkour, Asset::Armature::parkour, parkour_anim);
			parkour->get<SkinnedModel>()->mesh_first_person = Asset::Mesh::parkour;
			parkour->get<SkinnedModel>()->first_person_camera = Camera::list.iterator().item();
			data->cutscene_parkour = parkour->get<Animator>();
			data->cutscene_parkour.ref()->layers[0].blend_time = 0.0f;
		}

		{
			Game::level.finder.find("cutscene_props")->get<View>()->mask = RENDER_MASK_DEFAULT;
			Transform* cutscene = Game::level.finder.find("cutscene")->get<Transform>();
			rex->get<Transform>()->absolute(cutscene->pos, cutscene->rot);
			if (data->cutscene_parkour.ref())
				data->cutscene_parkour.ref()->get<Transform>()->absolute(cutscene->pos, cutscene->rot);
		}

		for (auto i = Actor::Instance::list.iterator(); !i.is_last(); i.next())
			i.item()->highlight = false;

		data->ivory_ad_text.ref()->get<View>()->mask = 0;
		Game::level.finder.find("ivory_ad")->get<View>()->mask = 0;
		Game::updates.add(&cutscene_update);
		Actor::cleanup();
	}

	void cutscene_init()
	{
		cutscene_init_common(Asset::Animation::hobo_trailer7, Asset::Animation::parkour_trailer7_parkour);
		Particles::clear();
		Game::level.rain = 0.0f;
	}

	void cutscene2_init()
	{
		cutscene_init_common(Asset::Animation::hobo_trailer8, Asset::Animation::parkour_trailer8_parkour);
		Particles::clear();
		Game::level.rain = 0.0f;
	}

	void cutscene3_init()
	{
		cutscene_init_common(Asset::Animation::hobo_trailer9, AssetNull);
		Game::level.sky_decals.length = 0;
		Game::level.clouds[0].color = Vec4(0.0f, 0.25f, 0.5f, 0.5f);
		Game::level.asteroids = 1.0f;
		World::remove(Game::level.finder.find("hobo"));
	}
#endif

	void update_title(const Update& u)
	{
#if !RELEASE_BUILD
		if (u.input->keys.get(s32(KeyCode::F1)) && !u.last_input->keys.get(s32(KeyCode::F1)))
			cutscene_init();
		else if (u.input->keys.get(s32(KeyCode::F2)) && !u.last_input->keys.get(s32(KeyCode::F2)))
			cutscene2_init();
		else if (u.input->keys.get(s32(KeyCode::F3)) && !u.last_input->keys.get(s32(KeyCode::F3)))
			cutscene3_init();
#endif

		if (Camera* camera = data->camera.ref())
		{
			Vec3 head_pos = Vec3::zero;
			data->character.ref()->to_world(Asset::Bone::character_head, &head_pos);
			r32 blend = data->transition_timer > 0.0f ? vi_min(1.0f, total_transition - data->transition_timer) : 0.0f;
			camera->pos = Vec3::lerp(blend, data->camera_start_pos, head_pos);

			camera->flag(CameraFlagColors | CameraFlagActive | CameraFlagFog, true);
			camera->viewport =
			{
				Vec2(0, 0),
				Vec2(r32(Settings::display().width), r32(Settings::display().height)),
			};
			camera->perspective(LMath::lerpf(blend * 0.5f, start_fov, end_fov), 0.1f, Game::level.far_plane_get());

			if (Game::level.mode == Game::Mode::Special
				&& !Overworld::active()
				&& !Overworld::transitioning())
				Menu::title_menu(u, camera);
		}

		if (data->transition_timer > 0.0f)
		{
			r32 old_timer = data->transition_timer;
			data->transition_timer = vi_max(0.0f, data->transition_timer - Game::real_time.delta);
			if (data->transition_timer < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
			{
				Game::time.total = Game::real_time.total = 0.0f;
				Particles::clear();

				data->camera = nullptr;

				World::remove(data->character.ref()->entity());
				Game::level.mode = Game::Mode::Parkour;
				data->dada->highlight = true;
				for (auto i = PlayerHuman::list.iterator(); !i.is_last(); i.next())
					i.item()->get<PlayerManager>()->flag(PlayerManager::FlagCanSpawn, true);
			}
		}
		else
		{
			if (Game::level.mode == Game::Mode::Special
				&& Menu::main_menu_state == Menu::State::Hidden
				&& Game::scheduled_load_level == AssetNull
				&& !Overworld::active() && !Overworld::transitioning()
				&& !Menu::dialog_active(0))
			{
				if (Game::session.type == SessionType::Multiplayer)
				{
					Overworld::show(data->camera.ref(), Game::multiplayer_is_online ? Overworld::State::MultiplayerOnline : Overworld::State::MultiplayerOffline);
					Overworld::skip_transition_full();
				}
				else if (any_input(u))
					Menu::show();
			}
		}
	}

	void draw_ui(const RenderParams& p)
	{
		if (Game::level.mode == Game::Mode::Special
			&& Game::scheduled_load_level == AssetNull
			&& data->transition_timer == 0.0f
			&& !Overworld::active()
			&& Game::session.type == SessionType::Story)
		{
			const Vec2 actual_size(1920, 458);
			Rect2 logo_rect;
			b8 draw = true;
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

				Vec2 size = actual_size * (p.camera->viewport.size.x * 0.5f / actual_size.x);
				logo_rect = { p.camera->viewport.size * 0.5f + size * Vec2(-0.5f, -0.5f), size };

				draw = !Menu::dialog_active(0);
			}
			else
			{
				Vec2 menu_pos;
				menu_pos.x = p.camera->viewport.size.x * 0.5f;
				if (Menu::main_menu_state == Menu::State::Credits)
					menu_pos.y = p.camera->viewport.size.y * 0.8f + MENU_ITEM_HEIGHT * -1.5f;
				else
					menu_pos.y = p.camera->viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f;
				Vec2 size = actual_size * ((MENU_ITEM_WIDTH + MENU_ITEM_PADDING * -2.0f) / actual_size.x);
				logo_rect = { menu_pos + size * Vec2(-0.5f, 0.0f) + Vec2(0.0f, MENU_ITEM_PADDING * 3.0f), size };
			}
			UI::sprite(p, Asset::Texture::logo, { logo_rect.pos + logo_rect.size * 0.5f, logo_rect.size });
		}

		if (data->energy.ref()
			&& data->state != TutorialState::Start && data->state != TutorialState::DadaSpotted && data->state != TutorialState::DadaTalking
			&& !data->energy.ref()->get<Transform>()->parent.ref())
			UI::indicator(p, data->energy.ref()->get<Transform>()->absolute_pos(), UI::color_accent(), true);

		if (data->transition_timer > 0.0f && data->transition_timer < TRANSITION_TIME)
			Menu::draw_letterbox(p, data->transition_timer, TRANSITION_TIME);
	}

	void player_jumped()
	{
		if (data->state == TutorialState::Jump)
		{
			data->state = TutorialState::Climb;
			Actor::tut_clear();
			Actor::tut(strings::tut_climb, Actor::TutMode::Left, 2.0f);
		}
	}

	void wallrun_tut(Entity*)
	{
		if (data->state == TutorialState::WallRun)
		{
			data->wallrun_footstep_index = 0;
			data->wallrun_footstep_timer = 0.0f;
			Actor::tut_clear();
			Actor::tut(Asset::String::tut_wallrun, Actor::TutMode::Left, 0.0f);
		}
	}

	void wallrun_tut_clear(Entity*)
	{
		if (data->state == TutorialState::WallRun)
			Actor::tut_clear();
	}

	void wallrun_done(Entity*)
	{
		if (data->state == TutorialState::WallRun)
		{
			Actor::tut_clear();
			data->state = TutorialState::Done;
		}
	}

	void update(const Update& u)
	{
		// ivory ad text
		data->ivory_ad_text.ref()->rot *= Quat::euler(0, u.time.delta * 0.2f, 0);

		// shopping cart sounds
		{
			s32 num_manifolds = Physics::btWorld->getDispatcher()->getNumManifolds();
			for (s32 i = 0; i < num_manifolds; i++)
			{
				btPersistentManifold* contact_manifold = Physics::btWorld->getDispatcher()->getManifoldByIndexInternal(i);
				Entity* a = &Entity::list[contact_manifold->getBody0()->getUserIndex()];
				Entity* b = &Entity::list[contact_manifold->getBody1()->getUserIndex()];

				PhysicsSoundEntity* sound = nullptr;
				for (s32 i = 0; i < data->carts.length; i++)
				{
					PhysicsSoundEntity* entry = &data->carts[i];
					Entity* e = entry->entity.ref();
					if (a == e)
					{
						sound = entry;
						break;
					}
					else if (b == e)
					{
						sound = entry;
						break;
					}
				}

				if (sound && Game::time.total - sound->last_sound > 0.3f)
				{
					s32 num_contacts = contact_manifold->getNumContacts();
					for (s32 j = 0; j < num_contacts; j++)
					{
						btManifoldPoint& pt = contact_manifold->getContactPoint(j);
						if (pt.getAppliedImpulse() > 0.5f)
						{
							sound->last_sound = Game::time.total;
							sound->entity.ref()->get<Audio>()->post(AK::EVENTS::PLAY_SHOPPING_CART_RATTLE);
							break;
						}
					}
				}
			}
		}

		// check if rex's cart is gone
		if (!data->rex_cart_gone)
		{
			if (data->rex_cart.ref()->absolute_pos().y < 5.0f)
			{
				data->rex_cart_gone = true;
				data->rex->stop();
				data->rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_c01);
				data->rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_c02);
				data->rex->cue(AK_InvalidID, Asset::Animation::hobo_idle, strings::rex_c03);
			}
		}

		{
			// fire
			r32 offset = Game::time.total * 10.0f;
			const r32 radius = 0.25f;
			data->fire.ref()->pos = data->fire_start_pos + Vec3(noise::sample2d(Vec2(offset)) * radius, noise::sample2d(Vec2(offset + 67)) * radius, noise::sample2d(Vec2(offset + 137)) * radius);
			data->fire.ref()->get<PointLight>()->radius = 7.0f + noise::sample2d(Vec2(offset + 191));

			const r32 fire_interval = 0.01f;
			data->fire_accumulator += u.time.delta;
			while (data->fire_accumulator > fire_interval)
			{
				r32 offset = (Game::time.total - data->fire_accumulator) * 10.0f;
				Vec3 pos = data->fire_start_pos + Vec3(noise::sample2d(Vec2(offset)) * radius, -0.4f + noise::sample2d(Vec2(offset + 67)) * radius, noise::sample2d(Vec2(offset + 137)) * radius);
				Particles::smoke.add(pos, Vec3(0, 1.5f, 0));
				data->fire_accumulator -= fire_interval;
			}
		}

		if (Parkour::list.count() > 0)
		{
			Parkour* parkour = Parkour::list.iterator().item();
			if (data->state == TutorialState::Done)
			{
				if (!data->demo_notified
					&& Game::save.zones[s32(Asset::Level::Isca)] == ZoneState::PvpFriendly
					&& parkour->get<Animator>()->layers[3].animation == AssetNull)
				{
					data->demo_notified = true;
					Menu::dialog(0, Menu::dialog_no_action, _(strings::demo_notify));
				}
			}
			else
			{
				if (!data->player.ref())
				{
					// player just spawned
					data->player = parkour;
					parkour->jumped.link(&player_jumped);
				}

				if (data->state == TutorialState::Climb
					&& (parkour->fsm.current == ParkourState::Climb || parkour->fsm.current == ParkourState::Mantle))
				{
					data->state = TutorialState::Shop;
					Actor::tut_clear();
				}
				else if (data->state == TutorialState::Shop && Game::save.resources[s32(Resource::WallRun)])
					data->state = TutorialState::WallRun;

				if (data->state == TutorialState::WallRun)
				{
					StaticArray<Ref<Transform>, 8>* footsteps = nullptr;
					if (data->wallrun_trigger_1.ref()->is_triggered())
						footsteps = &data->wallrun_footsteps1;
					else if (data->wallrun_trigger_2.ref()->is_triggered())
						footsteps = &data->wallrun_footsteps2;

					if (footsteps)
					{
						// spawn wallrun footstep shockwave effects
						data->wallrun_footstep_timer -= u.time.delta;
						if (data->wallrun_footstep_timer < 0.0f)
						{
							data->wallrun_footstep_timer += 2.0f / footsteps->length;
							data->wallrun_footstep_index = (data->wallrun_footstep_index + 1) % footsteps->length;
							EffectLight::add((*footsteps)[data->wallrun_footstep_index].ref()->absolute_pos(), 1.0f, 1.0f, EffectLight::Type::Shockwave);
						}
					}
				}
			}
		}
	}

	void init(const EntityFinder& entities)
	{
		vi_assert(!data);
		data = new Data();
		Game::cleanups.add(cleanup);

		entities.find("cutscene_props")->get<View>()->mask = 0;

		if (Game::level.mode == Game::Mode::Special)
		{
			data->camera = PlayerHuman::list.iterator().item()->camera.ref();

			Quat rot;
			entities.find("map_view")->get<Transform>()->absolute(&data->camera_start_pos, &rot);
			data->camera.ref()->pos = data->camera_start_pos;
			data->camera.ref()->rot = Quat::look(rot * Vec3(0, -1, 0));

			data->character = entities.find("character")->get<Animator>();
		}
		else if (Game::level.local)
			World::remove(entities.find("character"));

		if ((Game::save.zone_last == AssetNull || Game::save.zone_last == Asset::Level::Docks)
			&& entities.find("energy"))
		{
			entities.find("energy")->get<Collectible>()->amount = Overworld::resource_info[s32(Resource::WallRun)].cost + 80;
			entities.find("jump_trigger")->get<PlayerTrigger>()->entered.link(&jump_trigger);
			entities.find("dada_spotted_trigger")->get<PlayerTrigger>()->entered.link(&dada_spotted);
			entities.find("dada_talk_trigger")->get<PlayerTrigger>()->entered.link(&dada_talk);
			data->energy = entities.find("energy");

			Game::updates.add(update_title);
			Game::draws.add(draw_ui);

			data->dada = Actor::add(entities.find("dada"), Asset::Bone::dada_head);
			Loader::animation(Asset::Animation::dada_close_door);
			Loader::animation(Asset::Animation::dada_wait);
			Loader::animation(Asset::Animation::dada_talk);
			if (Game::level.mode != Game::Mode::Special)
				data->dada->highlight = true;

			data->wallrun_trigger_1 = entities.find("wallrun_trigger_1")->get<PlayerTrigger>();
			data->wallrun_trigger_1.ref()->entered.link(&wallrun_tut);
			data->wallrun_trigger_1.ref()->exited.link(&wallrun_tut_clear);
			entities.find("wallrun_success_1")->get<PlayerTrigger>()->entered.link(&wallrun_done);
			data->wallrun_trigger_2 = entities.find("wallrun_trigger_2")->get<PlayerTrigger>();
			data->wallrun_trigger_2.ref()->entered.link(&wallrun_tut);
			data->wallrun_trigger_2.ref()->exited.link(&wallrun_tut_clear);
			entities.find("wallrun_success_2")->get<PlayerTrigger>()->entered.link(&wallrun_done);
			for (s32 i = 0; i < 8; i++)
			{
				char name[64];
				sprintf(name, "wallrun_footstep_1.%03d", i);
				data->wallrun_footsteps1.add(entities.find(name)->get<Transform>());
			}
			for (s32 i = 0; i < 8; i++)
			{
				char name[64];
				sprintf(name, "wallrun_footstep_2.%03d", i);
				data->wallrun_footsteps2.add(entities.find(name)->get<Transform>());
			}
		}
		else
		{
			data->state = TutorialState::Done;
			Animator* dada = entities.find("dada")->get<Animator>();
			dada->layers[0].behavior = Animator::Behavior::Freeze;
			dada->layers[0].play(Asset::Animation::dada_close_door);
		}

		for (s32 i = 0; ; i++)
		{
			char name[16];
			snprintf(name, 16, "cart%d", i);
			Entity* cart = entities.find(name);
			if (cart)
			{
				cart->add<Audio>();
				data->carts.add({ cart, 0.0f });
			}
			else
				break;
		}

		data->rex_cart = entities.find("cart0")->get<Transform>();
		data->ivory_ad_text = entities.find("ivory_ad_text")->get<Transform>();
		data->fire = entities.find("fire")->get<Transform>();
		data->fire.ref()->entity()->add<Audio>()->post(AK::EVENTS::PLAY_FIRE_LOOP);
		data->fire_start_pos = data->fire.ref()->pos;

		Actor::init();
		Loader::animation(Asset::Animation::hobo_idle);
		data->rex = Actor::add(entities.find("hobo"), Asset::Bone::hobo_head);
		data->rex->interacted = rex_speak;

		Actor::Instance* ivory_ad = Actor::add(entities.find("ivory_ad"));
		ivory_ad->dialogue_radius = 25.0f;
		ivory_ad_play(ivory_ad);

		Game::updates.add(update);
	}

	void play()
	{
		Game::save.reset();
		Game::session.reset(SessionType::Story);
		data->transition_timer = total_transition;
	}
}

namespace tutorial
{
	enum class TutorialState : s8
	{
		None,
		ForceField,
		Battery,
		Upgrade,
		Ability,
		AbilityDone,
		Capture,
		Done,
		count,
	};

	struct Data
	{
		TutorialState state;
		Ref<Entity> player;
		Ref<ForceField> force_field;
	};

	Data* data;

	void drone_target_hit(Entity* e)
	{
		if ((data->state == TutorialState::ForceField || data->state == TutorialState::Battery) && e->has<Battery>())
		{
			data->state = TutorialState::Upgrade;
			Actor::tut(strings::tut_upgrade);
			Game::level.feature_level = Game::FeatureLevel::Abilities;
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			manager->energy = UpgradeInfo::list[0].cost;
		}
	}

	void ability_spawned(Ability)
	{
		if (data->state == TutorialState::Ability)
		{
			data->state = TutorialState::AbilityDone;
			Actor::tut(strings::tut_ability_done);
		}
	}

	void update(const Update& u)
	{
		// check if the player has spawned
		if (Game::level.mode == Game::Mode::Pvp && !data->player.ref() && PlayerControlHuman::list.count() > 0)
		{
			Entity* player = PlayerControlHuman::list.iterator().item()->entity();
			data->player = player;
			player->get<Drone>()->ability_spawned.link(&ability_spawned);
			player->get<Drone>()->hit.link(&drone_target_hit);

			if (s32(data->state) <= s32(TutorialState::ForceField))
			{
				data->state = TutorialState::ForceField;
				Actor::tut(strings::tut_force_field);
			}
		}

		if (data->state == TutorialState::ForceField)
		{
			if (ForceField::list.count() > 0
				&& ForceField::list.iterator().item()->get<Health>()->hp == 0)
			{
				data->state = TutorialState::Battery;
				Actor::tut_clear();
				Actor::tut(strings::tut_battery);
			}
		}

		if (data->state != TutorialState::Done
			&& Team::match_state == Team::MatchState::Done
			&& Game::level.mode == Game::Mode::Pvp)
		{
			data->state = TutorialState::Done;
			Game::save.tutorial_complete = true;
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
					if (human->ui_mode() != PlayerHuman::UIMode::PvpUpgrade)
					{
						data->state = TutorialState::Ability;
						Actor::tut(strings::tut_ability, Actor::TutMode::Left, 0.5f);
						Game::level.feature_level = Game::FeatureLevel::Abilities;
					}
				}
			}
		}
		else if (data->state == TutorialState::AbilityDone)
		{
			if (PlayerHuman::list.count() > 0)
			{
				Entity* drone = PlayerHuman::list.iterator().item()->get<PlayerManager>()->instance.ref();
				if (drone && drone->get<Drone>()->current_ability == Ability::None)
				{
					Actor::tut_clear();
					Team::match_time = 0.0f;
					Team::battery_spawn_delay = 1000.0f;
					data->state = TutorialState::Capture;
					Game::level.feature_level = Game::FeatureLevel::TutorialAll;
					Actor::tut(strings::tut_capture);
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
#if !SERVER
		if (Game::session.type == SessionType::Story
			&& Game::level.local
			&& !Game::save.tutorial_complete)
		{
			Actor::init();

			data = new Data();

			Game::level.feature_level = Game::FeatureLevel::Batteries;
			Game::level.battery_spawns.length = s8(vi_min(s32(Game::level.battery_spawns.length), 3));

			Game::updates.add(&update);
			Game::cleanups.add(&cleanup);
		}
#endif
	}
}

namespace locke
{
	struct Data
	{
		Actor::Instance* locke;
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
			&& !Game::save.locke_spoken)
		{
			Game::save.locke_index++; // locke_index starts at -1
			if (Game::save.locke_index == 8)
				Game::save.locke_index = 2; // skip the first two
			Game::save.locke_spoken = true;
			switch (Game::save.locke_index)
			{
				case 0:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_gesture_one_hand_short, strings::locke_a01, Actor::Loop::No);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_gesture_one_hand_short, strings::locke_a02, Actor::Loop::No, Actor::Overlap::Yes);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_gesture_both_arms, strings::locke_a03, Actor::Loop::No, Actor::Overlap::Yes);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_gesture_both_arms, strings::locke_a04, Actor::Loop::No);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_shift_weight, strings::locke_a05, Actor::Loop::No);
					break;
				}
				case 1:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_gesture_one_hand_short, strings::locke_b01, Actor::Loop::No);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_shift_weight, strings::locke_b02, Actor::Loop::No);
					break;
				}
				case 2:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_c01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_c02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_c03);
					break;
				}
				case 3:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_d01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_d02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_d03);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_d04);
					break;
				}
				case 4:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_e01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_e02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_e03);
					break;
				}
				case 5:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_f01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_f02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_f03);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_f04);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_f05);
					break;
				}
				case 6:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_g01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_g02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_g03);
					break;
				}
				case 7:
				{
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_h01);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_h02);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_h03);
					data->locke->cue(AK_InvalidID, Asset::Animation::locke_idle, strings::locke_h04);
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

namespace Channels
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

namespace Slum
{
	struct Data
	{
		Actor::Instance* meursault;
		Ref<Entity> anim_base;
		b8 anim_played;
		b8 energy_given;
	};
	Data* data;

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void give_energy()
	{
		if (!data->energy_given)
		{
			Overworld::resource_change(Resource::Energy, 1000);
			data->energy_given = true;
		}
	}

	void give_energy_animation_callback(Actor::Instance*)
	{
		give_energy();
	}

	void trigger(Entity* player)
	{
#if !SERVER
		if (!data->anim_played)
		{
			data->anim_played = true;

			data->meursault->cue(AK_InvalidID, Asset::Animation::meursault_intro, strings::meursault_arrow, Actor::Loop::No, Actor::Overlap::Yes, 0.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a01, Actor::Loop::No, Actor::Overlap::No, 3.6f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a02, Actor::Loop::No, Actor::Overlap::No, 3.6f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a03, Actor::Loop::No, Actor::Overlap::No, 5.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a04, Actor::Loop::No, Actor::Overlap::No, 4.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a05, Actor::Loop::No, Actor::Overlap::No, 2.5f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a06, Actor::Loop::No, Actor::Overlap::No, 1.5f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a07, Actor::Loop::No, Actor::Overlap::No, 2.8f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a08, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a09, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a10, Actor::Loop::No, Actor::Overlap::No, 5.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a11, Actor::Loop::No, Actor::Overlap::No, 2.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a12, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a13, Actor::Loop::No, Actor::Overlap::No, 3.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a14, Actor::Loop::No, Actor::Overlap::No, 1.0f);
			data->meursault->cue(&give_energy_animation_callback, 0.0f);
			data->meursault->cue(AK_InvalidID, AssetNull, strings::meursault_a15, Actor::Loop::No, Actor::Overlap::No, 2.0f);

			player->get<PlayerControlHuman>()->cinematic(data->anim_base.ref(), Asset::Animation::character_meursault_intro);
		}
#endif
	}

	void init(const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(&cleanup);

		data->meursault = Actor::add(entities.find("meursault"), Asset::Bone::meursault_head, Actor::IdleBehavior::Interrupt);

		entities.find("trigger")->get<PlayerTrigger>()->entered.link(&trigger);
		data->anim_base = entities.find("player_anim");

		Loader::animation(Asset::Animation::character_meursault_intro);
		Loader::animation(Asset::Animation::meursault_intro);
	}
}


namespace AudioLogs
{
	struct Data
	{
		Actor::Instance* actor;
	};
	static Data* data;

	struct Entry
	{
		const char* id;
		void(*function)();
	};

	void cleanup()
	{
		delete data;
	}

	void init()
	{
		if (!data)
		{
			data = new Data();
			data->actor = Actor::add();
			data->actor->dialogue_radius = 0.0f;
		}
	}

	void stop()
	{
		Audio::post_global(AK::EVENTS::STOP_AUDIOLOG);
		data->actor->cues.length = 0;
	}

	void done(Actor::Instance*)
	{
		if (PlayerHuman::list.count() > 0)
			PlayerHuman::list.iterator().item()->audio_log_stop();
	}

	void rex()
	{
		init();
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d01);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d02);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d03);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d04);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d05);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d06);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d07);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d08);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d09);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d10);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d11);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d12);
		data->actor->cue(AK_InvalidID, AssetNull, strings::rex_d13);
		data->actor->cue(&done);
	}

	static const Entry entries[] =
	{
		{ "rex", &rex },
		{ nullptr, nullptr },
	};

	AssetID get_id(const char* id)
	{
		s32 i = 0;
		while (true)
		{
			const Entry& entry = entries[i];
			if (entry.id)
			{
				if (strcmp(entry.id, id) == 0)
					return AssetID(i);
			}
			else
				break;
			i++;
		}
		return AssetNull;
	}

	void play(AssetID id)
	{
		entries[id].function();
	}
}


}

Script Script::list[] =
{
	{ "splash", Scripts::splash::init },
	{ "tutorial", Scripts::tutorial::init },
	{ "Docks", Scripts::Docks::init },
	{ "locke", Scripts::locke::init },
	{ "Channels", Scripts::Channels::init },
	{ "Slum", Scripts::Slum::init },
	{ nullptr, nullptr },
};
s32 Script::count; // set in Game::init


}
