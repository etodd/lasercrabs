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
struct DroneReflectEvent;
struct SpawnPosition;
struct SpawnPoint;
struct Drone;

namespace Net
{
	struct StreamRead;
}

struct PlayerHuman : public ComponentType<PlayerHuman>
{
	enum class UIMode : s8
	{
		ParkourDefault,
		ParkourDead,
		PvpDefault,
		PvpSelectTeam,
		PvpSelectSpawn,
		PvpKillCam,
		PvpSpectate,
		PvpUpgrade,
		PvpGameOver,
		Noclip,
		Pause,
		count,
	};

	enum class EmoteCategory : s8
	{
		TeamA,
		TeamB,
		Everyone,
		Misc,
		count,
		None = count,
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

	struct ChatEntry
	{
		r32 timestamp;
		char username[MAX_USERNAME + 1];
		char msg[MAX_CHAT + 1];
		AI::Team team;
		AI::TeamMask mask;
	};

	enum class ChatFocus : s8
	{
		None,
		Team,
		All,
		count,
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
			CoreModuleUnderAttack,
			CoreModuleDestroyed,
			ForceFieldUnderAttack,
			ForceFieldDestroyed,
			BatteryUnderAttack,
			BatteryLost,
			Spot,
			count,
		};

		Vec3 pos;
		r32 timer;
		Ref<Target> target;
		AI::Team team;
		Type type;
		b8 attached;
	};

	struct KillPopup
	{
		r32 timer;
		Ref<PlayerManager> victim;
	};
	
	enum Flags : s8
	{
		FlagNone = 0,
		FlagLocal = 1 << 0,
		FlagMessageGood = 1 << 1,
		FlagUpgradeMenuOpen = 1 << 2,
		FlagAudioLogPlaying = 1 << 3,
	};

	static Array<LogEntry> logs;
	static Array<ChatEntry> chats;
	static Array<Notification> notifications;
	static b8 notification(Entity*, AI::Team, Notification::Type);
	static b8 notification(const Vec3&, AI::Team, Notification::Type);

	static Vec2 camera_topdown_movement(const Update&, s8, const Quat&);
	static b8 players_on_same_client(const Entity*, const Entity*);

	static void update_all(const Update&);
	static s32 count_local();
	static s32 count_local_before(PlayerHuman*);
	static PlayerHuman* for_camera(const Camera*);
	static PlayerHuman* for_gamepad(s8);
	static void chat_add(const char*, PlayerManager* player, AI::TeamMask = AI::TeamAll);
	static void log_add(const char*, AI::Team = AI::TeamNone, AI::TeamMask = AI::TeamAll, const char* = nullptr, AI::Team = AI::TeamNone);
	static void clear();
	static void camera_setup_drone(Drone*, Camera*, Vec3*, r32);
	static void draw_logs(const RenderParams&, AI::Team, s8);

	Array<KillPopup> kill_popups;
	Array<SupportEntry> last_supported;
	TextField chat_field;
	u64 uuid;
	UIMenu menu;
	UIScroll score_summary_scroll;
	Quat kill_cam_rot;
	Vec3 camera_center;
	r32 msg_timer;
	r32 animation_time;
	r32 select_spawn_timer; // also used for spawn letterbox animation
	r32 angle_horizontal;
	r32 angle_vertical;
#if SERVER
	r32 afk_timer;
#endif
	r32 rumble;
	r32 emote_timer;
	r32 audio_log_prompt_timer;
	s32 spectate_index;
#if SERVER
	u32 ai_record_id;
#endif
	u32 master_id;
	Menu::State menu_state;
	s16 energy_notification_accumulator;
	Ref<SpawnPoint> selected_spawn;
	Ref<Entity> killed_by;
	Ref<Camera> camera;
	AssetID audio_log;
	EmoteCategory emote_category;
	Upgrade upgrade_last_visit_highest_available;
	ChatFocus chat_focus;
	s8 gamepad;
	s8 flags;
	s8 ability_upgrade_slot;
	char msg_text[UI_TEXT_MAX];
	
	PlayerHuman(b8 = false, s8 = 0);
	void awake();
	~PlayerHuman();

	inline b8 flag(Flags f) const
	{
		return flags & f;
	}

	void flag(Flags, b8);

	inline b8 local() const
	{
		return flags & FlagLocal;
	}

	void kill_popup(PlayerManager*);
	void draw_chats(const RenderParams&) const;
	b8 chat_enabled() const;
	b8 emotes_enabled() const;
	void msg(const char*, Flags);
	void audio_log_pickup(AssetID);
	void audio_log_stop();
	void rumble_add(r32);
	UIMode ui_mode() const;
	Vec2 ui_anchor(const RenderParams&) const;
	void upgrade_menu_show();
	void upgrade_menu_hide();
	void upgrade_station_try_exit();
	void upgrade_completed(Upgrade);
	Upgrade upgrade_selected() const;
	void update(const Update&);
	void update_late(const Update&);
	void update_camera_rotation(const Update&);
	void draw_ui_early(const RenderParams&) const;
	void draw_turret_battery_flag_icons(const RenderParams&) const;
	void draw_ui(const RenderParams&) const;
	void draw_alpha_late(const RenderParams&) const;
	void spawn(const SpawnPosition&);
	void assault_status_display();
	void energy_notify(s32);
	void team_set(AI::Team);
};

struct PlayerCommon : public ComponentType<PlayerCommon>
{
	r32 angle_horizontal;
	r32 angle_vertical;
	r32 recoil;
	r32 recoil_velocity;
	Ref<PlayerManager> manager;

	PlayerCommon(PlayerManager* = nullptr);
	void awake();

	void recoil_add(r32);
	void update_client(const Update&);
	Entity* incoming_attacker() const;
	Vec3 look_dir() const;
	Quat look() const;
	r32 detect_danger() const;
	void clamp_rotation(const Vec3&, r32 = 0.0f);
	b8 movement_enabled() const;
	r32 angle_vertical_total() const;
	void health_changed(const HealthEvent&);
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

	enum Flags : s8
	{
		FlagTryPrimary = 1 << 0,
		FlagTrySecondary = 1 << 1,
		FlagGrappleValid = 1 << 2,
		FlagGrappleCanceled = 1 << 3,
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
			Rectifier,
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
	static Vec3 get_movement(const Update&, const Quat&, s8);

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
	r32 spot_timer;
	r32 gamepad_rotation_speed;
	r32 cooldown_last;
	Ref<PlayerHuman> player;
	Ref<Entity> anim_base;
	s8 flags;

	inline b8 flag(s32 f) const
	{
		return flags & f;
	}

	inline void flag(s32 f, b8 value)
	{
		if (value)
			flags |= f;
		else
			flags &= ~f;
	}

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
	void ability_select(Ability);
	RemoteControl remote_control_get(const Update&) const;

	void terminal_exit();

	void update(const Update&);
	void update_late(const Update&);
	void draw_ui(const RenderParams&) const;
	void draw_alpha_late(const RenderParams&) const;

	void update_camera_input(const Update&, r32 = 1.0f, r32 = 1.0f);
	b8 input_enabled() const;
	b8 movement_enabled() const;
};

}
