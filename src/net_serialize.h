#pragma once

#include "types.h"
#include "data/array.h"
#include "vi_assert.h"
#include "lmath.h"

namespace VI
{

namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo

#define NET_PROTOCOL_ID 0x6906c2fe

u32 crc32(const u8*, memory_index, u32 value = 0);

template <u32 x> struct PopCount
{
	enum
	{
		a = x - ((x >> 1) & 0x55555555),
		b =   (((a >> 2) & 0x33333333) + (a & 0x33333333)),
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
	StaticArray<u32, NET_MAX_PACKET_SIZE / sizeof(u32)> data;

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
	StaticArray<u32, NET_MAX_PACKET_SIZE / sizeof(u32)> data;
	s32 bits_read;
	s32 bytes_total;

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

typedef u16 SequenceID;

void packet_init(StreamWrite*);
void packet_finalize(StreamWrite*);
void packet_decompress(StreamRead*, s32);

// true if s1 > s2
b8 sequence_more_recent(SequenceID, SequenceID);
b8 sequence_older_than(SequenceID, SequenceID);
s32 sequence_relative_to(SequenceID, SequenceID);
SequenceID sequence_advance(SequenceID, s32);

enum class Resolution : s8
{
	Low,
	Medium,
	High,
	count,
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
#define net_error() do { vi_debug_break(); return false; } while (0)
#else
#define net_error() do { return false; } while (0)
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
do\
{\
	vi_assert(s64(_min) < s64(_max));\
	u32 _b = Net::bits_required(_min, _max);\
	if ((stream)->would_overflow(_b))\
		net_error();\
	u32 _u;\
	if (Stream::IsWriting)\
	{\
		vi_assert(s64(value) >= s64(_min));\
		vi_assert(s64(value) <= s64(_max));\
		_u = u32(s64(value) - s64(_min));\
	}\
	(stream)->bits(_u, _b);\
	if (Stream::IsReading)\
	{\
		type _s = type(s64(_min) + s64(_u));\
		if (s64(_s) < s64(_min) || s64(_s) > s64(_max))\
			net_error();\
		value = _s;\
	}\
} while (0)

#define serialize_align(stream)\
do\
{\
	if (!(stream)->align())\
		net_error();\
} while (0)

#define serialize_bits(stream, type, value, count)\
do\
{\
	vi_assert(count > 0);\
	vi_assert(count <= 32);\
	if ((stream)->would_overflow(count))\
		net_error();\
	u32 _u;\
	if (Stream::IsWriting)\
		_u = u32(value);\
	(stream)->bits(_u, count);\
	if (Stream::IsReading)\
		value = type(_u);\
} while (0)

#define serialize_enum(stream, type, value) serialize_int(stream, type, value, 0, s32(type::count) - 1)
#define serialize_u8(stream, value) serialize_bits(stream, u8, value, 8)
#define serialize_s8(stream, value) serialize_bits(stream, s8, value, 8)
#define serialize_u16(stream, value) serialize_bits(stream, u16, value, 16)
#define serialize_s16(stream, value) serialize_bits(stream, s16, value, 16)
#define serialize_u32(stream, value) serialize_bits(stream, u32, value, 32)
#define serialize_s32(stream, value) serialize_bits(stream, s32, value, 32)

#define serialize_bool(stream, value) serialize_bits(stream, b8, value, 1)

#define serialize_u64(stream, value)\
do\
{\
	u32 _hi, _lo;\
	if (Stream::IsWriting)\
	{\
		_lo = value & 0xFFFFFFFF;\
		_hi = value >> 32;\
	}\
	serialize_bits(stream, u32, _lo, 32);\
	serialize_bits(stream, u32, _hi, 32);\
	if (Stream::IsReading)\
		value = (u64(_hi) << 32) | _lo;\
} while (0)

#define serialize_r32(stream, value)\
do\
{\
	if ((stream)->would_overflow(32))\
		net_error();\
	Net::Single _s;\
	if (Stream::IsWriting)\
		_s.value_r32 = value;\
	(stream)->bits(_s.value_u32, 32);\
	if (Stream::IsReading)\
		value = _s.value_r32;\
} while (0)

#define serialize_r32_range(stream, value, _min, _max, _bits)\
do\
{\
	vi_assert(_min < _max);\
	vi_assert(_bits > 0 && _bits < 32);\
	if ((stream)->would_overflow(_bits))\
		net_error();\
	u32 _u;\
	u32 _umax = (1 << _bits) - 1;\
	r32 _q = r32(_umax) / r32(_max - _min);\
	if (Stream::IsWriting)\
	{\
		r32 _v = value < _min ? _min : value;\
		_u = u32(r32(_v - _min) * _q);\
		_u = _u < _umax ? _u : _umax;\
	}\
	(stream)->bits(_u, _bits);\
	if (Stream::IsReading)\
		value = _min + r32(_u) / _q;\
} while (0)

#define serialize_r64(stream, value)\
do\
{\
	Net::Double _d;\
	if (Stream::IsWriting)\
		_d.value_r64 = value;\
	serialize_u64(stream, _d.value_u64);\
	if (Stream::IsReading)\
		value = _d.value_r64;\
} while (0)

#define serialize_bytes(stream, data, len)\
do\
{\
	if (!(stream)->align())\
		net_error();\
	if ((stream)->would_overflow(len * 8))\
		net_error();\
	(stream)->bytes(data, len);\
} while (0)

#define serialize_ref(stream, value)\
do\
{\
	b8 _b;\
	if (Stream::IsWriting)\
		_b = value.id != IDNull;\
	serialize_bool(stream, _b);\
	if (_b)\
	{\
		serialize_int(stream, ID, value.id, 0, MAX_ENTITIES - 1);\
		serialize_bits(stream, Revision, value.revision, 16);\
	}\
	else if (Stream::IsReading)\
		value.id = IDNull;\
} while (0)

#define serialize_asset(stream, value, _count)\
do\
{\
	b8 _b;\
	if (Stream::IsWriting)\
		_b = value == AssetNull ? false : true;\
	serialize_bool(stream, _b);\
	if (_b)\
		serialize_int(stream, AssetID, value, 0, _count - 1);\
	else if (Stream::IsReading)\
		value = AssetNull;\
} while (0)

template<typename Stream> b8 serialize_position(Stream* p, Vec3* pos, Resolution r)
{
	switch (r)
	{
		case Resolution::Low:
		{
			serialize_r32_range(p, pos->x, -512, 512, 17);
			serialize_r32_range(p, pos->y, -128, 128, 12);
			serialize_r32_range(p, pos->z, -512, 512, 17);
			break;
		}
		case Resolution::Medium:
		{
			serialize_r32_range(p, pos->x, -512, 512, 19);
			serialize_r32_range(p, pos->y, -128, 128, 14);
			serialize_r32_range(p, pos->z, -512, 512, 19);
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

template<typename Stream> b8 serialize_quat(Stream* p, Quat* rot, Resolution r)
{
	Quat q;
	s32 largest_index;
	if (Stream::IsWriting)
	{
		q = Quat::normalize(*rot);
		largest_index = 0; // w
		if (fabsf(q.x) > fabsf(q[largest_index]))
			largest_index = 1;
		if (fabsf(q.y) > fabsf(q[largest_index]))
			largest_index = 2;
		if (fabsf(q.z) > fabsf(q[largest_index]))
			largest_index = 3;
		if (q[largest_index] < 0.0f)
		{
			q.w *= -1.0f;
			q.x *= -1.0f;
			q.y *= -1.0f;
			q.z *= -1.0f;
		}
	}
	serialize_int(p, s32, largest_index, 0, 3);

	s32 indices[3];
	{
		s32 index = 0;
		for (s32 i = 0; i < 4; i++)
		{
			if (i != largest_index)
			{
				indices[index] = i;
				index++;
			}
		}
	}
	s32 bits = r == Resolution::High ? 16 : 9;
	serialize_r32_range(p, q[indices[0]], -0.707107f, 0.707107f, bits);
	serialize_r32_range(p, q[indices[1]], -0.707107f, 0.707107f, bits);
	serialize_r32_range(p, q[indices[2]], -0.707107f, 0.707107f, bits);

	if (Stream::IsReading)
	{
		r32 a = q[indices[0]];
		r32 b = q[indices[1]];
		r32 c = q[indices[2]];
		q[largest_index] = sqrtf(1.0f - (a * a) - (b * b) - (c * c));
		*rot = q;
	}
	return true;
}
		

}


}
