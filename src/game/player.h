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
	enum class UIMode : s8
	{
		PvpDefault,
		ParkourDefault,
		Pause,
		Dead,
		PvpUpgrading,
		PvpGameOver,
		count,
	};

	struct LogEntry
	{
		r32 timestamp;
		char a[UI_TEXT_MAX + 1];
		char b[UI_TEXT_MAX + 1];
		AI::TeamMask mask;
		AI::TeamMask a_team;
		AI::TeamMask b_team;
	};

	struct SupportEntry
	{
		Vec3 relative_position;
		r32 rotation;
		Ref<RigidBody> support;
	};

	struct Notification
	{
		enum class Type : s8
		{
			DroneDestroyed,
			TurretUnderAttack,
			TurretDestroyed,
			ForceFieldUnderAttack,
			ForceFieldDestroyed,
			BatteryUnderAttack,
			BatteryLost,
			count,
		};

		Vec3 pos;
		r32 timer;
		Ref<Transform> transform;
		AI::Team team;
		Type type;
	};

	static r32 danger;
	static Array<LogEntry> logs;
	static Array<Notification> notifications;
	static b8 notification(Entity*, AI::Team, Notification::Type);

	static Vec2 camera_topdown_movement(const Update&, s8, Camera*);
	static b8 players_on_same_client(const Entity*, const Entity*);

	static void update_all(const Update&);
	static s32 count_local();
	static s32 count_local_before(PlayerHuman*);
	static PlayerHuman* player_for_camera(const Camera*);
	static PlayerHuman* player_for_gamepad(s8);
	static void log_add(const char*, AI::Team = AI::TeamNone, AI::TeamMask = AI::TeamAll, const char* = nullptr, AI::Team = AI::TeamNone);
	static void clear();
	static void camera_setup_drone(Entity*, Camera*, Vec3*, r32*, r32);
	static void draw_logs(const RenderParams&, AI::Team, s8);

	Array<SupportEntry> last_supported;
	u64 uuid;
	UIMenu menu;
	UIScroll score_summary_scroll;
	UIText msg_text;
	Quat kill_cam_rot;
	Vec3 camera_center;
	r32 camera_smoothness;
	r32 msg_timer;
	r32 animation_time;
	r32 select_spawn_timer; // also used for spawn letterbox animation
	r32 angle_horizontal;
	r32 angle_vertical;
	r32 rumble;
	s32 spectate_index;
#if SERVER
	u32 ai_record_id;
#endif
	Upgrade upgrade_last_visit_highest_available;
	Menu::State menu_state;
	Sudoku sudoku;
	s16 energy_notification_accumulator;
	Ref<SpawnPoint> selected_spawn;
	Ref<Entity> killed_by;
	Ref<Camera> camera;
	s8 gamepad;
	b8 msg_good;
	b8 local;
	b8 upgrade_menu_open;
	
	PlayerHuman(b8 = false, s8 = 0);
	void awake();
	~PlayerHuman();

	void msg(const char*, b8);
	void rumble_add(r32);
	UIMode ui_mode() const;
	void upgrade_menu_show();
	void upgrade_menu_hide();
	void upgrade_station_try_exit();
	void upgrade_completed(Upgrade);
	Upgrade upgrade_selected() const;
	void update(const Update&);
	void update_late(const Update&);
	void update_camera_rotation(const Update&);
	void draw_ui(const RenderParams&) const;
	void draw_alpha(const RenderParams&) const;
	void spawn(const SpawnPosition&);
	void assault_status_display();
	void energy_notify(s32);
	void team_set(AI::Team);
	void game_mode_transitioning();
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

	enum class ReticleType : s8
	{
		None,
		Error,
		Normal,
		Target,
		Dash,
		DashCombo,
		DashTarget,
		DashError,
		count,
	};

	struct Reticle
	{
		Vec3 pos;
		Vec3 normal;
		ReticleType type;
	};

	struct TargetIndicator
	{
		enum class Type : s8
		{
			DroneVisible,
			DroneOutOfRange,
			Minion,
			Battery,
			BatteryOutOfRange,
			BatteryEnemy,
			BatteryEnemyOutOfRange,
			BatteryFriendly,
			BatteryFriendlyOutOfRange,
			Sensor,
			ForceField,
			Grenade,
			Turret,
			TurretAttacking,
			TurretOutOfRange,
			TurretFriendly,
			TurretFriendlyOutOfRange,
			CoreModule,
			CoreModuleFriendly,
			CoreModuleFriendlyOutOfRange,
			CoreModuleOutOfRange,
			count,
		};

		Vec3 pos;
		Vec3 velocity;
		Ref<Target> target;
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
	Ref<Entity> anim_base;
	b8 try_secondary;
	b8 try_primary;
	b8 sudoku_active;

	PlayerControlHuman(PlayerHuman* = nullptr);
	void awake();
	~PlayerControlHuman();

	b8 local() const;
	void health_changed(const HealthEvent&);
	void killed(Entity*);
	void camera_shake(r32);
	void camera_shake_update(const Update&, Camera*);
	void terminal_enter_animation_callback();
	void interact_animation_callback();
	const PositionEntry* remote_position(r32* = nullptr, r32* = nullptr) const;

	void cinematic(Entity*, AssetID);
	b8 cinematic_active() const;
	void drone_detaching();
	void drone_done_flying_or_dashing();
	void drone_reflecting(const DroneReflectEvent&);
	void parkour_landed(r32);
	void hit_target(Entity*);
	void remote_control_handle(const RemoteControl&);
	RemoteControl remote_control_get(const Update&) const;

	void update(const Update&);
	void update_late(const Update&);
	void draw_ui(const RenderParams&) const;

	void update_camera_input(const Update&, r32 = 1.0f, r32 = 1.0f);
	Vec3 get_movement(const Update&, const Quat&) const;
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}