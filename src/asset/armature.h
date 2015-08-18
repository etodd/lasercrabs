#pragma once
#include "types.h"
#include "lookup.h"

namespace VI
{

namespace Asset
{
	namespace Armature
	{
		const int count = 7;
		const AssetID Alpha = 0;
		const AssetID city1 = 1;
		const AssetID city2 = 2;
		const AssetID city3 = 3;
		const AssetID city4 = 4;
		const AssetID cube = 5;
		const AssetID skybox = 6;
	}
	namespace Bone
	{
		const int count = 71;
		const AssetID Alpha_Alpha_Head = 6;
		const AssetID Alpha_Alpha_Hips = 0;
		const AssetID Alpha_Alpha_Neck = 4;
		const AssetID Alpha_Alpha_Neck1 = 5;
		const AssetID Alpha_Alpha_Spine = 1;
		const AssetID Alpha_Alpha_Spine1 = 2;
		const AssetID Alpha_Alpha_Spine2 = 3;
		const AssetID Alpha_Left_Arm = 8;
		const AssetID Alpha_Left_Foot = 47;
		const AssetID Alpha_Left_ForeArm = 9;
		const AssetID Alpha_Left_Hand = 10;
		const AssetID Alpha_Left_HandIndex1 = 17;
		const AssetID Alpha_Left_HandIndex2 = 18;
		const AssetID Alpha_Left_HandIndex3 = 19;
		const AssetID Alpha_Left_HandMiddle1 = 14;
		const AssetID Alpha_Left_HandMiddle2 = 15;
		const AssetID Alpha_Left_HandMiddle3 = 16;
		const AssetID Alpha_Left_HandPinky1 = 23;
		const AssetID Alpha_Left_HandPinky2 = 24;
		const AssetID Alpha_Left_HandPinky3 = 25;
		const AssetID Alpha_Left_HandRing1 = 20;
		const AssetID Alpha_Left_HandRing2 = 21;
		const AssetID Alpha_Left_HandRing3 = 22;
		const AssetID Alpha_Left_HandThumb1 = 11;
		const AssetID Alpha_Left_HandThumb2 = 12;
		const AssetID Alpha_Left_HandThumb3 = 13;
		const AssetID Alpha_Left_Leg = 46;
		const AssetID Alpha_Left_Shoulder = 7;
		const AssetID Alpha_Left_ToeBase = 48;
		const AssetID Alpha_Left_UpLeg = 45;
		const AssetID Alpha_Right_Arm = 27;
		const AssetID Alpha_Right_Foot = 51;
		const AssetID Alpha_Right_ForeArm = 28;
		const AssetID Alpha_Right_Hand = 29;
		const AssetID Alpha_Right_HandIndex1 = 33;
		const AssetID Alpha_Right_HandIndex2 = 34;
		const AssetID Alpha_Right_HandIndex3 = 35;
		const AssetID Alpha_Right_HandMiddle1 = 36;
		const AssetID Alpha_Right_HandMiddle2 = 37;
		const AssetID Alpha_Right_HandMiddle3 = 38;
		const AssetID Alpha_Right_HandPinky1 = 42;
		const AssetID Alpha_Right_HandPinky2 = 43;
		const AssetID Alpha_Right_HandPinky3 = 44;
		const AssetID Alpha_Right_HandRing1 = 39;
		const AssetID Alpha_Right_HandRing2 = 40;
		const AssetID Alpha_Right_HandRing3 = 41;
		const AssetID Alpha_Right_HandThumb1 = 30;
		const AssetID Alpha_Right_HandThumb2 = 31;
		const AssetID Alpha_Right_HandThumb3 = 32;
		const AssetID Alpha_Right_Leg = 50;
		const AssetID Alpha_Right_Shoulder = 26;
		const AssetID Alpha_Right_ToeBase = 52;
		const AssetID Alpha_Right_UpLeg = 49;
		const AssetID city1_Camera = 2;
		const AssetID city1_Lamp = 1;
		const AssetID city1_city1 = 0;
		const AssetID city2_city2 = 0;
		const AssetID city3_Camera = 2;
		const AssetID city3_Lamp = 1;
		const AssetID city3_city3 = 0;
		const AssetID city4_Empty = 1;
		const AssetID city4_elevator = 3;
		const AssetID city4_shell = 4;
		const AssetID city4_spawn = 2;
		const AssetID city4_test = 0;
		const AssetID cube_Camera = 2;
		const AssetID cube_Lamp = 1;
		const AssetID cube_cube = 0;
		const AssetID skybox_Camera = 2;
		const AssetID skybox_Lamp = 1;
		const AssetID skybox_skybox = 0;
	}
	namespace Metadata
	{
		const int count = 5;
		const AssetID AspectH = 0;
		const AssetID AspectW = 1;
		const AssetID PlayerSpawn = 2;
		const AssetID Sentinel = 3;
		const AssetID StaticGeom = 4;
	}
	const AssetID mesh_refs[7][4] =
	{	{
		0,
	},
	{
		1,
	},
	{
		2,
	},
	{
		3,
	},
	{
		4,
		5,
		6,
		7,
	},
	{
		8,
	},
	{
		9,
	},
};
	const AssetID metadata_refs[7][3] =
	{	{
	},
	{
		0,
		1,
	},
	{
	},
	{
		0,
		1,
	},
	{
		2,
		3,
		4,
	},
	{
		0,
		1,
	},
	{
		0,
		1,
	},
};
}

}