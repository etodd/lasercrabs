//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Runtime processor supported features detection interface.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{   
	/// SIMD instruction sets.
	enum AkSIMDProcessorSupport
	{
		AK_SIMD_SSE = 1<<0,		///< SSE support.	
		AK_SIMD_SSE2 = 1<<1,	///< SSE2 support.
		AK_SIMD_SSE3 = 1<<2,	///< SSE3 support.
		AK_SIMD_SSSE3 = 1<<3	///< SSSE3 support.
	};

	/// Runtime processor supported features detection interface. Allows to query specific processor features
	/// to chose optimal implementation.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	class IAkProcessorFeatures
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkProcessorFeatures(){}

	public:
		/// Query for specific SIMD instruction set support. See AkSIMDProcessorSupport for options.
		virtual bool GetSIMDSupport(AkSIMDProcessorSupport in_eSIMD) = 0;
		/// Query L2 cache size to optimize prefetching techniques where necessary.
		virtual AkUInt32 GetCacheSize() = 0;
		/// Query cache line size to optimize prefetching techniques where necessary.
		virtual AkUInt32 GetCacheLineSize() = 0;
	};
}
