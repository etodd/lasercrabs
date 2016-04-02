#pragma once

// Stolen from https://github.com/gpakosz/Assert
#if defined(_WIN32)
#  define vi_debug_break() __debugbreak()
#else
#  if defined(__APPLE__)
#  include <TargetConditionals.h>
#  endif
#  if defined(__clang__) && !TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#    define vi_debug_break() __builtin_debugtrap()
#  elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
#    include <signal.h>
#    define vi_debug_break() raise(SIGTRAP)
#  elif defined(__GNUC__)
#    define vi_debug_break() __builtin_trap()
#  else
#    define vi_debug_break() ((void)0)
#  endif
#endif

#ifdef DEBUG
inline void vi_assert(bool x)
{
	if (!x) { vi_debug_break(); }
}
#else
#define vi_assert(x) ((void)0)
#endif
