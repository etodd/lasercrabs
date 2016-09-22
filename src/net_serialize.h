#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"

namespace VI
{


namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

#define MAX_PACKET_SIZE 1500
#define NET_PROTOCOL_ID 0x6906c2fe

u32 crc32(const u8*, memory_index, u32 value = 0);

template <u32 x> struct PopCount
{
	enum
	{
		a = x - ((x >> 1)       & 0x55555555),
		b =   (((a >> 2)       & 0x33333333) + (a & 0x33333333)),
		c =   (((b >> 4) + b) & 0x0f0f0f0f),
		d =   c + (c >> 8),
		e =   d + (d >> 16),
		result = e & 0x0000003f 
	};
};

template <u32 x> struct Log2
{
	enum
	{
		a = x | (x >> 1),
		b = a | (a >> 2),
		c = b | (b >> 4),
		d = c | (c >> 8),
		e = d | (d >> 16),
		f = e >> 1,
		result = PopCount<f>::result
	};
};

template <s64 min, s64 max> struct BitsRequired
{
	static const u32 result = (min == max) ? 0 : (Log2<u32(max - min)>::result + 1);
};

struct StreamWrite
{
	enum { IsWriting = 1 };
	enum { IsReading = 0 };

	u64 scratch;
	Array<u32> data;
	s32 scratch_bits; // number of bits we've used in the last u32
	StreamWrite();

	b8 would_overflow(s32) const;
	void bits(u32, s32);
	void bytes(const u8*, s32);
	s32 bits_written() const;
	s32 align_bits() const;
	b8 align();
	void flush();
	void finalize();
	void reset();
};

struct StreamRead
{
	enum { IsWriting = 0 };
	enum { IsReading = 1 };

	u64 scratch;
	s32 scratch_bits;
	StaticArray<u32, MAX_PACKET_SIZE / sizeof(u32)> data;
	s32 bits_read;
	StreamRead();

	b8 read_checksum();

	b8 would_overflow(s32) const;
	void bits(u32&, s32);
	void bytes(u8*, s32);
	s32 align_bits() const;
	b8 align();
	void reset();
};

union Double
{
	r64 value_r64;
	u64 value_u64;
};

union Single
{
	r32 value_r32;
	u32 value_u32;
};

#define BITS_REQUIRED(min, max) Net::BitsRequired<min, max>::result

#define serialize_int(stream, type, value, _min, _max)\
{\
	vi_assert(_min < _max);\
	u32 _b = BITS_REQUIRED(_min, _max);\
	u32 _u;\
	if (Stream::IsWriting)\
	{\
		vi_assert(s64(value) >= s64(_min));\
		vi_assert(s64(value) <= s64(_max));\
		_u = u32(s32(value) - _min);\
	} else if (stream->would_overflow(_b))\
		return false;\
	stream->bits(_u, _b);\
	if (Stream::IsReading)\
	{\
		value = type(s32(_min) + s32(_u));\
		if (s64(value) < s64(_min) || s64(value) > s64(_max))\
			return false;\
	}\
}

#define serialize_bits(stream, value, count)\
{\
	vi_assert(count > 0);\
	vi_assert(count <= 32);\
	if (!Stream::IsWriting && stream->would_overflow(count))\
		return false;\
	stream->bits(value, count);\
}

#define serialize_enum(stream, type, value) serialize_int(stream, type, value, 0, s32(type::count) - 1)

#define serialize_bool(stream, value)\
{\
	u32 _u = 0;\
	if (Stream::IsWriting)\
		_u = value ? 1 : 0;\
	serialize_bits(stream, _u, 1);\
	if (Stream::IsReading)\
		value = _u ? true : false;\
}

#define serialize_u64(stream, value)\
{\
	u32 _hi, _lo;\
	if (Stream::IsWriting)\
	{\
		_lo = value & 0xFFFFFFFF;\
		_hi = value >> 32;\
	}\
	serialize_bits(stream, _lo, 32);\
	serialize_bits(stream, _hi, 32);\
	if (Stream::IsReading)\
		value = (u64(_hi) << 32) | _lo;\
}

#define serialize_r32(stream, value)\
{\
	Net::Single _s;\
	if (Stream::IsWriting)\
		_s.value_r32 = value;\
	else if (stream->would_overflow(32))\
		return false;\
	stream->bits(_s.value_u32, 32);\
	if (Stream::IsReading)\
		value = _s.value_r32;\
}

#define serialize_r64(stream, value)\
{\
	Net::Double _d;\
	if (Stream::IsWriting)\
		_d.value_r64 = value;\
	serialize_u64(stream, _d.value_u64);\
	if (Stream::IsReading)\
		value = _d.value_r64;\
}

#define serialize_bytes(stream, data, len)\
{\
	if (!stream->align())\
		return false;\
	if (Stream::IsReading && stream->would_overflow(len * 8))\
		return false;\
	stream->bytes(data, len);\
}

#define serialize_ref(stream, value)\
{\
	u16 _i;\
	u32 _r;\
	if (Stream::IsWriting)\
	{\
		_i = value.id;\
		_r = value.revision;\
	}\
	serialize_int(stream, u16, _i, 0, MAX_ENTITIES);\
	serialize_bits(stream, _r, 16);\
	if (Stream::IsReading)\
	{\
		value.id = _i;\
		value.revision = u16(_r);\
	}\
}


}


}