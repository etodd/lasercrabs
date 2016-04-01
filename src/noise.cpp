#include "noise.h"
#include "mersenne/mersenne-twister.h"

namespace VI
{

// =========================
// Classic 2D and 3D perlin noise implementation
// Adapted from http://webstaff.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
// =========================

namespace noise
{

// Array of possible gradients to choose from for each cell.
// The permutation array indexes s32o this array.
const s32 gradient_count = 12;
Vec3 gradients[gradient_count] =
{
	Vec3(1, 1, 0),
	Vec3(-1, 1, 0),
	Vec3(1, -1, 0),
	Vec3(-1, -1, 0),
	Vec3(1, 0, 1),
	Vec3(-1, 0, 1),
	Vec3(1, 0, -1),
	Vec3(-1, 0, -1),
	Vec3(0, 1, 1),
	Vec3(0, -1, 1),
	Vec3(0, 1, -1),
	Vec3(0, -1, -1),
};

// Pseudo-random permutation index array.
// This is actually constant in the reference implementation.
// But we want a different map each time.
// This way I could set a seed if I wanted to re-generate the same map later.
const s32 permutation_count = 512;
s32 permutations[permutation_count];

void reseed()
{
	for (s32 i = 0; i < permutation_count; i++)
		permutations[i] = mersenne::rand() & 255;
}

// Get a pseudo-random gradient for the given 2D cell
Vec2 gradient_at_cell2d(s32 x, s32 y)
{
	Vec3 g = gradients[permutations[x + permutations[y]] % gradient_count];
	return Vec2(g.x, g.y);
}

struct Coord
{
	s32 x;
	s32 y;
	s32 z;
};

// Get a psuedo-random gradient for the given 3D cell
Vec3 gradient_at_cell3d(const Coord& coord)
{
	return gradients[permutations[coord.x + permutations[coord.y + permutations[coord.z]]] % gradient_count];
}

// f(x) = 6x^5 - 15x^4 + 10x^3
r32 blend_curve(r32 t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// Classic 2D perlin noise
r32 sample2d(const Vec2& pos)
{
	s32 x = (s32)floorf(pos.x) & 255;
	s32 y = (s32)floorf(pos.y) & 255;
	
	Vec2 withinCell = pos - Vec2(x, y);
	
	// Calculate contribution of gradients from each cell
	r32 contribution00 = gradient_at_cell2d(x + 0, y + 0).dot(withinCell - Vec2(0, 0));
	r32 contribution01 = gradient_at_cell2d(x + 0, y + 1).dot(withinCell - Vec2(0, 1));
	r32 contribution10 = gradient_at_cell2d(x + 1, y + 0).dot(withinCell - Vec2(1, 0));
	r32 contribution11 = gradient_at_cell2d(x + 1, y + 1).dot(withinCell - Vec2(1, 1));
	
	Vec2 blend(blend_curve(withinCell.x), blend_curve(withinCell.y));
	
	// Interpolate along X
	r32 contribution0 = LMath::lerpf(blend.x, contribution00, contribution10);
	r32 contribution1 = LMath::lerpf(blend.x, contribution01, contribution11);
	
	// Interpolate along Y
	return LMath::lerpf(blend.y, contribution0, contribution1);
}

// Classic 3D perlin noise
r32 sample3d(const Vec3& pos)
{
	Coord cell =
	{
		(s32)floorf(pos.x),
		(s32)floorf(pos.y),
		(s32)floorf(pos.z),
	};
	
	Vec3 withinCell = pos - Vec3(cell.x, cell.y, cell.z);

	cell.x &= 255;
	cell.y &= 255;
	cell.z &= 255;
	
	// Calculate contribution of gradients from each cell
	r32 contribution000 = gradient_at_cell3d({ cell.x + 0, cell.y + 0, cell.z + 0 }).dot(withinCell - Vec3(0, 0, 0));
	r32 contribution001 = gradient_at_cell3d({ cell.x + 0, cell.y + 0, cell.z + 1 }).dot(withinCell - Vec3(0, 0, 1));
	r32 contribution010 = gradient_at_cell3d({ cell.x + 0, cell.y + 1, cell.z + 0 }).dot(withinCell - Vec3(0, 1, 0));
	r32 contribution011 = gradient_at_cell3d({ cell.x + 0, cell.y + 1, cell.z + 1 }).dot(withinCell - Vec3(0, 1, 1));
	r32 contribution100 = gradient_at_cell3d({ cell.x + 1, cell.y + 0, cell.z + 0 }).dot(withinCell - Vec3(1, 0, 0));
	r32 contribution101 = gradient_at_cell3d({ cell.x + 1, cell.y + 0, cell.z + 1 }).dot(withinCell - Vec3(1, 0, 1));
	r32 contribution110 = gradient_at_cell3d({ cell.x + 1, cell.y + 1, cell.z + 0 }).dot(withinCell - Vec3(1, 1, 0));
	r32 contribution111 = gradient_at_cell3d({ cell.x + 1, cell.y + 1, cell.z + 1 }).dot(withinCell - Vec3(1, 1, 1));
	
	Vec3 blend(blend_curve(withinCell.x), blend_curve(withinCell.y), blend_curve(withinCell.z));
	
	// Interpolate along X
	r32 contribution00 = LMath::lerpf(blend.x, contribution000, contribution100);
	r32 contribution01 = LMath::lerpf(blend.x, contribution001, contribution101);
	r32 contribution10 = LMath::lerpf(blend.x, contribution010, contribution110);
	r32 contribution11 = LMath::lerpf(blend.x, contribution011, contribution111);
	
	// Interpolate along Y
	r32 contribution0 = LMath::lerpf(blend.y, contribution00, contribution10);
	r32 contribution1 = LMath::lerpf(blend.y, contribution01, contribution11);
	
	// Interpolate along Z
	return LMath::lerpf(contribution0, contribution1, blend.z);
}

}
	

}