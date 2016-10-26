#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"

namespace VI
{

struct Vec3;

namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

#define MAX_PACKET_SIZE 2000
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
	s32 scratch_bits; // number of bits we've used in the last u32
	StaticArray<u32, MAX_PACKET_SIZE / sizeof(u32)> data;

	StreamWrite();
	b8 would_overflow(s32) const;
	void bits(u32, s32);
	void bytes(const u8*, s32);
	s32 bits_written() const;
	s32 bytes_written() const;
	s32 align_bits() const;
	b8 align();
	void resize_bytes(s32);
	void flush();
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
	s32 bytes_read() const;
	void resize_bytes(s32);
	void reset();
	void rewind(s32 = 0);
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

#if DEBUG
#define net_error() { vi_debug_break(); }
#else
#define net_error() { return false; }
#endif

#define BITS_REQUIRED(min, max) Net::BitsRequired<min, max>::result

inline u32 popcount(u32 x)
{
#ifdef __GNUC__
	return __builtin_popcount( x );
#else // #ifdef __GNUC__
	const u32 a = x - ((x >> 1) & 0x55555555);
	const u32 b = (((a >> 2)       & 0x33333333) + (a & 0x33333333));
	const u32 c = (((b >> 4) + b) & 0x0f0f0f0f);
	const u32 d = c + (c >> 8);
	const u32 e = d + (d >> 16);
	const u32 result = e & 0x0000003f;
	return result;
#endif // #ifdef __GNUC__
}

#ifdef __GNUC__

inline int bits_required(u32 min, u32 max)
{
	return 32 - __builtin_clz(max - min);
}

#else // #ifdef __GNUC__

inline u32 log2(u32 x)
{
	const u32 a = x | (x >> 1);
	const u32 b = a | (a >> 2);
	const u32 c = b | (b >> 4);
	const u32 d = c | (c >> 8);
	const u32 e = d | (d >> 16);
	const u32 f = e >> 1;
	return popcount(f);
}

inline int bits_required(u32 min, u32 max)
{
	return (min == max) ? 0 : log2(max - min) + 1;
}

#endif // #ifdef __GNUC__

#define serialize_int(stream, type, value, _min, _max)\
{\
	vi_assert(s64(_min) < s64(_max));\
	u32 _b = Net::bits_required(_min, _max);\
	u32 _u;\
	if (Stream::IsWriting)\
	{\
		vi_assert(s64(value) >= s64(_min));\
		vi_assert(s64(value) <= s64(_max));\
		_u = u32(s64(value) - s64(_min));\
	} else if ((stream)->would_overflow(_b))\
		net_error();\
	(stream)->bits(_u, _b);\
	if (Stream::IsReading)\
	{\
		type _s = type(s64(_min) + s64(_u));\
		if (s64(_s) < s64(_min) || s64(_s) > s64(_max))\
			net_error();\
		value = _s;\
	}\
}

#define serialize_align(stream)\
{\
	if (!(stream)->align())\
		net_error();\
}

#define serialize_bits(stream, value, count)\
{\
	vi_assert(count > 0);\
	vi_assert(count <= 32);\
	if (!Stream::IsWriting && (stream)->would_overflow(count))\
		net_error();\
	(stream)->bits(value, count);\
}

#define serialize_enum(stream, type, value) serialize_int(stream, type, value, 0, s32(type::count) - 1)
#define serialize_u8(stream, value) serialize_int(stream, u8, value, 0, 255)
#define serialize_s8(stream, value) serialize_int(stream, s8, value, -128, 127)
#define serialize_u16(stream, value) serialize_int(stream, u16, value, 0, 65535)
#define serialize_s16(stream, value) serialize_int(stream, s16, value, -32768, 32767)
#define serialize_u32(stream, value) serialize_bits(stream, value, 32)
#define serialize_s32(stream, value)\
{\
	u32 _u;\
	if (Stream::IsWriting)\
	{\
		if ((stream)->would_overflow(32))\
			net_error();\
		_u = u32(value);\
	}\
	(stream)->bits(_u, 32);\
	if (Stream::IsReading)\
		value = s32(_u);\
}

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
	else if ((stream)->would_overflow(32))\
		net_error();\
	(stream)->bits(_s.value_u32, 32);\
	if (Stream::IsReading)\
		value = _s.value_r32;\
}

#define serialize_r32_range(stream, value, _min, _max, _bits)\
{\
	vi_assert(_min < _max);\
	vi_assert(_bits > 0 && _bits < 32);\
	u32 _u;\
	u32 _umax = (1 << _bits) - 1;\
	r32 _q = r32(_umax) / r32(_max - _min);\
	if (Stream::IsWriting)\
	{\
		r32 _v = value < _min ? _min : value;\
		_u = u32(r32(_v - _min) * _q);\
		_u = _u < _umax ? _u : _umax;\
	} else if ((stream)->would_overflow(_bits))\
		net_error();\
	(stream)->bits(_u, _bits);\
	if (Stream::IsReading)\
		value = _min + r32(_u) / _q;\
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
	if (!(stream)->align())\
		net_error();\
	if (Stream::IsReading && (stream)->would_overflow(len * 8))\
		net_error();\
	(stream)->bytes(data, len);\
}

#define serialize_ref(stream, value)\
{\
	u16 _i;\
	u32 _r;\
	b8 _b;\
	if (Stream::IsWriting)\
	{\
		_i = value.id;\
		_r = value.revision;\
		_b = value.id != IDNull;\
	}\
	serialize_bool(stream, _b);\
	if (_b)\
	{\
		serialize_int(stream, u16, _i, 0, MAX_ENTITIES - 1);\
		serialize_bits(stream, _r, 16);\
	}\
	if (Stream::IsReading)\
	{\
		if (_b)\
		{\
			value.id = _i;\
			value.revision = u16(_r);\
		}\
		else\
			value.id = IDNull;\
	}\
}

#define serialize_asset(stream, value, _count)\
{\
	b8 _b;\
	if (Stream::IsWriting)\
		_b = value == AssetNull ? false : true;\
	serialize_bool(stream, _b);\
	if (_b)\
	{\
		serialize_int(stream, AssetID, value, 0, _count - 1);\
	}\
	else if (Stream::IsReading)\
		value = AssetNull;\
}

enum class Resolution
{
	Low,
	High,
	count,
};

template<typename Stream> b8 serialize_position(Stream* p, Vec3* pos, Resolution r)
{
	switch (r)
	{
		case Resolution::Low:
		{
			serialize_r32_range(p, pos->x, -256, 256, 16);
			serialize_r32_range(p, pos->y, -32, 128, 12);
			serialize_r32_range(p, pos->z, -256, 256, 16);
			break;
		}
		case Resolution::High:
		{
			serialize_r32(p, pos->x);
			serialize_r32(p, pos->y);
			serialize_r32(p, pos->z);
			break;
		}
		default:
		{
			vi_assert(false);
			break;
		}
	}
	return true;
}
		

}


}