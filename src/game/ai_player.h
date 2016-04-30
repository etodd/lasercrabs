#pragma once
#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"
#include "game.h"

namespace VI
{

struct AIPlayerControl;
struct Transform;
struct PlayerManager;
struct Camera;
struct Target;

#define MAX_AI_PLAYERS 3

struct AIPlayer
{
	static PinArray<AIPlayer, MAX_AI_PLAYERS> list;

	enum class LowLevelLoop
	{
		Default,
		Noop,
	};

	enum class HighLevelLoop
	{
		Default,
		Noop,
	};

	struct Config
	{
		LowLevelLoop low_level;
		HighLevelLoop high_level;
		u16 hp_start;
		Config();
	};

	Ref<PlayerManager> manager;
	Revision revision;
	Config config;

	AIPlayer(PlayerManager*);
	inline ID id() const
	{
		return this - &list[0];
	}
	void spawn();
};

#define MAX_MEMORY 8

#define VISIBLE_RANGE (AWK_MAX_DISTANCE * 1.5f)

struct AIPlayerControl : public ComponentType<AIPlayerControl>
{
	struct Memory
	{
		Ref<Entity> entity;
		Vec3 pos;
	};

	typedef StaticArray<AIPlayerControl::Memory, MAX_MEMORY> MemoryArray;

	s8 path_priority;
	Repeat* loop_high_level;
	Repeat* loop_low_level;
	Repeat* loop_low_level_2;
	Repeat* loop_memory;
	Behavior* behavior_callback;
	MemoryArray memory[MAX_FAMILIES];
	AI::Path path;
	s32 path_index;
	Ref<AIPlayer> player;
	Ref<Target> target;
	b8 shot_at_target;
	b8 hit_target;
	r32 aim_timer;
	r32 inaccuracy;
#if DEBUG_AI_CONTROL
	Camera* camera;
#endif

	AIPlayerControl(AIPlayer*);
	void awake();
	~AIPlayerControl();

	template<typename Component, b8 (*filter)(const AIPlayerControl*, const Component*)>
	b8 update_memory()
	{
		MemoryArray* component_memories = &memory[Component::family];
		// remove outdated memories
		for (s32 i = 0; i < component_memories->length; i++)
		{
			AIPlayerControl::Memory* m = &(*component_memories)[i];
			if (in_range(m->pos, VISIBLE_RANGE))
			{
				b8 now_in_range = m->entity.ref() && in_range(m->entity.ref()->get<Transform>()->absolute_pos(), VISIBLE_RANGE) && filter(this, m->entity.ref()->get<Component>());
				if (!now_in_range)
				{
					component_memories->remove(i);
					i--;
				}
			}
		}

		// add new memories
		if (component_memories->length < component_memories->capacity())
		{
			for (auto i = Component::list.iterator(); !i.is_last(); i.next())
			{
				Vec3 pos = i.item()->get<Transform>()->absolute_pos();
				if (in_range(pos, VISIBLE_RANGE) && filter(this, i.item()))
				{
					Entity* entity = i.item()->entity();
					b8 already_found = false;
					for (s32 j = 0; j < component_memories->length; j++)
					{
						if ((*component_memories)[j].entity.ref() == entity)
						{
							already_found = true;
							break;
						}
					}

					if (!already_found)
					{
						AIPlayerControl::Memory* m = component_memories->add();
						m->entity = entity;
						m->pos = pos;
						if (component_memories->length == component_memories->capacity())
							break;
					}
				}
			}
		}
		return true; // this task always succeeds
	}

	void init_behavior_trees();
	void behavior_start(Behavior*);
	void behavior_done(b8);
	b8 restore_loops();
	b8 aim_and_shoot(const Update&, const Vec3&, b8);
	b8 in_range(const Vec3&, r32) const;
	void set_target(Target*, s8);
	void set_path(const AI::Path&, s8);
	void awk_attached();
	void awk_hit(Entity*);
	void awk_detached();
	void update(const Update&);
};

namespace AIBehaviors
{
	template<typename Derived> struct Base : public BehaviorBase<Derived>
	{
		AIPlayerControl* control;
		s8 path_priority;
		virtual void set_context(void* ctx)
		{
			control = (AIPlayerControl*)ctx;
		}

		void path_callback(const AI::Result& result)
		{
			if (active() && result.path.length > 0 && path_priority > control->path_priority)
			{
				control->behavior_start(this);
				control->set_path(result.path, path_priority);
			}
			else
				done(false);
		}

		void pathfind(const Vec3& target, b8 hit)
		{
			// if hit is false, pathfind as close as possible to the given target
			// if hit is true, pathfind to a point from which we can shoot through the given target
			auto ai_callback = ObjectLinkEntryArg<Base<Derived>, const AI::Result&, &Base<Derived>::path_callback>(id());
			Vec3 pos = control->get<Transform>()->absolute_pos();
			if (hit)
				AI::awk_pathfind_hit(pos, target, ai_callback);
			else
				AI::awk_pathfind(pos, target, ai_callback);
		}
	};

	struct RandomPath : Base<RandomPath>
	{
		RandomPath();
		void run();
	};

	template<typename Component> struct Find : Base<Find<Component>>
	{
		b8(*filter)(const AIPlayerControl*, const Component*);
		Find(s8 priority, b8(*filter)(const AIPlayerControl*, const Component*))
			: filter(filter)
		{
			path_priority = priority;
		}

		void run()
		{
			active(true);
			if (path_priority < control->path_priority)
			{
				done(false);
				return;
			}

			const AIPlayerControl::MemoryArray& memory = control->memory[Component::family];
			const AIPlayerControl::Memory* closest = nullptr;
			Entity* closest_entity;
			r32 closest_distance = FLT_MAX;
			Vec3 pos = control->get<Transform>()->absolute_pos();
			for (s32 i = 0; i < memory.length; i++)
			{
				r32 distance = (memory[i].pos - pos).length_squared();
				if (distance < closest_distance)
				{
					if (!control->in_range(memory[i].pos, VISIBLE_RANGE) || (memory[i].entity.ref() && filter(control, memory[i].entity.ref()->get<Component>())))
					{
						closest_distance = distance;
						closest = &memory[i];
					}
				}
			}
			if (closest)
				pathfind(closest->pos, false);
			else
				done(false);
		}
	};

	template<typename Component> struct React : Base<React<Component>>
	{
		s8 react_priority;
		b8(*filter)(const AIPlayerControl*, const Component*);
		React(s8 priority_path, s8 react_priority, b8(*filter)(const AIPlayerControl*, const Component*))
			: react_priority(react_priority), filter(filter)
		{
			path_priority = priority_path;
		}

		void run()
		{
			active(true);
			b8 can_path = path_priority > control->path_priority;
			b8 can_react = react_priority > control->path_priority;
			if (can_path || can_react)
			{
				Entity* closest = nullptr;
				r32 closest_distance = AWK_MAX_DISTANCE * AWK_MAX_DISTANCE;
				Vec3 pos = control->get<Transform>()->absolute_pos();
				const AIPlayerControl::MemoryArray& memory = control->memory[Component::family];
				for (s32 i = 0; i < memory.length; i++)
				{
					r32 distance = (memory[i].pos - pos).length_squared();
					if (distance < closest_distance)
					{
						if (!control->in_range(memory[i].pos, AWK_MAX_DISTANCE) || (memory[i].entity.ref() && filter(control, memory[i].entity.ref()->get<Component>())))
						{
							closest_distance = distance;
							closest = memory[i].entity.ref();
						}
					}
				}

				if (closest)
				{
					b8 can_hit_now = control->get<Awk>()->can_hit(closest->get<Target>());
					if (can_hit_now && can_react)
					{
						control->behavior_start(this);
						control->set_target(closest->get<Target>(), react_priority);
						return;
					}
					else if (can_path)
					{
						pathfind(closest->get<Target>()->absolute_pos(), true);
						return;
					}
				}
			}
			done(false);
		}
	};

	void update_active(const Update&);
}

}
