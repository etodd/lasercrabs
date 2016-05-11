#pragma once
#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"
#include "game.h"
#include "team.h"
#if DEBUG_AI_CONTROL
#include <typeinfo>
#endif
#include "data/components.h"
#include "awk.h"
#include "entities.h"

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
		r32 interval_memory_update;
		r32 interval_low_level;
		r32 interval_high_level;
		r32 inaccuracy_min;
		r32 inaccuracy_range;
		r32 aim_timeout;
		r32 aim_speed;
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
	Behavior* target_path_callback;
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

	void add_memory(MemoryArray*, Entity*, const Vec3&);

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
				Vec3 pos = i.item()->template get<Transform>()->absolute_pos();
				if (in_range(pos, VISIBLE_RANGE) && filter(this, i.item()))
				{
					add_memory(component_memories, i.item()->entity(), pos);
					if (component_memories->length == component_memories->capacity())
						break;
				}
			}
		}
		return true; // this task always succeeds
	}

	b8 update_awk_memory();

	void init_behavior_trees();
	void behavior_start(Behavior*, b8, s8);
	void behavior_done(b8);
	b8 restore_loops();
	b8 aim_and_shoot(const Update&, const Vec3&, b8);
	b8 in_range(const Vec3&, r32) const;
	void set_target(Target*);
	void set_path(const AI::Path&);
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
			this->control = (AIPlayerControl*)ctx;
		}

		void path_callback(const AI::Result& result)
		{
			if (this->active())
			{
				if (result.path.length > 0 && this->path_priority > this->control->path_priority)
				{
					this->control->behavior_start(this, true, this->path_priority);
					this->control->set_path(result.path);
				}
				else
					this->done(false);
			}
		}

		void pathfind(const Vec3& target, b8 hit)
		{
			// if hit is false, pathfind as close as possible to the given target
			// if hit is true, pathfind to a point from which we can shoot through the given target
#if DEBUG_AI_CONTROL
			vi_debug("Awk pathfind: %s", typeid(*this).name());
#endif
			auto ai_callback = ObjectLinkEntryArg<Base<Derived>, const AI::Result&, &Base<Derived>::path_callback>(this->id());
			Vec3 pos = this->control->template get<Transform>()->absolute_pos();
			if (hit)
				AI::awk_pathfind_hit(this->control->template get<AIAgent>()->team, pos, target, ai_callback);
			else
				AI::awk_pathfind(this->control->template get<AIAgent>()->team, pos, target, ai_callback);
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
			this->path_priority = priority;
		}

		void run()
		{
			this->active(true);
			if (this->path_priority < this->control->path_priority)
			{
				this->done(false);
				return;
			}

			const AIPlayerControl::MemoryArray& memory = this->control->memory[Component::family];
			const AIPlayerControl::Memory* closest = nullptr;
			Entity* closest_entity;
			r32 closest_distance = FLT_MAX;
			Vec3 pos = this->control->template get<Transform>()->absolute_pos();
			for (s32 i = 0; i < memory.length; i++)
			{
				r32 distance = (memory[i].pos - pos).length_squared();
				if (distance < closest_distance)
				{
					if (!this->control->in_range(memory[i].pos, VISIBLE_RANGE) || (memory[i].entity.ref() && filter(this->control, memory[i].entity.ref()->get<Component>())))
					{
						closest_distance = distance;
						closest = &memory[i];
					}
				}
			}
			if (closest)
				this->pathfind(closest->pos, false);
			else
				this->done(false);
		}
	};

	typedef b8(*AbilitySpawnFilter)(const AIPlayerControl*);
	struct AbilitySpawn : Base<AbilitySpawn>
	{
		AbilitySpawnFilter filter;
		Ability ability;
		AbilitySpawn(s8, Ability, AbilitySpawnFilter);
		void completed(Ability);
		virtual void set_context(void*);
		void run();
		virtual void abort();
	};

	template<typename Component> struct ReactTarget : Base<ReactTarget<Component> >
	{
		s8 react_priority;
		b8(*filter)(const AIPlayerControl*, const Component*);
		ReactTarget(s8 priority_path, s8 react_priority, b8(*filter)(const AIPlayerControl*, const Component*))
			: react_priority(react_priority), filter(filter)
		{
			this->path_priority = priority_path;
		}

		void run()
		{
			this->active(true);
			b8 can_path = this->path_priority > this->control->path_priority;
			b8 can_react = this->react_priority > this->control->path_priority;
			if (can_path || can_react)
			{
				Entity* closest = nullptr;
				r32 closest_distance = AWK_MAX_DISTANCE * AWK_MAX_DISTANCE;
				Vec3 pos = this->control->template get<Transform>()->absolute_pos();
				const AIPlayerControl::MemoryArray& memory = this->control->memory[Component::family];
				for (s32 i = 0; i < memory.length; i++)
				{
					r32 distance = (memory[i].pos - pos).length_squared();
					if (distance < closest_distance)
					{
						if (!this->control->in_range(memory[i].pos, AWK_MAX_DISTANCE) || (memory[i].entity.ref() && filter(this->control, memory[i].entity.ref()->get<Component>())))
						{
							closest_distance = distance;
							closest = memory[i].entity.ref();
						}
					}
				}

				if (closest)
				{
					b8 can_hit_now = this->control->template get<Awk>()->can_hit(closest->get<Target>());
					if (can_hit_now && can_react)
					{
						this->control->behavior_start(this, true, this->react_priority);
						this->control->set_target(closest->get<Target>());
						return;
					}
					else if (can_path)
					{
						this->pathfind(closest->get<Target>()->absolute_pos(), true);
						return;
					}
				}
			}
			this->done(false);
		}
	};

	void update_active(const Update&);
}

}
