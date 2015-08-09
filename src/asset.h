#pragma once
#include "types.h"

namespace VI
{

struct Asset
{
	struct Mesh
	{
		static const int count = 9;
		static const char* values[9];
		static const AssetID Alpha;
		static const AssetID city1;
		static const AssetID city2;
		static const AssetID city3;
		static const AssetID city4_elevator;
		static const AssetID city4_shell;
		static const AssetID city4_shell_1;
		static const AssetID cube;
		static const AssetID skybox;
	};
	struct Animation
	{
		static const int count = 2;
		static const char* values[2];
		static const AssetID elevator1;
		static const AssetID walk;
	};
	struct Armature
	{
		static const int count = 7;
		static const char* values[7];
		static const AssetID Alpha;
		static const AssetID city1;
		static const AssetID city2;
		static const AssetID city3;
		static const AssetID city4;
		static const AssetID cube;
		static const AssetID skybox;
	};
	struct Bone
	{
		static const int count = 68;
		static const AssetID Alpha_Alpha_Head;
		static const AssetID Alpha_Alpha_Hips;
		static const AssetID Alpha_Alpha_Neck;
		static const AssetID Alpha_Alpha_Neck1;
		static const AssetID Alpha_Alpha_Spine;
		static const AssetID Alpha_Alpha_Spine1;
		static const AssetID Alpha_Alpha_Spine2;
		static const AssetID Alpha_Left_Arm;
		static const AssetID Alpha_Left_Foot;
		static const AssetID Alpha_Left_ForeArm;
		static const AssetID Alpha_Left_Hand;
		static const AssetID Alpha_Left_HandIndex1;
		static const AssetID Alpha_Left_HandIndex2;
		static const AssetID Alpha_Left_HandIndex3;
		static const AssetID Alpha_Left_HandMiddle1;
		static const AssetID Alpha_Left_HandMiddle2;
		static const AssetID Alpha_Left_HandMiddle3;
		static const AssetID Alpha_Left_HandPinky1;
		static const AssetID Alpha_Left_HandPinky2;
		static const AssetID Alpha_Left_HandPinky3;
		static const AssetID Alpha_Left_HandRing1;
		static const AssetID Alpha_Left_HandRing2;
		static const AssetID Alpha_Left_HandRing3;
		static const AssetID Alpha_Left_HandThumb1;
		static const AssetID Alpha_Left_HandThumb2;
		static const AssetID Alpha_Left_HandThumb3;
		static const AssetID Alpha_Left_Leg;
		static const AssetID Alpha_Left_Shoulder;
		static const AssetID Alpha_Left_ToeBase;
		static const AssetID Alpha_Left_UpLeg;
		static const AssetID Alpha_Right_Arm;
		static const AssetID Alpha_Right_Foot;
		static const AssetID Alpha_Right_ForeArm;
		static const AssetID Alpha_Right_Hand;
		static const AssetID Alpha_Right_HandIndex1;
		static const AssetID Alpha_Right_HandIndex2;
		static const AssetID Alpha_Right_HandIndex3;
		static const AssetID Alpha_Right_HandMiddle1;
		static const AssetID Alpha_Right_HandMiddle2;
		static const AssetID Alpha_Right_HandMiddle3;
		static const AssetID Alpha_Right_HandPinky1;
		static const AssetID Alpha_Right_HandPinky2;
		static const AssetID Alpha_Right_HandPinky3;
		static const AssetID Alpha_Right_HandRing1;
		static const AssetID Alpha_Right_HandRing2;
		static const AssetID Alpha_Right_HandRing3;
		static const AssetID Alpha_Right_HandThumb1;
		static const AssetID Alpha_Right_HandThumb2;
		static const AssetID Alpha_Right_HandThumb3;
		static const AssetID Alpha_Right_Leg;
		static const AssetID Alpha_Right_Shoulder;
		static const AssetID Alpha_Right_ToeBase;
		static const AssetID Alpha_Right_UpLeg;
		static const AssetID city1_Camera;
		static const AssetID city1_Lamp;
		static const AssetID city1_city1;
		static const AssetID city2_city2;
		static const AssetID city3_Camera;
		static const AssetID city3_Lamp;
		static const AssetID city3_city3;
		static const AssetID city4_elevator;
		static const AssetID city4_shell;
		static const AssetID cube_Camera;
		static const AssetID cube_Lamp;
		static const AssetID cube_cube;
		static const AssetID skybox_Camera;
		static const AssetID skybox_Lamp;
		static const AssetID skybox_skybox;
	};
	struct Texture
	{
		static const int count = 2;
		static const char* values[2];
		static const AssetID skybox_horizon;
		static const AssetID test;
	};
	struct Shader
	{
		static const int count = 4;
		static const char* values[4];
		static const AssetID Armature;
		static const AssetID Standard;
		static const AssetID UI;
		static const AssetID flat_texture;
	};
	struct Uniform
	{
		static const int count = 9;
		static const char* values[9];
		static const AssetID ambient_color;
		static const AssetID bones;
		static const AssetID diffuse_color;
		static const AssetID diffuse_map;
		static const AssetID light_color;
		static const AssetID light_position;
		static const AssetID light_radius;
		static const AssetID m;
		static const AssetID mvp;
	};
	struct Font
	{
		static const int count = 2;
		static const char* values[2];
		static const AssetID SegoeUISymbol;
		static const AssetID lowpoly;
	};
	static const AssetID mesh_refs[7][3];
};

}