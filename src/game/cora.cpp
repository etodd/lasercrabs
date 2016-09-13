#include "cora.h"
#include "console.h"
#include "game.h"
#include "mersenne/mersenne-twister.h"
#include "audio.h"
#include "menu.h"
#include "asset/level.h"
#include "asset/Wwise_IDs.h"
#include "asset/mesh.h"
#include "asset/font.h"
#include "asset/dialogue.h"
#include "asset/texture.h"
#include "asset/lookup.h"
#include "strings.h"
#include "load.h"
#include "entities.h"
#include "render/particles.h"
#include "data/import_common.h"
#include "sha1/sha1.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "usernames.h"
#include <string>
#include <unordered_map>
#include "utf8/utf8.h"

namespace VI
{


#define MAX_NODES 4096
#define MAX_BRANCHES 8
#define MAX_CHOICES 4
#define MAX_JOIN_TIME 15.0f
namespace Cora
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
		EyeRoll,
		Shifty,
		Sarcastic,
		Evil,
		TongueOut,
		count,
	};

	struct Node
	{
		static Node list[MAX_NODES];
		enum class Type
		{
			Node,
			Text,
			Choice,
			Branch,
			Set,
		};

		struct Branch
		{
			AssetID value;
			ID target;
		};

		struct Text
		{
			Face face;
			r32 delay;
			AkUniqueID sound;
		};

		struct BranchData
		{
			AssetID variable;
			Branch branches[MAX_BRANCHES];
		};

		struct Set
		{
			AssetID variable;
			AssetID value;
		};

		Type type;
		AssetID name;
		ID next;
		ID choices[MAX_CHOICES];
		union
		{
			Text text;
			Set set;
			BranchData branch;
		};
	};

	Node Node::list[MAX_NODES];

	// map string IDs to node IDs
	// note: multiple nodes may use the same string ID
	ID node_lookup[(s32)strings::count];

	void global_init()
	{
		Loader::font_permanent(Asset::Font::pt_sans);

		Array<cJSON*> trees;
		std::unordered_map<std::string, ID> id_lookup;

		// load dialogue trees and build node ID lookup table
		{
			ID current_node_id = 0;
			for (s32 i = 0; i < Asset::DialogueTree::count; i++)
			{
				cJSON* tree = Loader::dialogue_tree(i);
				trees.add(tree);

				cJSON* json_node = trees[i]->child;

				while (json_node)
				{
					vi_assert(current_node_id < MAX_NODES);
					const char* id = Json::get_string(json_node, "id");
					id_lookup[id] = current_node_id;
					current_node_id++;
					json_node = json_node->next;
				}
			}
		}

		// parse nodes

		ID current_node_id = 0;

		for (s32 tree_index = 0; tree_index < Asset::DialogueTree::count; tree_index++)
		{
			cJSON* json_node = trees[tree_index]->child;

			while (json_node)
			{
				Node* node = &Node::list[current_node_id];

				// type
				{
					const char* type = Json::get_string(json_node, "type");
					if (utf8cmp(type, "Node") == 0)
						node->type = Node::Type::Node;
					else if (utf8cmp(type, "Text") == 0)
						node->type = Node::Type::Text;
					else if (utf8cmp(type, "Choice") == 0)
						node->type = Node::Type::Choice;
					else if (utf8cmp(type, "Branch") == 0)
						node->type = Node::Type::Branch;
					else if (utf8cmp(type, "Set") == 0)
						node->type = Node::Type::Set;
					else
						vi_assert(false);
				}

				// name
				const char* name_str = Json::get_string(json_node, "name");
				char hash[41];

				{
					if (node->type == Node::Type::Text || node->type == Node::Type::Choice)
					{
						sha1::hash(name_str, hash);
						name_str = hash;
					}

					node->name = strings_get(name_str);
					if (node->name != AssetNull && node->name < (s32)strings::count)
						node_lookup[node->name] = current_node_id;
				}

				// next
				{
					const char* next = Json::get_string(json_node, "next");
					if (next)
						node->next = id_lookup[next];
					else
						node->next = IDNull;
				}

				if (node->type == Node::Type::Text)
				{
					// face
					const char* face = Json::get_string(json_node, "face");
					if (face)
					{
						if (utf8cmp(face, "Sad") == 0)
							node->text.face = Face::Sad;
						else if (utf8cmp(face, "Upbeat") == 0)
							node->text.face = Face::Upbeat;
						else if (utf8cmp(face, "Urgent") == 0)
							node->text.face = Face::Urgent;
						else if (utf8cmp(face, "EyesClosed") == 0)
							node->text.face = Face::EyesClosed;
						else if (utf8cmp(face, "Smile") == 0)
							node->text.face = Face::Smile;
						else if (utf8cmp(face, "Wat") == 0)
							node->text.face = Face::Wat;
						else if (utf8cmp(face, "Unamused") == 0)
							node->text.face = Face::Unamused;
						else if (utf8cmp(face, "Angry") == 0)
							node->text.face = Face::Angry;
						else if (utf8cmp(face, "Concerned") == 0)
							node->text.face = Face::Concerned;
						else if (utf8cmp(face, "EyeRoll") == 0)
							node->text.face = Face::EyeRoll;
						else if (utf8cmp(face, "Shifty") == 0)
							node->text.face = Face::Shifty;
						else if (utf8cmp(face, "Sarcastic") == 0)
							node->text.face = Face::Sarcastic;
						else if (utf8cmp(face, "Evil") == 0)
							node->text.face = Face::Evil;
						else if (utf8cmp(face, "TongueOut") == 0)
							node->text.face = Face::TongueOut;
						else
							node->text.face = Face::Default;
					}
					else
						node->text.face = Face::Default;

					// delay
					node->text.delay = Json::get_r32(json_node, "delay");

					// sound
					if (name_str)
					{
						char event_name[512];
						sprintf(event_name, "Play_%s", name_str);
						node->text.sound = Audio::get_id(event_name);
					}
					else
						node->text.sound = AK_INVALID_UNIQUE_ID;
#if !DEBUG
					vi_assert(node->text.sound != AK_INVALID_UNIQUE_ID);
#endif
				}

				// choices
				{
					for (s32 i = 0; i < MAX_CHOICES; i++)
						node->choices[i] = IDNull;

					cJSON* json_choices = cJSON_GetObjectItem(json_node, "choices");
					if (json_choices)
					{
						s32 i = 0;
						cJSON* json_choice = json_choices->child;
						while (json_choice)
						{
							vi_assert(i < MAX_CHOICES);
							node->choices[i] = id_lookup[json_choice->valuestring];
							i++;
							json_choice = json_choice->next;
						}
					}
				}

				if (node->type == Node::Type::Branch || node->type == Node::Type::Set)
				{
					const char* variable = Json::get_string(json_node, "variable");
					if (variable)
					{
						node->set.variable = strings_get(variable);
						vi_assert(node->set.variable != AssetNull);
					}

					const char* value = Json::get_string(json_node, "value");
					if (value)
					{
						node->set.value = strings_get(value);
						vi_assert(node->set.value != AssetNull);
					}
				}

				if (node->type == Node::Type::Branch)
				{
					cJSON* json_branches = cJSON_GetObjectItem(json_node, "branches");
					if (json_branches)
					{
						for (s32 j = 0; j < MAX_BRANCHES; j++)
						{
							node->branch.branches[j].target = IDNull;
							node->branch.branches[j].value = AssetNull;
						}

						cJSON* json_branch = json_branches->child;
						s32 j = 0;
						while (json_branch)
						{
							vi_assert(j < MAX_BRANCHES);
							node->branch.branches[j].value = strings_get(json_branch->string);
							vi_assert(node->branch.branches[j].value != AssetNull);
							if (json_branch->valuestring)
								node->branch.branches[j].target = id_lookup[json_branch->valuestring];
							else
								node->branch.branches[j].target = IDNull;
							json_branch = json_branch->next;
							j++;
						}
					}
				}

				// check for disconnected nodes
				if (
					node->next == IDNull
					&& node->type != Node::Type::Node // okay for us to end on a node
					&& (node->type != Node::Type::Text || node->choices[0] == IDNull) // if it's a text and it doesn't have a next, it must have a choice
					&& (node->type != Node::Type::Branch || node->branch.branches[0].target == IDNull) // if it's a branch, it must have at least one branch target
					)
				{
					vi_debug("Dangling dialogue node in file: %s - %s", AssetLookup::DialogueTree::values[tree_index], Json::get_string(json_node, "name"));
					vi_assert(false);
				}

				json_node = json_node->next;

				current_node_id++;
			}

		}

		for (s32 i = 0; i < trees.length; i++)
			Loader::dialogue_tree_free(trees[i]);
	}

	// UV origin: top left
	const Vec2 face_texture_size = Vec2(91.0f, 85.0f);
	const Vec2 face_size = Vec2(17.0f, 27.0f);
	const Vec2 face_uv_size = face_size / face_texture_size;
	const Vec2 face_pixel = Vec2(1.0f, 1.0f) / face_texture_size;
	const Vec2 face_offset = face_uv_size + face_pixel;
	const Vec2 face_origin = face_pixel;
	const Vec2 faces[(s32)Face::count] =
	{
		face_origin + Vec2(face_offset.x * 0, face_offset.y * 0),
		face_origin + Vec2(face_offset.x * 1, face_offset.y * 0),
		face_origin + Vec2(face_offset.x * 2, face_offset.y * 0),
		face_origin + Vec2(face_offset.x * 3, face_offset.y * 0),
		face_origin + Vec2(face_offset.x * 4, face_offset.y * 0),
		face_origin + Vec2(face_offset.x * 0, face_offset.y * 1),
		face_origin + Vec2(face_offset.x * 1, face_offset.y * 1),
		face_origin + Vec2(face_offset.x * 2, face_offset.y * 1),
		face_origin + Vec2(face_offset.x * 3, face_offset.y * 1),
		face_origin + Vec2(face_offset.x * 4, face_offset.y * 1),
		face_origin + Vec2(face_offset.x * 0, face_offset.y * 2),
		face_origin + Vec2(face_offset.x * 1, face_offset.y * 2),
		face_origin + Vec2(face_offset.x * 2, face_offset.y * 2),
		face_origin + Vec2(face_offset.x * 3, face_offset.y * 2),
		face_origin + Vec2(face_offset.x * 4, face_offset.y * 2),
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
				const Entry& next_entry = entries[index + 1];
				if (t > next_entry.time)
				{
					index++;
					return true;
				}
			}
			return false;
		}
	};

	typedef void (*Callback)(const Update&);

	struct Choice
	{
		ID a;
		ID b;
		ID c;
		ID d;
	};

	struct LeaderboardEntry
	{
		const char* name;
		s32 rating;
	};

#define LEADERBOARD_COUNT 5

	struct Data
	{
		r32 time;
		Schedule<const char*> texts;
		Schedule<Face> faces;
		Schedule<AkUniqueID> audio_events;
		Schedule<Callback> callbacks;
		Schedule<Choice> choices;
		Schedule<AssetID> node_executions;
		Mode mode;
		ID current_text_node;
		b8 conversation_in_progress;
		r32 particle_accumulator;
		r32 animation_time;

		UIText text;
		r32 text_animation_time;

		UIMenu menu;

		LinkArg<AssetID> node_executed;
		Link conversation_finished;
	};

	static Data* data;

	b8 has_focus()
	{
		return data && data->mode == Mode::Active;
	}

	LinkArg<AssetID>& node_executed()
	{
		return data->node_executed;
	}

	Link& conversation_finished()
	{
		return data->conversation_finished;
	}

	void text_clear()
	{
		data->texts.clear();
		data->text.text("");
	}

	void text_schedule(r32 time, const char* txt)
	{
		data->texts.schedule(data->time + time, txt);
	}

	void clear()
	{
		if (data)
		{
			data->current_text_node = IDNull;
			data->time = 0.0f;
			data->text.text(nullptr);
			data->mode = Mode::Hidden;
			data->texts.clear();
			data->faces.clear();
			data->audio_events.clear();
			data->callbacks.clear();
			data->choices.clear();
			data->node_executions.clear();
		}
		Audio::post_global_event(AK::EVENTS::STOP_DIALOGUE);
		Audio::dialogue_done = false;
	}

	void variable(AssetID variable, AssetID value)
	{
		Game::save.variables[variable] = value;
	}

	AssetID variable(AssetID variable)
	{
		auto i = Game::save.variables.find(variable);
		if (i == Game::save.variables.end())
			return AssetNull;
		else
			return i->second;
	}

	void execute(ID node_id, r32 time = 0.0f)
	{
		const Node& node = Node::list[node_id];
		if (node.name != AssetNull)
			data->node_executions.schedule(time, node.name);
		switch (node.type)
		{
			case Node::Type::Text:
			{
				const char* str = _(node.name);
				data->texts.schedule(time, str);
				data->faces.schedule(time, node.text.face);
				if (node.text.sound != AK_INVALID_UNIQUE_ID)
					data->audio_events.schedule(time, node.text.sound);
				// stop executing the dialogue tree at this point
				// and save our current location in the tree
				// once the dialogue has been spoken, we will resume executing the tree
				data->current_text_node = node_id;
				break;
			}
			case Node::Type::Branch:
			{
				AssetID value = variable(node.branch.variable);
				b8 found_branch = false;
				for (s32 i = 0; i < MAX_BRANCHES; i++)
				{
					if (node.branch.branches[i].value == AssetNull)
						break;
					if (node.branch.branches[i].value == value)
					{
						if (node.branch.branches[i].target != IDNull)
							execute(node.branch.branches[i].target, time);
						found_branch = true;
						break;
					}
				}
				if (!found_branch)
				{
					// take default option
					for (s32 i = 0; i < MAX_BRANCHES; i++)
					{
						if (node.branch.branches[i].value == strings::_default)
						{
							if (node.branch.branches[i].target != IDNull)
								execute(node.branch.branches[i].target, time);
							break;
						}
					}
				}
				break;
			}
			case Node::Type::Choice:
			{
				// schedule a choice if one has not already been scheduled
				if (data->choices.entries.length == 0 || data->choices.entries[data->choices.entries.length - 1].time != time)
					data->choices.schedule(time, { IDNull, IDNull, IDNull, IDNull });

				// add this choice to the entry
				Schedule<Choice>::Entry& choice_entry = data->choices.entries[data->choices.entries.length - 1];
				if (choice_entry.data.a == IDNull)
					choice_entry.data.a = node_id;
				else if (choice_entry.data.b == IDNull)
					choice_entry.data.b = node_id;
				else if (choice_entry.data.c == IDNull)
					choice_entry.data.c = node_id;
				else if (choice_entry.data.d == IDNull)
					choice_entry.data.d = node_id;

				break;
			}
			case Node::Type::Set:
			{
				variable(node.set.variable, node.set.value);
				if (node.next != IDNull)
					execute(node.next, time);
				break;
			}
			case Node::Type::Node:
			{
				if (node.next == IDNull)
				{
					for (s32 i = 0; i < MAX_CHOICES; i++)
					{
						ID choice = node.choices[i];
						if (choice == IDNull)
							break;
						else
							execute(choice, data->time);
					}
				}
				else
					execute(node.next, time);
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}
	}

	void clear_and_execute(ID id, r32 delay = 0.0f)
	{
		clear();
		data->mode = Mode::Active;
		execute(id, delay);
	}

	void go(AssetID name)
	{
		r32 delay;
		if (data->mode == Mode::Hidden)
		{
			data->animation_time = Game::real_time.total;
			delay = 1.0f;
		}
		else
			delay = 0.5f;
		clear_and_execute(node_lookup[name], delay);
	}

	void conversation_in_progress(b8 a)
	{
		data->conversation_in_progress = a;
	}

	void activate(AssetID entry_point)
	{
		conversation_in_progress(true);
		go(entry_point);
	}

	void update(const Update& u)
	{
		if (Audio::dialogue_done)
		{
			Audio::dialogue_done = false;

			// we've completed displaying a text message
			// continue executing the dialogue tree
			if (data->current_text_node != IDNull) // might be null if the dialogue got cut off or something
			{
				Node& node = Node::list[data->current_text_node];
				data->current_text_node = IDNull;
				if (node.next == IDNull)
				{
					for (s32 i = 0; i < MAX_CHOICES; i++)
					{
						ID choice = node.choices[i];
						if (choice == IDNull)
							break;
						else
							execute(choice, data->time + node.text.delay);
					}
				}
				else
					execute(node.next, data->time + node.text.delay);
			}
		}

		if (data->node_executions.update(u, data->time))
			data->node_executed.fire(data->node_executions.current());
		data->faces.update(u, data->time);

		if (data->texts.update(u, data->time))
		{
			data->text.clip = 1;
			data->text_animation_time = Game::real_time.total;
			data->text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			data->text.text_raw(data->texts.current());
		}

		if (data->callbacks.update(u, data->time))
			data->callbacks.current()(u);

		if (data->audio_events.update(u, data->time))
		{
#if DEBUG
			// HACK to keep things working even with missing audio
			if (!Audio::post_dialogue_event(data->audio_events.current()))
				Audio::post_dialogue_event(AK::EVENTS::PLAY_9E8F17E1A058341BE597A4032D80C01BF566D167);
#else
			Audio::post_dialogue_event(data->audio_events.current());
#endif
		}

		UIMenu::text_clip(&data->text, data->text_animation_time, 80.0f);

		if (data->choices.update(u, data->time))
			data->menu.animate();

		// show choices
		data->menu.clear();
		if (data->choices.active())
		{
			const Choice& choice = data->choices.current();
			if (choice.a != IDNull)
			{
				s32 choice_count = 1;
				if (choice.b != IDNull)
					choice_count++;
				if (choice.c != IDNull)
					choice_count++;
				if (choice.d != IDNull)
					choice_count++;

				data->menu.start(u, 0, has_focus());

				{
					Node& node = Node::list[choice.a];
					if (data->menu.item(u, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.b != IDNull)
				{
					Node& node = Node::list[choice.b];
					if (data->menu.item(u, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.c != IDNull)
				{
					Node& node = Node::list[choice.c];
					if (data->menu.item(u, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.d != IDNull)
				{
					Node& node = Node::list[choice.d];
					if (data->menu.item(u, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}

				data->menu.end();
			}
		}

		data->time += Game::real_time.delta;
	}

	void draw(const RenderParams& params, const Vec2& pos_center)
	{
		const Rect2& vp = params.camera->viewport;

		// Cora face and UI

		if (data->mode != Mode::Hidden)
		{
			Vec2 pos = pos_center + Vec2(0.0f, 96.0f * UI::scale);
			Face face = data->faces.current();

			r32 scale = UI::scale * 6.0f;

			if (face == Face::Default)
			{
				// blink
				const r32 blink_delay = 4.0f;
				const r32 blink_time = 0.1f;
				if (fmod(data->time, blink_delay) < blink_time)
					face = Face::EyesClosed;
			}

			r32 animation_time = Game::real_time.total - data->animation_time;

			// frame
			{
				// visualize dialogue volume
				r32 volume_scale;
				if (Settings::sfx > 0)
					volume_scale = 1.0f + (Audio::dialogue_volume / ((r32)Settings::sfx / 100.0f)) * 0.5f;
				else
					volume_scale = 1.0f;

				// animate the frame into existence
				r32 animation_scale = Ease::cubic_out(vi_min(1.0f, animation_time * 2.0f), 0.0f, 1.0f);

				Vec2 frame_size(24.0f * scale * volume_scale * animation_scale);
				UI::centered_box(params, { pos, frame_size }, UI::color_default, PI * 0.25f);

				const Vec4* color;
				switch (face)
				{
					case Face::Default:
					case Face::Upbeat:
					case Face::Concerned:
					case Face::TongueOut:
					{
						color = &UI::color_background;
						break;
					}
					case Face::Sad:
					case Face::Unamused:
					case Face::Wat:
					case Face::Shifty:
					{
						color = &Team::ui_color_friend;
						break;
					}
					case Face::EyesClosed:
					{
						color = &UI::color_disabled;
						break;
					}
					case Face::Urgent:
					case Face::Smile:
					{
						color = &UI::color_accent;
						break;
					}
					case Face::Angry:
					case Face::Evil:
					case Face::Sarcastic:
					case Face::EyeRoll:
					{
						color = &UI::color_alert;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				UI::centered_border(params, { pos, frame_size }, scale * 0.6666f, *color, PI * 0.25f);
			}

			// face
			// animation: wait 0.5 seconds before displaying, then flash for 0.5 seconds, then display
			if (animation_time > 1.0f || (animation_time > 0.5f && UI::flash_function(Game::real_time.total)))
			{
				Vec2 face_uv = faces[(s32)face];
				UI::sprite(params, Asset::Texture::cora, { pos, face_size * scale }, UI::color_background, { face_uv, face_uv_size });
			}
		}

		// text
		if (data->text.has_text())
		{
			Vec2 text_pos;
			switch (data->mode)
			{
				case Mode::Hidden:
				{
					data->text.anchor_x = UIText::Anchor::Center;
					data->text.anchor_y = UIText::Anchor::Max;
					data->text.color = UI::color_accent;
					text_pos = pos_center;
					break;
				}
				case Mode::Active:
				{
					data->text.anchor_x = UIText::Anchor::Center;
					data->text.anchor_y = UIText::Anchor::Max;
					data->text.color = UI::color_default;
					text_pos = pos_center + Vec2(0, -64 * UI::scale);
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			UI::box(params, data->text.rect(text_pos).outset(MENU_ITEM_PADDING), UI::color_background);
			data->text.draw(params, text_pos);

			// menu
			data->menu.draw_alpha(params, text_pos + Vec2(0, -data->text.bounds().y + (MENU_ITEM_FONT_SIZE * -UI::scale) - MENU_ITEM_PADDING), UIText::Anchor::Center, UIText::Anchor::Max);
		}
	}

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void on_node_executed(AssetID node)
	{
		if (node == strings::conversation_done)
		{
			conversation_in_progress(false);
			clear();
			data->conversation_finished.fire();
		}
	}

	void init()
	{
		vi_assert(!data);
		data = new Data();
		data->conversation_in_progress = false;
		Audio::dialogue_done = false;
		Game::updates.add(update);
		Game::cleanups.add(cleanup);

		data->node_executed.link(&on_node_executed);

		Loader::texture(Asset::Texture::cora, RenderTextureWrap::Clamp, RenderTextureFilter::Nearest);
	}
}


}
