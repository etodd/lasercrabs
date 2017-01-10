#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Armature
	{
		const s32 count = 10;
		const AssetID awk = 0;
		const AssetID character = 1;
		const AssetID hobo = 2;
		const AssetID interactable = 3;
		const AssetID meursault = 4;
		const AssetID parkour = 5;
		const AssetID parkour_headless = 6;
		const AssetID sailor = 7;
		const AssetID terminal = 8;
		const AssetID tram_doors = 9;
	}
	namespace Bone
	{
		const s32 count = 257;
		const AssetID awk_a1 = 5;
		const AssetID awk_a2 = 6;
		const AssetID awk_b1 = 1;
		const AssetID awk_b2 = 2;
		const AssetID awk_c1 = 3;
		const AssetID awk_c2 = 4;
		const AssetID awk_root = 0;
		const AssetID character_camera = 0;
		const AssetID character_claw1_L = 9;
		const AssetID character_claw1_R = 15;
		const AssetID character_claw2_L = 11;
		const AssetID character_claw2_R = 16;
		const AssetID character_claw3_L = 10;
		const AssetID character_claw3_R = 17;
		const AssetID character_foot_L = 20;
		const AssetID character_foot_R = 23;
		const AssetID character_forearm_L = 7;
		const AssetID character_forearm_R = 13;
		const AssetID character_hand_L = 8;
		const AssetID character_hand_R = 14;
		const AssetID character_head = 5;
		const AssetID character_hips = 2;
		const AssetID character_master = 1;
		const AssetID character_neck = 4;
		const AssetID character_shin_L = 19;
		const AssetID character_shin_R = 22;
		const AssetID character_spine = 3;
		const AssetID character_thigh_L = 18;
		const AssetID character_thigh_R = 21;
		const AssetID character_upper_arm_L = 6;
		const AssetID character_upper_arm_R = 12;
		const AssetID hobo_ball_l = 47;
		const AssetID hobo_ball_r = 51;
		const AssetID hobo_calf_l = 45;
		const AssetID hobo_calf_r = 49;
		const AssetID hobo_clavicle_l = 4;
		const AssetID hobo_clavicle_r = 23;
		const AssetID hobo_foot_l = 46;
		const AssetID hobo_foot_r = 50;
		const AssetID hobo_hand_l = 7;
		const AssetID hobo_hand_r = 26;
		const AssetID hobo_head = 43;
		const AssetID hobo_index_01_l = 8;
		const AssetID hobo_index_01_r = 27;
		const AssetID hobo_index_02_l = 9;
		const AssetID hobo_index_02_r = 28;
		const AssetID hobo_index_03_l = 10;
		const AssetID hobo_index_03_r = 29;
		const AssetID hobo_lowerarm_l = 6;
		const AssetID hobo_lowerarm_r = 25;
		const AssetID hobo_middle_01_l = 11;
		const AssetID hobo_middle_01_r = 30;
		const AssetID hobo_middle_02_l = 12;
		const AssetID hobo_middle_02_r = 31;
		const AssetID hobo_middle_03_l = 13;
		const AssetID hobo_middle_03_r = 32;
		const AssetID hobo_neck_01 = 42;
		const AssetID hobo_pelvis = 0;
		const AssetID hobo_pinky_01_l = 14;
		const AssetID hobo_pinky_01_r = 33;
		const AssetID hobo_pinky_02_l = 15;
		const AssetID hobo_pinky_02_r = 34;
		const AssetID hobo_pinky_03_l = 16;
		const AssetID hobo_pinky_03_r = 35;
		const AssetID hobo_ring_01_l = 17;
		const AssetID hobo_ring_01_r = 36;
		const AssetID hobo_ring_02_l = 18;
		const AssetID hobo_ring_02_r = 37;
		const AssetID hobo_ring_03_l = 19;
		const AssetID hobo_ring_03_r = 38;
		const AssetID hobo_spine_01 = 1;
		const AssetID hobo_spine_02 = 2;
		const AssetID hobo_spine_03 = 3;
		const AssetID hobo_thigh_l = 44;
		const AssetID hobo_thigh_r = 48;
		const AssetID hobo_thumb_01_l = 20;
		const AssetID hobo_thumb_01_r = 39;
		const AssetID hobo_thumb_02_l = 21;
		const AssetID hobo_thumb_02_r = 40;
		const AssetID hobo_thumb_03_l = 22;
		const AssetID hobo_thumb_03_r = 41;
		const AssetID hobo_upperarm_l = 5;
		const AssetID hobo_upperarm_r = 24;
		const AssetID interactable_outer = 4;
		const AssetID interactable_part1 = 1;
		const AssetID interactable_part2 = 2;
		const AssetID interactable_part3 = 3;
		const AssetID interactable_root = 0;
		const AssetID meursault_ball_l = 47;
		const AssetID meursault_ball_r = 51;
		const AssetID meursault_calf_l = 45;
		const AssetID meursault_calf_r = 49;
		const AssetID meursault_clavicle_l = 4;
		const AssetID meursault_clavicle_r = 23;
		const AssetID meursault_foot_l = 46;
		const AssetID meursault_foot_r = 50;
		const AssetID meursault_hand_l = 7;
		const AssetID meursault_hand_r = 26;
		const AssetID meursault_head = 43;
		const AssetID meursault_index_01_l = 8;
		const AssetID meursault_index_01_r = 27;
		const AssetID meursault_index_02_l = 9;
		const AssetID meursault_index_02_r = 28;
		const AssetID meursault_index_03_l = 10;
		const AssetID meursault_index_03_r = 29;
		const AssetID meursault_lowerarm_l = 6;
		const AssetID meursault_lowerarm_r = 25;
		const AssetID meursault_middle_01_l = 11;
		const AssetID meursault_middle_01_r = 30;
		const AssetID meursault_middle_02_l = 12;
		const AssetID meursault_middle_02_r = 31;
		const AssetID meursault_middle_03_l = 13;
		const AssetID meursault_middle_03_r = 32;
		const AssetID meursault_neck_01 = 42;
		const AssetID meursault_pelvis = 0;
		const AssetID meursault_pinky_01_l = 14;
		const AssetID meursault_pinky_01_r = 33;
		const AssetID meursault_pinky_02_l = 15;
		const AssetID meursault_pinky_02_r = 34;
		const AssetID meursault_pinky_03_l = 16;
		const AssetID meursault_pinky_03_r = 35;
		const AssetID meursault_ring_01_l = 17;
		const AssetID meursault_ring_01_r = 36;
		const AssetID meursault_ring_02_l = 18;
		const AssetID meursault_ring_02_r = 37;
		const AssetID meursault_ring_03_l = 19;
		const AssetID meursault_ring_03_r = 38;
		const AssetID meursault_spine_01 = 1;
		const AssetID meursault_spine_02 = 2;
		const AssetID meursault_spine_03 = 3;
		const AssetID meursault_thigh_l = 44;
		const AssetID meursault_thigh_r = 48;
		const AssetID meursault_thumb_01_l = 20;
		const AssetID meursault_thumb_01_r = 39;
		const AssetID meursault_thumb_02_l = 21;
		const AssetID meursault_thumb_02_r = 40;
		const AssetID meursault_thumb_03_l = 22;
		const AssetID meursault_thumb_03_r = 41;
		const AssetID meursault_upperarm_l = 5;
		const AssetID meursault_upperarm_r = 24;
		const AssetID parkour_attach_point = 0;
		const AssetID parkour_camera = 1;
		const AssetID parkour_claw1_L = 9;
		const AssetID parkour_claw1_R = 15;
		const AssetID parkour_claw2_L = 11;
		const AssetID parkour_claw2_R = 16;
		const AssetID parkour_claw3_L = 10;
		const AssetID parkour_claw3_R = 17;
		const AssetID parkour_foot_L = 20;
		const AssetID parkour_foot_R = 23;
		const AssetID parkour_forearm_L = 7;
		const AssetID parkour_forearm_R = 13;
		const AssetID parkour_hand_L = 8;
		const AssetID parkour_hand_R = 14;
		const AssetID parkour_head = 5;
		const AssetID parkour_headless_attach_point = 0;
		const AssetID parkour_headless_camera = 1;
		const AssetID parkour_headless_claw1_L = 9;
		const AssetID parkour_headless_claw1_R = 15;
		const AssetID parkour_headless_claw2_L = 11;
		const AssetID parkour_headless_claw2_R = 16;
		const AssetID parkour_headless_claw3_L = 10;
		const AssetID parkour_headless_claw3_R = 17;
		const AssetID parkour_headless_foot_L = 20;
		const AssetID parkour_headless_foot_R = 23;
		const AssetID parkour_headless_forearm_L = 7;
		const AssetID parkour_headless_forearm_R = 13;
		const AssetID parkour_headless_hand_L = 8;
		const AssetID parkour_headless_hand_R = 14;
		const AssetID parkour_headless_head = 5;
		const AssetID parkour_headless_hips = 2;
		const AssetID parkour_headless_neck = 4;
		const AssetID parkour_headless_shin_L = 19;
		const AssetID parkour_headless_shin_R = 22;
		const AssetID parkour_headless_spine = 3;
		const AssetID parkour_headless_thigh_L = 18;
		const AssetID parkour_headless_thigh_R = 21;
		const AssetID parkour_headless_upper_arm_L = 6;
		const AssetID parkour_headless_upper_arm_R = 12;
		const AssetID parkour_hips = 2;
		const AssetID parkour_neck = 4;
		const AssetID parkour_shin_L = 19;
		const AssetID parkour_shin_R = 22;
		const AssetID parkour_spine = 3;
		const AssetID parkour_thigh_L = 18;
		const AssetID parkour_thigh_R = 21;
		const AssetID parkour_upper_arm_L = 6;
		const AssetID parkour_upper_arm_R = 12;
		const AssetID sailor_ball_l = 49;
		const AssetID sailor_ball_r = 53;
		const AssetID sailor_calf_l = 47;
		const AssetID sailor_calf_r = 51;
		const AssetID sailor_clavicle_l = 6;
		const AssetID sailor_clavicle_r = 25;
		const AssetID sailor_door = 0;
		const AssetID sailor_foot_l = 48;
		const AssetID sailor_foot_r = 52;
		const AssetID sailor_hand_l = 9;
		const AssetID sailor_hand_r = 28;
		const AssetID sailor_head = 45;
		const AssetID sailor_index_01_l = 10;
		const AssetID sailor_index_01_r = 29;
		const AssetID sailor_index_02_l = 11;
		const AssetID sailor_index_02_r = 30;
		const AssetID sailor_index_03_l = 12;
		const AssetID sailor_index_03_r = 31;
		const AssetID sailor_lowerarm_l = 8;
		const AssetID sailor_lowerarm_r = 27;
		const AssetID sailor_middle_01_l = 13;
		const AssetID sailor_middle_01_r = 32;
		const AssetID sailor_middle_02_l = 14;
		const AssetID sailor_middle_02_r = 33;
		const AssetID sailor_middle_03_l = 15;
		const AssetID sailor_middle_03_r = 34;
		const AssetID sailor_neck_01 = 44;
		const AssetID sailor_pelvis = 2;
		const AssetID sailor_pinky_01_l = 16;
		const AssetID sailor_pinky_01_r = 35;
		const AssetID sailor_pinky_02_l = 17;
		const AssetID sailor_pinky_02_r = 36;
		const AssetID sailor_pinky_03_l = 18;
		const AssetID sailor_pinky_03_r = 37;
		const AssetID sailor_ring_01_l = 19;
		const AssetID sailor_ring_01_r = 38;
		const AssetID sailor_ring_02_l = 20;
		const AssetID sailor_ring_02_r = 39;
		const AssetID sailor_ring_03_l = 21;
		const AssetID sailor_ring_03_r = 40;
		const AssetID sailor_root = 1;
		const AssetID sailor_spine_01 = 3;
		const AssetID sailor_spine_02 = 4;
		const AssetID sailor_spine_03 = 5;
		const AssetID sailor_thigh_l = 46;
		const AssetID sailor_thigh_r = 50;
		const AssetID sailor_thumb_01_l = 22;
		const AssetID sailor_thumb_01_r = 41;
		const AssetID sailor_thumb_02_l = 23;
		const AssetID sailor_thumb_02_r = 42;
		const AssetID sailor_thumb_03_l = 24;
		const AssetID sailor_thumb_03_r = 43;
		const AssetID sailor_upperarm_l = 7;
		const AssetID sailor_upperarm_r = 26;
		const AssetID terminal_bottom_L = 1;
		const AssetID terminal_bottom_R = 2;
		const AssetID terminal_middle_L = 3;
		const AssetID terminal_middle_R = 4;
		const AssetID terminal_monitor = 8;
		const AssetID terminal_monitor_arm_1 = 6;
		const AssetID terminal_monitor_arm_2 = 7;
		const AssetID terminal_root = 0;
		const AssetID terminal_top = 5;
		const AssetID tram_doors_door1 = 0;
		const AssetID tram_doors_door2 = 1;
		const AssetID tram_doors_door3 = 2;
		const AssetID tram_doors_door4 = 3;
		const AssetID tram_doors_door5 = 4;
		const AssetID tram_doors_door6 = 5;
	}
}

}