#pragma once

#include "types.h"
#include "data/pin_array.h"

namespace VI
{

struct RenderParams;
struct Update;

#define SUDOKU_PUZZLES 64

struct Sudoku
{
	static s8 puzzles[SUDOKU_PUZZLES][16];
	r32 timer;
	s16 solved;
	s8 state[16]; // stored as a list of rows
	s8 current_pos;
	s8 current_value;

	b8 complete() const;
	void solve();
	s8 get(s32, s32) const;
	s32 solved_count() const;

	Sudoku();
	void update(const Update&, s8);
	void draw(const RenderParams&, s8) const;
};

}