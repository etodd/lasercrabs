#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 14;
		const AssetID Alpha = 0;
		const AssetID awk = 1;
		const AssetID city1 = 2;
		const AssetID city2 = 3;
		const AssetID city3_city3 = 4;
		const AssetID city3_city3_1 = 5;
		const AssetID city4_elevator = 6;
		const AssetID city4_shell = 7;
		const AssetID city4_shell_1 = 8;
		const AssetID cone = 9;
		const AssetID cube = 10;
		const AssetID skybox = 11;
		const AssetID sphere = 12;
		const AssetID tri_tube = 13;
	}
	const AssetID mesh_refs[4][3] =
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
		},
		{
			6,
			7,
			8,
		},
	};
}

}