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

	b8 update_memory();
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

	struct Find : Base<Find>
	{
		b8(*filter)(const AIPlayerControl*, const Entity*);
		Family family;
		Find(Family, s8, b8(*)(const AIPlayerControl*, const Entity*));

		void run();
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

	struct ReactTarget : Base<ReactTarget>
	{
		s8 react_priority;
		b8(*filter)(const AIPlayerControl*, const Entity*);
		Family family;
		ReactTarget(Family, s8, s8, b8(*)(const AIPlayerControl*, const Entity*));

		void run();
	};

	void update_active(const Update&);
}

}
