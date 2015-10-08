#pragma once
#include "types.h"

namespace VI
{

namespace Asset
{
	namespace Mesh
	{
		const int count = 10;
		const AssetID Alpha = 0;
		const AssetID city1 = 1;
		const AssetID city2 = 2;
		const AssetID city3_city3 = 3;
		const AssetID city3_city3_1 = 4;
		const AssetID city4_elevator = 5;
		const AssetID city4_shell = 6;
		const AssetID city4_shell_1 = 7;
		const AssetID cube = 8;
		const AssetID skybox = 9;
	}
	const AssetID mesh_refs[4][3] =
	{
		{
			1,
		},
		{
			2,
		},
		{
			3,
			4,
		},
		{
			5,
			6,
			7,
		},
	};
}

}