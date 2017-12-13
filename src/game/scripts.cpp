#if !SERVER
#include "mongoose/mongoose.h"
#endif
#undef sleep // ugh

#include "scripts.h"
#include "net.h"
#include "mersenne/mersenne-twister.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "strings.h"
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
#if !defined(__ORBIS__)
#include "steam/steam_api.h"
#endif

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

void update(const Update& u)
{
	if (!data)
		return;

	for (auto i = Instance::list.iterator(); !i.is_last(); i.next())
	{
		Instance* instance = i.item();

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
		if (instance->model.ref() && instance->model.ref()->has<Animator>())
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

		if (layer)
			instance->collision.ref()->pos = instance->collision_offset();
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
			UIMenu::text_clip(&text, data->text_tut_real_time, 80.0f);

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
		WallRun,
		Done,
		count,
	};

#if !SERVER && !defined(__ORBIS__)
	struct SteamCallback
	{
		static SteamCallback instance;

		STEAM_CALLBACK(SteamCallback, auth_session_ticket_callback, GetAuthSessionTicketResponse_t);
	};

	void SteamCallback::auth_session_ticket_callback(GetAuthSessionTicketResponse_t* data)
	{
		Net::Client::master_send_auth();
	}
#endif

	struct Data
	{
#if !SERVER
		mg_mgr mg_mgr;
		mg_connection* mg_conn_ipv4;
		mg_connection* mg_conn_ipv6;
#if !defined(__ORBIS__)
		SteamCallback steam_callback;
#endif

#endif

		Actor::Instance* dada;
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
		Ref<Entity> energy;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps1;
		StaticArray<Ref<Transform>, 8> wallrun_footsteps2;
		TutorialState state;
		b8 dada_talked;
	};
	
	static Data* data;

	void cleanup()
	{
#if !SERVER
		if (data->mg_conn_ipv4 || data->mg_conn_ipv6)
			mg_mgr_free(&data->mg_mgr);
#endif

		delete data;
		data = nullptr;
	}

	void jump_trigger(Entity*)
	{
		if (data->state == TutorialState::Start
			|| data->state == TutorialState::DadaSpotted
			|| data->state == TutorialState::DadaTalking)
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
			data->dada->cue(AK::EVENTS::PLAY_DADA01, Asset::Animation::dada_wait, strings::dada01);
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
			data->dada->cue(AK::EVENTS::PLAY_DADA02, Asset::Animation::dada_talk, strings::dada02);
			data->dada->cue(AK::EVENTS::PLAY_DADA03, Asset::Animation::dada_talk, strings::dada03);
			data->dada->cue(AK_InvalidID, Asset::Animation::dada_close_door, AssetNull, Actor::Loop::No);
			data->dada->cue(&dada_done, 0.0f);
			data->dada->cue(&Actor::done, 0.0f);
		}
	}

	void hobo_speak(Actor::Instance* hobo)
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
		hobo->cue(&hobo_speak, 4.0f);
	}

	void ivory_ad_play(Actor::Instance* ad)
	{
		ad->cue(AK::EVENTS::PLAY_IVORY_AD01, AssetNull, strings::ivory_ad01);
		ad->cue(AK::EVENTS::PLAY_IVORY_AD02, AssetNull, strings::ivory_ad02);
		ad->cue(AK::EVENTS::PLAY_IVORY_AD03, AssetNull, strings::ivory_ad03);
		ad->cue(AK::EVENTS::PLAY_IVORY_AD04, AssetNull, strings::ivory_ad04);
		ad->cue(AK::EVENTS::PLAY_IVORY_AD05, AssetNull, strings::ivory_ad05);
		ad->cue(AK::EVENTS::PLAY_IVORY_AD06, AssetNull, strings::ivory_ad06);
		ad->cue(&ivory_ad_play, 4.0f);
	}

#if SERVER
	void gamejolt_prompt() { }
#else
	void gamejolt_token_callback(const TextField& text_field)
	{
		strncpy(Settings::gamejolt_token, text_field.value.data, MAX_AUTH_KEY);
		Net::Client::master_send_auth();
	}

	void gamejolt_username_callback(const TextField& text_field)
	{
		strncpy(Settings::gamejolt_username, text_field.value.data, MAX_PATH_LENGTH);
		Menu::dialog_text(&gamejolt_token_callback, "", MAX_AUTH_KEY, _(strings::prompt_gamejolt_token));
	}

	void gamejolt_prompt()
	{
		Menu::dialog_text(&gamejolt_username_callback, "", MAX_PATH_LENGTH, _(strings::prompt_gamejolt_username));
	}

	void itch_handle_oauth(mg_connection* conn, int ev, void* ev_data)
	{
		if (ev == MG_EV_HTTP_REQUEST)
		{
			// GET
			http_message* msg = (http_message*)(ev_data);

			mg_printf
			(
				conn, "%s",
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/html\r\n"
				"\r\n"
				"<html>"
				"<head><title>Deceiver</title>"
				"<style>"
				"* { box-sizing: border-box; }"
				"body { background-color: #000; color: #fff; font-family: sans-serif; font-size: x-large; font-weight: bold; }"
				"img { display: block; margin-left: auto; margin-right: auto; width: 100%; max-width: 980px; padding: 3em; padding-bottom: 0px; }"
				"p { display: block; text-align: center; padding: 3em; }"
				"</style>"
				"</head>"
				"<body>"
				"<img src=\"http://deceivergame.com/public/header-backdrop.svg\" />"
				"<p id=\"msg\">Logging in...</p>"
				"<script>"
				"var data = new FormData();"
				"data.append('access_token', window.location.hash.substr(window.location.hash.indexOf('=') + 1));"
				"var ajax = new XMLHttpRequest();"
				"var $msg = document.getElementById('msg');"
				"function msg_error()"
				"{"
				"	$msg.innerHTML = 'Login failed! Please try again.';"
				"}"
				"ajax.addEventListener('load', function()"
				"{"
				"	if (this.status === 200)"
				"		$msg.innerHTML = 'Login successful! You can close this window and return to the game.';"
				"	else"
				"		msg_error();"
				"});"
				"ajax.addEventListener('error', msg_error);"
				"ajax.open('POST', '/auth', true);"
				"ajax.send(data);"
				"</script>"
				"</body>"
				"</html>"
			);
			conn->flags |= MG_F_SEND_AND_CLOSE;
		}
		else if (ev == MG_EV_HTTP_PART_DATA)
		{
			// POST
			mg_http_multipart_part* part = (mg_http_multipart_part*)(ev_data);
			if (strcmp(part->var_name, "access_token") == 0 && part->data.len <= MAX_AUTH_KEY)
			{
				// got the access token
				strncpy(Settings::itch_api_key, part->data.p, part->data.len);
				Loader::settings_save();
				Game::auth_key_length = vi_max(0, vi_min(MAX_AUTH_KEY, s32(part->data.len)));
				memcpy(Game::auth_key, part->data.p, Game::auth_key_length);
				Menu::dialog_clear(0);
				Net::Client::master_send_auth();

				mg_printf
				(
					conn, "%s",
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
				);
				conn->flags |= MG_F_SEND_AND_CLOSE;
			}
			else
			{
				mg_printf
				(
					conn, "%s",
					"HTTP/1.1 400 Bad Request\r\n"
					"Content-Type: text/html\r\n"
					"\r\n"
				);
				conn->flags |= MG_F_SEND_AND_CLOSE;
			}
		}
	}

	void itch_redirect(s8)
	{
		Menu::open_url("https://itch.io/user/oauth?client_id=96b70c5d535c7101941dcbb0648ca2e3&scope=profile%3Ame&response_type=token&redirect_uri=http%3A%2F%2Flocalhost%3A3499%2Fauth");
	}

	void itch_prompt()
	{
		Menu::dialog(0, &itch_redirect, _(strings::prompt_itch));
	}

	void itch_register_endpoints(mg_connection* conn)
	{
		mg_register_http_endpoint(conn, "/auth", itch_handle_oauth);
	}

	void itch_ev_handler(mg_connection* conn, int ev, void* ev_data)
	{
		switch (ev)
		{
			case MG_EV_HTTP_REQUEST:
			{
				mg_printf
				(
					conn, "%s",
					"HTTP/1.1 403 Forbidden\r\n"
					"Content-Type: text/html\r\n"
					"Transfer-Encoding: chunked\r\n"
					"\r\n"
				);
				mg_printf_http_chunk(conn, "%s", "Forbidden");
				mg_send_http_chunk(conn, "", 0);
				break;
			}
		}
	}
#endif

	void update_title(const Update& u)
	{
#if !SERVER
		if (data->mg_conn_ipv4 || data->mg_conn_ipv6)
		{
			mg_mgr_poll(&data->mg_mgr, 0);
			if (Game::auth_key_length == 0 && !Menu::dialog_active(0))
				itch_prompt();
		}
#endif

		if (data->camera.ref())
		{
			Vec3 head_pos = Vec3::zero;
			data->character.ref()->to_world(Asset::Bone::character_head, &head_pos);
			r32 blend = data->transition_timer > 0.0f ? vi_min(1.0f, total_transition - data->transition_timer) : 0.0f;
			data->camera.ref()->pos = Vec3::lerp(blend, data->camera_start_pos, head_pos);
			Audio::listener_update(0, data->camera.ref()->pos, data->camera.ref()->rot);

			data->camera.ref()->viewport =
			{
				Vec2(0, 0),
				Vec2(r32(Settings::display().width), r32(Settings::display().height)),
			};
			data->camera.ref()->perspective(LMath::lerpf(blend * 0.5f, start_fov, end_fov), 0.1f, Game::level.far_plane_get());

			if (Game::user_key.id)
			{
				if (Game::level.mode == Game::Mode::Special
					&& !Overworld::active()
					&& !Overworld::transitioning())
					Menu::title_menu(u, data->camera.ref());
			}
			else
			{
				if (u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0) && !Game::cancel_event_eaten[0])
					Menu::dialog(0, &Menu::exit, _(strings::confirm_quit));
			}
		}

		if (data->transition_timer > 0.0f)
		{
			r32 old_timer = data->transition_timer;
			data->transition_timer = vi_max(0.0f, data->transition_timer - Game::real_time.delta);
			if (data->transition_timer < TRANSITION_TIME * 0.5f && old_timer >= TRANSITION_TIME * 0.5f)
			{
				Game::time.total = Game::real_time.total = 0.0f;
				Particles::clear();

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
				&& !Overworld::active() && !Overworld::transitioning()
				&& !Menu::dialog_active(0)
				&& Game::user_key.id)
			{
				if (Game::session.type == SessionType::Multiplayer)
				{
					Overworld::show(data->camera.ref(), Overworld::State::Multiplayer);
					Overworld::skip_transition_full();
				}
				else if (any_input(u))
					Menu::show();
			}
		}
	}

	void draw_ui(const RenderParams& p)
	{
		if (Game::user_key.id)
		{
			if (Game::level.mode == Game::Mode::Special
				&& Game::scheduled_load_level == AssetNull
				&& data->transition_timer == 0.0f
				&& !Overworld::active()
				&& Game::session.type == SessionType::Story
				&& !Menu::dialog_active(0))
			{
				const Vec2 actual_size(1185, 374);
				Rect2 logo_rect;
				if (Menu::main_menu_state == Menu::State::Hidden)
				{
					UIText text;
					text.color = UI::color_default;
					text.text(0, "[{{Start}}]");
					text.anchor_x = UIText::Anchor::Center;
					text.anchor_y = UIText::Anchor::Center;
					Vec2 pos = p.camera->viewport.size * Vec2(0.5f, 0.1f);
					UI::box(p, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
					text.draw(p, pos);

					Vec2 size = actual_size * (p.camera->viewport.size.x * 0.5f / actual_size.x);
					logo_rect = { p.camera->viewport.size * 0.5f + size * Vec2(-0.5f, -0.5f), size };
				}
				else
				{
					Vec2 menu_pos(p.camera->viewport.size.x * 0.5f, p.camera->viewport.size.y * 0.65f + MENU_ITEM_HEIGHT * -1.5f);
					Vec2 size = actual_size * ((MENU_ITEM_WIDTH + MENU_ITEM_PADDING * -2.0f) / actual_size.x);
					logo_rect = { menu_pos + size * Vec2(-0.5f, 0.0f) + Vec2(0.0f, MENU_ITEM_PADDING * 3.0f), size };
				}
				UI::sprite(p, Asset::Texture::logo, { logo_rect.pos + logo_rect.size * 0.5f, logo_rect.size });
			}
		}
		else
			Menu::progress_infinite(p, _(strings::connecting), p.camera->viewport.size * 0.5f);

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

		if (Parkour::list.count() > 0 && data->state != TutorialState::Done)
		{
			Parkour* parkour = Parkour::list.iterator().item();

			if (!data->player.ref())
			{
				// player just spawned
				data->player = parkour;
				parkour->jumped.link(&player_jumped);
			}

			if (data->state == TutorialState::Climb
				&& (parkour->fsm.current == Parkour::State::Climb || parkour->fsm.current == Parkour::State::Mantle))
			{
				data->state = TutorialState::WallRun;
				Actor::tut_clear();
			}

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

	void init(const EntityFinder& entities)
	{
		vi_assert(!data);
		data = new Data();
		Game::cleanups.add(cleanup);

#if !SERVER
		if (!Game::user_key.id)
		{
			switch (Game::auth_type)
			{
				case Net::Master::AuthType::GameJolt:
				{
					// check if we already have the username and token
					if (Settings::gamejolt_username[0])
						Net::Client::master_send_auth();
					else
						gamejolt_prompt();
					break;
				}
				case Net::Master::AuthType::Itch:
				{
					if (Game::auth_key_length) // launched from itch app
						Net::Client::master_send_auth();
					else // launched standalone
					{
						Game::auth_type = Net::Master::AuthType::ItchOAuth;

						if (Settings::itch_api_key[0]) // already got an OAuth token
						{
							Game::auth_key_length = vi_max(0, vi_min(s32(strlen(Settings::itch_api_key)), MAX_AUTH_KEY));
							memcpy(Game::auth_key, Settings::itch_api_key, Game::auth_key_length);
							Net::Client::master_send_auth();
						}
						else
						{
							// get an OAuth token

							// launch server

							mg_mgr_init(&data->mg_mgr, nullptr);
							{
								char addr[32];
								sprintf(addr, "127.0.0.1:%d", NET_OAUTH_PORT);
								data->mg_conn_ipv4 = mg_bind(&data->mg_mgr, addr, itch_ev_handler);

								sprintf(addr, "[::1]:%d", NET_OAUTH_PORT);
								data->mg_conn_ipv6 = mg_bind(&data->mg_mgr, addr, itch_ev_handler);
							}

							if (data->mg_conn_ipv4)
							{
								mg_set_protocol_http_websocket(data->mg_conn_ipv4);
								itch_register_endpoints(data->mg_conn_ipv4);
								printf("Bound to 127.0.0.1:%d\n", NET_OAUTH_PORT);
							}

							if (data->mg_conn_ipv6)
							{
								mg_set_protocol_http_websocket(data->mg_conn_ipv6);
								itch_register_endpoints(data->mg_conn_ipv6);
								printf("Bound to [::1]:%d\n", NET_OAUTH_PORT);
							}

							vi_assert(data->mg_conn_ipv4 || data->mg_conn_ipv6);
						}
					}
					break;
				}
				case Net::Master::AuthType::Steam:
#if !defined(__ORBIS__)
				{
					strncpy(Game::steam_username, SteamFriends()->GetPersonaName(), MAX_USERNAME);
					u32 auth_key_length;
					SteamUser()->GetAuthSessionTicket(Game::auth_key, MAX_AUTH_KEY, &auth_key_length);
					Game::auth_key_length = vi_max(0, vi_min(MAX_AUTH_KEY, s32(auth_key_length)));
					Game::auth_key[Game::auth_key_length] = '\0';
					break;
				}
#endif
				case Net::Master::AuthType::None:
					// we either have the auth token or we don't
					Net::Client::master_send_auth();
					break;
				default:
					vi_assert(false);
					break;
			}
		}
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
			&& entities.find("energy"))
		{
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

		data->ivory_ad_text = entities.find("ivory_ad_text")->get<Transform>();
		data->fire = entities.find("fire")->get<Transform>();
		data->fire.ref()->entity()->add<Audio>()->post(AK::EVENTS::PLAY_FIRE_LOOP);
		data->fire_start_pos = data->fire.ref()->pos;

		Actor::init();
		Loader::animation(Asset::Animation::hobo_idle);
		Actor::Instance* hobo = Actor::add(entities.find("hobo"), Asset::Bone::hobo_head);
		hobo_speak(hobo);

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
		Crawl,
		Battery,
		Upgrade,
		Ability,
		Turrets,
		Capture,
		Done,
		count,
	};

	struct Data
	{
		TutorialState state;
		Ref<Entity> player;
	};

	Data* data;

	void drone_target_hit(Entity* e)
	{
		if ((data->state == TutorialState::Crawl || data->state == TutorialState::Battery) && e->has<Battery>())
		{
			data->state = TutorialState::Upgrade;
			Actor::tut(strings::tut_upgrade);

			Game::level.feature_level = Game::FeatureLevel::Abilities;
			PlayerManager* manager = PlayerHuman::list.iterator().item()->get<PlayerManager>();
			manager->energy = UpgradeInfo::list[s32(Upgrade::Grenade)].cost + AbilityInfo::list[s32(Ability::Grenade)].spawn_cost * 2;
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

	void update(const Update& u)
	{
		// check if the player has spawned
		if (!data->player.ref() && PlayerControlHuman::list.count() > 0)
		{
			Entity* player = PlayerControlHuman::list.iterator().item()->entity();
			data->player = player;
			player->get<Drone>()->ability_spawned.link(&ability_spawned);
			player->get<Drone>()->hit.link(&drone_target_hit);

			if (s32(data->state) <= s32(TutorialState::Crawl))
			{
				data->state = TutorialState::Crawl;
				Actor::tut(strings::tut_crawl);
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
		else if (data->state == TutorialState::Turrets)
		{
			if (Turret::list.count() == 0)
			{
				Team::core_module_delay = 1.0f;
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
#if !SERVER
		if (Game::session.type == SessionType::Story
			&& Game::level.local
			&& !Game::save.tutorial_complete)
		{
			Game::level.ai_config.length = 0; // no bots

			Actor::init();

			data = new Data();

			{
				SpawnPoint* default_spawn = Team::list[1].default_spawn_point();
				Entity* trigger_entity = World::create<Empty>();
				trigger_entity->get<Transform>()->absolute_pos(default_spawn->get<Transform>()->absolute_pos());
				PlayerTrigger* trigger = trigger_entity->create<PlayerTrigger>();
				trigger->radius = DRONE_MAX_DISTANCE;
				trigger->exited.link(&crawl_complete);
				World::awake(trigger_entity);
				Net::finalize(trigger_entity);
			}

			Game::level.core_force_field.ref()->get<Health>()->hp = 1;
			for (auto i = CoreModule::list.iterator(); !i.is_last(); i.next())
				i.item()->get<Health>()->hp = 1;
			for (auto i = Turret::list.iterator(); !i.is_last(); i.next())
				i.item()->get<Health>()->hp = 1;

			Game::level.feature_level = Game::FeatureLevel::Base;

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
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_A02, Asset::Animation::locke_gesture_one_hand_short, strings::locke_a02, Actor::Loop::No, Actor::Overlap::Yes);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_A03, Asset::Animation::locke_gesture_both_arms, strings::locke_a03, Actor::Loop::No, Actor::Overlap::Yes);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_A04, Asset::Animation::locke_gesture_both_arms, strings::locke_a04, Actor::Loop::No);
					data->locke->cue(AK::EVENTS::PLAY_LOCKE_A05, Asset::Animation::locke_shift_weight, strings::locke_a05, Actor::Loop::No);
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
		b8 drones_given;
	};
	Data* data;

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void give_drones()
	{
		if (!data->drones_given)
		{
			Overworld::resource_change(Resource::Drones, 2);
			data->drones_given = true;
		}
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
		data->actor->cue(AK::EVENTS::PLAY_REX01, AssetNull, strings::rex01);
		data->actor->cue(AK::EVENTS::PLAY_REX02, AssetNull, strings::rex02);
		data->actor->cue(AK::EVENTS::PLAY_REX03, AssetNull, strings::rex03);
		data->actor->cue(AK::EVENTS::PLAY_REX04, AssetNull, strings::rex04);
		data->actor->cue(AK::EVENTS::PLAY_REX05, AssetNull, strings::rex05);
		data->actor->cue(AK::EVENTS::PLAY_REX06, AssetNull, strings::rex06);
		data->actor->cue(AK::EVENTS::PLAY_REX07, AssetNull, strings::rex07);
		data->actor->cue(AK::EVENTS::PLAY_REX08, AssetNull, strings::rex08);
		data->actor->cue(AK::EVENTS::PLAY_REX09, AssetNull, strings::rex09);
		data->actor->cue(AK::EVENTS::PLAY_REX10, AssetNull, strings::rex10);
		data->actor->cue(AK::EVENTS::PLAY_REX11, AssetNull, strings::rex11);
		data->actor->cue(AK::EVENTS::PLAY_REX12, AssetNull, strings::rex12);
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