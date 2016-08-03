#include "terminal.h"
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
#include "utf8/utf8.h"
#include "sha1/sha1.h"
#include "vi_assert.h"
#include "cjson/cJSON.h"
#include "settings.h"
#include "usernames.h"

namespace VI
{


#define MAX_NODES 4096
#define MAX_BRANCHES 8
#define MAX_CHOICES 4
#define MAX_JOIN_TIME 15.0f
namespace Penelope
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

	enum class Matchmake
	{
		None,
		Searching,
		Found,
		Joining,
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
						else
							node->text.face = Face::Default;
					}
					else
						node->text.face = Face::Default;

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
		StaticArray<Ref<PlayerTrigger>, MAX_PLAYERS> terminals;
		Schedule<const char*> texts;
		Schedule<Face> faces;
		Schedule<AkUniqueID> audio_events;
		Schedule<Callback> callbacks;
		Schedule<Choice> choices;
		Schedule<AssetID> node_executions;
		Mode mode;
		Mode default_mode;
		ID current_text_node;
		AssetID entry_point;
		AssetID active_data_fragment;
		r32 fragment_time;
		Matchmake matchmake_mode;
		r32 matchmake_timer;
		s32 matchmake_player_count;
		Link terminal_activated;
		b8 terminal_active;
		r32 particle_accumulator;
		r32 penelope_animation_time;
		b8 leaderboard_active;
		r32 leaderboard_animation_time;
		LeaderboardEntry leaderboard[LEADERBOARD_COUNT];

		UIText text;
		r32 text_animation_time;

		UIMenu menu;

		LinkArg<AssetID> node_executed;
	};

	static Data* data;

	Link& terminal_activated()
	{
		return data->terminal_activated;
	}

	b8 has_focus()
	{
		return data && (data->mode == Mode::Center || data->active_data_fragment != AssetNull);
	}

	void matchmake_search()
	{
		variable(strings::matchmaking, strings::yes);
		data->matchmake_mode = Matchmake::Searching;
		data->matchmake_timer = 10.0f + mersenne::randf_oo() * 90.0f;
	}

	void link_node_executed(void(*handler)(AssetID))
	{
		data->node_executed.link(handler);
	}

	void text_clear()
	{
		data->texts.clear();
	}

	void text_schedule(r32 time, const char* txt)
	{
		data->texts.schedule(time, txt);
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
		data->mode = data->default_mode;
		execute(id, delay);
	}

	void go(AssetID name)
	{
		data->leaderboard_active = false;
		r32 delay;
		if (data->mode == Mode::Hidden)
		{
			data->penelope_animation_time = Game::real_time.total;
			delay = 1.0f;
		}
		else
			delay = 0.5f;
		clear_and_execute(node_lookup[name], delay);
	}

	void terminal_active(b8 a)
	{
		data->terminal_active = a;
	}

	void update(const Update& u)
	{
		PlayerTrigger* terminal = nullptr;
		for (s32 i = 0; i < data->terminals.length; i++)
		{
			if (data->terminals[i].ref()->count() > 0)
			{
				terminal = data->terminals[i].ref();
				break;
			}
		}

		if (data->terminal_active)
		{
			// spawn particles on terminals
			const r32 interval = 0.1f;
			data->particle_accumulator += u.time.delta;
			while (data->particle_accumulator > interval)
			{
				data->particle_accumulator -= interval;
				for (s32 i = 0; i < data->terminals.length; i++)
				{
					PlayerTrigger* terminal = data->terminals[i].ref();
					Vec3 pos = terminal->get<Transform>()->absolute_pos();
					pos.y += 1.0f;

					Particles::eased_particles.add
					(
						pos + Quat::euler(0.0f, mersenne::randf_co() * PI * 2.0f, (mersenne::randf_co() - 0.5f) * PI) * Vec3(0, 0, 2.0f),
						pos,
						0
					);
				}
			}
		}

		if (terminal
			&& !Console::visible
			&& !UIMenu::active[0]
			&& u.last_input->get(Controls::Interact, 0) && !u.input->get(Controls::Interact, 0))
		{
			terminal_active(false);
			if (Game::save.last_round_loss)
				go(strings::consolation);
			else
				go(data->entry_point);
			data->terminal_activated.fire();
		}

		if (Audio::dialogue_done)
		{
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
							execute(choice, data->time);
					}
				}
				else
					execute(node.next, data->time);
			}

			Audio::dialogue_done = false;
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
			b8 success = Audio::post_dialogue_event(data->audio_events.current());
#if DEBUG
			// HACK to keep things working even with missing audio
			if (!success)
				Audio::post_dialogue_event(AK::EVENTS::PLAY_4E8907711D4DB021E656CB8CC752C7A4AF11E1F8);
#endif
		}

		UIMenu::text_clip(&data->text, data->text_animation_time, 80.0f);

		if (data->choices.update(u, data->time))
			data->menu.animate();

		// data fragment
		DataFragment* fragment = nullptr;
		if (PlayerCommon::list.count() > 0)
			fragment = DataFragment::in_range(PlayerCommon::list.iterator().item()->get<Transform>()->absolute_pos());

		if (data->active_data_fragment != AssetNull && !fragment)
			data->active_data_fragment = AssetNull; // player was reading a fragment, but now it's out of range; stop reading it

		// show choices
		data->menu.clear();
		if (data->choices.active() && data->active_data_fragment == AssetNull && !fragment)
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

				data->menu.start(u, 0, choice_count, has_focus());

				Vec2 p;
				if (data->mode == Mode::Left)
					p = Vec2(u.input->width * 0.9f - MENU_ITEM_WIDTH, u.input->height * 0.3f);
				else
					p = Vec2(u.input->width * 0.5f + MENU_ITEM_WIDTH * -0.5f, u.input->height * 0.3f);

				{
					Node& node = Node::list[choice.a];
					if (data->menu.item(u, &p, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.b != IDNull)
				{
					Node& node = Node::list[choice.b];
					if (data->menu.item(u, &p, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.c != IDNull)
				{
					Node& node = Node::list[choice.c];
					if (data->menu.item(u, &p, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}
				if (choice.d != IDNull)
				{
					Node& node = Node::list[choice.d];
					if (data->menu.item(u, &p, _(node.name)))
						clear_and_execute(node.next, 0.25f);
				}

				data->menu.end();
			}
		}

		// get focus if we need it
		if (data->choices.active()
			&& !has_focus() // penelope is waiting on us, but she doesn't have focus yet
			&& !fragment) // fragments take precedence over dialogue
		{
			if (!Console::visible
				&& !UIMenu::active[0]
				&& u.last_input->get(Controls::Interact, 0)
				&& !u.input->get(Controls::Interact, 0))
				data->mode = Mode::Center; // we have focus now!
		}

		// data fragments
		{
			if (data->active_data_fragment == AssetNull)
			{
				if (fragment)
				{
					if (!Console::visible
						&& !UIMenu::active[0]
						&& u.last_input->get(Controls::Interact, 0)
						&& !u.input->get(Controls::Interact, 0))
					{
						fragment->collect();
						data->active_data_fragment = fragment->text();
						data->fragment_time = Game::real_time.total;
					}
				}
			}
			else
			{
				// we're reading a data fragment
				if (!Console::visible && u.last_input->get(Controls::Cancel, 0) && !u.input->get(Controls::Cancel, 0))
					data->active_data_fragment = AssetNull;
			}
		}

		// update matchmaking
		switch (data->matchmake_mode)
		{
			case Matchmake::None:
			{
				break;
			}
			case Matchmake::Searching:
			{
				data->matchmake_timer -= Game::real_time.delta;
				if ((s32)data->matchmake_timer % 5 == 0
					&& (s32)data->matchmake_timer != (s32)(data->matchmake_timer + Game::real_time.delta))
				{
					data->matchmake_player_count = vi_max(5, data->matchmake_player_count + (mersenne::rand() % 5 - 2));
				}

				if (data->matchmake_timer < 0.0f)
				{
					data->matchmake_mode = Matchmake::Found;
					data->matchmake_timer = MAX_JOIN_TIME;
					data->active_data_fragment = AssetNull; // if we're looking at a data fragment, close it
					go(strings::match_found);
				}
				break;
			}
			case Matchmake::Found:
			{
				data->matchmake_timer -= Game::real_time.delta;
				if (data->matchmake_timer < 0.0f)
				{
					clear();
					// don't reset timer; time expired, instantly start the game
					data->matchmake_mode = Matchmake::Joining;
				}
				break;
			}
			case Matchmake::Joining:
			{
				data->matchmake_timer -= Game::real_time.delta;
				if (data->matchmake_timer < 0.0f)
				{
					if (Game::save.round == 0)
						Game::schedule_load_level(Game::levels[Game::save.level_index], Game::Mode::Pvp);
					else
					{
						// must play another round before advancing to the next level
						// play a random map that has already been unlocked so far (except the starting/tutorial maps)
						Game::schedule_load_level(Game::levels[Game::tutorial_levels + (s32)(mersenne::randf_co() * (Game::save.level_index - Game::tutorial_levels))], Game::Mode::Pvp);
					}
				}
				break;
			}
			default:
			{
				vi_assert(false);
				break;
			}
		}

		data->time += Game::real_time.delta;
	}

	void draw(const RenderParams& params)
	{
		const Rect2& vp = params.camera->viewport;

		DataFragment* fragment = nullptr;
		if (PlayerCommon::list.count() > 0)
			fragment = DataFragment::in_range(PlayerCommon::list.iterator().item()->get<Transform>()->absolute_pos());

		PlayerTrigger* terminal = nullptr;
		for (s32 i = 0; i < data->terminals.length; i++)
		{
			if (data->terminals[i].ref()->count() > 0)
			{
				terminal = data->terminals[i].ref();
				break;
			}
		}

		// fragment UI
		if (data->active_data_fragment == AssetNull)
		{
			if (fragment)
			{
				// prompt to read it
				UIText text;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Max;
				text.size = 16.0f;
				text.color = UI::accent_color;
				text.text(_(strings::data_fragment_prompt));

				Vec2 p = vp.size * Vec2(0.5f, 0.8f);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
		}
		else
		{
			// reading the data fragment
			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.size = 20.0f;
			text.font = Asset::Font::pt_sans;
			text.color = UI::default_color;
			text.wrap_width = MENU_ITEM_WIDTH * 1.5f;
			text.text(_(strings::data_fragment), DataFragment::count_collected(), DataFragment::list.count(), _(fragment->text()));
			UIMenu::text_clip(&text, data->fragment_time, 300.0f);
			Vec2 p = vp.size * Vec2(0.5f, 0.9f);
			UI::box(params, text.rect(p).outset(16.0f * UI::scale), UI::background_color);
			text.draw(params, p);

			text.color = UI::accent_color;
			text.font = Asset::Font::lowpoly;
			text.size = 16.0f;
			text.clip = 0;
			text.wrap_width = 0;
			text.text("[{{Cancel}}]");
			p = vp.size * Vec2(0.5f, 0.1f);
			UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
			text.draw(params, p);
		}

		// prompt the player to interact if near a terminal or if penelope is waiting on the player to answer a question
		if (!fragment && data->active_data_fragment == AssetNull) // if there's a fragment, that takes precedence
		{
			b8 show_interact_prompt = false;
			Vec2 p;
			if (terminal)
			{
				show_interact_prompt = true;
				p = vp.size * Vec2(0.5f, 0.4f);
			}
			else if (data->choices.active() && !has_focus())
			{
				show_interact_prompt = true;
				p = data->menu.items[0].pos + Vec2(-32.0f * UI::scale - MENU_ITEM_PADDING_LEFT, data->menu.items.length * MENU_ITEM_HEIGHT * -0.5f);
			}

			if (show_interact_prompt)
			{
				// we are near a terminal, or penelope is waiting on the player to answer a question
				// prompt the player to hit the "Interact" button
				UIText text;
				text.color = UI::accent_color;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.size = 16.0f;
				text.text("[{{Interact}}]");

				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
		}

		// matchmake UI
		if (data->matchmake_mode != Matchmake::None)
		{
			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;
			text.color = UI::accent_color;

			if (data->matchmake_mode == Matchmake::Searching)
				text.text(_(strings::match_searching), (s32)data->matchmake_player_count);
			else if (data->matchmake_mode == Matchmake::Found)
				text.text(_(strings::match_starting), ((s32)data->matchmake_timer) + 1);
			else // joining
				text.text(_(strings::waiting));

			Vec2 pos = vp.pos + vp.size * Vec2(0.1f, 0.25f) + Vec2(12.0f * UI::scale, -100 * UI::scale);

			UI::box(params, text.rect(pos).pad({ Vec2((10.0f + 24.0f) * UI::scale, 10.0f * UI::scale), Vec2(8.0f * UI::scale) }), UI::background_color);

			Vec2 triangle_pos
			(
				pos.x + text.bounds().x * -0.5f - 16.0f * UI::scale,
				pos.y
			);

			if (data->matchmake_mode == Matchmake::Found) // solid triangle
				UI::triangle(params, { triangle_pos, Vec2(text.size * UI::scale) }, UI::accent_color);
			else // hollow, rotating triangle
				UI::triangle_border(params, { triangle_pos, Vec2(text.size * 0.5f * UI::scale) }, 4, UI::accent_color, Game::real_time.total * -8.0f);

			text.draw(params, pos);
		}

		// penelope face and UI

		if (data->mode != Mode::Hidden && data->active_data_fragment == AssetNull) // hide penelope while reading a data fragment
		{
			Face face = data->faces.current();

			r32 scale = UI::scale;
			Vec2 pos;
			switch (data->mode)
			{
				case Mode::Center:
				{
					pos = vp.pos + vp.size * Vec2(0.5f, 0.6f);
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
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			if (face == Face::Default)
			{
				// blink
				const r32 blink_delay = 4.0f;
				const r32 blink_time = 0.1f;
				if (fmod(data->time, blink_delay) < blink_time)
					face = Face::EyesClosed;
			}

			r32 animation_time = Game::real_time.total - data->penelope_animation_time;

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

				Vec2 frame_size(32.0f * scale * volume_scale * animation_scale);
				UI::centered_box(params, { pos, frame_size }, UI::background_color, PI * 0.25f);

				const Vec4* color;
				switch (face)
				{
					case Face::Default:
					case Face::Upbeat:
					case Face::Concerned:
					{
						color = &UI::default_color;
						break;
					}
					case Face::Sad:
					{
						color = &Team::ui_color_friend;
						break;
					}
					case Face::EyesClosed:
					case Face::Unamused:
					case Face::Wat:
					{
						color = &UI::disabled_color;
						break;
					}
					case Face::Urgent:
					case Face::Smile:
					{
						color = &UI::accent_color;
						break;
					}
					case Face::Angry:
					{
						color = &UI::alert_color;
						break;
					}
					default:
					{
						vi_assert(false);
						break;
					}
				}

				UI::centered_border(params, { pos, frame_size }, 2.0f, *color, PI * 0.25f);
			}

			// face
			// animation: wait 0.5 seconds before displaying, then flash for 0.5 seconds, then display
			if (animation_time > 1.0f || (animation_time > 0.5f && UI::flash_function(Game::real_time.total)))
			{
				Vec2 face_uv = faces[(s32)face];
				UI::sprite(params, Asset::Texture::penelope, { pos, face_size * scale }, UI::default_color, { face_uv, face_uv_size });
			}
		}

		// text
		if (data->text.has_text() && data->active_data_fragment == AssetNull)
		{
			Vec2 pos;
			switch (data->mode)
			{
				case Mode::Center:
				{
					data->text.anchor_x = UIText::Anchor::Center;
					data->text.anchor_y = UIText::Anchor::Max;
					data->text.color = UI::default_color;
					pos = Vec2(vp.size.x * 0.5f, vp.size.y * 0.6f) + Vec2(0, -100 * UI::scale);
					break;
				}
				case Mode::Hidden:
				{
					data->text.anchor_x = UIText::Anchor::Center;
					data->text.anchor_y = UIText::Anchor::Max;
					data->text.color = UI::accent_color;
					pos = Vec2(vp.size.x * 0.5f, vp.size.y * 0.9f);
					break;
				}
				case Mode::Left:
				{
					data->text.anchor_x = UIText::Anchor::Min;
					data->text.anchor_y = UIText::Anchor::Center;
					data->text.color = UI::default_color;
					pos = Vec2(vp.size.x * 0.18f, vp.size.y * 0.25f);
					break;
				}
				default:
				{
					vi_assert(false);
					break;
				}
			}

			UI::box(params, data->text.rect(pos).outset(MENU_ITEM_PADDING), UI::background_color);
			data->text.draw(params, pos);
		}

		// leaderboard
		if (data->leaderboard_active)
		{
			UIText text;
			text.size = 16.0f;
			text.wrap_width = MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f;
			text.anchor_x = UIText::Anchor::Min;
			text.anchor_y = UIText::Anchor::Min;

			Vec2 p = Vec2(vp.size.x * 0.9f - MENU_ITEM_WIDTH, vp.size.y * 0.3f - MENU_ITEM_HEIGHT * 2);

			for (s32 i = LEADERBOARD_COUNT - 1; i >= 0; i--)
			{
				text.color = i == 2 ? UI::accent_color : UI::default_color;

				// username
				text.text("%d %s", vi_max(1, i + 79 - (Game::save.rating / 400)), data->leaderboard[i].name);
				UI::box(params, text.rect(p).outset(MENU_ITEM_PADDING), UI::background_color);
				UIMenu::text_clip(&text, data->leaderboard_animation_time, 50.0f + vi_min(i, 6) * -5.0f);
				text.draw(params, p);

				// rating
				UIText rating = text;
				rating.anchor_x = UIText::Anchor::Max;
				rating.wrap_width = 0;
				rating.text("%d", data->leaderboard[i].rating);
				rating.draw(params, p + Vec2(MENU_ITEM_WIDTH - MENU_ITEM_PADDING * 2.0f, 0));
				p.y += text.bounds().y + MENU_ITEM_PADDING * 2.0f;
			}
		}

		// menu
		data->menu.draw_alpha(params);
	}

	void cleanup()
	{
		delete data;
		data = nullptr;
	}

	void node_executed(AssetID node)
	{
		if (node == strings::terminal_reset)
			terminal_active(true);
		else if (node == strings::penelope_hide)
			clear();
		else if (node == strings::consolation_done) // we're done consoling; enter into normal conversation mode
			go(data->entry_point);
		else if (node == strings::second_round_go)
			go(strings::second_round);
		else if (node == strings::match_go)
		{
			clear();
			data->matchmake_timer -= mersenne::randf_co() * (MAX_JOIN_TIME * 0.75f); // the other player hit "join" at a random time
			data->matchmake_mode = Matchmake::Joining;
		}
		else if (node == strings::matchmaking_start)
			matchmake_search();
		else if (node == strings::leaderboard_show)
		{
			data->leaderboard_active = true;
			data->leaderboard_animation_time = Game::real_time.total;
		}
		else if (node == strings::leaderboard_hide)
			data->leaderboard_active = false;
	}

	void init(AssetID entry_point, Mode default_mode)
	{
		vi_assert(!data);
		data = new Data();
		data->entry_point = entry_point;
		data->default_mode = default_mode;
		data->active_data_fragment = AssetNull;
		data->matchmake_player_count = 7 + mersenne::rand() % 9;
		data->terminal_active = true;
		Audio::dialogue_done = false;
		Game::updates.add(update);
		Game::cleanups.add(cleanup);
		Game::draws.add(draw);

		// fill out leaderboard
		if (Game::state.mode == Game::Mode::Special)
		{
			s32 rating = Game::save.rating + 21 + (mersenne::rand() % 600);
			for (s32 i = 0; i < 2; i++)
			{
				data->leaderboard[i] =
				{
					Usernames::all[mersenne::rand_u32() % Usernames::count],
					rating,
				};
				rating = vi_max(Game::save.rating + 11, rating - (mersenne::rand() % 300));
			}
			data->leaderboard[2] =
			{
				Game::save.username,
				Game::save.rating
			};
			rating = Game::save.rating;
			for (s32 i = 3; i < LEADERBOARD_COUNT; i++)
			{
				rating -= 11 + (mersenne::rand() % 300);
				data->leaderboard[i] =
				{
					Usernames::all[mersenne::rand_u32() % Usernames::count],
					rating,
				};
			}
		}

		data->node_executed.link(&node_executed);

		variable(strings::matchmaking, AssetNull);
		variable(strings::round, Game::save.round == 0 ? AssetNull : strings::second_round);
		variable(strings::tried, Game::save.last_round_loss ? strings::yes : AssetNull);

		Loader::texture(Asset::Texture::penelope, RenderTextureWrap::Clamp, RenderTextureFilter::Nearest);
	}

	void add_terminal(Entity* term)
	{
		data->terminals.add(term->get<PlayerTrigger>());
	}
}


namespace Terminal
{

#define CONNECT_DELAY_MIN 4.0f
#define CONNECT_DELAY_RANGE 3.0f
#define CONNECT_OFFLINE_DELAY 3.0f

struct LevelNode
{
	s32 index;
	AssetID id;
	Ref<Transform> pos;
};

r32 transition_timer;
AssetID transition_level = AssetNull;
AssetID last_level = AssetNull;
AssetID next_level = AssetNull;
b8 splitscreen_level_selected = false;
Camera* camera;
Vec3 camera_offset;
Array<LevelNode> levels;
s32 tip_index;
r32 tip_time;

const s32 tip_count = 10;
const AssetID tips[tip_count] =
{
	strings::tip_0,
	strings::tip_1,
	strings::tip_2,
	strings::tip_3,
	strings::tip_4,
	strings::tip_5,
	strings::tip_6,
	strings::tip_7,
	strings::tip_8,
	strings::tip_9,
};

b8 splitscreen_teams_are_valid()
{
	s32 player_count = 0;
	s32 team_counts[(s32)AI::Team::count] = {};
	for (s32 i = 0; i < MAX_GAMEPADS; i++)
	{
		AI::Team team = Game::state.local_player_config[i];
		if (team != AI::Team::None)
			team_counts[(s32)team]++;
	}
	for (s32 i = 0; i < (s32)AI::Team::count; i++)
	{
		if (team_counts[i] == 0)
			return false;
	}
	return true;
}

void init(const Update& u, const EntityFinder& entities)
{
	tip_index = mersenne::rand() % tip_count;

	camera = Camera::add();

	{
		Entity* map_view_entity = entities.find("map_view");
		if (map_view_entity)
		{
			Transform* map_view = map_view_entity->get<Transform>();
			camera->rot = Quat::look(map_view->absolute_rot() * Vec3(0, -1, 0));
			camera_offset = map_view->absolute_pos();
		}
	}

	s32 start_level = Game::state.local_multiplayer ? Game::tutorial_levels : 0; // skip tutorial levels if we're in splitscreen mode
	for (s32 i = start_level; i < Asset::Level::count; i++)
	{
		AssetID level_id = Game::levels[i];
		if (level_id == AssetNull)
			break;

		Entity* entity = entities.find(AssetLookup::Level::names[level_id]);
		if (entity)
		{
			levels.add({ i, level_id, entity->get<Transform>() });
			if (level_id == last_level)
				camera->pos = entity->get<Transform>()->absolute_pos() + camera_offset;
		}
	}

	camera->viewport =
	{
		Vec2(0, 0),
		Vec2(u.input->width, u.input->height),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.1f, Game::level.skybox.far_plane);

	const char* entry_point_str = Loader::level_name(Game::levels[Game::save.level_index]);
	Penelope::init(strings_get(entry_point_str));
}

void update(const Update& u)
{
	if (Console::visible)
		return;

	if (Game::state.level == Asset::Level::terminal)
	{
		if (Game::state.local_multiplayer)
		{
			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				b8 player_active = u.input->gamepads[i].active || i == 0;

				AI::Team* team = &Game::state.local_player_config[i];
				if (player_active)
				{
					if (i > 0 || Menu::main_menu_state == Menu::State::Hidden) // ignore player 0 input if the main menu is open
					{
						// handle D-pad
						b8 left = u.input->get(Controls::Left, i) && !u.last_input->get(Controls::Left, i);
						b8 right = u.input->get(Controls::Right, i) && !u.last_input->get(Controls::Right, i);

						// handle joysticks
						{
							r32 last_x = Input::dead_zone(u.last_input->gamepads[i].left_x, UI_JOYSTICK_DEAD_ZONE);
							if (last_x == 0.0f)
							{
								r32 x = Input::dead_zone(u.input->gamepads[i].left_x, UI_JOYSTICK_DEAD_ZONE);
								if (x < 0.0f)
									left = true;
								else if (x > 0.0f)
									right = true;
							}
						}

						if (u.input->get(Controls::Cancel, i) && !u.last_input->get(Controls::Cancel, i))
						{
							if (i > 0) // player 0 must stay in
							{
								*team = AI::Team::None;
								Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
							}
						}
						else if (left)
						{
							if (*team == AI::Team::B)
							{
								if (i == 0) // player 0 must stay in
									*team = AI::Team::A;
								else
									*team = AI::Team::None;
								Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
							}
							else if (*team == AI::Team::None)
							{
								*team = AI::Team::A;
								Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
							}
						}
						else if (right)
						{
							if (*team == AI::Team::A)
							{
								if (i == 0) // player 0 must stay in
									*team = AI::Team::B;
								else
									*team = AI::Team::None;
								Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
							}
							else if (*team == AI::Team::None)
							{
								*team = AI::Team::B;
								Audio::post_global_event(AK::EVENTS::PLAY_BEEP_GOOD);
							}
						}
					}
				}
				else // controller is gone
					*team = AI::Team::None;
			}

			if (u.input->get(Controls::Interact, 0) && !u.last_input->get(Controls::Interact, 0)
				&& splitscreen_teams_are_valid())
			{
				Game::save = Game::Save();
				Game::save.level_index = Game::tutorial_levels; // start at first non-tutorial level
				Game::schedule_load_level(Game::levels[Game::save.level_index], Game::Mode::Pvp);
			}
		}
		else
		{
			// singleplayer
		}

		// level select

		s32 index = -1;
		for (s32 i = 0; i < levels.length; i++)
		{
			const LevelNode& node = levels[i];
			if (node.id == next_level)
			{
				index = i;
				break;
			}
		}

		if (index != -1)
		{
			if (Game::state.local_multiplayer
				&& !splitscreen_level_selected
				&& !UIMenu::active[0])
			{
				// select level
				if ((!u.input->get(Controls::Forward, 0)
					&& u.last_input->get(Controls::Forward, 0))
					|| (Input::dead_zone(u.last_input->gamepads[0].left_y) < 0.0f
						&& Input::dead_zone(u.input->gamepads[0].left_y) >= 0.0f))
				{
					index = vi_min(index + 1, levels.length - 1);
					next_level = levels[index].id;
				}
				else if ((!u.input->get(Controls::Backward, 0)
					&& u.last_input->get(Controls::Backward, 0))
					|| (Input::dead_zone(u.last_input->gamepads[0].left_y) > 0.0f
						&& Input::dead_zone(u.input->gamepads[0].left_y) <= 0.0f))
				{
					index = vi_max(index - 1, 0);
					next_level = levels[index].id;
				}
			}

			Vec3 target = camera_offset + levels[index].pos.ref()->absolute_pos();
			camera->pos += (target - camera->pos) * vi_min(1.0f, Game::real_time.delta) * 3.0f;
		}

		if (tip_time == 0.0f && (!Game::state.local_multiplayer || splitscreen_level_selected))
			tip_time = Game::real_time.total;
	}
}

void draw(const RenderParams& params)
{
	if (params.technique != RenderTechnique::Default)
		return;

	if (Game::state.level == Asset::Level::terminal)
	{
		const Rect2& viewport = params.camera->viewport;
		if (Game::state.local_multiplayer)
		{
			// splitscreen
			const Vec2 box_size(512 * UI::scale, (512 - 64) * UI::scale);
			UI::box(params, { viewport.size * 0.5f - box_size * 0.5f, box_size }, UI::background_color);

			UIText text;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Max;
			text.color = UI::accent_color;
			text.wrap_width = box_size.x - 48.0f * UI::scale;
			text.text(_(splitscreen_teams_are_valid() ? strings::splitscreen_prompt_ready : strings::splitscreen_prompt));
			Vec2 pos(viewport.size.x * 0.5f, viewport.size.y * 0.5f + box_size.y * 0.5f - (16.0f * UI::scale));
			text.draw(params, pos);
			pos.y -= 64.0f * UI::scale;

			// draw team labels
			const r32 team_offset = 128.0f * UI::scale;
			text.wrap_width = 0;
			text.text(_(strings::team_a));
			text.draw(params, pos + Vec2(-team_offset, 0));
			text.text(_(strings::team_b));
			text.draw(params, pos + Vec2(team_offset, 0));

			// set up text for gamepad number labels
			text.color = UI::background_color;
			text.wrap_width = 0;
			text.anchor_x = UIText::Anchor::Center;
			text.anchor_y = UIText::Anchor::Center;

			for (s32 i = 0; i < MAX_GAMEPADS; i++)
			{
				pos.y -= 64.0f * UI::scale;

				AI::Team team = Game::state.local_player_config[i];

				const Vec4* color;
				r32 x_offset;
				if (i > 0 && !params.sync->input.gamepads[i].active)
				{
					color = &UI::disabled_color;
					x_offset = 0.0f;
				}
				else if (team == AI::Team::None)
				{
					color = &UI::default_color;
					x_offset = 0.0f;
				}
				else if (team == AI::Team::A)
				{
					color = &UI::accent_color;
					x_offset = -team_offset;
				}
				else if (team == AI::Team::B)
				{
					color = &UI::accent_color;
					x_offset = team_offset;
				}
				else
					vi_assert(false);

				Vec2 icon_pos = pos + Vec2(x_offset, 0);
				UI::mesh(params, Asset::Mesh::icon_gamepad, icon_pos, Vec2(48.0f * UI::scale), *color);
				text.text("%d", i + 1);
				text.draw(params, icon_pos);
			}
		}
		else
		{
			// singleplayer
		}

		// highlight level locations
		const LevelNode* current_level = nullptr;
		for (s32 i = 0; i < levels.length; i++)
		{
			const LevelNode& node = levels[i];
			Transform* pos = node.pos.ref();
			const Vec4* color;
			if (Game::state.local_multiplayer)
				color = node.id == next_level ? &UI::accent_color : &Team::ui_color_friend;
			else
				color = node.index <= Game::save.level_index ? &UI::accent_color : &UI::alert_color;
			UI::indicator(params, pos->absolute_pos(), *color, false);

			if (node.id == next_level)
				current_level = &node;
		}

		// draw current level name
		if (current_level)
		{
			Vec2 p;
			if (UI::project(params, current_level->pos.ref()->absolute_pos(), &p))
			{
				p.y += 32.0f * UI::scale;
				UIText text;
				text.color = UI::accent_color;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.text_raw(AssetLookup::Level::names[current_level->id]);
				UI::box(params, text.rect(p).outset(8.0f * UI::scale), UI::background_color);
				text.draw(params, p);
			}
		}

		if (Game::state.local_multiplayer)
		{
			if (!splitscreen_level_selected)
			{
				UIText text;
				text.anchor_x = text.anchor_y = UIText::Anchor::Center;
				text.color = UI::accent_color;
				text.text(_(strings::deploy_prompt));

				Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

				UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

				text.draw(params, pos);
			}
		}

		if (current_level)
		{
			// show "loading..."
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.color = UI::accent_color;
			text.text(_(Game::state.local_multiplayer ? strings::loading_offline : strings::connecting));
			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f);

			UI::box(params, text.rect(pos).pad({ Vec2(64, 24) * UI::scale, Vec2(18, 24) * UI::scale }), UI::background_color);

			text.draw(params, pos);

			Vec2 triangle_pos = Vec2
			(
				pos.x - text.bounds().x * 0.5f - 32.0f * UI::scale,
				pos.y
			);
			UI::triangle_border(params, { triangle_pos, Vec2(20 * UI::scale) }, 9, UI::accent_color, Game::real_time.total * -8.0f);

			if (Game::state.forfeit == Game::Forfeit::None
				&& Game::save.level_index >= Game::tutorial_levels)
			{
				// show a tip
				UIText text;
				text.anchor_x = UIText::Anchor::Center;
				text.anchor_y = UIText::Anchor::Min;
				text.color = UI::accent_color;
				text.wrap_width = MENU_ITEM_WIDTH;
				text.text(_(tips[tip_index]));
				UIMenu::text_clip(&text, tip_time, 80.0f);

				Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.2f) + Vec2(0, 48.0f * UI::scale);

				UI::box(params, text.rect(pos).outset(12 * UI::scale), UI::background_color);

				text.draw(params, pos);
			}
		}

		if (Game::state.forfeit != Game::Forfeit::None)
		{
			// the previous match was forfeit; let the player know
			UIText text;
			text.anchor_x = text.anchor_y = UIText::Anchor::Center;
			text.color = UI::accent_color;
			text.text(_(Game::state.forfeit == Game::Forfeit::NetworkError ? strings::forfeit_network_error : strings::forfeit_opponent_quit));

			Vec2 pos = params.camera->viewport.size * Vec2(0.5f, 0.8f);

			UI::box(params, text.rect(pos).outset(8 * UI::scale), UI::background_color);

			text.draw(params, pos);
		}
	}
}

void show()
{
	Game::schedule_load_level(Asset::Level::terminal, Game::Mode::Special);
}

}

}
