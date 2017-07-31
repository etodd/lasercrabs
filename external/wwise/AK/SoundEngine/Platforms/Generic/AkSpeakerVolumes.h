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

// AkSpeakerVolumes.h

/// \file 
/// AK::SpeakerVolumes extension - Generic implementation

#ifndef _AKSPEAKERVOLUMES_GENERIC_H_
#define _AKSPEAKERVOLUMES_GENERIC_H_

#include <AK/SoundEngine/Common/AkTypes.h>

#if defined( AK_CPU_ARM_NEON ) || ( ( defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 ) ) && !defined(AK_IOS) )

	#define AKSIMD_SPEAKER_VOLUME

#endif

#ifdef AKSIMD_SPEAKER_VOLUME
	


// Extend AK::SpeakerVolumes. AKSIMD implementation
namespace AK
{
namespace SpeakerVolumes
{
	namespace Vector
	{
#ifdef AKSIMD_V4F32_SUPPORTED
#define SIZEOF_AKSIMD_V4F32		16
#define SIZEOF_AKSIMD_F32		4	
		/// Compute size (in number of v4 elements) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumV4F32( AkUInt32 in_uNumChannels )
		{
			return (in_uNumChannels + SIZEOF_AKSIMD_F32 - 1) >> 2;
		}

		/// Compute size (in number of elements/floats) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV4F32( in_uNumChannels ) * 4;
		}

		/// Compute size (in bytes) required for given number of channels in vector.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV4F32( in_uNumChannels ) * SIZEOF_AKSIMD_V4F32;
		}

#elif defined (AKSIMD_V2F32_SUPPORTED)
#define SIZEOF_AKSIMD_V2F32		8	

		/// Compute size (in number of paired-single elements) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumV2F32( AkUInt32 in_uNumChannels )
		{
			return ( in_uNumChannels + ( SIZEOF_AKSIMD_V2F32 / 2 ) - 1 ) >> 1;
		}

		/// Compute size (in number of elements/floats) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV2F32( in_uNumChannels ) * 2;
		}

		/// Compute size (in bytes) required for given number of channels in vector.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV2F32( in_uNumChannels ) * SIZEOF_AKSIMD_V2F32;
		}

#else
#error Should use scalar implementation.
#endif
	}
}
}

#else

// Extend AK::SpeakerVolumes. Scalar implementation.
namespace AK
{
namespace SpeakerVolumes
{
	namespace Vector
	{
		/// Compute size (in bytes) required for given channel configuration.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannels ) 
		{
			return in_uNumChannels * sizeof( AkReal32 );
		}

		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannels ) 
		{
			return in_uNumChannels;
		}
	}
}
}

#endif // AKSIMD_SPEAKER_VOLUME

#endif //_AKSPEAKERVOLUMES_GENERIC_H_



