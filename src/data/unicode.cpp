#include "unicode.h"
#include <cstring>
#include "lmath.h"

namespace VI
{


namespace Unicode
{


s32 codepoint(const char* s)
{
	if (0xf0 == (0xf8 & s[0]))
		return ((0x07 & s[0]) << 18)
			| ((0x3f & s[1]) << 12)
			| ((0x3f & s[2]) << 6)
			| (0x3f & s[3]); // 4 byte utf8 codepoint
	else if (0xe0 == (0xf0 & s[0]))
		return ((0x0f & s[0]) << 12)
			| ((0x3f & s[1]) << 6)
			| (0x3f & s[2]); // 3 byte utf8 codepoint
	else if (0xc0 == (0xe0 & s[0]))
		return ((0x1f & s[0]) << 6) | (0x3f & s[1]); // 2 byte utf8 codepoint
	else
		return 0xff & s[0]; // 1 byte utf8 codepoint otherwise
}

const char* codepoint_next(const char* s)
{
	if (0xf0 == (0xf8 & s[0]))
		s += 4;
	else if (0xe0 == (0xf0 & s[0]))
		s += 3;
	else if (0xc0 == (0xe0 & s[0]))
		s += 2;
	else
		s += 1;

	return s;
}

s32 codepoint_count(const char* s)
{
	s32 count = 0;
	while (*s)
	{
		count++;
		s = codepoint_next(s);
	}
	return count;
}

void truncate(char* str, s32 length, const char* ellipsis, EllipsisMode mode)
{
	s32 length_str = s32(strlen(str));
	if (length_str > length || mode == EllipsisMode::Always)
	{
		// find last utf-8 codepoint before length
		if (length_str > length)
		{
			const char* cutoff = str;
			while (true)
			{
				const char* next = codepoint_next(cutoff);
				if (next - str > length)
				{
					// str is longer than length; cut at last codepoint
					length = s32(cutoff - str);
					break;
				}
				else
					cutoff = next;
			}
		}

		if (ellipsis)
		{
			s32 length_ellipsis = s32(strlen(ellipsis));
			strcpy(&str[vi_min(length_str, length - length_ellipsis)], ellipsis);
		}
		str[length] = '\0';
	}
}


}


}
