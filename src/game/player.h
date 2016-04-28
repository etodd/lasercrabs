#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "menu.h"
#include "team.h"

namespace VI
{

struct RigidBody;
struct Transform;
struct LocalPlayerControl;
struct PlayerManager;
struct Health;
struct DamageEvent;

#define MAX_PLAYER_COMBOS 28 // C(MAX_PLAYERS, 2)

struct LocalPlayer
{
	enum class UIMode { Default, Pause, Spawning, };

	static PinArray<LocalPlayer, MAX_PLAYERS> list;

	u8 gamepad;
	UIMenu menu;
	Ref<Transform> map_view;
	Ref<PlayerManager> manager;
	Camera* camera;
	r32 msg_timer;
	UIText msg_text;
	b8 msg_good;
	Revision revision;
	Menu::State menu_state;

	inline ID id() const
	{
		return this - &list[0];
	}

	LocalPlayer(PlayerManager*, u8);

	static r32 danger;
	static void update_all(const Update&);
	void msg(const char*, b8);
	UIMode ui_mode() const;
	void update(const Update&);
	void draw_alpha(const RenderParams&) const;
	void ensure_camera(const Update&, b8);
	void spawn();
};

struct PlayerCommon : public ComponentType<PlayerCommon>
{
	r32 cooldown;
	r32 cooldown_multiplier;
	Ref<PlayerManager> manager;
	r32 angle_horizontal;
	r32 last_angle_horizontal;
	r32 angle_vertical;
	Quat attach_quat;

	static s32 visibility_hash(const PlayerCommon*, const PlayerCommon*);
	static Bitmask<MAX_PLAYERS * MAX_PLAYERS> visibility;

	PlayerCommon(PlayerManager*);
	void awake();

	Vec3 look_dir() const;
	r32 detect_danger() const;
	void update(const Update&);
	void awk_detached();
	void awk_attached();
	void clamp_rotation(const Vec3&, r32 = 0.0f);
};

struct LocalPlayerControl : public ComponentType<LocalPlayerControl>
{
	enum class TraceType
	{
		None,
		Normal,
		Target,
	};

	struct TraceEntry
	{
		TraceType type;
		Vec3 pos;
	};

	struct Indicator
	{
		Vec3 pos;
		const Vec4* color;
		b8 offscreen;
	};

	static LocalPlayerControl* player_for_camera(const Camera*);

	Ref<LocalPlayer> player;

	TraceEntry tracer;
	StaticArray<Indicator, 32> indicators;
	Camera* camera;
	r32 fov_blend;
	r32 damage_timer;
	r32 health_flash_timer;
	Vec3 detach_dir;
	b8 allow_zoom;
	b8 try_parkour;
	b8 try_jump;
	b8 try_slide;
	u8 gamepad;
	b8 enable_input;
	b8 enable_move;
	r32 rumble;

	LocalPlayerControl(u8);
	~LocalPlayerControl();
	void awake();

	void awk_attached();
	void hit_target(Entity*);
	void damaged(const DamageEvent&);
	void hit_by(const TargetEvent&);
	void health_picked_up();
	void add_target_indicator(Target*, const Vec4&, b8 = false);
	void parkour_landed(r32);

	void update(const Update&);
	void draw_alpha(const RenderParams&) const;

	void detach();

	void update_camera_input(const Update&);
	Vec3 get_movement(const Update&, const Quat&);
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}
