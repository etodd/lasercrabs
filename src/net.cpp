#include "net.h"

namespace VI
{


namespace Net
{

// borrows heavily from https://github.com/networkprotocol/libyojimbo


StreamWrite::StreamWrite()
	: data(),
	scratch(),
	scratch_bits()
{
}

void StreamWrite::bits(u32 value, s32 bits)
{
	vi_assert(bits > 0);
	vi_assert(bits <= 32);
	vi_assert((value & ((u64(1) << bits) - 1)) == value);

	scratch |= u64(value) << scratch_bits;

	scratch_bits += bits;

	if (scratch_bits >= 32)
	{
		data.add(u32(scratch & 0xFFFFFFFF));
		scratch >>= 32;
		scratch_bits -= 32;
	}
}

b8 StreamWrite::align()
{
	const int remainder_bits = scratch_bits % 8;
	if (remainder_bits != 0)
	{
		bits(u32(0), 8 - remainder_bits);
		vi_assert((scratch_bits % 8) == 0);
	}
	return true;
}

s32 StreamWrite::bits_written() const
{
	return (data.length * 32) + scratch_bits;
}

s32 StreamWrite::align_bits() const
{
	return (8 - (scratch_bits % 8 )) % 8;
}

void StreamWrite::bytes(const u8* buffer, s32 bytes)
{
	vi_assert(align_bits() == 0);
	vi_assert((bits_written() % 32) == 0 || (bits_written() % 32) == 8 || (bits_written() % 32) == 16 || (bits_written() % 32 ) == 24);

	s32 head_bytes = (4 - (bits_written() % 32 ) / 8) % 4;
	if (head_bytes > bytes)
		head_bytes = bytes;
	for (s32 i = 0; i < head_bytes; i++)
		bits(buffer[i], 8);
	if (head_bytes == bytes)
		return;

	flush();

	vi_assert(align_bits() == 0);

	s32 num_words = (bytes - head_bytes) / sizeof(u32);
	if (num_words > 0)
	{
		vi_assert((bits_written() % 32) == 0);
		s32 write_pos = data.length;
		data.resize(data.length + num_words);
		memcpy(&data[write_pos], &buffer[head_bytes], num_words * sizeof(u32));
		scratch = 0;
	}

	vi_assert(align_bits() == 0);

	s32 tail_start = head_bytes + num_words * sizeof(u32);
	s32 tail_bytes = bytes - tail_start;
	vi_assert(tail_bytes >= 0 && tail_bytes < sizeof(u32));
	for (s32 i = 0; i < tail_bytes; i++)
		bits(buffer[tail_start + i], 8);

	vi_assert(align_bits() == 0);

	vi_assert(head_bytes + num_words * sizeof(u32) + tail_bytes == bytes);
}

void StreamWrite::flush()
{
	if (scratch_bits != 0)
	{
		data.add(u32(scratch & 0xFFFFFFFF));
		scratch >>= 32;
		scratch_bits -= 32;
	}
}

b8 StreamWrite::would_overflow(s32 bits) const
{
	return false;
}

StreamRead::StreamRead()
	: scratch(),
	scratch_bits(),
	data(),
	bits_read()
{
}

b8 StreamRead::would_overflow(s32 bits) const
{
	return bits_read + bits > data.length * 32;
}

void StreamRead::bits(u32& output, s32 bits)
{
	vi_assert(bits > 0);
	vi_assert(bits <= 32);
	vi_assert(bits_read + bits <= data.length * 32);

	if (scratch_bits < bits)
	{
		scratch |= u64(data[(bits_read + scratch_bits) / 32]) << scratch_bits;
		scratch_bits += 32;
	}

	bits_read += bits;

	vi_assert(scratch_bits >= bits);

	output = scratch & ((u64(1) << bits) - 1);

	scratch >>= bits;
	scratch_bits -= bits;
}

void StreamRead::bytes(u8* buffer, s32 bytes)
{
	vi_assert(align_bits() == 0);
	vi_assert(bits_read + bytes * 8 <= data.length * 32);
	vi_assert((bits_read % 32) == 0 || (bits_read % 32) == 8 || (bits_read % 32) == 16 || (bits_read % 32) == 24);

	s32 head_bytes = (4 - (bits_read % 32) / 8) % 4;
	if (head_bytes > bytes)
		head_bytes = bytes;
	for (s32 i = 0; i < head_bytes; i++)
	{
		u32 value;
		bits(value, 8);
		buffer[i] = (u8)value;
	}
	if (head_bytes == bytes)
		return;

	vi_assert(align_bits() == 0);

	s32 num_words = (bytes - head_bytes) / 4;
	if (num_words > 0)
	{
		vi_assert((bits_read % 32) == 0);
		memcpy(&buffer[head_bytes], &data[bits_read / 32], num_words * sizeof(u32));
		bits_read += num_words * 32;
	}

	vi_assert(align_bits() == 0);

	s32 tail_start = head_bytes + num_words * sizeof(u32);
	s32 tail_bytes = bytes - tail_start;
	vi_assert(tail_bytes >= 0 && tail_bytes < sizeof(u32));
	for (s32 i = 0; i < tail_bytes; i++)
	{
		u32 value;
		bits(value, 8);
		buffer[tail_start + i] = (u8)value;
	}

	vi_assert(align_bits() == 0);

	vi_assert(head_bytes + num_words * sizeof(u32) + tail_bytes == bytes);
}

s32 StreamRead::align_bits() const
{
	return (8 - bits_read % 8) % 8;
}

b8 StreamRead::align()
{
	const s32 remainder_bits = bits_read % 8;
	if (remainder_bits != 0)
	{
		u32 value;
		bits(value, 8 - remainder_bits);
		vi_assert(bits_read % 8 == 0);
		if (value != 0)
			return false;
	}
	return true;
}


}


}