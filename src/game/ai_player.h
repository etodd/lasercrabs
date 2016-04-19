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

	Ref<PlayerManager> manager;
	Revision revision;

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
	b8 path_request_active;
	MemoryArray memory[MAX_FAMILIES];
	AI::Path path;
	s32 path_index;
	Ref<AIPlayer> player;
	Ref<Target> target;
	b8 shot_at_target;
	b8 hit_target;
	r32 aim_timer;
#if DEBUG_AI_CONTROL
	Camera* camera;
#endif

	AIPlayerControl(AIPlayer*);
	void awake();
	~AIPlayerControl();

	void reaction_start(Behavior*);
	b8 reaction_end();
	b8 aim_and_shoot(const Update&, const Vec3&, b8);

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

	b8 in_range(const Vec3&, r32) const;

	void pathfind(const Vec3&, Behavior*, s8, b8 = false);
	void set_target(Target*, Behavior*, s8);
	void random_path(Behavior*);
	void set_path(const AI::Path&);
	b8 go(const Vec3&);
	void awk_attached();
	void task_done(b8);
	void awk_hit(Entity*);
	void awk_detached();
	void update(const Update&);
};

namespace AIBehaviors
{
	template<typename Derived> struct Base : public BehaviorBase<Derived>
	{
		AIPlayerControl* control;
		virtual void set_context(void* ctx)
		{
			control = (AIPlayerControl*)ctx;
		}
	};

	struct RandomPath : Base<RandomPath>
	{
		void run();
	};

	template<typename Component> struct Find : Base<Find<Component>>
	{
		s8 priority;
		b8(*filter)(const AIPlayerControl*, const Component*);
		Find(s8 priority, b8(*filter)(const AIPlayerControl*, const Component*))
			: priority(priority), filter(filter)
		{
		}

		void run()
		{
			active(true);
			if (priority < control->path_priority)
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
				control->pathfind(closest->pos, this, priority);
			else
				done(false);
		}
	};

	template<typename Component> struct React : Base<React<Component>>
	{
		s8 priority_path;
		s8 priority_react;
		b8(*filter)(const AIPlayerControl*, const Component*);
		React(s8 priority_path, s8 priority_react, b8(*filter)(const AIPlayerControl*, const Component*))
			: priority_path(priority_path), priority_react(priority_react), filter(filter)
		{
		}

		void run()
		{
			active(true);
			b8 can_path = priority_path > control->path_priority;
			b8 can_react = priority_react > control->path_priority;
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
						control->reaction_start(this);
						control->set_target(closest->get<Target>(), this, priority_react);
						return;
					}
					else if (can_path)
					{
						control->reaction_start(this);
						control->pathfind(closest->get<Target>()->absolute_pos(), this, priority_path, true);
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
