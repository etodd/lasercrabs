#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 53;
		const AssetID Alpha = 0;
		const AssetID awk = 1;
		const AssetID compass_inner = 2;
		const AssetID compass_outer = 3;
		const AssetID cone = 4;
		const AssetID cube = 5;
		const AssetID icon_power = 6;
		const AssetID level1_Plane_1 = 7;
		const AssetID level1_city2 = 8;
		const AssetID level1_city2_001 = 9;
		const AssetID level1_city2_002_2 = 10;
		const AssetID level1_main = 11;
		const AssetID level1_main_2 = 12;
		const AssetID level1_scenery1 = 13;
		const AssetID level1_scenery2 = 14;
		const AssetID level2_Plane_001_2 = 15;
		const AssetID level2_door_001_1 = 16;
		const AssetID level2_door_002_1 = 17;
		const AssetID level2_door_1 = 18;
		const AssetID level2_env_000_1 = 19;
		const AssetID level2_env_001 = 20;
		const AssetID level2_env_001_1 = 21;
		const AssetID level2_env_002 = 22;
		const AssetID level2_env_002_1 = 23;
		const AssetID level3_city2_door = 24;
		const AssetID level3_city2_scenery_2 = 25;
		const AssetID level3_half1_1 = 26;
		const AssetID level3_half1_2 = 27;
		const AssetID level3_half2_1 = 28;
		const AssetID level3_half2_2 = 29;
		const AssetID level3_half3_1 = 30;
		const AssetID level3_half3_2 = 31;
		const AssetID level4_shell = 32;
		const AssetID level4_shell_1 = 33;
		const AssetID logo = 34;
		const AssetID msg = 35;
		const AssetID point_grid = 36;
		const AssetID pvp_pvp_half1 = 37;
		const AssetID pvp_pvp_half1_1 = 38;
		const AssetID pvp_pvp_half2 = 39;
		const AssetID pvp_pvp_half2_1 = 40;
		const AssetID pvp_pvp_scenery_1 = 41;
		const AssetID skybox = 42;
		const AssetID socket = 43;
		const AssetID sphere = 44;
		const AssetID title_city2_scenery = 45;
		const AssetID title_half1 = 46;
		const AssetID title_half1_1 = 47;
		const AssetID title_half2 = 48;
		const AssetID title_half2_1 = 49;
		const AssetID title_half3 = 50;
		const AssetID title_half3_1 = 51;
		const AssetID tri_tube = 52;
	}
	const AssetID mesh_refs[10][9] =
	{
		{
		},
		{
		},
		{
		},
		{
			7,
			8,
			9,
			10,
			11,
			12,
			13,
			14,
		},
		{
			15,
			16,
			17,
			18,
			19,
			20,
			21,
			22,
			23,
		},
		{
			24,
			25,
			26,
			27,
			28,
			29,
			30,
			31,
		},
		{
			32,
			33,
		},
		{
		},
		{
			37,
			38,
			39,
			40,
			41,
		},
		{
			45,
			46,
			47,
			48,
			49,
			50,
			51,
		},
	};
}

}