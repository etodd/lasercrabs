#include "sudoku.h"
#include "render/ui.h"
#include "mersenne/mersenne-twister.h"
#include "render/render.h"
#include "team.h"
#include "strings.h"
#include "menu.h"
#include "player.h"
#include "console.h"
#include "net_serialize.h" // for popcount
#include "noise.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"

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

b8 sudoku_valid_cell(const Sudoku& s, s32 cell_column, s32 cell_row)
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
			for (s32 i = 0; i < 15; i++)
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

s32 sudoku_index(s32 x, s32 y)
{
	return x + (y * 4);
}

void sudoku_remove_possibilities_column(const Sudoku& s, s32 x, s32* possibilities)
{
	for (s32 y = 0; y < 4; y++)
	{
		if (s.solved & (1 << sudoku_index(x, y)))
			*possibilities &= ~(1 << s.get(x, y));
	}
}

void sudoku_remove_possibilities_row(const Sudoku& s, s32 y, s32* possibilities)
{
	for (s32 x = 0; x < 4; x++)
	{
		if (s.solved & (1 << sudoku_index(x, y)))
			*possibilities &= ~(1 << s.get(x, y));
	}
}

void sudoku_remove_possibilities_cell(const Sudoku& s, s32 x, s32 y, s32* possibilities)
{
	vi_assert(x == 0 || x == 2);
	vi_assert(y == 0 || y == 2);
	if (s.solved & (1 << sudoku_index(x + 0, y + 0)))
		*possibilities &= ~(1 << s.get(x + 0, y + 0));
	if (s.solved & (1 << sudoku_index(x + 0, y + 1)))
		*possibilities &= ~(1 << s.get(x + 0, y + 1));
	if (s.solved & (1 << sudoku_index(x + 1, y + 0)))
		*possibilities &= ~(1 << s.get(x + 1, y + 0));
	if (s.solved & (1 << sudoku_index(x + 1, y + 1)))
		*possibilities &= ~(1 << s.get(x + 1, y + 1));
}

s32 sudoku_possibilities(const Sudoku& s, s32 x, s32 y)
{
	// how many different numbers could this cell be?
	vi_assert(!(s.solved & (1 << sudoku_index(x, y))));

	s32 possibilities = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);

	sudoku_remove_possibilities_column(s, x, &possibilities);
	sudoku_remove_possibilities_row(s, y, &possibilities);
	sudoku_remove_possibilities_cell(s, (x / 2) * 2, (y / 2) * 2, &possibilities);

	return Net::popcount(possibilities);
}

s8 sudoku_get_free_number(const Sudoku& s)
{
	s32 lowest_possibilities = 4;
	StaticArray<s32, 16> indices;
	for (s32 i = 0; i < 16; i++)
	{
		if (!(s.solved & (1 << i)))
		{
			// this number has not been solved
			s32 possibilities = sudoku_possibilities(s, i % 4, i / 4);
			if (possibilities < lowest_possibilities)
			{
				indices.length = 0;
				lowest_possibilities = possibilities;
			}
			if (possibilities <= lowest_possibilities)
				indices.add(i);
		}
	}
	if (indices.length == 0)
		return -1;
	else
		return s.state[indices[mersenne::rand() % indices.length]];
}

inline void sudoku_swap(s8* a, s8* b)
{
	s8 tmp = *a;
	*a = *b;
	*b = tmp;
}

inline void sudoku_swap(s8* state, s16* solved, s32 a, s32 b)
{
	{
		s8 tmp = state[a];
		state[a] = state[b];
		state[b] = tmp;
	}

	{
		b8 tmp = *solved & (1 << a);
		if (*solved & (1 << b))
			*solved |= 1 << a;
		else
			*solved &= ~(1 << a);
		if (tmp)
			*solved |= 1 << b;
		else
			*solved &= ~(1 << b);
	}
}

#define SUDOKU_AUTO_SOLVE_TIME 3.0f
#define SUDOKU_FLASH_TIME 0.2f
#define SUDOKU_ANIMATION_TIME 0.5f
#define SUDOKU_ERROR_TIME 0.5f

void sudoku_mark_solved(Sudoku* s, s8 index, PlayerHuman* player)
{
	s->solved |= 1 << index;
	s->timer = 0.0f;
	s->flash_pos = index;
	s->flash_timer = SUDOKU_FLASH_TIME;
	if (s->solved_count() >= 15)
	{
		s->solved = s16(-1); // solve the last number automatically
		s->timer_animation = SUDOKU_ANIMATION_TIME;
		player->rumble_add(0.5f);
		Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_WIN);
	}
}

Sudoku::Sudoku()
{
	reset();
}

void Sudoku::reset()
{
	timer = 0.0f;
	timer_animation = SUDOKU_ANIMATION_TIME;
	timer_error = 0.0f;
	flash_timer = 0.0f;

	const Puzzle& puzzle = puzzles[mersenne::rand() % SUDOKU_PUZZLES];
	memcpy(state, puzzle.state, sizeof(state));
	solved = puzzle.solved;

	// replace numbers according to lookup table
	s8 lookup[4] = { 0, 1, 2, 3 };
	for (s32 i = 0; i < 3; i++)
		sudoku_swap(&lookup[i], &lookup[i + mersenne::rand() % (4 - i)]);
	for (s32 i = 0; i < 16; i++)
		state[i] = lookup[state[i]];

	if (mersenne::rand() % 2 == 0)
	{
		// flip horizontally
		for (s32 i = 0; i < 4; i++)
		{
			sudoku_swap(state, &solved, i * 4 + 0, i * 4 + 3);
			sudoku_swap(state, &solved, i * 4 + 1, i * 4 + 2);
		}
	}

	if (mersenne::rand() % 2 == 0)
	{
		// flip vertically
		for (s32 i = 0; i < 4; i++)
		{
			sudoku_swap(state, &solved, i + 0, i + 12);
			sudoku_swap(state, &solved, i + 4, i + 8);
		}
	}

	for (s32 i = 0; i < 16; i++)
	{
		if (!(solved & (1 << i)))
		{
			current_pos = i;
			break;
		}
	}
	current_value = sudoku_get_free_number(*this);
}

void Sudoku::update(const Update& u, s8 gamepad, PlayerHuman* player)
{
	if (flash_timer > 0.0f)
		flash_timer = vi_max(0.0f, flash_timer - Game::real_time.delta);

	if (timer_animation > 0.0f)
	{
		if (Game::edge_trigger(timer_animation, 16.0f / SUDOKU_ANIMATION_TIME, UI::flash_function))
			Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_TILE);
		timer_animation = vi_max(0.0f, timer_animation - Game::real_time.delta);
	}
	else if (timer_error > 0.0f)
		timer_error = vi_max(0.0f, timer_error - Game::real_time.delta);
	else if (!complete())
	{
		timer += Game::real_time.delta;
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
				sudoku_mark_solved(this, second_tier_candidates[mersenne::rand() % second_tier_candidates.length], player);
				current_value = sudoku_get_free_number(*this);
			}
			else
			{
				// take one of the candidates; player can keep their number
				sudoku_mark_solved(this, candidates[mersenne::rand() % candidates.length], player);
			}
		}

		s32 x = current_pos % 4;
		s32 y = current_pos / 4;
		if (!Console::visible)
		{
			b8 move = false;
			s32 delta = UI::input_delta_horizontal(u, gamepad);
			if (delta != 0)
				move = true;
			x += delta;
			delta = UI::input_delta_vertical(u, gamepad);
			if (delta != 0)
				move = true;
			y += delta;
			if (move)
				Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_MOVE);
		}
		x = vi_max(0, vi_min(3, x));
		y = vi_max(0, vi_min(3, y));
		current_pos = x + y * 4;

		if (!Console::visible
			&& !(solved & (1 << current_pos))
			&& u.input->get(Controls::Interact, gamepad) && !u.last_input->get(Controls::Interact, gamepad))
		{
			if (state[current_pos] == current_value)
			{
				// player got it right, insert it
				Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_CORRECT);
				sudoku_mark_solved(this, current_pos, player);
				current_value = sudoku_get_free_number(*this);
				player->rumble_add(0.2f);
			}
			else
			{
				// player got it wrong; maybe take away a number
				Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_INCORRECT);
				if (mersenne::randf_co() > 0.5f && solved != 0)
				{
					StaticArray<s32, 16> indices;
					for (s32 i = 0; i < 16; i++)
					{
						if (solved & (1 << i))
							indices.add(i);
					}
					solved &= ~(1 << indices[mersenne::rand() % indices.length]);
					timer_error = SUDOKU_ERROR_TIME;
					player->rumble_add(1.0f);
					Audio::post_global_event(AK::EVENTS::PLAY_SUDOKU_ERROR);
				}
			}
		}
	}
}

void Sudoku::solve(PlayerHuman* player)
{
	for (s32 i = 0; i < 16; i++)
		sudoku_mark_solved(this, i, player);
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
	const Vec2 cell_spacing(64.0f * UI::scale);
	const Vec2 cell_size(48.0f * UI::scale);

	text.size = UI_TEXT_SIZE_DEFAULT * 2.0f;
	Vec2 offset = params.camera->viewport.size * 0.5f + cell_spacing * -1.5f;
	if (timer_error > 0.0f)
	{
		r32 shake = (timer_error / SUDOKU_ERROR_TIME) * 20.0f * UI::scale;
		r32 time_offset = Game::time.total * 10.0f;
		offset += Vec2(noise::sample3d(Vec3(time_offset)) * shake, noise::sample3d(Vec3(time_offset + 64)) * shake);
	}
	s32 clip_index = s32(15.0f * (timer_animation / SUDOKU_ANIMATION_TIME));
	if (!complete())
		clip_index = 15 - clip_index;
	for (s32 y = 0; y < 4; y++)
	{
		for (s32 x = 0; x < 4; x++)
		{
			s32 index = y * 4 + x;
			if (index > clip_index)
				break;

			Vec2 p = offset + Vec2(x, 3 - y) * cell_spacing;
			UI::centered_box(params, { p, cell_size }, UI::color_background);
			b8 hovering = !complete() && timer_animation == 0.0f && index == current_pos;

			if (index == flash_pos && flash_timer > 0.0f)
			{
				if (UI::flash_function(Game::real_time.total))
				{
					text.color = UI::color_accent();
					text.text(gamepad, "%d", s32(state[index]) + 1);
					text.draw(params, p);

					UI::centered_border(params, { p, cell_size }, 4.0f, UI::color_accent());
				}
			}
			else
			{
				b8 already_solved = solved & (1 << index);
				if (already_solved)
				{
					// draw existing number
					// fade out number when player is hovering over it
					text.color = hovering
						? UI::color_disabled()
						: (timer_error > 0.0f ? UI::color_alert() : UI::color_accent());
					text.text(gamepad, "%d", s32(state[index]) + 1);
					text.draw(params, p);
				}

				if (hovering)
				{
					b8 pressed = !Console::visible && params.sync->input.get(Controls::Interact, gamepad);

					const Vec4& color = already_solved || pressed || timer_error > 0.0f ? UI::color_alert() : UI::color_default;

					UI::centered_border(params, { p, cell_size }, 4.0f, color);

					text.color = color;
					text.text(gamepad, "%d", s32(current_value) + 1);
					text.draw(params, p);
				}
			}
		}
	}

	// progress bar
	{
		text.size = UI_TEXT_SIZE_DEFAULT;
		if (complete())
		{
			if (UI::flash_function(Game::real_time.total))
			{
				text.text(gamepad, _(strings::hack_complete));
				Vec2 pos = params.camera->viewport.size * 0.5f;
				UI::box(params, text.rect(pos).outset(MENU_ITEM_PADDING), UI::color_background);
				text.color = UI::color_accent();
				text.draw(params, pos);
			}
		}
		else if (timer_animation == 0.0f)
		{
			text.text(gamepad, _(strings::hacking));

			Vec2 pos = params.camera->viewport.size * 0.5f + Vec2(0, cell_spacing.y * 2.5f);
			Rect2 box = text.rect(pos).outset(MENU_ITEM_PADDING);
			UI::box(params, box, UI::color_background);
			UI::border(params, box, 4, UI::color_accent());

			r32 progress = (r32(solved_count()) + (timer / SUDOKU_AUTO_SOLVE_TIME)) / 16.0f;
			UI::box(params, { box.pos, Vec2(box.size.x * progress, box.size.y) }, UI::color_accent());

			text.color = UI::color_background;
			text.draw(params, pos);

			pos = params.camera->viewport.size * 0.5f + Vec2(0, cell_spacing.y * -2.5f);
			text.text(gamepad, _(strings::prompt_sudoku_place));
			UI::box(params, text.rect(pos).outset(8.0f * UI::scale), UI::color_background);
			text.color = UI::color_accent();
			text.draw(params, pos);
		}
	}

}

Sudoku::Puzzle Sudoku::puzzles[SUDOKU_PUZZLES] =
{
	{
		s16(0b0001100000011000),
		3, 0, 1, 2,
		1, 2, 3, 0,
		2, 1, 0, 3,
		0, 3, 2, 1,
	},
	{
		s16(0b0101000000001010),
		1, 3, 0, 2,
		2, 0, 1, 3,
		3, 1, 2, 0,
		0, 2, 3, 1,
	},
	{
		s16(0b1010000000000101),
		0, 1, 3, 2,
		3, 2, 1, 0,
		1, 0, 2, 3,
		2, 3, 0, 1,
	},
	{
		s16(0b1100100000010011),
		0, 2, 1, 3,
		3, 1, 2, 0,
		2, 0, 3, 1,
		1, 3, 0, 2,
	},
	{
		s16(0b0001000110001000),
		2, 1, 0, 3,
		3, 0, 2, 1,
		0, 3, 1, 2,
		1, 2, 3, 0,
	},
	{
		s16(0b1010000000000101),
		2, 1, 0, 3,
		3, 0, 1, 2,
		1, 3, 2, 0,
		0, 2, 3, 1,
	},
	{
		s16(0b1000110000110001),
		2, 0, 3, 1,
		1, 3, 0, 2,
		3, 1, 2, 0,
		0, 2, 1, 3,
	},
	{
		s16(0b0001010000101000),
		3, 2, 1, 0,
		0, 1, 3, 2,
		1, 0, 2, 3,
		2, 3, 0, 1,
	},
	{
		s16(0b0001100000011000),
		1, 3, 0, 2,
		0, 2, 1, 3,
		2, 0, 3, 1,
		3, 1, 2, 0,
	},
	{
		s16(0b0001000110001000),
		0, 2, 1, 3,
		3, 1, 0, 2,
		1, 3, 2, 0,
		2, 0, 3, 1,
	},
	{
		s16(0b0001000110001000),
		1, 2, 3, 0,
		3, 0, 2, 1,
		2, 1, 0, 3,
		0, 3, 1, 2,
	},
	{
		s16(0b0111000000001110),
		0, 1, 3, 2,
		3, 2, 0, 1,
		1, 0, 2, 3,
		2, 3, 1, 0,
	},
	{
		s16(0b0010100000010100),
		3, 2, 0, 1,
		0, 1, 2, 3,
		1, 0, 3, 2,
		2, 3, 1, 0,
	},
	{
		s16(0b0010001111000100),
		3, 0, 2, 1,
		1, 2, 0, 3,
		0, 3, 1, 2,
		2, 1, 3, 0,
	},
	{
		s16(0b0010010000100100),
		2, 1, 0, 3,
		3, 0, 2, 1,
		0, 3, 1, 2,
		1, 2, 3, 0,
	},
	{
		s16(0b1000100000010001),
		1, 0, 2, 3,
		3, 2, 1, 0,
		0, 1, 3, 2,
		2, 3, 0, 1,
	},
	{
		s16(0b1000000110000001),
		0, 3, 1, 2,
		1, 2, 0, 3,
		3, 1, 2, 0,
		2, 0, 3, 1,
	},
	{
		s16(0b1100000000000011),
		1, 3, 2, 0,
		2, 0, 1, 3,
		0, 1, 3, 2,
		3, 2, 0, 1,
	},
	{
		s16(0b0010010000100100),
		2, 0, 1, 3,
		3, 1, 2, 0,
		1, 3, 0, 2,
		0, 2, 3, 1,
	},
	{
		s16(0b0001000110001000),
		2, 0, 1, 3,
		1, 3, 0, 2,
		3, 1, 2, 0,
		0, 2, 3, 1,
	},
	{
		s16(0b0001100000011000),
		2, 0, 1, 3,
		1, 3, 2, 0,
		0, 1, 3, 2,
		3, 2, 0, 1,
	},
	{
		s16(0b1100000000000011),
		2, 0, 3, 1,
		3, 1, 2, 0,
		1, 2, 0, 3,
		0, 3, 1, 2,
	},
	{
		s16(0b1000001001000001),
		1, 0, 3, 2,
		2, 3, 0, 1,
		3, 2, 1, 0,
		0, 1, 2, 3,
	},
	{
		s16(0b0101000000001010),
		3, 0, 1, 2,
		2, 1, 0, 3,
		0, 3, 2, 1,
		1, 2, 3, 0,
	},
	{
		s16(0b0001000110001000),
		0, 1, 3, 2,
		3, 2, 1, 0,
		1, 0, 2, 3,
		2, 3, 0, 1,
	},
	{
		s16(0b1000100000010001),
		1, 3, 0, 2,
		2, 0, 1, 3,
		3, 1, 2, 0,
		0, 2, 3, 1,
	},
	{
		s16(0b0001000110001000),
		1, 2, 0, 3,
		3, 0, 1, 2,
		2, 1, 3, 0,
		0, 3, 2, 1,
	},
	{
		s16(0b0010001001000100),
		2, 0, 3, 1,
		3, 1, 0, 2,
		1, 3, 2, 0,
		0, 2, 1, 3,
	},
	{
		s16(0b1000000110000001),
		1, 2, 0, 3,
		0, 3, 1, 2,
		3, 0, 2, 1,
		2, 1, 3, 0,
	},
	{
		s16(0b0000010110100000),
		3, 1, 2, 0,
		2, 0, 1, 3,
		1, 3, 0, 2,
		0, 2, 3, 1,
	},
	{
		s16(0b0010010000100100),
		3, 1, 0, 2,
		0, 2, 1, 3,
		2, 0, 3, 1,
		1, 3, 2, 0,
	},
	{
		s16(0b0000101001010000),
		0, 3, 2, 1,
		1, 2, 0, 3,
		3, 0, 1, 2,
		2, 1, 3, 0,
	},
	{
		s16(0b1001010000101001),
		3, 1, 0, 2,
		2, 0, 3, 1,
		1, 3, 2, 0,
		0, 2, 1, 3,
	},
	{
		s16(0b0000010110100000),
		0, 1, 3, 2,
		2, 3, 0, 1,
		1, 0, 2, 3,
		3, 2, 1, 0,
	},
	{
		s16(0b1100100000010011),
		0, 3, 1, 2,
		1, 2, 0, 3,
		3, 0, 2, 1,
		2, 1, 3, 0,
	},
	{
		s16(0b0000110000110000),
		2, 0, 3, 1,
		3, 1, 2, 0,
		0, 3, 1, 2,
		1, 2, 0, 3,
	},
	{
		s16(0b0101000000001010),
		1, 3, 2, 0,
		0, 2, 3, 1,
		3, 1, 0, 2,
		2, 0, 1, 3,
	},
	{
		s16(0b1000001001000001),
		0, 3, 1, 2,
		1, 2, 3, 0,
		2, 1, 0, 3,
		3, 0, 2, 1,
	},
	{
		s16(0b0001100000011000),
		1, 2, 0, 3,
		0, 3, 1, 2,
		3, 1, 2, 0,
		2, 0, 3, 1,
	},
	{
		s16(0b1000100000010001),
		0, 1, 3, 2,
		2, 3, 0, 1,
		1, 0, 2, 3,
		3, 2, 1, 0,
	},
	{
		s16(0b0000010110100000),
		3, 1, 2, 0,
		0, 2, 3, 1,
		1, 3, 0, 2,
		2, 0, 1, 3,
	},
	{
		s16(0b1000001001000001),
		1, 3, 2, 0,
		2, 0, 3, 1,
		0, 2, 1, 3,
		3, 1, 0, 2,
	},
	{
		s16(0b0001000110001000),
		2, 1, 0, 3,
		3, 0, 2, 1,
		1, 2, 3, 0,
		0, 3, 1, 2,
	},
	{
		s16(0b1000110000110001),
		0, 3, 2, 1,
		1, 2, 3, 0,
		2, 1, 0, 3,
		3, 0, 1, 2,
	},
	{
		s16(0b0100010000100010),
		0, 1, 3, 2,
		2, 3, 0, 1,
		1, 0, 2, 3,
		3, 2, 1, 0,
	},
	{
		s16(0b0001011001101000),
		0, 1, 2, 3,
		2, 3, 0, 1,
		3, 0, 1, 2,
		1, 2, 3, 0,
	},
	{
		s16(0b0101000000001010),
		1, 3, 2, 0,
		0, 2, 1, 3,
		3, 1, 0, 2,
		2, 0, 3, 1,
	},
	{
		s16(0b0010010110100100),
		2, 1, 0, 3,
		0, 3, 2, 1,
		1, 2, 3, 0,
		3, 0, 1, 2,
	},
	{
		s16(0b0100010000100010),
		1, 3, 2, 0,
		0, 2, 1, 3,
		3, 1, 0, 2,
		2, 0, 3, 1,
	},
	{
		s16(0b1010000000000101),
		2, 1, 3, 0,
		0, 3, 2, 1,
		1, 2, 0, 3,
		3, 0, 1, 2,
	},
	{
		s16(0b0001100000011000),
		0, 3, 2, 1,
		2, 1, 0, 3,
		1, 0, 3, 2,
		3, 2, 1, 0,
	},
	{
		s16(0b1100000000000011),
		3, 0, 2, 1,
		1, 2, 0, 3,
		2, 3, 1, 0,
		0, 1, 3, 2,
	},
	{
		s16(0b0100000110000010),
		0, 2, 1, 3,
		3, 1, 2, 0,
		2, 3, 0, 1,
		1, 0, 3, 2,
	},
	{
		s16(0b1001000110001001),
		1, 2, 3, 0,
		3, 0, 1, 2,
		2, 1, 0, 3,
		0, 3, 2, 1,
	},
	{
		s16(0b1010000000000101),
		2, 0, 1, 3,
		1, 3, 0, 2,
		0, 2, 3, 1,
		3, 1, 2, 0,
	},
	{
		s16(0b0100001001000010),
		1, 0, 2, 3,
		3, 2, 1, 0,
		2, 3, 0, 1,
		0, 1, 3, 2,
	},
	{
		s16(0b0001011001101000),
		2, 3, 1, 0,
		1, 0, 2, 3,
		0, 2, 3, 1,
		3, 1, 0, 2,
	},
	{
		s16(0b1000100000010001),
		3, 0, 1, 2,
		2, 1, 3, 0,
		1, 2, 0, 3,
		0, 3, 2, 1,
	},
	{
		s16(0b0010100000010100),
		3, 0, 2, 1,
		1, 2, 3, 0,
		2, 1, 0, 3,
		0, 3, 1, 2,
	},
	{
		s16(0b0000011111100000),
		0, 2, 1, 3,
		1, 3, 0, 2,
		2, 0, 3, 1,
		3, 1, 2, 0,
	},
	{
		s16(0b0100001001000010),
		0, 3, 2, 1,
		1, 2, 0, 3,
		2, 1, 3, 0,
		3, 0, 1, 2,
	},
	{
		s16(0b0011000110001100),
		2, 1, 3, 0,
		0, 3, 1, 2,
		3, 0, 2, 1,
		1, 2, 0, 3,
	},
	{
		s16(0b0000101001010000),
		0, 1, 2, 3,
		3, 2, 1, 0,
		1, 3, 0, 2,
		2, 0, 3, 1,
	},
	{
		s16(0b0100001001000010),
		1, 0, 3, 2,
		3, 2, 1, 0,
		2, 3, 0, 1,
		0, 1, 2, 3,
	},
};


}
