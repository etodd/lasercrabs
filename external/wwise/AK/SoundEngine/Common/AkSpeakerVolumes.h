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
/// Multi-channel volume definitions and services.
/// Always associated with an AkChannelConfig. In the case of standard configurations, the volume items ordering
/// match the bit ordering in the channel mask, except for the LFE which is skipped and placed at the end of the
/// volume array.
/// Refer to \ref goingfurther_speakermatrixcallback for an example of how to manipulate speaker volume vectors/matrices.

#ifndef _AK_SPEAKER_VOLUMES_H_
#define _AK_SPEAKER_VOLUMES_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Platforms/Generic/AkSpeakerVolumes.h>

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

		/// Compute the sum of all components of a volume vector.
		AkForceInline AkReal32 L1Norm(ConstVectorPtr io_pVolumes, AkUInt32 in_uNumChannels)
		{
			AkReal32 total = 0;
			AKASSERT((io_pVolumes) || in_uNumChannels == 0);
			for (AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++)
			{
				total += io_pVolumes[uChan];
			}

			return total;
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
		AkForceInline void Add(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements(in_uNumChannelsIn, in_uNumChannelsOut);
			AKASSERT((in_pVolumesDst && in_pVolumesSrc) || uNumElements == 0);
			for (AkUInt32 uChan = 0; uChan < uNumElements; uChan++)
			{
				in_pVolumesDst[uChan] += in_pVolumesSrc[uChan];
			}
		}
		
		/// Get absolute max for all elements of two volume matrices, independently.
		AkForceInline void AbsMax(MatrixPtr in_pVolumesDst, ConstMatrixPtr in_pVolumesSrc, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut)
		{
			AkUInt32 uNumElements = Matrix::GetNumElements( in_uNumChannelsIn, in_uNumChannelsOut );
			AKASSERT( ( in_pVolumesDst && in_pVolumesSrc ) || uNumElements == 0 );
			for ( AkUInt32 uChan = 0; uChan < uNumElements; uChan++ )
			{
				in_pVolumesDst[uChan] = ((in_pVolumesDst[uChan] * in_pVolumesDst[uChan]) > (in_pVolumesSrc[uChan] * in_pVolumesSrc[uChan])) ? in_pVolumesDst[uChan] : in_pVolumesSrc[uChan];
			}
		}
	}
}
}

#endif  //_AK_SPEAKER_VOLUMES_H_
