//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSpeakerVolumes.h

/// \file 
/// AK::SpeakerVolumes extension - Generic implementation

#ifndef _AKSPEAKERVOLUMES_GENERIC_H_
#define _AKSPEAKERVOLUMES_GENERIC_H_

#include <AK/SoundEngine/Common/AkTypes.h>

#if defined( AK_CPU_ARM_NEON ) || ( ( defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 ) ) && !defined(AK_IOS) && !defined(AK_ANDROID)) || defined( AK_WIIU )

	#define AKSIMD_SPEAKER_VOLUME

#endif

#ifdef AKSIMD_SPEAKER_VOLUME

#include <AK/SoundEngine/Common/AkSimd.h>
	
// Extend AK::SpeakerVolumes. AKSIMD implementation
namespace AK
{
namespace SpeakerVolumes
{
	namespace Vector
	{
#ifdef AKSIMD_V4F32_SUPPORTED

		/// Compute size (in number of v4 elements) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumV4F32( AkUInt32 in_uNumChannels )
		{
			return ( in_uNumChannels + ( sizeof( AKSIMD_V4F32 ) / 4 ) - 1 ) >> 2;
		}

		/// Compute size (in number of elements/floats) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV4F32( in_uNumChannels ) * 4;
		}

		/// Compute size (in bytes) required for given number of channels in vector.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV4F32( in_uNumChannels ) * sizeof( AKSIMD_V4F32 );
		}

#elif defined (AKSIMD_V2F32_SUPPORTED)

		/// Compute size (in number of paired-single elements) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumV2F32( AkUInt32 in_uNumChannels )
		{
			return ( in_uNumChannels + ( sizeof( AKSIMD_V2F32 ) / 2 ) - 1 ) >> 1;
		}

		/// Compute size (in number of elements/floats) required for given number of channels in vector.
		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV2F32( in_uNumChannels ) * 2;
		}

		/// Compute size (in bytes) required for given number of channels in vector.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannels ) 
		{
			return GetNumV2F32( in_uNumChannels ) * sizeof( AKSIMD_V2F32 );
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



