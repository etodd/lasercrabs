#pragma once

#include <math.h>

namespace VI
{

// Easing functions based on http://www.gizma.com/easing/
namespace Ease
{
	template<typename T> T linear(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * x + start;
	}
			
	// quadratic easing in - accelerating from zero velocity
	template<typename T> T quad_in(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * x * x + start;
	}
			
	// quadratic easing out - decelerating to zero velocity
	template<typename T> T quad_out(float x, T start = 0.0f, T end = 1.0f)
	{
		return (start - end) * x * (x - 2.0f) + start;
	};

	// quadratic easing in/out - acceleration until halfway, then deceleration
	template<typename T> T quad_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return c / 2.0f * x * x + start;
		x--;
		return -c / 2.0f * (x * (x - 2.0f) - 1.0f) + start;
	}

	// cubic easing in - accelerating from zero velocity
	template<typename T> T cubic_in(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		return c * x * x * x + start;
	}

	// cubic easing out - decelerating to zero velocity
	template<typename T> T cubic_out(float x, T start = 0.0f, T end = 1.0f)
	{
		x--;
		return (end - start) * (x * x * x + 1.0f) + start;
	}

	// cubic easing in/out - acceleration until halfway, then deceleration
	template<typename T> T cubic_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return c / 2.0f * x * x * x + start;
		x -= 2.0f;
		return c / 2.0f * (x * x * x + 2.0f) + start;
	}

	// quartic easing in - accelerating from zero velocity
	template<typename T> T quart_in(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * x * x * x * x + start;
	}

	// quartic easing out - decelerating to zero velocity
	template<typename T> T quart_out(float x, T start = 0.0f, T end = 1.0f)
	{
		x--;
		return (start - end) * (x * x * x * x - 1.0f) + start;
	}

	// quartic easing in/out - acceleration until halfway, then deceleration
	template<typename T> T quart_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return c / 2.0f * x * x * x * x + start;
		x -= 2.0f;
		return -c / 2.0f  *  (x * x * x * x - 2.0f) + start;
	}

	// quintic easing in - accelerating from zero velocity
	template<typename T> T quint_in(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * x * x * x * x * x + start;
	}

	// quintic easing out - decelerating to zero velocity
	template<typename T> T quint_out(float x, T start = 0.0f, T end = 1.0f)
	{
		x--;
		return (end - start)  *  (x * x * x * x * x + 1.0f) + start;
	}

	// quintic easing in/out - acceleration until halfway, then deceleration
	template<typename T> T quint_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return c / 2.0f  * x * x * x * x * x + start;
		x -= 2.0f;
		return c / 2.0f  *  (x * x * x * x * x + 2.0f) + start;
	}

	// sinusoidal easing in - accelerating from zero velocity
	template<typename T> T sin_in(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		return -c * cosf(x * HALF_PI) + c + start;
	}

	// sinusoidal easing out - decelerating to zero velocity
	template<typename T> T sin_out(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * sinf(x * HALF_PI) + start;
	}

	// sinusoidal easing in/out - accelerating until halfway, then decelerating
	template<typename T> T sin_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		return -c / 2.0f * (cosf(x * PI) - 1.0f) + start;
	}

	// exponential easing in - accelerating from zero velocity
	template<typename T> T expo_in(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * powf(2.0f, 10.0f * (x - 1.0f) ) + start;
	}

	// exponential easing out - decelerating to zero velocity
	template<typename T> T expo_out(float x, T start = 0.0f, T end = 1.0f)
	{
		return (end - start) * (powf(2.0f, -10.0f * x) + 1.0f) + start;
	}

	// exponential easing in/out - accelerating until halfway, then decelerating
	template<typename T> T expo_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return c / 2.0f * powf(2.0f, 10.0f * (x - 1.0f)) + start;
		x--;
		return c / 2.0f * (-powf(2.0f, -10.0f * x) + 2.0f) + start;
	}
			
	// circular easing in - accelerating from zero velocity
	template<typename T> T circ_in(float x, T start = 0.0f, T end = 1.0f)
	{
		return (start - end) * (sqrtf(1.0f - x * x) - 1.0f) + start;
	}

	// circular easing out - decelerating to zero velocity
	template<typename T> T circ_out(float x, T start = 0.0f, T end = 1.0f)
	{
		x--;
		return (end - start) * sqrtf(1.0f - x * x) + start;
	}

	// circular easing in/out - acceleration until halfway, then deceleration
	template<typename T> T circ_in_out(float x, T start = 0.0f, T end = 1.0f)
	{
		T c = end - start;
		x *= 2.0f;
		if (x < 1.0f)
			return -c / 2.0f * (sqrtf(1.0f - x * x) - 1.0f) + start;
		x -= 2.0f;
		return c / 2.0f * (sqrtf(1.0f - x * x) + 1.0f) + start;
	}
}

}
