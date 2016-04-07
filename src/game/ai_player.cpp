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
	behavior_callback(),
	path_request_active(),
	path_priority(),
	path(),
	loop_high_level(),
	loop_low_level(),
	target(),
	hit_target()
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
	link_arg<Entity*, &AIPlayerControl::awk_hit>(get<Awk>()->hit);
	link<&AIPlayerControl::awk_detached>(get<Awk>()->detached);
}

b8 AIPlayerControl::in_range(const Vec3& p) const
{
	Vec3 to_entity = p - get<Transform>()->absolute_pos();
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
	loop_high_level->~Repeat();
	loop_low_level->~Repeat();
}

void AIPlayerControl::awk_attached()
{
	path_index++;
}

void AIPlayerControl::awk_detached()
{
	hit_target = false;
}

void AIPlayerControl::awk_hit(Entity* e)
{
	hit_target = true;
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

void AIPlayerControl::set_target(Target* t, Behavior* callback)
{
	printf("%f target\n", Game::time.total);
	target = t;
	path.length = 0;
	behavior_callback = callback;
}

void AIPlayerControl::pathfind(const Vec3& p, Behavior* callback, s8 priority)
{
	printf("%f pathfind\n", Game::time.total);
	path.length = 0;
	behavior_callback = callback;
	path_priority = priority;
	path_request_active = true;
	AI::awk_pathfind(get<Transform>()->absolute_pos(), p, ObjectLinkEntryArg<AIPlayerControl, const AI::Path&, &AIPlayerControl::set_path>(id()));
}

void AIPlayerControl::random_path(Behavior* callback)
{
	printf("%f random\n", Game::time.total);
	path.length = 0;
	behavior_callback = callback;
	path_priority = 0;
	path_request_active = true;
	AI::awk_random_path(get<Transform>()->absolute_pos(), ObjectLinkEntryArg<AIPlayerControl, const AI::Path&, &AIPlayerControl::set_path>(id()));
}

void AIPlayerControl::resume_loop_high_level()
{
	if (!loop_high_level->active())
		loop_high_level->run();
}

void AIPlayerControl::set_path(const AI::Path& p)
{
	path_request_active = false;
	path = p;
	path_index = 0;
	printf("%f path: %d\n", Game::time.total, path.length);
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

b8 AIPlayerControl::aim_and_shoot(const Update& u, const Vec3& target, b8 exact)
{
	PlayerCommon* common = get<PlayerCommon>();

	Vec3 pos = get<Awk>()->center();
	Vec3 to_target = Vec3::normalize(target - pos);
	Vec3 wall_normal = common->attach_quat * Vec3(0, 0, 1);

	r32 target_angle_horizontal;
	{
		target_angle_horizontal = LMath::closest_angle(atan2(to_target.x, to_target.z), common->angle_horizontal);
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
		target_angle_vertical = LMath::closest_angle(atan2(-to_target.y, Vec2(to_target.x, to_target.z).length()), common->angle_vertical);
		r32 dir_vertical = target_angle_vertical > common->angle_vertical ? 1.0f : -1.0f;

		{
			// make sure we don't try to turn through the wall
			r32 half_angle = (common->angle_vertical + target_angle_vertical) * 0.5f;
			if (half_angle < -PI * 0.5f
				|| half_angle > PI * 0.5f
				|| (Quat::euler(half_angle, common->angle_horizontal, 0) * Vec3(0, 0, 1)).dot(wall_normal) < -0.5f)
				dir_vertical *= -1.0f; // go the other way
		}

		common->angle_vertical = dir_vertical > 0.0f
			? vi_min(target_angle_vertical, common->angle_vertical + LOOK_SPEED * u.time.delta)
			: vi_max(target_angle_vertical, common->angle_vertical - LOOK_SPEED * u.time.delta);
		common->angle_vertical = LMath::angle_range(common->angle_vertical);
	}

	common->angle_vertical = LMath::clampf(common->angle_vertical, PI * -0.495f, PI * 0.495f);
	common->clamp_rotation(wall_normal, 0.5f);

	if (common->cooldown == 0.0f
		&& common->angle_horizontal == target_angle_horizontal
		&& common->angle_vertical == target_angle_vertical)
	{
		Vec3 look_dir = common->look_dir();
		Vec3 hit;
		if (get<Awk>()->can_go(look_dir, &hit))
		{
			if (!exact || (hit - target).length() < AWK_RADIUS * 2.0f) // make sure we're actually going to land at the right spot
			{
				if (get<Awk>()->detach(look_dir))
					return true;
			}
		}
	}
	
	return false;
}

template<typename Component>
void update_memory(AIPlayerControl* control, AIPlayerControl::MemoryArray* memory, b8 (*filter)(const AIPlayerControl*, const Component*))
{
	// remove outdated memories
	for (s32 i = 0; i < memory->length; i++)
	{
		AIPlayerControl::Memory* m = &(*memory)[i];
		if (control->in_range(m->pos))
		{
			b8 now_in_range = m->entity.ref() && control->in_range(m->entity.ref()->get<Transform>()->absolute_pos()) && filter(control, m->entity.ref()->get<Component>());
			if (!now_in_range)
			{
				memory->remove(i);
				i--;
			}
		}
	}

	// add new memories
	if (memory->length < memory->capacity())
	{
		for (auto i = Component::list.iterator(); !i.is_last(); i.next())
		{
			Vec3 pos = i.item()->get<Transform>()->absolute_pos();
			if (control->in_range(pos) && filter(control, i.item()))
			{
				Entity* entity = i.item()->entity();
				b8 already_found = false;
				for (s32 j = 0; j < memory->length; j++)
				{
					if ((*memory)[j].entity.ref() == entity)
					{
						already_found = true;
						break;
					}
				}

				if (!already_found)
				{
					AIPlayerControl::Memory* m = memory->add();
					m->entity = entity;
					m->pos = pos;
					if (memory->length == memory->capacity())
						break;
				}
			}
		}
	}
}

b8 health_pickup_filter(const AIPlayerControl* control, const HealthPickup* h)
{
	return h->owner.ref() == nullptr;
}

b8 minion_filter(const AIPlayerControl* control, const MinionAI* m)
{
	return m->get<AIAgent>()->team != control->get<AIAgent>()->team;
}

b8 minion_spawn_filter(const AIPlayerControl* control, const MinionSpawn* m)
{
	return m->minion.ref() == nullptr;
}

void AIPlayerControl::update(const Update& u)
{
	update_memory<HealthPickup>(this, &memory[HealthPickup::family], &health_pickup_filter);
	update_memory<MinionAI>(this, &memory[MinionAI::family], &minion_filter);
	update_memory<MinionSpawn>(this, &memory[MinionSpawn::family], &minion_spawn_filter);

	if (get<Transform>()->parent.ref())
	{
		if (!loop_high_level)
		{
			loop_high_level = Repeat::alloc
			(
				Succeed::alloc
				(
					Sequence::alloc
					(
						Delay::alloc(0.3f),
						Select::alloc
						(
							AIBehaviors::Find<HealthPickup>::alloc(1),
							AIBehaviors::Find<MinionAI>::alloc(1),
							AIBehaviors::Find<MinionSpawn>::alloc(1),
							AIBehaviors::RandomPath::alloc()
						)
					)
				)
			);
			loop_high_level->set_context(this);
			loop_high_level->run();

			loop_low_level = Repeat::alloc
			(
				Succeed::alloc
				(
					Sequence::alloc
					(
						Delay::alloc(0.3f),
						Select::alloc // if any of these succeed, they will abort the high level loop
						(
							AIBehaviors::React<HealthPickup>::alloc(0, 1),
							AIBehaviors::React<MinionAI>::alloc(0, 1),
							AIBehaviors::React<MinionSpawn>::alloc(0, 1)
						),
						Execute<AIPlayerControl, &AIPlayerControl::resume_loop_high_level>::alloc()->set(this) // restart the high level loop
					)
				)
			);
			loop_low_level->set_context(this);
			loop_low_level->run();
		}

		if (target.ref())
		{
			Vec3 intersection;
			if (get<Awk>()->can_hit(target.ref(), &intersection))
				aim_and_shoot(u, intersection, false);
			else
			{
				target = nullptr;
				if (behavior_callback)
					behavior_callback->done(false);
			}
		}
		else if (path_index < path.length)
		{
			// look at next target
			aim_and_shoot(u, path[path_index], true);
		}
		else
		{
			// look randomly
			PlayerCommon* common = get<PlayerCommon>();
			r32 offset = Game::time.total * 1.0f;
			common->angle_horizontal += noise::sample3d(Vec3(offset)) * LOOK_SPEED * u.time.delta;
			common->angle_vertical += noise::sample3d(Vec3(offset + 64)) * LOOK_SPEED * u.time.delta;
			common->angle_vertical = LMath::clampf(common->angle_vertical, PI * -0.495f, PI * 0.495f);
			common->clamp_rotation(common->attach_quat * Vec3(0, 0, 1), 0.5f);
		}
	}

	if (behavior_callback && !path_request_active && (!target.ref() && path_index >= path.length) || (target.ref() && hit_target))
	{
		Behavior* cb = behavior_callback;
		behavior_callback = nullptr;
		path_priority = 0;
		target = nullptr;
		printf("%f done: %d\n", Game::time.total, hit_target || path.length > 0);
		cb->done(hit_target || path.length > 0);
	}

#if DEBUG_AI_CONTROL
	// update camera
	s32 player_count = LocalPlayer::list.count() + AIPlayer::list.count();
	Camera::ViewportBlueprint* viewports = Camera::viewport_blueprints[player_count - 1];
	Camera::ViewportBlueprint* blueprint = &viewports[LocalPlayer::list.count() + player.id];

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
	active(true);
	control->random_path(this);
}

void FollowPath::run()
{
	active(true);
	if (control->path_index >= control->path.length)
		done(true);
	else
		control->behavior_callback = this;
}

void update_active(const Update& u)
{
}

}

}