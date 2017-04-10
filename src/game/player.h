#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "physics.h"
#include "render/ui.h"
#include "ai.h"
#include "menu.h"
#include "constants.h"
#include "sudoku.h"

namespace VI
{

struct RigidBody;
struct Transform;
struct PlayerManager;
struct Interactable;
struct TargetEvent;
struct HealthEvent;
struct Target;
struct DroneReflectEvent;
struct SpawnPosition;
struct SpawnPoint;

namespace Net
{
	struct StreamRead;
}

struct PlayerHuman : public ComponentType<PlayerHuman>
{
	enum class UIMode
	{
		PvpDefault,
		ParkourDefault,
		Pause,
		Dead,
		Upgrading,
		GameOver,
		count,
	};

	struct LogEntry
	{
		r32 timestamp;
		char text[512];
		AI::Team team;
	};

	static r32 danger;
	static StaticArray<LogEntry, 4> logs;

	static Vec2 camera_topdown_movement(const Update&, s8, Camera*);

	static void update_all(const Update&);
	static s32 count_local();
	static s32 count_local_before(PlayerHuman*);
	static PlayerHuman* player_for_camera(const Camera*);
	static void log_add(const char*, AI::Team = AI::TeamNone);
	static void clear();
	static void camera_setup_drone(Entity*, Camera*, r32);

#if SERVER
	AI::RecordedLife ai_record;
#endif
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
	r32 rumble;
	Upgrade upgrade_last_visit_highest_available;
	Menu::State menu_state;
	Sudoku sudoku;
	Ref<SpawnPoint> selected_spawn;
	s8 gamepad;
	b8 msg_good;
	b8 upgrade_menu_open;
	b8 local;
	
	PlayerHuman(b8 = false, s8 = 0);
	void awake();
	~PlayerHuman();

#if SERVER
	void ai_record_save();
#endif
	void msg(const char*, b8);
	void rumble_add(r32);
	UIMode ui_mode() const;
	void upgrade_menu_show();
	void upgrade_menu_hide();
	void update(const Update&);
	void update_late(const Update&);
	void update_camera_rotation(const Update&);
	void draw_ui(const RenderParams&) const;
	void spawn(const SpawnPosition&);
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
	void drone_detaching();
	void drone_done_flying();
	void drone_reflecting(const DroneReflectEvent&);
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
			DroneVisible,
			DroneTracking,
			Minion,
			Energy,
			Sensor,
			Rocket,
			ForceField,
			Grenade,
			Turret,
			count,
		};

		Vec3 pos;
		Vec3 velocity;
		Type type;
	};

	static b8 net_msg(Net::StreamRead*, PlayerControlHuman*, Net::MessageSource, Net::SequenceID);
	static s32 count_local();

	Array<TargetIndicator> target_indicators;
	Array<PositionEntry> position_history;
#if SERVER
	AI::RecordedLife::Tag ai_record_tag;
#endif
	Reticle reticle;
	RemoteControl remote_control;
	Vec3 last_pos;
#if SERVER
	r32 ai_record_wait_timer;
	r32 rtt;
#endif
	r32 fov;
	r32 camera_shake_timer;
	r32 last_gamepad_input_time;
	r32 gamepad_rotation_speed;
	Ref<PlayerHuman> player;
	Ref<Interactable> interactable;
	b8 try_secondary;
	b8 try_primary;
	b8 try_slide;
	b8 sudoku_active;

	PlayerControlHuman(PlayerHuman* = nullptr);
	void awake();
	~PlayerControlHuman();

	r32 look_speed() const;
	b8 local() const;
	void health_changed(const HealthEvent&);
	void camera_shake(r32 = 1.0f);
	void camera_shake_update(const Update&, Camera*);
	void terminal_enter_animation_callback();
	void interact_animation_callback();

	void drone_detaching();
	void drone_done_flying_or_dashing();
	void drone_reflecting(const DroneReflectEvent&);
	void parkour_landed(r32);
	void hit_target(Entity*);
	void remote_control_handle(const RemoteControl&);

	void update(const Update&);
	void update_late(const Update&);
	void draw_ui(const RenderParams&) const;

	void update_camera_input(const Update&, r32 = 1.0f);
	Vec3 get_movement(const Update&, const Quat&);
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}