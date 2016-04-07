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

#define MAX_MEMORY 10

struct AIPlayerControl : public ComponentType<AIPlayerControl>
{
	struct Memory
	{
		Ref<Entity> entity;
		Vec3 pos;
	};

	typedef StaticArray<AIPlayerControl::Memory, MAX_MEMORY> MemoryArray;

	Behavior* behavior;
	Behavior* path_callback;
	b8 path_request_active;
	MemoryArray memory[MAX_FAMILIES];
	AI::Path path;
	s32 path_index;
	Ref<AIPlayer> player;
#if DEBUG_AI_CONTROL
	Camera* camera;
#endif

	AIPlayerControl(AIPlayer*);
	void awake();
	~AIPlayerControl();

	b8 in_range(const Entity*) const;

	void pathfind(const Vec3&, Behavior*);
	void random_path(Behavior*);
	void set_path(const AI::Path&);
	b8 go(const Vec3&);
	void awk_attached();
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
		void run()
		{
			const AIPlayerControl::MemoryArray& memory = control->memory[Component::family];
			if (memory.length > 0)
			{
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
				control->path_callback = this;
			}
			else
				done(false);
		}
	};

	void update_active(const Update&);
}

}
