/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: v2016.2.4  Build: 6097
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkPlatformStreamingDefaults.h
//
// Platform-specific default values for streaming and I/O device settings.
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

