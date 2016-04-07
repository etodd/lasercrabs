#include "ai_player.h"
#include "mersenne/mersenne-twister.h"
#include "usernames.h"
#include "entities.h"
#include "console.h"
#include "awk.h"
#include "minion.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "minion.h"
#include "noise.h"
#if DEBUG_AI_CONTROL
#include "render/views.h"
#endif

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

	e->add<AIPlayerControl>(this);
	Vec3 pos;
	Quat rot;
	manager.ref()->team.ref()->player_spawn.ref()->absolute(&pos, &rot);
	pos += Vec3(0, 0, PLAYER_SPAWN_RADIUS); // spawn it around the edges
	e->get<Transform>()->absolute(pos, rot);
}

AIPlayerControl::AIPlayerControl(AIPlayer* p)
	: player(p),
	path_index(),
	memory(),
	path_callback(),
	path(),
	behavior()
{
#if DEBUG_AI_CONTROL
	camera = Camera::add();
#endif
}

void AIPlayerControl::awake()
{
#if DEBUG_AI_CONTROL
	camera->fog = false;
	camera->team = (u8)get<AIAgent>()->team;
	camera->mask = 1 << camera->team;
	camera->range = AWK_MAX_DISTANCE;
#endif
	link<&AIPlayerControl::awk_attached>(get<Awk>()->attached);
}

b8 AIPlayerControl::in_range(const Entity* e) const
{
	Vec3 to_entity = e->get<Transform>()->absolute_pos() - get<Transform>()->absolute_pos();
	r32 distance_squared = to_entity.length_squared();
	if (distance_squared < AWK_MAX_DISTANCE * AWK_MAX_DISTANCE)
	{
		r32 distance = sqrtf(distance_squared);
		to_entity /= distance;
		if (get<PlayerCommon>()->look_dir().dot(to_entity) < 0.77f)
			return true;
	}
	return false;
}

AIPlayerControl::~AIPlayerControl()
{
#if DEBUG_AI_CONTROL
	camera->remove();
#endif
	behavior->~Behavior();
}

void AIPlayerControl::awk_attached()
{
	path_index++;
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

void AIPlayerControl::pathfind(const Vec3& p, Behavior* callback)
{
	path.length = 0;
	path_callback = callback;
	path_request_active = true;
	AI::awk_pathfind(get<Transform>()->absolute_pos(), p, ObjectLinkEntryArg<AIPlayerControl, const AI::Path&, &AIPlayerControl::set_path>(id()));
}

void AIPlayerControl::random_path(Behavior* callback)
{
	path.length = 0;
	path_callback = callback;
	path_request_active = true;
	AI::awk_random_path(get<Transform>()->absolute_pos(), ObjectLinkEntryArg<AIPlayerControl, const AI::Path&, &AIPlayerControl::set_path>(id()));
}

void AIPlayerControl::set_path(const AI::Path& p)
{
	path_request_active = false;
	path = p;
	path_index = 0;
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

#define LOOK_SPEED 0.7f

void AIPlayerControl::update(const Update& u)
{
	if (get<Transform>()->parent.ref())
	{
		if (!behavior)
		{
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
								AIBehaviors::Find<HealthPickup>::alloc(),
								AIBehaviors::Find<MinionAI>::alloc(),
								AIBehaviors::Find<MinionSpawn>::alloc(),
								AIBehaviors::RandomPath::alloc()
							),
							Sequence::alloc
							(
								Delay::alloc(0.3f)
								/*
								Invert::alloc
								(
									Select::alloc
									(
									)
								)
								*/
							)
						)
					),
					AIBehaviors::FollowPath::alloc()
				)
			);
			behavior->set_context(this);
			behavior->run();
		}

		PlayerCommon* common = get<PlayerCommon>();
		if (path_index < path.length)
		{
			// look at next target
			const Vec3& next = path[path_index];

			Vec3 pos = get<Awk>()->center();
			Vec3 to_next = Vec3::normalize(next - pos);
			Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

			r32 target_angle_horizontal;
			{
				target_angle_horizontal = LMath::closest_angle(atan2(to_next.x, to_next.z), common->angle_horizontal);
				r32 dir_horizontal = target_angle_horizontal > common->angle_horizontal ? 1.0f : -1.0f;

				{
					// make sure we don't try to turn through the wall
					r32 half_angle = (common->angle_horizontal + target_angle_horizontal) * 0.5f;
					if ((Quat::euler(0, half_angle, 0) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
						dir_horizontal *= -1.0f; // go the other way
				}

				common->angle_horizontal = dir_horizontal > 0.0f
					? vi_min(target_angle_horizontal, common->angle_horizontal + LOOK_SPEED * u.time.delta)
					: vi_max(target_angle_horizontal, common->angle_horizontal - LOOK_SPEED * u.time.delta);
				common->angle_horizontal = LMath::angle_range(common->angle_horizontal);
			}

			r32 target_angle_vertical;
			{
				target_angle_vertical = LMath::closest_angle(atan2(-to_next.y, Vec2(to_next.x, to_next.z).length()), common->angle_vertical);
				r32 dir_vertical = target_angle_vertical > common->angle_vertical ? 1.0f : -1.0f;
				common->angle_vertical = dir_vertical > 0.0f
					? vi_min(target_angle_vertical, common->angle_vertical + LOOK_SPEED * u.time.delta)
					: vi_max(target_angle_vertical, common->angle_vertical - LOOK_SPEED * u.time.delta);
				common->angle_vertical = LMath::angle_range(common->angle_vertical);
			}

			common->clamp_rotation(wall_normal, 0.5f);

			if (common->cooldown == 0.0f
				&& common->angle_horizontal == target_angle_horizontal
				&& common->angle_vertical == target_angle_vertical)
			{
				Vec3 look_dir = common->look_dir();
				Vec3 hit;
				if (get<Awk>()->can_go(look_dir, &hit))
				{
					if ((hit - next).length() < AWK_RADIUS * 2.0f) // make sure we're actually going to land at the right spot
						get<Awk>()->detach(look_dir);
				}
			}
		}
		else
		{
			// look randomly
			r32 offset = Game::time.total * 1.0f;
			common->angle_horizontal += noise::sample3d(Vec3(offset)) * LOOK_SPEED * u.time.delta;
			common->angle_vertical += noise::sample3d(Vec3(offset + 64)) * LOOK_SPEED * u.time.delta;
			common->clamp_rotation(common->attach_quat * Vec3(0, 0, 1), 0.5f);
		}
	}

	if (path_callback && !path_request_active && path_index >= path.length)
	{
		Behavior* cb = path_callback;
		path_callback = nullptr;
		cb->done(path.length > 0);
	}

#if DEBUG_AI_CONTROL
	// update camera
	s32 player_count = LocalPlayer::list.count() + AIPlayer::list.count();
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[1 + player.id];

	camera->viewport =
	{
		Vec2((s32)(blueprint->x * (r32)u.input->width), (s32)(blueprint->y * (r32)u.input->height)),
		Vec2((s32)(blueprint->w * (r32)u.input->width), (s32)(blueprint->h * (r32)u.input->height)),
	};
	r32 aspect = camera->viewport.size.y == 0 ? 1 : (r32)camera->viewport.size.x / (r32)camera->viewport.size.y;
	camera->perspective((80.0f * PI * 0.5f / 180.0f), aspect, 0.02f, Skybox::far_plane);
	camera->rot = Quat::euler(0.0f, get<PlayerCommon>()->angle_horizontal, get<PlayerCommon>()->angle_vertical);
	camera->range = AWK_MAX_DISTANCE;
	camera->wall_normal = camera->rot.inverse() * ((get<Transform>()->absolute_rot() * get<Awk>()->lerped_rotation) * Vec3(0, 0, 1));
	camera->pos = get<Awk>()->center();
#endif
}

namespace AIBehaviors
{

void RandomPath::run()
{
	control->random_path(this);
}

void FollowPath::run()
{
	if (control->path_index >= control->path.length)
		done(true);
	else
		control->path_callback = this;
}

void update_active(const Update& u)
{
}

}

}