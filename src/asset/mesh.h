#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 25;
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
		const AssetID level1_city2 = 13;
		const AssetID level1_city2_001 = 14;
		const AssetID level1_city2_002 = 15;
		const AssetID level1_city3 = 16;
		const AssetID level1_city3_1 = 17;
		const AssetID level1_scenery1 = 18;
		const AssetID logo = 19;
		const AssetID skybox = 20;
		const AssetID socket_socket = 21;
		const AssetID socket_socket_1 = 22;
		const AssetID sphere = 23;
		const AssetID tri_tube = 24;
	}
	const AssetID mesh_refs[9][6] =
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
		},
		{
		},
		{
		},
	};
}

}