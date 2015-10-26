#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 28;
		const AssetID Alpha = 0;
		const AssetID awk = 1;
		const AssetID city1 = 2;
		const AssetID city2 = 3;
		const AssetID city3_city3 = 4;
		const AssetID city3_city3_1 = 5;
		const AssetID city3_elevator_2 = 6;
		const AssetID city4_elevator = 7;
		const AssetID city4_shell = 8;
		const AssetID city4_shell_1 = 9;
		const AssetID cone = 10;
		const AssetID cube = 11;
		const AssetID icon_power = 12;
		const AssetID level1_Plane_1 = 13;
		const AssetID level1_city2 = 14;
		const AssetID level1_city2_001 = 15;
		const AssetID level1_city2_002_2 = 16;
		const AssetID level1_main = 17;
		const AssetID level1_main_2 = 18;
		const AssetID level1_scenery1 = 19;
		const AssetID level1_scenery2 = 20;
		const AssetID logo = 21;
		const AssetID msg = 22;
		const AssetID point_grid = 23;
		const AssetID skybox = 24;
		const AssetID socket = 25;
		const AssetID sphere = 26;
		const AssetID tri_tube = 27;
	}
	const AssetID mesh_refs[9][8] =
	{
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
		},
		{
			7,
			8,
			9,
		},
		{
		},
		{
		},
		{
			13,
			14,
			15,
			16,
			17,
			18,
			19,
			20,
		},
		{
		},
		{
		},
	};
}

}