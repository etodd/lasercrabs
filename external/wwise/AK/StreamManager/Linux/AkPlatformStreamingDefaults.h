//////////////////////////////////////////////////////////////////////
//
// AkPlatformStreamingDefaults.h
//
// Platform-specific default values for streaming and I/O device settings.
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

// I/O pool settings.

#define AK_DEFAULT_BLOCK_ALLOCATION_TYPE	(AkMalloc)		// Block allocation type. See note below about alignment.

// Note that the I/O pool is a FixedSizedBlock-style pool and it has no lock: all allocations have 
// the same size, which makes it very efficient. System memory alignment depends on the allocation
// hook. Otherwise, all allocations are aligned to multiples of the granularity. For optimal performance,
// AkMallocHook on the MAC should return 4-bytes aligned memory, and streaming granularity 
// should also be a multiple of 4.

#define AK_REQUIRED_IO_POOL_ALIGNMENT		(4)			// 4 bytes (see note above). User granularity is checked against this number.

