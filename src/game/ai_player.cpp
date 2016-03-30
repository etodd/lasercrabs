#include "ai_player.h"
#include "mersenne/mersenne-twister.h"
#include "usernames.h"
#include "game.h"
#include "entities.h"
#include "console.h"
#include "awk.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

namespace VI
{


#define AWK_VIEW_RANGE 100.0f

PinArray<AIPlayer, MAX_AI_PLAYERS> AIPlayer::list;

AIPlayer::AIPlayer(PlayerManager* m)
	: manager(m), revision()
{
	strcpy(manager.ref()->username, Usernames::all[mersenne::rand_u32() % Usernames::count]);
	m->spawn.link<AIPlayer, &AIPlayer::spawn>(this);
}

void AIPlayer::update(const Update& u)
{
	/*
	// always stupidly try to use our ability
	if (manager.ref()->entity.ref())
		manager.ref()->ability_use();
	*/
}

void AIPlayer::spawn()
{
	Entity* e = World::create<AwkEntity>(manager.ref()->team.ref()->team());

	e->add<PlayerCommon>(manager.ref());

	manager.ref()->entity = e;

	e->add<AIPlayerControl>();
	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	pos += Vec3(0, 0, PLAYER_SPAWN_RADIUS); // spawn it around the edges
	e->get<Transform>()->absolute(pos, rot);
}

AIPlayerControl::AIPlayerControl()
{
	reset_move_timer();
	behavior = Repeat::alloc
	(
		Sequence::alloc
		(
			Succeed::alloc
			(
				Parallel::alloc
				(
					Select::alloc
					(
					),
					Sequence::alloc
					(
						Delay::alloc(0.3f),
						Invert::alloc
						(
							Select::alloc
							(
							)
						)
					)
				)
			),
			AIBehaviors::FollowPath::alloc()
		)
	);
}

void AIPlayerControl::awake()
{
	link<&AIPlayerControl::awk_attached>(get<Awk>()->attached);
}

AIPlayerControl::~AIPlayerControl()
{
	behavior->~Behavior();
}

void AIPlayerControl::awk_attached()
{
	reset_move_timer();
}

void AIPlayerControl::reset_move_timer()
{
	move_timer = 0.2f + mersenne::randf_co() * 0.5f;
}

Entity* find_goal(Entity* me, Entity* not_entity)
{
	Entity* enemy = AI::vision_query(me->get<AIAgent>(), me->get<Transform>()->absolute_pos(), me->get<Transform>()->absolute_rot() * Vec3(0, 0, 1), AWK_VIEW_RANGE, PI);
	if (enemy)
		return enemy;

	AI::Team team = me->get<AIAgent>()->team;

	Vec3 pos = me->get<Transform>()->absolute_pos();

	for (auto i = Sensor::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->team != team && AI::vision_check(pos, i.item()->get<Transform>()->absolute_pos(), me, i.item()->entity()))
			return i.item()->entity();
	}

	for (auto i = MinionSpawn::list.iterator(); !i.is_last(); i.next())
	{
		Entity* minion = i.item()->minion.ref();
		if (!minion || minion->get<AIAgent>()->team != team && AI::vision_check(pos, minion->get<Transform>()->absolute_pos(), me, minion))
			return i.item()->entity();
	}

	for (auto i = HealthPickup::list.iterator(); !i.is_last(); i.next())
	{
		Health* owner = i.item()->owner.ref();
		if ((!owner || owner->get<AIAgent>()->team != team) && AI::vision_check(pos, i.item()->get<Transform>()->absolute_pos(), me, i.item()->entity()))
			return i.item()->entity();
	}

	return nullptr;
}

b8 AIPlayerControl::go(const Vec3& target)
{
	Vec3 pos;
	Quat quat;
	get<Transform>()->absolute(&pos, &quat);

	Vec3 wall_normal = quat * Vec3(0, 0, 1);
	Vec3 to_goal = Vec3::normalize(target - pos);
	{
		const r32 random_range = 0.01f;
		to_goal = Quat::euler(mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range) * to_goal;
		if (get<Awk>()->can_go(to_goal))
		{
			get<Awk>()->detach(to_goal);
			return true;
		}
	}

	return false;
}

void AIPlayerControl::update(const Update& u)
{
	b8 can_shoot = get<PlayerCommon>()->cooldown == 0.0f;

	if (get<Transform>()->parent.ref() && can_shoot)
	{
		move_timer -= u.time.delta;
		if (move_timer < 0.0f)
		{
		}
	}
}

namespace AIBehaviors
{

void FollowPath::run()
{
}

void FollowPath::update(const Update& u)
{
}

void update_active(const Update& u)
{
	FollowPath::update_active(u);
}

}

}