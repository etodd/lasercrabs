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

namespace soren
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
		count
	};

	// UV origin: top left
	const Vec2 face_texture_size = Vec2(91.0f, 57.0f);
	const Vec2 face_size = Vec2(17.0f, 27.0f) / face_texture_size;
	const Vec2 face_pixel = Vec2(1.0f, 1.0f) / face_texture_size;
	const Vec2 face_offset = face_size + face_pixel;
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

		const T& current() const
		{
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
		Entity* base;
		Entity* screen;
		Entity* rim;
		r32 time;
		Schedule<const char*> texts;
		Schedule<Face> faces;
		Schedule<AkUniqueID> audio_events;
		Schedule<Callback> callbacks;
		Schedule<Choice> choices;
		Face face;

		UIText text;
		r32 text_clip;

		UIMenu menu;
	};

	static Data* data;

	void face(const Update& u, Face f)
	{
		Mesh* m = Loader::mesh(Asset::Mesh::soren_screen);
		u.render->write(RenderOp::UpdateAttribBuffer);
		u.render->write(Asset::Mesh::soren_screen);
		u.render->write(2); // UV
		u.render->write(4); // element count

		Vec2 uv = faces[(s32)f];

		Vec2 uvs[4] =
		{
			uv + Vec2(face_size.x, face_size.y),
			uv + Vec2(face_size.x, 0),
			uv,
			uv + Vec2(0, face_size.y),
		};

		u.render->write(uvs, 4);
	}

	void update(const Update& u)
	{
		if (data->faces.update(u, data->time))
		{
			data->face = data->faces.current();
			face(u, data->face);
		}

		if (data->face == Face::Default)
		{
			const r32 blink_delay = 3.5f;
			const r32 blink_time = 0.1f;
			face(u, fmod(data->time, blink_delay) < blink_time ? Face::EyesClosed : data->face);
		}

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

		// visualize dialogue volume
		{
			r32 scale = 1.0f + Audio::dialogue_volume * 0.5f;
			data->rim->get<View>()->offset = Mat4::make_scale(Vec3(scale, scale, 1.0f));
		}

		data->time += u.time.delta;
	}

	void draw(const RenderParams& params)
	{
		if (data->text.bounds().length_squared() > 0.0f)
		{
			const Rect2& vp = params.camera->viewport;
			Vec2 p = Vec2(vp.size.x * 0.5f, vp.size.y * 0.25f);
			UI::box(params, data->text.rect(p).outset(12.0f * UI::scale), Vec4(0, 0, 0, 1));
			data->text.draw(params, p);
		}

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

		Entity* base = data->base = World::create<Prop>(Asset::Mesh::soren_case);
		Entity* screen = data->screen = World::create<Prop>(Asset::Mesh::soren_screen);
		Entity* rim = data->rim = World::create<Prop>(Asset::Mesh::soren_rim);

		base->get<Transform>()->pos = Vec3(0, 0.6f, 2.5f);
		base->get<Transform>()->rot = Quat::euler(0, PI, 0);
		screen->get<Transform>()->parent = base->get<Transform>();
		screen->get<View>()->alpha();

		Loader::texture(Asset::Texture::soren, RenderTextureWrap::Clamp, RenderTextureFilter::Nearest);
		screen->get<View>()->texture = Asset::Texture::soren;
		screen->get<View>()->shader = Asset::Shader::flat_texture;
		rim->get<Transform>()->parent = base->get<Transform>();
		rim->get<View>()->alpha();
		rim->get<View>()->shader = Asset::Shader::flat;

		data->text.anchor_x = UIText::Anchor::Center;
		data->text.anchor_y = UIText::Anchor::Min;
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
		Menu::transition(Asset::Level::transit);
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

		soren::init();
		soren::data->audio_events.schedule(1.0f, AK::EVENTS::PLAY_SOREN1);
		soren::data->texts.schedule(1.0f, "Hello. I am Soren.");
		soren::data->faces.schedule(0.0f, soren::Face::Default);
		soren::data->texts.schedule(3.0f, "My job is to match you against other online players.");
		soren::data->faces.schedule(3.0f, soren::Face::Upbeat);
		soren::data->faces.schedule(7.0f, soren::Face::Default);
		soren::data->texts.schedule(7.0f, "But first, let's load my favorite map: tutorial 01.");
		soren::data->faces.schedule(9.0f, soren::Face::Smile);
		soren::data->texts.schedule(12.0f, "Are you ready?");
		soren::data->faces.schedule(12.0f, soren::Face::Default);
		soren::data->choices.schedule(12.0f, { nullptr, "Yes", "No" });
		soren::data->texts.schedule(13.0f, "Great. I'm so glad.");
		soren::data->faces.schedule(13.0f, soren::Face::Unamused);
		soren::data->choices.schedule(13.0f, { });
		soren::data->faces.schedule(14.0f, soren::Face::Smile);
		soren::data->callbacks.schedule(16.5f, go);
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
	{ 0, 0, },
};

}
