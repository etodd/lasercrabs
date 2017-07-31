/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

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
