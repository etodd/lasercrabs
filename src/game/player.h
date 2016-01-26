#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "menu.h"

namespace VI
{

struct RigidBody;
struct Transform;
struct LocalPlayerControl;

#define PLAYER_SPAWN_DELAY 5.0f
#define MINION_SPAWN_INITIAL_DELAY 10.0f
#define MINION_SPAWN_INTERVAL 60.0f

#define MAX_PLAYERS 8
#define MAX_PLAYER_COMBOS 28 // C(MAX_PLAYERS, 2)

struct Camera;

struct PlayerManager
{
	static PinArray<PlayerManager, MAX_PLAYERS> list;

	char username[255];
	AI::Team team;
	Ref<Transform> player_spawn;
	StaticArray<Ref<Transform>, 4> minion_spawns;
	r32 player_spawn_timer;
	r32 minion_spawn_timer;
	Revision revision;
	Ref<Entity> entity;
	Link spawn;

	PlayerManager(AI::Team);

	void update(const Update&);

	inline ID id() const
	{
		return this - &list[0];
	}
};

struct LocalPlayer
{
	enum class UIMode { Default, Pause, Spawning };

	static PinArray<LocalPlayer, MAX_PLAYERS> list;

	u8 gamepad;
	b8 pause;
	UIMenu menu;
	Ref<Transform> map_view;
	Ref<PlayerManager> manager;
	Camera* camera;
	r32 msg_timer;
	UIText msg_text;
	UIText credits_text;
	Revision revision;

	inline ID id() const
	{
		return this - &list[0];
	}

	LocalPlayer(PlayerManager*, u8);

	void msg(const char*);
	UIMode ui_mode() const;
	void update(const Update&);
	void draw_alpha(const RenderParams&) const;
	void ensure_camera(const Update&, b8);
	void spawn();
};

struct PlayerCommon : public ComponentType<PlayerCommon>
{
	static b8 visibility[MAX_PLAYER_COMBOS];
	static s32 visibility_count;

	static void update_visibility();
	static s32 visibility_hash(const PlayerCommon*, const PlayerCommon*);

	r32 cooldown;
	UIText username_text;
	s32 visibility_index;

	PlayerCommon(const char*);
	void awake();
	void draw_alpha(const RenderParams&) const;
	void update(const Update&);
	void transfer_to(Entity*);
};

struct LocalPlayerControl : public ComponentType<LocalPlayerControl>
{
	enum class TraceType
	{
		None,
		Normal,
		Target,
		Danger,
	};

	struct TraceEntry
	{
		TraceType type;
		Vec3 pos;
	};

	static LocalPlayerControl* player_for_camera(const Camera*);

	Ref<LocalPlayer> player;

	TraceEntry tracer;
	r32 angle_horizontal;
	r32 angle_vertical;
	Quat attach_quat;
	Camera* camera;
	r32 fov_blend;
	r32 lean;
	r32 last_angle_horizontal;
	b8 allow_zoom;
	b8 try_parkour;
	b8 try_jump;
	u8 gamepad;
	b8 enable_input;

	LocalPlayerControl(u8);
	~LocalPlayerControl();
	void awake();

	void awk_bounce(const Vec3&);
	void awk_attached();
	void hit_target(Entity*);

	void update(const Update&);
	void draw_alpha(const RenderParams&) const;

	void update_camera_input(const Update&);
	Vec3 get_movement(const Update&, const Quat&);
	b8 input_enabled() const;
};

}
