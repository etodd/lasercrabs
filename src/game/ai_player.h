#pragma once
#include "data/entity.h"
#include "ai.h"
#include "constants.h"
#include "game.h"
#if DEBUG_AI_CONTROL
#include <typeinfo>
#endif
#include "data/components.h"
#include "drone.h"
#include "entities.h"
#include "data/priority_queue.h"

namespace VI
{

struct PlayerControlAI;
struct Transform;
struct PlayerManager;
struct Camera;
struct Target;

struct PlayerAI
{
	static PinArray<PlayerAI, MAX_PLAYERS> list;

	static AI::Config generate_config(AI::Team, r32);

	Ref<PlayerManager> manager;
	Revision revision;
	AI::Config config;
	b8 spawning;

	PlayerAI(PlayerManager*, const AI::Config&);
	inline ID id() const
	{
		return ID(this - &list[0]);
	}
	void update_server(const Update&);
	void spawn(const SpawnPosition&);
	void spawn_callback(const AI::DronePathNode&);
};

struct PlayerControlAI : public ComponentType<PlayerControlAI>
{
	struct Action
	{
		static const s8 TypeNone = 0;
		static const s8 TypeMove = 1;
		static const s8 TypeAttack = 2;
		static const s8 TypeUpgrade = 3;
		static const s8 TypeAbility = 4;
		static const s8 TypeRunAway = 5;

		Vec3 pos; // for move, spawn, and build ability actions
		Vec3 normal; // for move, spawn, and build ability actions
		s32 priority;
		Ref<Entity> target;
		s8 type;
		union
		{
			Ability ability; // for build and shoot ability actions
			Upgrade upgrade; // for upgrade actions
		};
		Action();
		Action& operator=(const Action&);

		b8 fuzzy_equal(const Action&) const;
	};

	struct ActionKey
	{
		r32 priority(const Action& e)
		{
			return r32(e.priority);
		}
	};

	struct FailedAction
	{
		r32 timestamp;
		Action action;
	};

	PriorityQueue<Action, ActionKey> action_queue;
	StaticArray<FailedAction, 4> recent_failed_actions;
	Action action_current;
	Vec3 target_pos;
	Vec3 random_look;
	u32 active_callback;
	r32 aim_timeout; // time we've been able to shoot but haven't due to aiming
	r32 aim_timer; // total aim time including cooldowns etc.
	r32 reeval_timer;
	r32 inaccuracy;
	AI::DronePath path;
	s32 path_index;
	Ref<PlayerAI> player;
	Ref<Entity> target;
#if DEBUG_AI_CONTROL
	Ref<Camera> camera;
#endif
	b8 target_shot_at;
	b8 target_hit;
	b8 target_active;
	ActionKey action_queue_key;

	PlayerControlAI(PlayerAI* = nullptr);
	void awake();
	~PlayerControlAI();

	void action_clear();
	void action_execute(const Action&);
	void action_done(b8);
	void actions_populate();

	void callback_path(const AI::DroneResult&);
	void upgrade_completed(Upgrade);
	Vec2 aim(const Update&, const Vec3&, r32);
	void aim_and_shoot_target(const Update&, const Vec3&, Target*);
	b8 aim_and_shoot_location(const Update&, const AI::DronePathNode&, const AI::DronePathNode&, r32);
	b8 in_range(const Vec3&, r32) const;
	void set_path(const AI::DronePath&);
	void drone_done_flying_or_dashing();
	void drone_hit(Entity*);
	void drone_detaching();
	void update_server(const Update&);
	const AI::Config& config() const;
};


}