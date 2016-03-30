#pragma once
#include "data/entity.h"
#include "ai.h"
#include "data/behavior.h"

namespace VI
{

struct AIPlayerControl;
struct Transform;
struct PlayerManager;

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

struct AIPlayerControl : public ComponentType<AIPlayerControl>
{
	r32 move_timer;

	Behavior* behavior;

	AIPlayerControl();
	void awake();
	~AIPlayerControl();

	b8 go(const Vec3&);
	void reset_move_timer();
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
		void update(const Update&);
	};

	void update_active(const Update&);
}

}
