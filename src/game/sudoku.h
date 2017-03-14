#pragma once

#include "types.h"
#include "data/pin_array.h"

namespace VI
{

struct RenderParams;
struct Update;
struct PlayerHuman;

#define SUDOKU_PUZZLES 64

struct Sudoku
{
	struct Puzzle
	{
		s16 solved;
		s8 state[16];
	};
	static Puzzle puzzles[SUDOKU_PUZZLES];
	r32 timer;
	r32 timer_animation; // for opening/closing animation
	r32 timer_error; 
	r32 flash_timer;
	s16 solved;
	s8 state[16]; // stored as a list of rows
	s8 current_pos;
	s8 flash_pos;
	s8 current_value;

	b8 complete() const;
	s8 get(s32, s32) const;
	s32 solved_count() const;
	void solve(PlayerHuman*);

	Sudoku();
	void reset();
	void update(const Update&, s8, PlayerHuman*);
	void draw(const RenderParams&, s8) const;
};

}