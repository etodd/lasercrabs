#pragma once
#include "types.h"

namespace VI
{


namespace Unicode
{
	enum class EllipsisMode : s8
	{
		IfNecessary,
		Always,
		count,
	};

	s32 codepoint(const char*);
	const char* codepoint_next(const char*);
	s32 codepoint_count(const char*);
	void truncate(char*, s32, const char* = nullptr, EllipsisMode = EllipsisMode::IfNecessary);
}


}