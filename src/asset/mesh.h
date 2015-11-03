#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 35;
		const AssetID Alpha = 0;
		const AssetID awk = 1;
		const AssetID city1 = 2;
		const AssetID city2 = 3;
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
		const AssetID level3_city3 = 24;
		const AssetID level3_city3_1 = 25;
		const AssetID level4_shell = 26;
		const AssetID level4_shell_1 = 27;
		const AssetID logo = 28;
		const AssetID msg = 29;
		const AssetID point_grid = 30;
		const AssetID skybox = 31;
		const AssetID socket = 32;
		const AssetID sphere = 33;
		const AssetID tri_tube = 34;
	}
	const AssetID mesh_refs[11][9] =
	{
		{
			2,
		},
		{
			3,
		},
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
		},
		{
			26,
			27,
		},
		{
		},
		{
		},
	};
}

}