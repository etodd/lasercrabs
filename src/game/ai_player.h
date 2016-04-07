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
	void update(const Update&);
	void spawn();
};

#define MAX_MEMORY 8

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
	Behavior* behavior_callback;
	b8 path_request_active;
	MemoryArray memory[MAX_FAMILIES];
	AI::Path path;
	s32 path_index;
	Ref<AIPlayer> player;
	Ref<Target> target;
	b8 hit_target;
#if DEBUG_AI_CONTROL
	Camera* camera;
#endif

	AIPlayerControl(AIPlayer*);
	void awake();
	~AIPlayerControl();

	b8 aim_and_shoot(const Update&, const Vec3&, b8);

	b8 in_range(const Vec3&) const;

	void pathfind(const Vec3&, Behavior*, s8);
	void resume_loop_high_level();
	void set_target(Target*, Behavior*);
	void random_path(Behavior*);
	void set_path(const AI::Path&);
	b8 go(const Vec3&);
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
		virtual void set_context(void* ctx)
		{
			control = (AIPlayerControl*)ctx;
		}
	};

	struct FollowPath : Base<FollowPath>
	{
		void run();
	};

	struct RandomPath : Base<RandomPath>
	{
		void run();
	};

	template<typename Component> struct Find : Base<Find<Component>>
	{
		s8 priority;
		Find(s8 priority)
			: priority(priority)
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
			Vec3 closest;
			r32 closest_distance = FLT_MAX;
			Vec3 pos = control->get<Transform>()->absolute_pos();
			for (s32 i = 0; i < memory.length; i++)
			{
				r32 distance = (memory[i].pos - pos).length_squared();
				if (distance < closest_distance)
				{
					closest_distance = distance;
					closest = memory[i].pos;
				}
			}
			if (closest_distance < FLT_MAX)
				control->pathfind(closest, this, priority);
			else
				done(false);
		}
	};

	template<typename Component> struct React : Base<React<Component>>
	{
		s8 priority_path;
		s8 priority_react;
		React(s8 priority_path, s8 priority_react)
			: priority_path(priority_path), priority_react(priority_react)
		{
		}

		void run()
		{
			active(true);
			b8 can_path = priority_path >= control->path_priority;
			b8 can_react = priority_react >= control->path_priority;
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
						closest_distance = distance;
						closest = memory[i].entity.ref();
					}
				}

				if (closest)
				{
					b8 can_hit_now = control->get<Awk>()->can_hit(closest->get<Target>());
					if (can_hit_now && can_react)
					{
						control->loop_high_level->abort();
						control->set_target(closest->get<Target>(), this);
						return;
					}
					else if (can_path)
					{
						control->loop_high_level->abort();
						control->pathfind(closest->get<Target>()->absolute_pos(), this, priority_path);
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
