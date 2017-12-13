#include "noise.h"
#include "mersenne/mersenne-twister.h"

namespace VI
{

// =========================
// classic 2D perlin noise implementation
// adapted from http://webstaff.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
// =========================

namespace noise
{


// array of possible gradients to choose from for each cell.
// the permutation array indexes s32o this array.
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

// pseudo-random permutation index array.
// this is actually constant in the reference implementation.
// but we want a different map each time.
// this way I could set a seed if I wanted to re-generate the same map later.
const s32 permutation_count = 512;
s32 permutations[permutation_count];

void reseed()
{
	for (s32 i = 0; i < permutation_count; i++)
		permutations[i] = mersenne::rand() & 255;
}

// get a pseudo-random gradient for the given 2D cell
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

// get a psuedo-random gradient for the given 3D cell
Vec3 gradient_at_cell3d(const Coord& coord)
{
	return gradients[permutations[coord.x + permutations[coord.y + permutations[coord.z]]] % gradient_count];
}

// f(x) = 6x^5 - 15x^4 + 10x^3
r32 blend_curve(r32 t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// classic 2D perlin noise
r32 sample2d(const Vec2& pos)
{
	s32 x = s32(floorf(pos.x)) & 255;
	s32 y = s32(floorf(pos.y)) & 255;
	
	Vec2 within_cell = Vec2(fmodf(pos.x, 256.0f), fmodf(pos.y, 256.0f)) - Vec2(r32(x), r32(y));
	
	// calculate contribution of gradients from each cell
	r32 contribution00 = gradient_at_cell2d(x + 0, y + 0).dot(within_cell - Vec2(0, 0));
	r32 contribution01 = gradient_at_cell2d(x + 0, y + 1).dot(within_cell - Vec2(0, 1));
	r32 contribution10 = gradient_at_cell2d(x + 1, y + 0).dot(within_cell - Vec2(1, 0));
	r32 contribution11 = gradient_at_cell2d(x + 1, y + 1).dot(within_cell - Vec2(1, 1));
	
	Vec2 blend(blend_curve(within_cell.x), blend_curve(within_cell.y));
	
	// interpolate along X
	r32 contribution0 = LMath::lerpf(blend.x, contribution00, contribution10);
	r32 contribution1 = LMath::lerpf(blend.x, contribution01, contribution11);
	
	// interpolate along Y
	return LMath::lerpf(blend.y, contribution0, contribution1) * 2.0f;
}


}
	

}