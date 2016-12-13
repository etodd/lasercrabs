#include "sudoku.h"
#include "render/ui.h"
#include "mersenne/mersenne-twister.h"
#include "render/render.h"
#include "team.h"
#include "strings.h"
#include "menu.h"

namespace VI
{

b8 sudoku_valid_row(const Sudoku& s, s32 row)
{
	vi_assert(row >= 0 && row < 4);
	s32 counter = 0;
	s32 start = row * 4;
	for (s32 i = 0; i < 4; i++)
		counter |= (1 << s.state[start + i]);
	return counter == 15;
}

b8 sudoku_valid_column(const Sudoku& s, s32 column)
{
	vi_assert(column >= 0 && column < 4);
	s32 counter = 0;
	for (s32 i = column; i < 16; i += 4)
		counter |= (1 << s.state[i]);
	return counter == 15;
}

b8 sudoku_valid_cell(const Sudoku& s, s32 cell_row, s32 cell_column)
{
	vi_assert(cell_row == 0 || cell_row == 2);
	vi_assert(cell_column == 0 || cell_column == 2);
	s32 counter = 0;
	counter |= (1 << s.get(cell_column + 0, cell_row + 0));
	counter |= (1 << s.get(cell_column + 0, cell_row + 1));
	counter |= (1 << s.get(cell_column + 1, cell_row + 0));
	counter |= (1 << s.get(cell_column + 1, cell_row + 1));
	return counter == 15;
}

b8 sudoku_valid(const Sudoku& s)
{
	for (s32 i = 0; i < 4; i++)
	{
		if (!sudoku_valid_row(s, i))
			return false;
	}

	for (s32 i = 0; i < 4; i++)
	{
		if (!sudoku_valid_column(s, i))
			return false;
	}

	if (!sudoku_valid_cell(s, 0, 0))
		return false;
	if (!sudoku_valid_cell(s, 0, 2))
		return false;
	if (!sudoku_valid_cell(s, 2, 0))
		return false;
	if (!sudoku_valid_cell(s, 2, 2))
		return false;

	return true;
}

void sudoku_generate(Sudoku* s)
{
	for (s32 i = 0; i < 10; i++)
	{
		while (true)
		{
			// fill initial values
			for (s32 i = 0; i < 16; i++)
				s->state[i] = i % 4;

			// shuffle
			for (s32 i = 0; i < 16; i++)
			{
				s32 swap_index = i + mersenne::rand() % (16 - i);
				s8 swap_value = s->state[swap_index];
				s->state[swap_index] = s->state[i];
				s->state[i] = swap_value;
			}

			// check if it follows sudoku rules
			if (sudoku_valid(*s))
				break;
		}
	}
}

s8 sudoku_get_free_number(const Sudoku& s)
{
	StaticArray<s32, 16> indices;
	for (s32 i = 0; i < 16; i++)
	{
		if (!(s.solved & (1 << i)))
			indices.add(i);
	}
	if (indices.length == 0)
		return -1;
	else
		return s.state[indices[mersenne::rand() % indices.length]];
}

Sudoku::Sudoku()
	: timer(),
	solved(),
	current_pos(5)
{
	memcpy(state, puzzles[mersenne::rand() % SUDOKU_PUZZLES], sizeof(state));
	StaticArray<s32, 16> indices;
	for (s32 i = 0; i < 16; i++)
		indices.add(i);
	for (s32 i = 0; i < 11; i++)
		indices.remove(mersenne::rand() % indices.length);
	for (s32 i = 0; i < indices.length; i++)
		solved |= 1 << indices[i];
	current_value = sudoku_get_free_number(*this);
}

void Sudoku::solve()
{
	solved = s16(-1);
}

#define SUDOKU_AUTO_SOLVE_TIME 8.0f

void Sudoku::update(const Update& u, s8 gamepad)
{
	if (!complete())
	{
		timer += u.time.delta;
		if (timer > SUDOKU_AUTO_SOLVE_TIME)
		{
			// solve one cell automatically
			StaticArray<s32, 16> candidates; // candidates for which cell to automatically solve
			StaticArray<s32, 16> second_tier_candidates; // candidates which unfortunately are the same number the player has currently selected
			for (s32 i = 0; i < 16; i++)
			{
				if (!(solved & (1 << i)))
				{
					if (state[i] == current_value)
						second_tier_candidates.add(i);
					else
						candidates.add(i);
				}
			}

			if (candidates.length == 0)
			{
				// we have to give the player a new number
				solved |= 1 << second_tier_candidates[mersenne::rand() % second_tier_candidates.length];
				current_value = sudoku_get_free_number(*this);
			}
			else
			{
				// take one of the candidates; player can keep their number
				solved |= 1 << candidates[mersenne::rand() % candidates.length];
			}

			timer = 0.0f;
		}
	}

	s32 x = current_pos % 4;
	s32 y = current_pos / 4;
	x += UI::input_delta_horizontal(u, gamepad);
	y -= UI::input_delta_vertical(u, gamepad);
	x = vi_max(0, vi_min(3, x));
	y = vi_max(0, vi_min(3, y));
	current_pos = x + y * 4;

	if (!(solved & (1 << current_pos))
		&& u.input->get(Controls::Interact, gamepad)
		&& state[current_pos] == current_value)
	{
		// player got it right, insert it
		solved |= 1 << current_pos;
		current_value = sudoku_get_free_number(*this);
		timer = 0.0f;
	}
}

s8 Sudoku::get(s32 x, s32 y) const
{
	vi_assert(x >= 0 && x < 4 && y >= 0 && y < 4);
	return state[x + (y * 4)];
}

s32 Sudoku::solved_count() const
{
	s32 counter = 0;
	for (s32 i = 0; i < 16; i++)
	{
		if (solved & (1 << i))
			counter++;
	}
	return counter;
}

b8 Sudoku::complete() const
{
	return solved == s16(-1);
}

void Sudoku::draw(const RenderParams& params, s8 gamepad) const
{
	UIText text;
	text.anchor_x = text.anchor_y = UIText::Anchor::Center;
	text.size = UI_TEXT_SIZE_DEFAULT * 2.0f;
	const Vec2 cell_spacing(64.0f * UI::scale);
	const Vec2 cell_size(48.0f * UI::scale);

	// progress bar
	{
		text.text(_(strings::hacking));

		Vec2 pos = params.camera->viewport.size * 0.5f + Vec2(0, cell_spacing.y * 3.0f);
		Rect2 box = text.rect(pos).outset(MENU_ITEM_PADDING);
		UI::box(params, box, UI::color_background);
		UI::border(params, box, 2, UI::color_accent);

		r32 progress = (r32(solved_count()) + (timer / SUDOKU_AUTO_SOLVE_TIME)) / 16.0f;
		UI::box(params, { box.pos, Vec2(box.size.x * progress, box.size.y) }, UI::color_accent);

		text.color = UI::color_background;
		text.draw(params, pos);
	}

	Vec2 offset = params.camera->viewport.size * 0.5f + cell_spacing * -1.5f;
	for (s32 x = 0; x < 4; x++)
	{
		for (s32 y = 0; y < 4; y++)
		{
			Vec2 p = offset + Vec2(x, y) * cell_spacing;
			UI::centered_box(params, { p, cell_size }, UI::color_background);
			s32 index = y * 4 + x;
			b8 hovering = !complete() && index == current_pos;
			b8 already_solved = solved & (1 << index);
			if (already_solved)
			{
				// fade out number when player is hovering over it
				text.color = hovering ? UI::color_disabled : UI::color_accent;
				text.text("%d", s32(state[index]) + 1);
				text.draw(params, p);
			}

			if (hovering)
			{
				b8 pressed = params.sync->input.get(Controls::Interact, gamepad)
					&& (already_solved || state[current_pos] != current_value);

				const Vec4* color;
				if (pressed)
					color = &UI::color_disabled;
				else
					color = already_solved ? &UI::color_alert : &UI::color_default;
				UI::centered_border(params, { p, cell_size }, 4.0f, *color);

				if (!pressed)
				{
					text.color = *color;
					text.text("%d", s32(current_value) + 1);
					text.draw(params, p);
				}
			}
		}
	}
}

s8 Sudoku::puzzles[64][16] =
{
	{
		1, 0, 3, 2,
		2, 3, 0, 1,
		3, 2, 1, 0,
		0, 1, 2, 3,
	},
	{
		2, 0, 3, 1,
		1, 3, 0, 2,
		0, 1, 2, 3,
		3, 2, 1, 0,
	},
	{
		2, 1, 0, 3,
		3, 0, 1, 2,
		1, 2, 3, 0,
		0, 3, 2, 1,
	},
	{
		1, 0, 3, 2,
		3, 2, 0, 1,
		0, 1, 2, 3,
		2, 3, 1, 0,
	},
	{
		1, 3, 0, 2,
		2, 0, 3, 1,
		0, 1, 2, 3,
		3, 2, 1, 0,
	},
	{
		0, 1, 2, 3,
		2, 3, 0, 1,
		1, 0, 3, 2,
		3, 2, 1, 0,
	},
	{
		2, 3, 1, 0,
		1, 0, 2, 3,
		0, 2, 3, 1,
		3, 1, 0, 2,
	},
	{
		3, 0, 1, 2,
		1, 2, 0, 3,
		0, 3, 2, 1,
		2, 1, 3, 0,
	},
	{
		0, 2, 3, 1,
		1, 3, 0, 2,
		3, 1, 2, 0,
		2, 0, 1, 3,
	},
	{
		1, 0, 2, 3,
		3, 2, 1, 0,
		0, 1, 3, 2,
		2, 3, 0, 1,
	},
	{
		1, 0, 3, 2,
		3, 2, 1, 0,
		0, 3, 2, 1,
		2, 1, 0, 3,
	},
	{
		3, 1, 2, 0,
		0, 2, 1, 3,
		2, 0, 3, 1,
		1, 3, 0, 2,
	},
	{
		0, 1, 3, 2,
		2, 3, 1, 0,
		1, 2, 0, 3,
		3, 0, 2, 1,
	},
	{
		1, 0, 3, 2,
		3, 2, 0, 1,
		0, 1, 2, 3,
		2, 3, 1, 0,
	},
	{
		1, 3, 0, 2,
		2, 0, 3, 1,
		0, 1, 2, 3,
		3, 2, 1, 0,
	},
	{
		0, 1, 2, 3,
		3, 2, 0, 1,
		1, 0, 3, 2,
		2, 3, 1, 0,
	},
	{
		0, 3, 2, 1,
		2, 1, 0, 3,
		1, 2, 3, 0,
		3, 0, 1, 2,
	},
	{
		2, 1, 0, 3,
		3, 0, 1, 2,
		1, 2, 3, 0,
		0, 3, 2, 1,
	},
	{
		0, 3, 2, 1,
		2, 1, 0, 3,
		3, 2, 1, 0,
		1, 0, 3, 2,
	},
	{
		1, 0, 2, 3,
		3, 2, 0, 1,
		0, 3, 1, 2,
		2, 1, 3, 0,
	},
	{
		3, 1, 2, 0,
		0, 2, 1, 3,
		2, 0, 3, 1,
		1, 3, 0, 2,
	},
	{
		0, 2, 1, 3,
		3, 1, 2, 0,
		2, 3, 0, 1,
		1, 0, 3, 2,
	},
	{
		2, 0, 3, 1,
		3, 1, 2, 0,
		0, 2, 1, 3,
		1, 3, 0, 2,
	},
	{
		2, 0, 3, 1,
		1, 3, 2, 0,
		0, 2, 1, 3,
		3, 1, 0, 2,
	},
	{
		2, 1, 3, 0,
		0, 3, 1, 2,
		3, 2, 0, 1,
		1, 0, 2, 3,
	},
	{
		2, 3, 1, 0,
		1, 0, 3, 2,
		0, 1, 2, 3,
		3, 2, 0, 1,
	},
	{
		2, 3, 0, 1,
		0, 1, 3, 2,
		3, 2, 1, 0,
		1, 0, 2, 3,
	},
	{
		3, 2, 1, 0,
		0, 1, 3, 2,
		2, 3, 0, 1,
		1, 0, 2, 3,
	},
	{
		1, 0, 3, 2,
		2, 3, 1, 0,
		0, 1, 2, 3,
		3, 2, 0, 1,
	},
	{
		0, 2, 1, 3,
		1, 3, 2, 0,
		2, 0, 3, 1,
		3, 1, 0, 2,
	},
	{
		2, 1, 3, 0,
		3, 0, 1, 2,
		0, 3, 2, 1,
		1, 2, 0, 3,
	},
	{
		0, 2, 3, 1,
		1, 3, 2, 0,
		3, 0, 1, 2,
		2, 1, 0, 3,
	},
	{
		1, 3, 2, 0,
		2, 0, 3, 1,
		3, 1, 0, 2,
		0, 2, 1, 3,
	},
	{
		0, 3, 1, 2,
		2, 1, 3, 0,
		3, 0, 2, 1,
		1, 2, 0, 3,
	},
	{
		3, 0, 1, 2,
		1, 2, 3, 0,
		2, 1, 0, 3,
		0, 3, 2, 1,
	},
	{
		3, 2, 1, 0,
		1, 0, 2, 3,
		0, 1, 3, 2,
		2, 3, 0, 1,
	},
	{
		2, 3, 1, 0,
		0, 1, 2, 3,
		1, 0, 3, 2,
		3, 2, 0, 1,
	},
	{
		1, 0, 2, 3,
		2, 3, 1, 0,
		0, 2, 3, 1,
		3, 1, 0, 2,
	},
	{
		1, 0, 2, 3,
		2, 3, 1, 0,
		3, 1, 0, 2,
		0, 2, 3, 1,
	},
	{
		0, 2, 1, 3,
		3, 1, 2, 0,
		1, 0, 3, 2,
		2, 3, 0, 1,
	},
	{
		0, 2, 3, 1,
		1, 3, 0, 2,
		2, 0, 1, 3,
		3, 1, 2, 0,
	},
	{
		0, 1, 2, 3,
		3, 2, 1, 0,
		1, 0, 3, 2,
		2, 3, 0, 1,
	},
	{
		3, 2, 0, 1,
		0, 1, 3, 2,
		2, 0, 1, 3,
		1, 3, 2, 0,
	},
	{
		3, 2, 1, 0,
		0, 1, 2, 3,
		1, 0, 3, 2,
		2, 3, 0, 1,
	},
	{
		1, 0, 3, 2,
		2, 3, 0, 1,
		0, 1, 2, 3,
		3, 2, 1, 0,
	},
	{
		0, 2, 1, 3,
		3, 1, 2, 0,
		2, 0, 3, 1,
		1, 3, 0, 2,
	},
	{
		0, 3, 1, 2,
		1, 2, 0, 3,
		2, 1, 3, 0,
		3, 0, 2, 1,
	},
	{
		0, 3, 2, 1,
		1, 2, 3, 0,
		3, 0, 1, 2,
		2, 1, 0, 3,
	},
	{
		0, 2, 1, 3,
		3, 1, 2, 0,
		1, 3, 0, 2,
		2, 0, 3, 1,
	},
	{
		0, 2, 1, 3,
		1, 3, 0, 2,
		3, 0, 2, 1,
		2, 1, 3, 0,
	},
	{
		2, 1, 0, 3,
		3, 0, 1, 2,
		0, 2, 3, 1,
		1, 3, 2, 0,
	},
	{
		2, 0, 3, 1,
		1, 3, 0, 2,
		0, 2, 1, 3,
		3, 1, 2, 0,
	},
	{
		1, 3, 0, 2,
		0, 2, 1, 3,
		3, 0, 2, 1,
		2, 1, 3, 0,
	},
	{
		2, 0, 1, 3,
		3, 1, 2, 0,
		1, 3, 0, 2,
		0, 2, 3, 1,
	},
	{
		1, 0, 3, 2,
		3, 2, 0, 1,
		2, 3, 1, 0,
		0, 1, 2, 3,
	},
	{
		2, 0, 3, 1,
		1, 3, 0, 2,
		3, 1, 2, 0,
		0, 2, 1, 3,
	},
	{
		0, 1, 2, 3,
		3, 2, 1, 0,
		1, 0, 3, 2,
		2, 3, 0, 1,
	},
	{
		3, 1, 2, 0,
		2, 0, 3, 1,
		0, 3, 1, 2,
		1, 2, 0, 3,
	},
	{
		1, 2, 3, 0,
		0, 3, 1, 2,
		2, 1, 0, 3,
		3, 0, 2, 1,
	},
	{
		3, 0, 2, 1,
		2, 1, 3, 0,
		0, 3, 1, 2,
		1, 2, 0, 3,
	},
	{
		3, 1, 2, 0,
		2, 0, 3, 1,
		1, 3, 0, 2,
		0, 2, 1, 3,
	},
	{
		3, 2, 0, 1,
		0, 1, 3, 2,
		1, 3, 2, 0,
		2, 0, 1, 3,
	},
	{
		2, 1, 0, 3,
		3, 0, 2, 1,
		0, 3, 1, 2,
		1, 2, 3, 0,
	},
	{
		1, 3, 2, 0,
		2, 0, 1, 3,
		3, 1, 0, 2,
		0, 2, 3, 1,
	},
};


}
