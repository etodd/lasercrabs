#include "scripts.h"
#include "entities.h"
#include "common.h"
#include "game.h"
#include "player.h"
#include "awk.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "mersenne/mersenne-twister.h"
#include "asset/level.h"
#include "asset/mesh.h"
#include "asset/texture.h"
#include "asset/shader.h"
#include "menu.h"
#include "render/views.h"

namespace VI
{

enum class TutorialState
{
	Inactive,
	Active,
	Hidden,
};

Script* Script::find(const char* name)
{
	s32 i = 0;
	while (true)
	{
		if (!Script::all[i].name)
			break;

		if (strcmp(Script::all[i].name, name) == 0)
			return &Script::all[i];

		i++;
	}
	return nullptr;
}

namespace Soren
{
	enum class Face
	{
		Default,
		Sad,
		Upbeat,
		Urgent,
		EyesClosed,
		Smile,
		Wat,
		Unamused,
		Angry,
		Concerned,
		count,
	};

	enum class Mode
	{
		Hidden,
		Center,
		Left,
		TextOnly,
	};

	// UV origin: top left
	const Vec2 face_texture_size = Vec2(91.0f, 57.0f);
	const Vec2 face_size = Vec2(17.0f, 27.0f);
	const Vec2 face_uv_size = face_size / face_texture_size;
	const Vec2 face_pixel = Vec2(1.0f, 1.0f) / face_texture_size;
	const Vec2 face_offset = face_uv_size + face_pixel;
	const Vec2 face_origin = face_pixel;
	const Vec2 faces[(s32)Face::count] =
	{
		face_origin + Vec2(face_offset.x * 0, 0),
		face_origin + Vec2(face_offset.x * 1, 0),
		face_origin + Vec2(face_offset.x * 2, 0),
		face_origin + Vec2(face_offset.x * 3, 0),
		face_origin + Vec2(face_offset.x * 4, 0),
		face_origin + Vec2(face_offset.x * 0, face_offset.y),
		face_origin + Vec2(face_offset.x * 1, face_offset.y),
		face_origin + Vec2(face_offset.x * 2, face_offset.y),
		face_origin + Vec2(face_offset.x * 3, face_offset.y),
		face_origin + Vec2(face_offset.x * 4, face_offset.y),
	};

	template<typename T> struct Schedule
	{
		struct Entry
		{
			r32 time;
			T data;
		};

		s32 index;
		Array<Entry> entries;

		Schedule()
			: index(-1), entries()
		{
		}

		const T current() const
		{
			if (index < 0)
				return T();
			return entries[index].data;
		}

		b8 active() const
		{
			return index >= 0 && index < entries.length;
		}

		void schedule(r32 f, const T& data)
		{
			entries.add({ f, data });
		}

		void clear()
		{
			index = -1;
			entries.length = 0;
		}

		b8 update(const Update& u, r32 t)
		{
			if (index < entries.length - 1)
			{
				Entry& next_entry = entries[index + 1];
				if (t <= next_entry.time && t + u.time.delta > next_entry.time)
				{
					index++;
					return true;
				}
			}
			return false;
		}
	};

	typedef void (*Callback)(const Update&);
	typedef void (*ChoiceCallback)(const Update&, const char*);

	struct Choice
	{
		ChoiceCallback callback;
		const char* a;
		const char* b;
		const char* c;
	};

	struct Data
	{
		r32 time;
		Schedule<const char*> texts;
		Schedule<Face> faces;
		Schedule<AkUniqueID> audio_events;
		Schedule<Callback> callbacks;
		Schedule<Choice> choices;
		Schedule<Mode> modes;

		UIText text;
		r32 text_clip;

		UIMenu menu;
	};

	static Data* data;

	void clear()
	{
		data->time = 0.0f;
		data->texts.clear();
		data->faces.clear();
		data->audio_events.clear();
		data->callbacks.clear();
		data->choices.clear();
		data->modes.clear();
	}

	void update(const Update& u)
	{
		data->modes.update(u, data->time);
		data->faces.update(u, data->time);

		if (data->texts.update(u, data->time))
		{
			data->text.clip = data->text_clip = 1;
			data->text.wrap_width = 384.0f * UI::scale;
			data->text.text(data->texts.current());
		}

		if (data->callbacks.update(u, data->time))
			data->callbacks.current()(u);

		data->choices.update(u, data->time);

		if (data->audio_events.update(u, data->time))
			Audio::post_global_event(data->audio_events.current());

		if (data->text.clipped())
		{
			// We haven't shown the whole string yet
			r32 delta = u.time.delta * 80.0f;
			data->text_clip += delta;
			data->text.clip = data->text_clip;

			if ((s32)data->text_clip % 3 == 0
				&& (s32)(data->text_clip - delta) < (s32)data->text_clip
				&& data->text.rendered_string[(s32)data->text_clip] != ' '
				&& data->text.rendered_string[(s32)data->text_clip] != '\t'
				&& data->text.rendered_string[(s32)data->text_clip] != '\n')
			{
				Audio::post_global_event(AK::EVENTS::PLAY_CONSOLE_KEY);
			}
		}

		// show choices
		data->menu.clear();
		if (data->choices.active())
		{
			const Choice& choice = data->choices.current();
			if (choice.a || choice.b || choice.c)
			{
				data->menu.start(u, 0);

				Vec2 p(u.input->width * 0.5f + MENU_ITEM_WIDTH * -0.5f, u.input->height * 0.2f);

				if (choice.a && data->menu.item(u, 0, &p, choice.a) && choice.callback)
					choice.callback(u, choice.a);
				if (choice.b && data->menu.item(u, 0, &p, choice.b) && choice.callback)
					choice.callback(u, choice.b);
				if (choice.c && data->menu.item(u, 0, &p, choice.c) && choice.callback)
					choice.callback(u, choice.c);

				data->menu.end();
			}
		}

		data->time += u.time.delta;
	}

	void draw(const RenderParams& params)
	{
		Face face = data->faces.current();

		const Rect2& vp = params.camera->viewport;

		r32 scale = UI::scale;
		Vec2 pos;
		switch (data->modes.current())
		{
			case Mode::Center:
			case Mode::TextOnly:
			{
				pos = vp.pos + vp.size * 0.5f;
				scale *= 4.0f;
				break;
			}
			case Mode::Left:
			{
				pos = vp.pos + vp.size * Vec2(0.1f, 0.25f);
				scale *= 3.0f;
				break;
			}
			case Mode::Hidden:
			{
				return;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		if (data->modes.current() != Mode::TextOnly)
		{
			// frame
			{
				// visualize dialogue volume
				r32 volume_scale = 1.0f + Audio::dialogue_volume * 0.5f;
				Vec2 frame_size(28.0f * scale * volume_scale);
				UI::centered_box(params, { pos, frame_size }, UI::background_color, PI * 0.25f);
				UI::centered_border(params, { pos, frame_size }, 2, UI::default_color, PI * 0.25f);
			}

			// face
			{
				if (face == Face::Default)
				{
					// blink
					const r32 blink_delay = 3.5f;
					const r32 blink_time = 0.1f;
					if (fmod(data->time, blink_delay) < blink_time)
						face = Face::EyesClosed;
				}

				Vec2 face_uv = faces[(s32)face];

				UI::sprite(params, Asset::Texture::soren, { pos, face_size * scale }, UI::default_color, { face_uv, face_uv_size });
			}
		}

		// text
		if (data->text.bounds().length_squared() > 0.0f)
		{
			Vec2 pos;
			switch (data->modes.current())
			{
				case Mode::Center:
				case Mode::TextOnly:
				{
					data->text.anchor_x = UIText::Anchor::Center;
					data->text.anchor_y = UIText::Anchor::Min;
					pos = Vec2(vp.size.x * 0.5f, vp.size.y * 0.25f);
					break;
				}
				case Mode::Left:
				{
					data->text.anchor_x = UIText::Anchor::Min;
					data->text.anchor_y = UIText::Anchor::Center;
					pos = Vec2(vp.size.x * 0.18f, vp.size.y * 0.25f);
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			UI::box(params, data->text.rect(pos).outset(12.0f * UI::scale), UI::background_color);
			UI::border(params, data->text.rect(pos).outset(12.0f * UI::scale), 2, UI::default_color);
			data->text.draw(params, pos);
		}

		// menu
		data->menu.draw_alpha(params);
	}

	void cleanup()
	{
		delete data;
	}

	void init()
	{
		data = new Data();
		Game::updates.add(update);
		Game::cleanups.add(cleanup);
		Game::draws.add(draw);

		Loader::texture(Asset::Texture::soren, RenderTextureWrap::Clamp, RenderTextureFilter::Nearest);
	}
}

namespace scene
{
	struct Data
	{
		UIText text;
		r32 clip;
		Camera* camera;
	};
	
	static Data* data;

	void cleanup()
	{
		data->camera->remove();
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		data = new Data();

		Transform* camera = entities.find("Camera")->get<Transform>();

		data->camera = Camera::add();
		data->camera->viewport =
		{
			Vec2(0, 0),
			Vec2(u.input->width, u.input->height),
		};
		r32 aspect = data->camera->viewport.size.y == 0 ? 1 : (r32)data->camera->viewport.size.x / (r32)data->camera->viewport.size.y;
		data->camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Skybox::far_plane);
		data->camera->pos = camera->pos;
		data->camera->rot = Quat::look(camera->rot * Vec3(0, -1, 0));

		Game::cleanups.add(cleanup);
	}
}

namespace start
{
	struct Data
	{
		Camera* camera;
	};

	Data* data;

	void cleanup()
	{
		data->camera->remove();
		delete data;
	}

	void go(const Update& u)
	{
		Menu::transition(Asset::Level::tutorial_01);
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(cleanup);

		data->camera = Camera::add();
		data->camera->viewport =
		{
			Vec2(0, 0),
			Vec2(u.input->width, u.input->height),
		};
		r32 aspect = data->camera->viewport.size.y == 0 ? 1 : (r32)data->camera->viewport.size.x / (r32)data->camera->viewport.size.y;
		data->camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.01f, 100.0f);

		Soren::init();
		Soren::data->modes.schedule(0.0f, Soren::Mode::Center);
		Soren::data->faces.schedule(0.0f, Soren::Face::EyesClosed);
		Soren::data->faces.schedule(1.0f, Soren::Face::Default);
		Soren::data->audio_events.schedule(2.0f, AK::EVENTS::SOREN1);
		Soren::data->texts.schedule(2.0f, "Hello. I am Soren.");
		Soren::data->texts.schedule(4.0f, "My job is to match you against other online players.");
		Soren::data->faces.schedule(4.0f, Soren::Face::Upbeat);
		Soren::data->faces.schedule(8.0f, Soren::Face::Default);
		Soren::data->texts.schedule(8.0f, "But first, let's load my favorite map: tutorial 01.");
		Soren::data->faces.schedule(10.0f, Soren::Face::Smile);
		Soren::data->texts.schedule(13.0f, "Are you ready?");
		Soren::data->faces.schedule(13.0f, Soren::Face::Default);
		Soren::data->choices.schedule(13.0f, { nullptr, "Yes", "No" });
		Soren::data->texts.schedule(14.0f, "Great. I'm so glad.");
		Soren::data->faces.schedule(14.0f, Soren::Face::Unamused);
		Soren::data->choices.schedule(14.0f, { });
		Soren::data->faces.schedule(15.0f, Soren::Face::Smile);
		Soren::data->callbacks.schedule(17.5f, go);
	}
}

namespace tutorial01
{
	struct Data
	{
		bool minion_dialogue_done;
		bool movement_tutorial_done;
	};
	static Data* data;

	void minion1_dialogue(Entity*)
	{
		if (!data->minion_dialogue_done)
		{
			data->minion_dialogue_done = true;
			Soren::clear();
			Soren::data->modes.schedule(1.0f, Soren::Mode::TextOnly);
			Soren::data->texts.schedule(1.0f, "When a minion's health is low, its helmet opens to expose the head.");
			Soren::data->modes.schedule(6.0f, Soren::Mode::Hidden);
		}
	}

	void minion2_dialogue(Entity*)
	{
		Team::list[1].set_spawn_vulnerable();

		Soren::clear();
		Soren::data->modes.schedule(1.0f, Soren::Mode::TextOnly);
		Soren::data->texts.schedule(1.0f, "Destroy the enemy spawn.");
	}

	void done(const Update&)
	{
		Menu::transition(Asset::Level::tutorial_02);
	}

	void destroyed_enemy_spawn()
	{
		Soren::clear();
		Soren::data->modes.schedule(2.0f, Soren::Mode::TextOnly);
		Soren::data->texts.schedule(2.0f, "Tutorial 01 complete.");
		Soren::data->callbacks.schedule(4.0f, &done);
	}

	void shoot_tutorial(Entity*)
	{
		Game::data.allow_detach = true;
		Soren::clear();
		Soren::data->modes.schedule(0.0f, Soren::Mode::TextOnly);
		Soren::data->texts.schedule(0.0f, "[{{Primary}}] to shoot. [{{Secondary}}] to zoom.");
	}

	void movement_tutorial_done(Entity*)
	{
		if (!data->movement_tutorial_done)
		{
			data->movement_tutorial_done = true;
			Soren::clear();
		}
	}

	void cleanup()
	{
		Game::data.allow_detach = true;
		delete data;
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		data = new Data();
		Game::cleanups.add(cleanup);
		Game::data.allow_detach = false;

		Soren::init();
		Soren::data->modes.schedule(3.0f, Soren::Mode::TextOnly);
		Soren::data->texts.schedule(3.0f, "Find the minion and shoot through its head.");
		Soren::data->modes.schedule(8.0f, Soren::Mode::Hidden);

		entities.find("minion1")->get<Health>()->killed.link(&minion1_dialogue);
		entities.find("minion2")->get<Health>()->killed.link(&minion2_dialogue);
		entities.find("shoot_tutorial")->get<PlayerTrigger>()->entered.link(&shoot_tutorial);
		entities.find("movement_tutorial_done")->get<PlayerTrigger>()->entered.link(&movement_tutorial_done);
		Team::list[1].lost.link(&destroyed_enemy_spawn);
	}
}

namespace level4
{
	struct Data
	{
		r32 danger;
		r32 last_danger_time;
	};

	static Data* data;

	void update(const Update& u)
	{
		b8 alert = false;
		for (auto i = Awk::list.iterator(); !i.is_last(); i.next())
		{
			for (auto j = i; !j.is_last(); j.next())
			{
				if (i.item()->get<AIAgent>()->team != j.item()->get<AIAgent>()->team)
				{
					if ((i.item()->get<Transform>()->absolute_pos() - j.item()->get<Transform>()->absolute_pos()).length_squared() < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
					{
						alert = true;
						break;
					}
				}
			}
			if (alert)
				break;
		}

		const r32 danger_rampup_time = 2.0f;
		const r32 danger_linger_time = 3.0f;
		const r32 danger_rampdown_time = 4.0f;
		
		b8 update_audio = false;
		if (alert)
		{
			data->last_danger_time = Game::real_time.total;
			data->danger = fmin(1.0f, data->danger + Game::real_time.delta / danger_rampup_time);
			update_audio = true;
		}
		else if (Game::real_time.total - data->last_danger_time > danger_linger_time)
		{
			data->danger = fmax(0.0f, data->danger - Game::real_time.delta / danger_rampdown_time);
			update_audio = true;
		}

		if (update_audio)
			Audio::global_param(AK::GAME_PARAMETERS::DANGER, data->danger);
	}

	void cleanup()
	{
		delete data;
	}

	void init(const Update& u, const EntityFinder& entities)
	{
		data = new Data();

		s32 player_count = LocalPlayer::list.count();

		// TODO: spawn enemies based on number of local players

		Game::updates.add(update);
		Game::cleanups.add(cleanup);
	}

	void init_pvp(const Update& u, const EntityFinder& entities)
	{
		data = new Data();

		Game::updates.add(update);
		Game::cleanups.add(cleanup);
	}
}

Script Script::all[] =
{
	{ "scene", scene::init },
	{ "start", start::init },
	{ "level4", level4::init },
	{ "pvp", level4::init_pvp },
	{ "tutorial01", tutorial01::init },
	{ 0, 0, },
};

}
