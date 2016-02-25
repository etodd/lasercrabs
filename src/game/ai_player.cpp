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
	// TODO: stuff
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

	e->get<Health>()->killed.link<Team, Entity*, &Team::player_killed_by>(manager.ref()->team.ref());
}

AIPlayerControl::AIPlayerControl()
	: goal(), last_pos(), stick_timer()
{
	move_timer = mersenne::randf_co() * 2.0f;
}

void AIPlayerControl::awake()
{
	goal.pos = get<Transform>()->absolute_pos();

	link<&AIPlayerControl::awk_attached>(get<Awk>()->attached);
	link_arg<Entity*, &AIPlayerControl::awk_hit>(get<Awk>()->hit);
}

void AIPlayerControl::awk_attached()
{
	reset_move_timer();
}

void AIPlayerControl::awk_hit(Entity* target)
{
	if (target == goal.entity.ref())
	{
		goal = find_goal(target);
		goal_timer = 0.0f;
	}
}

void AIPlayerControl::reset_move_timer()
{
	move_timer = 0.5f + mersenne::randf_co() * (goal.entity.ref() ? 0.5f : 10.0f);
	goal.vision_timer = 0.0f;
}

Vec3 get_goal_pos(const Entity* e)
{
	if (e->has<MinionCommon>())
		return e->get<MinionCommon>()->head_pos();
	else
		return e->get<Transform>()->absolute_pos();
}

s32 get_goal_priority(const Entity* e)
{
	if (e->has<AIAgent>())
		return 1;
	else
		return 0;
}

b8 AIPlayerControl::goal_reachable(const Entity* e) const
{
	Vec3 pos = get<Transform>()->absolute_pos();
	Vec3 goal_pos = get_goal_pos(e);
	Vec3 to_goal = goal_pos - pos;
	r32 distance_squared = to_goal.length_squared();

	if (distance_squared < AWK_VIEW_RANGE * AWK_VIEW_RANGE)
	{
		btCollisionWorld::ClosestRayResultCallback rayCallback(pos, goal_pos);
		rayCallback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces | btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
		rayCallback.m_collisionFilterGroup = rayCallback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

		Physics::btWorld->rayTest(pos, goal_pos, rayCallback);
		if (rayCallback.hasHit() && rayCallback.m_collisionObject->getUserIndex() == e->id())
			return true;
	}
	return false;
}

AIPlayerControl::Goal AIPlayerControl::find_goal(const Entity* not_entity) const
{
	Entity* enemy = AI::vision_query(get<AIAgent>(), get<Transform>()->absolute_pos(), get<Transform>()->absolute_rot() * Vec3(0, 0, 1), AWK_VIEW_RANGE, PI);
	if (enemy)
	{
		Goal g;
		g.entity = enemy;
		return g;
	}

	{
		Goal g;
		dtPolyRef poly;
		Vec3 target;
		AI::nav_mesh_query->findRandomPoint(&AI::default_query_filter, mersenne::randf_co, &poly, (r32*)&g.pos);
		return g;
	}
}

AIPlayerControl::Goal::Goal()
	: entity(), pos(), vision_timer()
{

}

Vec3 AIPlayerControl::Goal::get_pos() const
{
	Entity* e = entity.ref();
	if (e)
		return get_goal_pos(e);
	else
		return pos;
}

s32 AIPlayerControl::Goal::priority() const
{
	Entity* e = entity.ref();
	if (e)
		return get_goal_priority(e);
	else
		return 0;
}

r32 AIPlayerControl::Goal::inaccuracy() const
{
	Entity* e = entity.ref();
	if (e)
	{
		if (e->has<Awk>())
			return PI * 0.025f;
		else
			return PI * 0.005f;
	}
	else
		return 0;
}

b8 can_go_goal(const Update& u, const AIPlayerControl::Goal& goal)
{
	Entity* e = goal.entity.ref();
	if (e)
	{
		if (goal.vision_timer > 0.5f)
		{
			if (e->has<Awk>())
				return u.time.total - e->get<Awk>()->attach_time > 0.5f;
			else
				return true;
		}
		else
			return false;
	}
	else
		return true;
}

void AIPlayerControl::update(const Update& u)
{
	goal_timer += u.time.delta;
	if (goal_timer > 0.5f)
	{
		goal_timer = 0.0f;
		Goal g = find_goal();
		if (g.priority() > goal.priority())
			goal = g;
	}

	b8 can_shoot = get<PlayerCommon>()->cooldown == 0.0f;

	if (goal.entity.ref() && goal_reachable(goal.entity.ref()))
		goal.vision_timer += u.time.delta;
	else
		goal.vision_timer = 0.0f;

	if (get<Transform>()->parent.ref() && can_shoot)
	{
		move_timer -= u.time.delta;
		if (move_timer < 0.0f && can_go_goal(u, goal))
		{
			Vec3 pos;
			Quat quat;
			get<Transform>()->absolute(&pos, &quat);

			Vec3 wall_normal = quat * Vec3(0, 0, 1);
			Vec3 goal_pos = goal.get_pos();
			Vec3 to_goal = Vec3::normalize(goal_pos - pos);
			{
				const r32 random_range = goal.inaccuracy();
				to_goal = Quat::euler(mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range) * to_goal;
				if (get<Awk>()->can_go(to_goal))
				{
					get<Awk>()->detach(u, to_goal);
					return;
				}
			}

			const s32 tries = 20;
			r32 random_range = PI * (2.0f / (r32)(tries + 1));
			r32 best_distance = -1.0f;
			r32 best_travel_distance = 1.0f;
			Vec3 best_dir;
			for (s32 i = 0; i < tries; i++)
			{
				Vec3 dir = Quat::euler(mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range, mersenne::randf_oo() * random_range) * to_goal;
				if (dir.dot(wall_normal) < 0.0f)
					dir = dir.reflect(wall_normal);
				Vec3 final_pos;
				if (get<Awk>()->can_go(dir, &final_pos))
				{
					r32 distance = (final_pos - goal_pos).length_squared();
					r32 travel_distance = (final_pos - pos).length_squared();
					if (best_distance < 0.0f || (distance < best_distance && travel_distance > best_travel_distance))
					{
						best_distance = distance;
						best_travel_distance = travel_distance;
						best_dir = dir;
					}
				}
				random_range += PI * (2.0f / (r32)(tries + 1));
			}

			if (best_distance > 0.0f)
			{
				get<Awk>()->detach(u, best_dir);
				return;
			}
		}
	}

	if (get<Transform>()->parent.ref())
	{
		if ((get<Transform>()->pos - last_pos).length_squared() > 0.5f * 0.5f)
		{
			// We're not stuck, stay on target
			last_pos = get<Transform>()->pos;
			stick_timer = 0.0f;
		}

		if (can_shoot)
			stick_timer += u.time.delta;

		if (stick_timer > 4.0f)
		{
			goal = find_goal(goal.entity.ref());
			stick_timer = 0.0f;
		}
	}
}

}
