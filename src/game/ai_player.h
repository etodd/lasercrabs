#pragma once
#include "data/entity.h"
#include "ai.h"
#include "constants.h"
#include "game.h"
#if DEBUG_AI_CONTROL
#include <typeinfo>
#endif
#include "data/components.h"
#include "awk.h"
#include "entities.h"

namespace VI
{

struct PlayerControlAI;
struct Transform;
struct PlayerManager;
struct Camera;
struct Target;
struct ControlPoint;
struct PlayerSpawnPosition;

struct PlayerAI
{
	struct Memory
	{
		Vec3 pos;
		Vec3 velocity;
		Ref<Entity> entity;
	};

	static PinArray<PlayerAI, MAX_PLAYERS> list;

	static AI::Config generate_config(AI::Team, r32);

	Array<Memory> memory;
	Ref<PlayerManager> manager;
	Revision revision;
	AI::Config config;
	b8 spawning;

	PlayerAI(PlayerManager*, const AI::Config&);
	inline ID id() const
	{
		return this - &list[0];
	}
	void update(const Update&);
	void spawn(const PlayerSpawnPosition&);
	void spawn_callback(const AI::AwkPathNode&);
};

struct PlayerControlAI : public ComponentType<PlayerControlAI>
{
#if DEBUG_AI_CONTROL
	Camera* camera;
#endif
	Vec3 random_look;
	r32 aim_timeout; // time we've been able to shoot but haven't due to aiming
	r32 aim_timer; // total aim time including cooldowns etc.
	r32 inaccuracy;
	AI::AwkPath path;
	s32 path_index;
	Ref<PlayerAI> player;
	Ref<Entity> target;
	b8 shot_at_target;
	b8 hit_target;
	b8 panic;
	s8 path_priority;

	PlayerControlAI(PlayerAI* = nullptr);
	void awake();
	~PlayerControlAI();

	void behavior_clear();
	void behavior_done(b8);
	b8 update_memory();
	b8 sniper_or_bolter_cancel();
	Vec2 aim(const Update&, const Vec3&);
	void aim_and_shoot_target(const Update&, const Vec3&, Target*);
	b8 go(const Update&, const AI::AwkPathNode&, const AI::AwkPathNode&, r32);
	b8 in_range(const Vec3&, r32) const;
	void set_target(Entity*, r32 = 0.0f);
	void set_path(const AI::AwkPath&);
	void awk_done_flying_or_dashing();
	void awk_hit(Entity*);
	void awk_detaching();
	void update(const Update&);
	const AI::Config& config() const;
};


}
