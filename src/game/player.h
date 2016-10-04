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

struct LocalPlayer
{
	enum class UIMode { Default, Pause, Dead, Upgrading, GameOver };

	static PinArray<LocalPlayer, MAX_PLAYERS> list;

	Camera* camera;
	UIMenu menu;
	UIScroll score_summary_scroll;
	UIText msg_text;
	r32 msg_timer;
	Menu::State menu_state;
	r32 upgrade_animation_time;
	r32 angle_horizontal;
	r32 angle_vertical;
	s32 spectate_index;
	Revision revision;
	Ref<Transform> map_view;
	Ref<PlayerManager> manager;
	b8 msg_good;
	b8 upgrade_menu_open;
	u8 gamepad;
	
	inline ID id() const
	{
		return this - &list[0];
	}

	LocalPlayer(PlayerManager*, u8);
	void awake(const Update&);

	static r32 danger;
	static void update_all(const Update&);
	void msg(const char*, b8);
	UIMode ui_mode() const;
	void update(const Update&);
	void update_camera_rotation(const Update&);
	void draw_alpha(const RenderParams&) const;
	void spawn();
};

struct PlayerCommon : public ComponentType<PlayerCommon>
{
	Quat attach_quat;
	r32 angle_horizontal;
	r32 last_angle_horizontal;
	r32 angle_vertical;
	Ref<PlayerManager> manager;

	static s32 visibility_hash(const PlayerCommon*, const PlayerCommon*);
	static Bitmask<MAX_PLAYERS * MAX_PLAYERS> visibility;

	PlayerCommon(PlayerManager*);
	void awake();

	Vec3 look_dir() const;
	r32 detect_danger() const;
	void update(const Update&);
	void awk_detached();
	void awk_done_flying();
	void awk_bounce(const Vec3&);
	void clamp_rotation(const Vec3&, r32 = 0.0f);
	b8 movement_enabled() const;
};

struct LocalPlayerControl : public ComponentType<LocalPlayerControl>
{
	enum class ReticleType
	{
		None,
		Error,
		Normal,
		Target,
		Dash,
		DashError,
	};

	struct Reticle
	{
		Vec3 pos;
		Vec3 normal;
		ReticleType type;
		Ref<Entity> entity;
	};

	struct TargetIndicator
	{
		enum class Type
		{
			AwkVisible,
			AwkTracking,
			Minion,
			MinionAttacking,
			Energy,
		};

		Vec3 pos;
		Vec3 velocity;
		Type type;
	};

	Reticle reticle;
	StaticArray<TargetIndicator, 32> target_indicators;
	Vec3 last_pos;
	r32 fov;
	r32 damage_timer;
	r32 rumble;
	r32 last_gamepad_input_time;
	r32 gamepad_rotation_speed;
	Ref<LocalPlayer> player;
	b8 try_zoom;
	b8 try_primary;
	u8 gamepad;

	LocalPlayerControl(u8);
	~LocalPlayerControl();
	void awake();

	r32 look_speed() const;

	void awk_detached();
	void awk_done_flying_or_dashing();
	void hit_target(Entity*);
	void hit_by(const TargetEvent&);
	b8 add_target_indicator(Target*, TargetIndicator::Type);

	void update(const Update&);
	void draw_alpha(const RenderParams&) const;

	void update_camera_input(const Update&, r32 = 1.0f);
	Vec3 get_movement(const Update&, const Quat&);
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}
