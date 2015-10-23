#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 19;
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
		const AssetID logo = 13;
		const AssetID skybox = 14;
		const AssetID socket_socket = 15;
		const AssetID socket_socket_1 = 16;
		const AssetID sphere = 17;
		const AssetID tri_tube = 18;
	}
	const AssetID mesh_refs[8][3] =
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
		},
		{
		},
	};
}

}