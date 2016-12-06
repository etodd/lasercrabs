#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "menu.h"
#include "constants.h"

namespace VI
{

struct RigidBody;
struct Transform;
struct PlayerManager;
struct Interactable;
struct TargetEvent;
struct HealthEvent;
struct Target;

namespace Net
{
	struct StreamRead;
}

struct PlayerHuman : public ComponentType<PlayerHuman>
{
	enum class UIMode { PvpDefault, ParkourDefault, Pause, Dead, Upgrading, GameOver };

	struct LogEntry
	{
		r32 timestamp;
		char text[512];
		AI::Team team;
	};

	static r32 danger;
	static StaticArray<LogEntry, 4> logs;

	static void update_all(const Update&);
	static s32 count_local();
	static s32 count_local_before(PlayerHuman*);
	static PlayerHuman* player_for_camera(const Camera*);
	static void log_add(const char*, AI::Team = AI::TeamNone);
	static void clear();
	static void camera_setup_awk(Entity*, Camera*, r32);

	u64 uuid;
	Camera* camera;
	UIMenu menu;
	UIScroll score_summary_scroll;
	UIText msg_text;
	r32 msg_timer;
	r32 upgrade_animation_time;
	r32 angle_horizontal;
	r32 angle_vertical;
	s32 spectate_index;
	Menu::State menu_state;
	b8 msg_good;
	b8 upgrade_menu_open;
	s8 gamepad;
	b8 local;
	
	PlayerHuman(b8 = false, s8 = 0);
	void awake();

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
	r32 angle_vertical;
	Ref<PlayerManager> manager;

	PlayerCommon(PlayerManager* = nullptr);
	void awake();

	Entity* incoming_attacker() const;
	Vec3 look_dir() const;
	Quat look() const;
	r32 detect_danger() const;
	void update(const Update&);
	void awk_detached();
	void awk_done_flying();
	void awk_reflecting(const Vec3&);
	void clamp_rotation(const Vec3&, r32 = 0.0f);
	b8 movement_enabled() const;
};

struct PlayerControlHuman : public ComponentType<PlayerControlHuman>
{
	struct PositionEntry // absolute, not relative
	{
		Quat rot;
		Vec3 pos;
		r32 timestamp;
	};
	
	struct RemoteControl
	{
		Quat rot;
		Vec3 pos;
		Vec3 movement;
		Ref<Transform> parent;
	};

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
	};

	struct TargetIndicator
	{
		enum class Type
		{
			AwkVisible,
			AwkTracking,
			Minion,
			Energy,
		};

		Vec3 pos;
		Vec3 velocity;
		Type type;
	};

	static b8 net_msg(Net::StreamRead*, PlayerControlHuman*, Net::MessageSource);
	static s32 count_local();

	Reticle reticle;
	StaticArray<TargetIndicator, 32> target_indicators;
	StaticArray<PositionEntry, 60> position_history;
	RemoteControl remote_control;
	Vec3 last_pos;
	r32 fov;
	r32 camera_shake_timer;
	r32 rumble;
	r32 last_gamepad_input_time;
	r32 gamepad_rotation_speed;
	Ref<PlayerHuman> player;
	Ref<Interactable> interactable;
	b8 try_secondary;
	b8 try_primary;
	b8 try_slide;

	PlayerControlHuman(PlayerHuman* = nullptr);
	~PlayerControlHuman();
	void awake();

	r32 look_speed() const;
	b8 local() const;
	void health_changed(const HealthEvent&);
	void camera_shake(r32 = 1.0f);
	void camera_shake_update(const Update&, Camera*);
	void terminal_enter_animation_callback();
	void interact_animation_callback();

	void awk_detached();
	void awk_done_flying_or_dashing();
	void awk_reflecting(const Vec3&);
	void parkour_landed(r32);
	void hit_target(Entity*);
	b8 add_target_indicator(Target*, TargetIndicator::Type);
	void remote_control_handle(const RemoteControl&);

	void update(const Update&);
	void draw(const RenderParams&) const;
	void draw_alpha(const RenderParams&) const;

	void update_camera_input(const Update&, r32 = 1.0f);
	Vec3 get_movement(const Update&, const Quat&);
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}
