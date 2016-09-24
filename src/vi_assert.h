#pragma once

// Stolen from https://github.com/gpakosz/Assert
#if defined(_WIN32)
#  define vi_debug_break() __debugbreak()
#elif defined(__ORBIS__)
#  define vi_debug_break() __builtin_trap()
#elif defined(__clang__)
#  define vi_debug_break() __builtin_debugtrap()
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
#  include <signal.h>
#  define vi_debug_break() raise(SIGTRAP)
#elif defined(__GNUC__)
#  define vi_debug_break() __builtin_trap()
#else
#  define vi_debug_break() ((void)0)
#endif

#if DEBUG

#define vi_debug(fmt, ...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, __VA_ARGS__)

inline void vi_assert(bool x)
{
	if (!x) { vi_debug_break(); }
}

#else

#define vi_debug(fmt, ...) ((void)0)

#define vi_assert(x) ((void)0)

#endif