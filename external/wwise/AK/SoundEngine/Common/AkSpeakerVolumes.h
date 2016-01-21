//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSpeakerVolumes.h

/// \file 
/// Multi-channel volume definitions and services.
/// Always associated with an AkChannelConfig. In the case of standard configurations, the volume items ordering
/// match the bit ordering in the channel mask, except for the LFE which is skipped and placed at the end of the
/// volume array.
/// Refer to \ref goingfurther_speakermatrixcallback for an example of how to manipulate speaker volume vectors/matrices.

#ifndef _AK_SPEAKER_VOLUMES_H_
#define _AK_SPEAKER_VOLUMES_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

// Multi-channel volumes/mix.
// ------------------------------------------------

// Platform-specific section.
//----------------------------------------------------------------------------------------------------

#if defined( AK_XBOX360 )

	#include <AK/SoundEngine/Platforms/XBox360/AkSpeakerVolumes.h>

#elif defined (AK_PS3)

	#include <AK/SoundEngine/Platforms/PS3/AkSpeakerVolumes.h>
	
#else

	#include <AK/SoundEngine/Platforms/Generic/AkSpeakerVolumes.h>

#endif

// Cross-platform section.
//----------------------------------------------------------------------------------------------------

namespace AK
{
/// Multi-channel volume definitions and services.
namespace SpeakerVolumes
{
	typedef AkReal32 * VectorPtr;				///< Volume vector. Access each element with the standard bracket [] operator.
	typedef AkReal32 * MatrixPtr;				///< Volume matrix. Access each input channel vector with AK::SpeakerVolumes::Matrix::GetChannel().
	typedef const AkReal32 * ConstVectorPtr;	///< Constant volume vector. Access each element with the standard bracket [] operator.
	typedef const AkReal32 * ConstMatrixPtr;	///< Constant volume matrix. Access each input channel vector with AK::SpeakerVolumes::Matrix::GetChannel().

	/// Volume vector services.
	namespace Vector
	{
		/// Copy volumes.
		AkForceInline void Copy( VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			if ( in_uNumChannels )
				memcpy( in_pVolumesDst, in_pVolumesSrc, in_uNumChannels * sizeof( AkReal32 ) );
		}

		/// Copy volumes with gain.
		AkForceInline void Copy( VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels, AkReal32 in_fGain )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] = in_pVolumesSrc[uChan] * in_fGain;
			}
		}

		/// Clear volumes.
		AkForceInline void Zero( VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			if ( in_uNumChannels )
				memset( in_pVolumes, 0, in_uNumChannels * sizeof( AkReal32 ) );
		}

		/// Accumulate two volume vectors.
		AkForceInline void Add( VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan];
			}
		}

		/// Multiply volume vector with a scalar.
		AkForceInline void Mul( VectorPtr in_pVolumesDst, const AkReal32 in_fVol, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumesDst || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] *= in_fVol;
			}
		}

		/// Multiply two volume vectors.
		AkForceInline void Mul( VectorPtr in_pVolumesDst, ConstVectorPtr in_pVolumesSrc, AkUInt32 in_uNumChannels )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] *= in_pVolumesSrc[uChan];
			}
		}

		/// Get max for all elements of two volume vectors, independently.
		AkForceInline void Max( AkReal32 * in_pVolumesDst, const AkReal32 * in_pVolumesSrc, AkUInt32 in_uNumChannels )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] = AkMax( in_pVolumesDst[uChan], in_pVolumesSrc[uChan] );
			}
		}
		
		/// Get min for all elements of two volume vectors, independently.
		AkForceInline void Min( AkReal32 * in_pVolumesDst, const AkReal32 * in_pVolumesSrc, AkUInt32 in_uNumChannels )
		{
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumesDst[uChan] = AkMin( in_pVolumesDst[uChan], in_pVolumesSrc[uChan] );
			}
		}
	}

	/// Volume matrix (multi-in/multi-out channel configurations) services.
	namespace Matrix
	{
		/// Compute size (in bytes) required for given channel configurations.
		AkForceInline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut ) 
		{
			return in_uNumChannelsIn * Vector::GetRequiredSize( in_uNumChannelsOut );
		}

		/// Compute size (in number of elements) required for given channel configurations.
		AkForceInline AkUInt32 GetNumElements( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut ) 
		{
			return in_uNumChannelsIn * Vector::GetNumElements( in_uNumChannelsOut );
		}
		
		/// Get pointer to volume distribution for input channel in_uIdxChannelIn.
		AkForceInline VectorPtr GetChannel( MatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut ) 
		{
			AKASSERT( in_pVolumeMx );
			return in_pVolumeMx + in_uIdxChannelIn * Vector::GetNumElements( in_uNumChannelsOut );
		}

		/// Get pointer to volume distribution for input channel in_uIdxChannelIn.
		AkForceInline ConstVectorPtr GetChannel( ConstMatrixPtr in_pVolumeMx, AkUInt32 in_uIdxChannelIn, AkUInt32 in_uNumChannelsOut ) 
		{
			AKASSERT( in_pVolumeMx );
			return in_pVolumeMx + in_uIdxChannelIn * Vector::GetNumElements( in_uNumChannelsOut );
		}

		/// Copy matrix.
		AkForceInline void Copy( MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || uNumElements == 0 );
			if ( uNumElements )
				memcpy( in_pVolumesDst, in_pVolumesSrc, uNumElements * sizeof( AkReal32 ) );
		}

		/// Copy matrix with gain.
		AkForceInline void Copy( MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkReal32 in_fGain )
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || uNumElements == 0 );
			for ( AkUInt32 uChan = 0; uChan < uNumElements; uChan++ )
			{
				in_pVolumesDst[uChan] = in_pVolumesSrc[uChan] * in_fGain;
			}
		}

		/// Clear matrix.
		AkForceInline void Zero( MatrixPtr in_pVolumes, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( in_pVolumes || uNumElements == 0 );
			if ( uNumElements )
				memset( in_pVolumes, 0, uNumElements * sizeof( AkReal32 ) );
		}

		/// Multiply a matrix with a scalar.
		AkForceInline void Mul( MatrixPtr in_pVolumesDst, const AkReal32 in_fVol, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( in_pVolumesDst || uNumElements == 0 );
			for ( AkUInt32 uChan = 0; uChan < uNumElements; uChan++ )
			{
				in_pVolumesDst[uChan] *= in_fVol;
			}
		}

		/// Add all elements of two volume matrices, independently.
		AkForceInline void Add(AkReal32 * in_pVolumesDst, const AkReal32 * in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			AKASSERT((in_pVolumesDst && in_pVolumesSrc) || uNumElements == 0);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan];
			}
		}
		
		/// Get max for all elements of two volume matrices, independently.
		AkForceInline void Max( AkReal32 * in_pVolumesDst, const AkReal32 * in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || uNumElements == 0 );
			for ( AkUInt32 uChan = 0; uChan < uNumElements; uChan++ )
			{
				in_pVolumesDst[uChan] = AkMax( in_pVolumesDst[uChan], in_pVolumesSrc[uChan] );
			}
		}
	}
}
}

#endif  //_AK_SPEAKER_VOLUMES_H_
