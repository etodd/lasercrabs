//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// Accumulate (+=) signal into output buffer
// To be used on mono signals, create as many instances as there are channels if need be

#ifndef _AKAPPLYGAIN_H_
#define _AKAPPLYGAIN_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkSimd.h>

#if defined (AKSIMD_V4F32_SUPPORTED) || defined (AKSIMD_V2F32_SUPPORTED)
#include <AK/Plugin/PluginServices/AkVectorValueRamp.h>
// Otherwise, it is preferrable not to use a generic implementation of a vector type.
#endif

namespace AK
{
	namespace DSP
	{
		/// Single channel, in-place interpolating gain helper (do not call directly) use ApplyGain instead.
		static inline void ApplyGainRamp(	
			AkSampleType * AK_RESTRICT io_pfBuffer, 
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			AkUInt32 in_uNumFrames )
		{
			AkSampleType * AK_RESTRICT pfBuf = (AkSampleType *) io_pfBuffer;
			const AkSampleType * const pfEnd = io_pfBuffer + in_uNumFrames;

#ifdef AKSIMD_V4F32_SUPPORTED
			const AkUInt32 uNumVecIter = in_uNumFrames/4;
			CAkVectorValueRamp vGainRamp;
			AKSIMD_V4F32 vfGain = vGainRamp.Setup(in_fCurGain,in_fTargetGain,uNumVecIter*4);	
			const AkSampleType * const pfVecEnd = io_pfBuffer + uNumVecIter*4;
			while ( pfBuf < pfVecEnd )
			{
				AKSIMD_V4F32 vfIn = AKSIMD_LOAD_V4F32((AKSIMD_F32*)pfBuf);
				AKSIMD_V4F32 vfOut = AKSIMD_MUL_V4F32( vfIn, vfGain );
				AKSIMD_STORE_V4F32( (AKSIMD_F32*)pfBuf, vfOut );
				vfGain = vGainRamp.Tick();
				pfBuf+=4;
			}
#elif defined (AKSIMD_V2F32_SUPPORTED)
			// Unroll 4 times x v2.
			const AkUInt32 uNumVecIter = in_uNumFrames/8;
			CAkVectorValueRampV2 vGainRamp;
			AKSIMD_V2F32 vfGain = vGainRamp.Setup(in_fCurGain,in_fTargetGain,uNumVecIter*8);	
			const AkSampleType * const AK_RESTRICT pfVecEnd = io_pfBuffer + uNumVecIter*8;
			while ( pfBuf < pfVecEnd )
			{
				AKSIMD_V2F32 vfIn1 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 0 );
				AKSIMD_V2F32 vfIn2 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 8 );
				AKSIMD_V2F32 vfIn3 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 16 );
				AKSIMD_V2F32 vfIn4 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 24 );
				AKSIMD_V2F32 vfOut1 = AKSIMD_MUL_V2F32( vfIn1, vfGain );
				vfGain = vGainRamp.Tick();
				AKSIMD_V2F32 vfOut2 = AKSIMD_MUL_V2F32( vfIn2, vfGain );
				vfGain = vGainRamp.Tick();
				AKSIMD_V2F32 vfOut3 = AKSIMD_MUL_V2F32( vfIn3, vfGain );
				vfGain = vGainRamp.Tick();
				AKSIMD_V2F32 vfOut4 = AKSIMD_MUL_V2F32( vfIn4, vfGain );
				vfGain = vGainRamp.Tick();
				AKSIMD_STORE_V2F32_OFFSET( pfBuf, 0, vfOut1 );
				AKSIMD_STORE_V2F32_OFFSET( pfBuf, 8, vfOut2 );
				AKSIMD_STORE_V2F32_OFFSET( pfBuf, 16, vfOut3 );
				AKSIMD_STORE_V2F32_OFFSET( pfBuf, 24, vfOut4 );
				pfBuf+=8;
			}
			/*
			const AkUInt32 uNumVecIter = in_uNumFrames/2;
			CAkVectorValueRampV2 vGainRamp;
			AKSIMD_V2F32 vfGain = vGainRamp.Setup(in_fCurGain,in_fTargetGain,uNumVecIter*2);	
			const AkSampleType * const pfVecEnd = io_pfBuffer + uNumVecIter*2;
			while ( pfBuf < pfVecEnd )
			{
				AKSIMD_V2F32 vfIn = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 0 );
				AKSIMD_V2F32 vfOut = AKSIMD_MUL_V2F32( vfIn, vfGain );
				AKSIMD_STORE_V2F32_OFFSET( pfBuf, 0, vfOut );
				vfGain = vGainRamp.Tick();
				pfBuf+=2;
			}
			*/
#endif
			if ( pfBuf < pfEnd )
			{
				const AkReal32 fInc = (in_fTargetGain - in_fCurGain) / in_uNumFrames;
				while ( pfBuf < pfEnd )
				{
					*pfBuf = (AkSampleType)(*pfBuf * in_fCurGain);
					in_fCurGain += fInc;
					pfBuf++;
				}
			}

		}

		/// Single channel, out-of-place interpolating gain helper (do not call directly) use ApplyGain instead.
		static inline void ApplyGainRamp(	
			AkSampleType * AK_RESTRICT in_pfBufferIn, 
			AkSampleType * AK_RESTRICT out_pfBufferOut, 
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			AkUInt32 in_uNumFrames )
		{
			AkSampleType * AK_RESTRICT pfInBuf = (AkSampleType * ) in_pfBufferIn;
			AkSampleType * AK_RESTRICT pfOutBuf = (AkSampleType * ) out_pfBufferOut;
			const AkSampleType * const pfEnd = pfInBuf + in_uNumFrames;

#ifdef AKSIMD_V4F32_SUPPORTED
			const AkUInt32 uNumVecIter = in_uNumFrames/4;
			CAkVectorValueRamp vGainRamp;
			AKSIMD_V4F32 vfGain = vGainRamp.Setup(in_fCurGain,in_fTargetGain,uNumVecIter*4);	
			const AkSampleType * const pfVecEnd = in_pfBufferIn + uNumVecIter*4;
			while ( pfInBuf < pfVecEnd )
			{
				AKSIMD_V4F32 vfIn = AKSIMD_LOAD_V4F32((AKSIMD_F32*)pfInBuf);
				AKSIMD_V4F32 vfOut = AKSIMD_MUL_V4F32( vfIn, vfGain );
				AKSIMD_STORE_V4F32( (AKSIMD_F32*)pfOutBuf, vfOut );
				vfGain = vGainRamp.Tick();
				pfInBuf+=4;
				pfOutBuf+=4;
			}
#elif defined (AKSIMD_V2F32_SUPPORTED)
			const AkUInt32 uNumVecIter = in_uNumFrames/2;
			CAkVectorValueRampV2 vGainRamp;
			f32x2 vfGain = vGainRamp.Setup(in_fCurGain,in_fTargetGain,uNumVecIter*2);	
			const AkSampleType * const pfVecEnd = in_pfBufferIn + uNumVecIter*2;
			while ( pfInBuf < pfVecEnd )
			{
				AKSIMD_V2F32 vfIn = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 0 );
				AKSIMD_V2F32 vfOut = AKSIMD_MUL_V2F32( vfIn, vfGain );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 0, vfOut );
				vfGain = vGainRamp.Tick();
				pfInBuf+=2;
				pfOutBuf+=2;
			}
#endif
			if ( pfInBuf < pfEnd )
			{
				const AkReal32 fInc = (in_fTargetGain - in_fCurGain) / in_uNumFrames;
				while ( pfInBuf < pfEnd )
				{
					*pfOutBuf++ = (AkSampleType)(*pfInBuf++ * in_fCurGain);
					in_fCurGain += fInc;
				}
			}
		}	

		/// Single channel, in-place static gain.
		static inline void ApplyGain(	
			AkSampleType * AK_RESTRICT io_pfBuffer, 
			AkReal32 in_fGain,
			AkUInt32 in_uNumFrames )
		{
			if ( in_fGain != 1.f )
			{
				AkSampleType * AK_RESTRICT pfBuf = (AkSampleType * ) io_pfBuffer;
				const AkSampleType * const pfEnd = io_pfBuffer + in_uNumFrames;

#ifdef AKSIMD_V4F32_SUPPORTED
				const AkUInt32 uNumVecIter = in_uNumFrames/4;
				const AkSampleType * const pfVecEnd = io_pfBuffer + uNumVecIter*4;
				const AKSIMD_V4F32 vfGain = AKSIMD_LOAD1_V4F32( in_fGain );
				while ( pfBuf < pfVecEnd )
				{
					AKSIMD_V4F32 vfIn = AKSIMD_LOAD_V4F32((AKSIMD_F32*)pfBuf);
					AKSIMD_V4F32 vfOut = AKSIMD_MUL_V4F32( vfIn, vfGain );
					AKSIMD_STORE_V4F32( (AKSIMD_F32*)pfBuf, vfOut );
					pfBuf+=4;
				}
#elif defined (AKSIMD_V2F32_SUPPORTED)
				// Unroll 4 times x 2 floats
				const AkUInt32 uNumVecIter = in_uNumFrames/8;
				AKSIMD_V2F32 vfGain = __PS_FDUP( in_fGain );
				const AkSampleType * const pfVecEnd = io_pfBuffer + uNumVecIter*8;
				while ( pfBuf < pfVecEnd )
				{
					AKSIMD_V2F32 vfIn1 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 0 );
					AKSIMD_V2F32 vfIn2 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 8 );
					AKSIMD_V2F32 vfIn3 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 16 );
					AKSIMD_V2F32 vfIn4 = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 24 );
					AKSIMD_V2F32 vfOut1 = AKSIMD_MUL_V2F32( vfIn1, vfGain );
					AKSIMD_V2F32 vfOut2 = AKSIMD_MUL_V2F32( vfIn2, vfGain );
					AKSIMD_V2F32 vfOut3 = AKSIMD_MUL_V2F32( vfIn3, vfGain );
					AKSIMD_V2F32 vfOut4 = AKSIMD_MUL_V2F32( vfIn4, vfGain );
					AKSIMD_STORE_V2F32_OFFSET( pfBuf, 0, vfOut1 );
					AKSIMD_STORE_V2F32_OFFSET( pfBuf, 8, vfOut2 );
					AKSIMD_STORE_V2F32_OFFSET( pfBuf, 16, vfOut3 );
					AKSIMD_STORE_V2F32_OFFSET( pfBuf, 24, vfOut4 );
					pfBuf+=8;
				}
				/*
				const AkUInt32 uNumVecIter = in_uNumFrames/2;
				AKSIMD_V2F32 vfGain = __PS_FDUP( in_fGain );
				const AkSampleType * const pfVecEnd = io_pfBuffer + uNumVecIter*2;
				while ( pfBuf < pfVecEnd )
				{
					AKSIMD_V2F32 vfIn = AKSIMD_LOAD_V2F32_OFFSET( pfBuf, 0 );
					AKSIMD_V2F32 vfOut = AKSIMD_MUL_V2F32( vfIn, vfGain );
					AKSIMD_STORE_V2F32_OFFSET( pfBuf, 0, vfOut );
					pfBuf+=2;
				}
				*/
#endif
				while ( pfBuf < pfEnd )
				{
					*pfBuf = (AkSampleType)(*pfBuf * in_fGain);
					pfBuf++;
				}
			}
		}

		/// Single channel, Out-of-place static gain.
		static inline void ApplyGain(	
			AkSampleType * AK_RESTRICT in_pfBufferIn, 
			AkSampleType * AK_RESTRICT out_pfBufferOut, 
			AkReal32 in_fGain,
			AkUInt32 in_uNumFrames )
		{
			AkSampleType * AK_RESTRICT pfInBuf = (AkSampleType * ) in_pfBufferIn;
			AkSampleType * AK_RESTRICT pfOutBuf = (AkSampleType * ) out_pfBufferOut;
			const AkSampleType * const pfEnd = in_pfBufferIn + in_uNumFrames;

#ifdef AKSIMD_V4F32_SUPPORTED
			const AkUInt32 uNumVecIter = in_uNumFrames/4;			
			const AkSampleType * const pfVecEnd = in_pfBufferIn + uNumVecIter*4;
			const AKSIMD_V4F32 vfGain = AKSIMD_LOAD1_V4F32( in_fGain );
			while ( pfInBuf < pfVecEnd )
			{
				AKSIMD_V4F32 vfIn = AKSIMD_LOAD_V4F32((AKSIMD_F32*)pfInBuf);
				AKSIMD_V4F32 vfOut = AKSIMD_MUL_V4F32( vfIn, vfGain );
				AKSIMD_STORE_V4F32( (AKSIMD_F32*)pfOutBuf, vfOut );
				pfInBuf+=4;
				pfOutBuf+=4;
			}
#elif defined (AKSIMD_V2F32_SUPPORTED)
			// Unroll 4 times x 2 floats
			const AkUInt32 uNumVecIter = in_uNumFrames/8;
			const AkSampleType * const pfVecEnd = in_pfBufferIn + uNumVecIter*8;
			AKSIMD_V2F32 vfGain = __PS_FDUP( in_fGain );
			while ( pfInBuf < pfVecEnd )
			{
				AKSIMD_V2F32 vfIn1 = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 0 );
				AKSIMD_V2F32 vfIn2 = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 8 );
				AKSIMD_V2F32 vfIn3 = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 16 );
				AKSIMD_V2F32 vfIn4 = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 24 );
				pfInBuf+=8;
				AKSIMD_V2F32 vfOut1 = AKSIMD_MUL_V2F32( vfIn1, vfGain );
				AKSIMD_V2F32 vfOut2 = AKSIMD_MUL_V2F32( vfIn2, vfGain );
				AKSIMD_V2F32 vfOut3 = AKSIMD_MUL_V2F32( vfIn3, vfGain );
				AKSIMD_V2F32 vfOut4 = AKSIMD_MUL_V2F32( vfIn4, vfGain );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 0, vfOut1 );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 8, vfOut2 );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 16, vfOut3 );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 24, vfOut4 );
				pfOutBuf+=8;
			}
			/*
			const AkUInt32 uNumVecIter = in_uNumFrames/2;
			const AkSampleType * const pfVecEnd = in_pfBufferIn + uNumVecIter*2;
			AKSIMD_V2F32 vfGain = __PS_FDUP( in_fGain );
			while ( pfInBuf < pfVecEnd )
			{
				AKSIMD_V2F32 vfIn = AKSIMD_LOAD_V2F32_OFFSET( pfInBuf, 0 );
				AKSIMD_V2F32 vfOut = AKSIMD_MUL_V2F32( vfIn, vfGain );
				AKSIMD_STORE_V2F32_OFFSET( pfOutBuf, 0, vfOut );
				pfInBuf+=2;
				pfOutBuf+=2;
			}
			*/
#endif
			while ( pfInBuf < pfEnd )
			{
				*pfOutBuf++ = (AkSampleType)(*pfInBuf++ * in_fGain);
			}
		}


		/// Single channel, In-place (possibly interpolating) gain.
		static inline void ApplyGain(	
			AkSampleType * AK_RESTRICT io_pfBuffer, 
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			AkUInt32 in_uNumFrames )
		{

			if ( in_fTargetGain == in_fCurGain )
				ApplyGain(io_pfBuffer, in_fCurGain, in_uNumFrames );
			else
				ApplyGainRamp( io_pfBuffer, in_fCurGain, in_fTargetGain, in_uNumFrames );
		}

		/// Single channel, Out-of-place (possibly interpolating) gain.
		static inline void ApplyGain(	
			AkSampleType * AK_RESTRICT in_pfBufferIn, 
			AkSampleType * AK_RESTRICT out_pfBufferOut, 
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			AkUInt32 in_uNumFrames )
		{
			if ( in_fTargetGain == in_fCurGain )
				ApplyGain(in_pfBufferIn, out_pfBufferOut, in_fCurGain, in_uNumFrames );
			else
				ApplyGainRamp( in_pfBufferIn, out_pfBufferOut, in_fCurGain, in_fTargetGain, in_uNumFrames );
		}

		/// Multi-channel in-place (possibly interpolating) gain.
		static inline void ApplyGain( 
			AkAudioBuffer * io_pBuffer,
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			bool in_bProcessLFE = true )
		{
			AkUInt32 uNumChannels = io_pBuffer->NumChannels();
			if ( !in_bProcessLFE && io_pBuffer->HasLFE() )
				uNumChannels--;
			const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
			if ( in_fTargetGain == in_fCurGain )
			{
				// No need for interpolation
				for ( AkUInt32 i = 0; i < uNumChannels; i++ )
				{
					AkSampleType * pfChan = io_pBuffer->GetChannel( i );
					ApplyGain(pfChan, in_fCurGain, uNumFrames );
				}
			}
			else
			{
				// Interpolate gains toward target
				for ( AkUInt32 i = 0; i < uNumChannels; i++ )
				{
					AkSampleType * pfChan = io_pBuffer->GetChannel( i );
					ApplyGainRamp(pfChan, in_fCurGain, in_fTargetGain, uNumFrames );
				}
			}
		}

		/// Single-channel LFE in-place (possibly interpolating) gain.
		static inline void ApplyGainLFE( 
			AkAudioBuffer * io_pBuffer,
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain )
		{
			if( io_pBuffer->HasLFE() )
			{
				AkUInt32 uLFEChannelIdx = io_pBuffer->NumChannels()-1;
				const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
				AkSampleType * pfChan = io_pBuffer->GetChannel( uLFEChannelIdx );
				if ( in_fTargetGain == in_fCurGain )
				{
					// No need for interpolation
					ApplyGain(pfChan, in_fCurGain, uNumFrames );
				}
				else
				{
					// Interpolate gains toward target
					ApplyGainRamp(pfChan, in_fCurGain, in_fTargetGain, uNumFrames );
				}
			}
		}

		/// Multi-channel out-of-place (possibly interpolating) gain.
		static inline void ApplyGain( 
			AkAudioBuffer * in_pBuffer,
			AkAudioBuffer * out_pBuffer,
			AkReal32 in_fCurGain,
			AkReal32 in_fTargetGain,
			bool in_bProcessLFE = true )
		{
			AKASSERT( in_pBuffer->NumChannels() == out_pBuffer->NumChannels() );
			AkUInt32 uNumChannels = in_pBuffer->NumChannels();
			if ( !in_bProcessLFE && in_pBuffer->HasLFE() )
				uNumChannels--;
			const AkUInt32 uNumFrames = AkMin( in_pBuffer->uValidFrames, out_pBuffer->MaxFrames() );
			if ( in_fTargetGain == in_fCurGain )
			{
				// No need for interpolation
				for ( AkUInt32 i = 0; i < uNumChannels; i++ )
				{
					AkSampleType * pfInChan = in_pBuffer->GetChannel( i );
					AkSampleType * pfOutChan = out_pBuffer->GetChannel( i );
					ApplyGain(pfInChan, pfOutChan, in_fCurGain, uNumFrames );
				}
			}
			else
			{
				// Interpolate gains toward target
				for ( AkUInt32 i = 0; i < uNumChannels; i++ )
				{
					AkSampleType * pfInChan = in_pBuffer->GetChannel( i );
					AkSampleType * pfOutChan = out_pBuffer->GetChannel( i );
					ApplyGainRamp( pfInChan, pfOutChan, in_fCurGain, in_fTargetGain, uNumFrames );
				}
			}
		}

	} // namespace DSP
} // namespace AK

#endif // _AKAPPLYGAIN_H_
