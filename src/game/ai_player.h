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
struct ControlPoint;

struct AIPlayer
{
	static PinArray<AIPlayer, MAX_PLAYERS> list;

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

	enum class UpgradeStrategy
	{
		Ignore,
		SaveUp,
		IfAvailable,
	};

	struct Config
	{
		LowLevelLoop low_level;
		HighLevelLoop high_level;
		r32 interval_memory_update;
		r32 interval_low_level;
		r32 interval_high_level;
		r32 inaccuracy_min;
		r32 inaccuracy_range;
		r32 aim_timeout;
		r32 aim_speed;
		r32 aim_min_delay;
		r32 dodge_chance;
		Upgrade upgrade_priority[(s32)Upgrade::count];
		UpgradeStrategy upgrade_strategies[(s32)Upgrade::count];
		Config();
	};

	static Config generate_config();

	Ref<PlayerManager> manager;
	Revision revision;
	Config config;

	AIPlayer(PlayerManager*, const Config&);
	inline ID id() const
	{
		return this - &list[0];
	}
	void update(const Update&);
	void spawn();
	s32 save_up_priority() const;
};

#define MAX_MEMORY 8

struct AIPlayerControl : public ComponentType<AIPlayerControl>
{
	struct Memory
	{
		Vec3 pos;
		Ref<Entity> entity;
	};

	typedef StaticArray<AIPlayerControl::Memory, MAX_MEMORY> MemoryArray;

#if DEBUG_AI_CONTROL
	Camera* camera;
#endif
	Repeat* loop_high_level;
	Repeat* loop_low_level;
	Repeat* loop_low_level_2;
	Repeat* loop_memory;
	Behavior* active_behavior;
	Vec3 random_look;
	r32 aim_timer;
	r32 inaccuracy;
	AI::AwkPath path;
	s32 path_index;
	MemoryArray memory[MAX_FAMILIES];
	Ref<AIPlayer> player;
	Ref<Entity> target;
	b8 shot_at_target;
	b8 hit_target;
	b8 panic;
	s8 path_priority;

	AIPlayerControl(AIPlayer* = nullptr);
	void awake();
	~AIPlayerControl();

	b8 update_memory();
	void behavior_start(Behavior*, s8);
	void behavior_clear();
	b8 restore_loops();
	b8 snipe_stop();
	Vec2 aim(const Update&, const Vec3&);
	b8 aim_and_shoot_target(const Update&, const Vec3&, Target*);
	b8 go(const Update&, const AI::AwkPathNode&, const AI::AwkPathNode&, r32);
	b8 in_range(const Vec3&, r32) const;
	void set_target(Entity*);
	void set_path(const AI::AwkPath&);
	void awk_done_flying_or_dashing();
	void awk_hit(Entity*);
	void awk_detached();
	void update(const Update&);
	const AIPlayer::Config& config() const;
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

		void path_callback(const AI::AwkResult& result)
		{
			if (this->active())
			{
				if (result.path.length > 1 && this->control->template get<Awk>()->state() == Awk::State::Crawl && this->path_priority > this->control->path_priority)
				{
					vi_assert(this->control->active_behavior != this);
					this->control->behavior_start(this, this->path_priority);
					this->control->set_path(result.path);
				}
				else
					this->done(false);
			}
		}

		virtual void done(b8 success)
		{
			if (this->control->active_behavior == this)
			{
				vi_assert(this->active());
				this->control->behavior_clear();
#if DEBUG_AI_CONTROL
				Behavior* r = this->root();
				const char* loop;
				if (r == this->control->loop_low_level)
					loop = "low-level 1";
				else if (r == this->control->loop_low_level_2)
					loop = "low-level 2";
				else
					loop = "high-level";
				vi_debug("%s %s: %s", success ? "succ" : "fail", loop, typeid(*this).name());
#endif
			}
			BehaviorBase<Derived>::done(success);
		}

		virtual void abort()
		{
			if (this->active())
			{
#if DEBUG_AI_CONTROL
				Behavior* r = this->root();
				const char* loop;
				if (r == this->control->loop_low_level)
					loop = "low-level 1";
				else if (r == this->control->loop_low_level_2)
					loop = "low-level 2";
				else
					loop = "high-level";
				vi_debug("%s: %s", loop, typeid(*this).name());
#endif
			}
			if (this->control->active_behavior == this)
				this->control->behavior_clear();
			BehaviorBase<Derived>::abort();
		}

		void pathfind(const Vec3& target, const Vec3& normal, AI::AwkPathfind type, AI::AwkAllow rule = AI::AwkAllow::All)
		{
			vi_assert(this->control->template get<Awk>()->state() == Awk::State::Crawl);
			auto ai_callback = ObjectLinkEntryArg<Base<Derived>, const AI::AwkResult&, &Base<Derived>::path_callback>(this->id());
			Vec3 pos;
			Quat rot;
			this->control->template get<Transform>()->absolute(&pos, &rot);
			AI::awk_pathfind(type, rule, this->control->template get<AIAgent>()->team, pos, rot * Vec3(0, 0, 1), target, normal, ai_callback);
		}
	};

	struct RandomPath : Base<RandomPath>
	{
		RandomPath(s8);
		void run();
	};

	struct Find : Base<Find>
	{
		b8(*filter)(const AIPlayerControl*, const Entity*);
		Family family;
		Find(Family, s8, b8(*)(const AIPlayerControl*, const Entity*));

		void run();
	};

	struct Chance : Base<Chance>
	{
		r32 odds;
		Chance(r32);
		void run();
	};

	struct HasUpgrade : Base<HasUpgrade>
	{
		Upgrade upgrade;
		HasUpgrade(Upgrade);
		void run();
	};

	struct Panic : Base<Panic>
	{
		Panic(s8);
		void abort();
		void done(b8);
		void run();
	};

	struct Test : Base<Test>
	{
		b8(*filter)(const AIPlayerControl*);
		Test(b8(*)(const AIPlayerControl*));
		void run();
	};

	// low-level behaviors

	struct WaitForAttachment : Base<WaitForAttachment>
	{
		void set_context(void*);
		void done_flying_or_dashing();
		void upgrade_completed(Upgrade);
		void run();
	};

	typedef b8(*AbilitySpawnFilter)(const AIPlayerControl*);
	struct AbilitySpawn : Base<AbilitySpawn>
	{
		AbilitySpawn();
		b8 try_spawn(s8, Upgrade, Ability, AbilitySpawnFilter);
		void run();
	};

	struct ReactTarget : Base<ReactTarget>
	{
		s8 react_priority;
		b8(*filter)(const AIPlayerControl*, const Entity*);
		Family family;
		ReactTarget(Family, s8, s8, b8(*)(const AIPlayerControl*, const Entity*));

		void run();
	};

	struct ReactControlPoint : Base<ReactControlPoint>
	{
		ReactControlPoint(s8, b8(*)(const AIPlayerControl*, const Entity*));
		b8(*filter)(const AIPlayerControl*, const Entity*);
		void run();
	};

	struct DoUpgrade : Base<DoUpgrade>
	{
		DoUpgrade(s8);
		void completed(Upgrade);
		void set_context(void*);
		void run();
	};

	struct CaptureControlPoint : Base<CaptureControlPoint>
	{
		CaptureControlPoint(s8);
		void completed(b8);
		void set_context(void*);
		void run();
	};

	struct RunAway : Base<RunAway>
	{
		b8(*filter)(const AIPlayerControl*, const Entity*);
		Family family;
		RunAway(Family, s8, b8(*)(const AIPlayerControl*, const Entity*));
		void run();
	};

	void update_active(const Update&);
}

}
