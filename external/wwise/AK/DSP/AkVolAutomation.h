/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version: <VERSION>  Build: <BUILDNUMBER>
Copyright (c) <COPYRIGHTYEAR> Audiokinetic Inc.
***********************************************************************/

#ifndef _AKVOLUMEAUTOMATION_H_
#define _AKVOLUMEAUTOMATION_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkSimd.h>

namespace AK
{
	namespace DSP
	{
		// Takes and audio buffer, a volume automation buffer, and a transform, and applies the transformed volume automation to the audio buffer.
		static inline void ApplyVolAutomation(AkAudioBuffer& in_pAudioBuffer, const AkModulatorXfrm in_xfrms, const AkReal32* in_pAutmBuf )
		{
			AKSIMD_V4F32 vOffset = AKSIMD_LOAD1_V4F32( in_xfrms.m_fOffset ); 
			AKSIMD_V4F32 vScale = AKSIMD_LOAD1_V4F32( in_xfrms.m_fScale ); 

			for( AkUInt32 ch = 0; ch < in_pAudioBuffer.NumChannels(); ++ch)
			{
				AKSIMD_V4F32 * AK_RESTRICT pModSrcBuf = (AKSIMD_V4F32* AK_RESTRICT) in_pAutmBuf;
				AKSIMD_V4F32 * AK_RESTRICT pAudioBuf = (AKSIMD_V4F32* AK_RESTRICT) ( in_pAudioBuffer.GetChannel(ch) );
				AKSIMD_V4F32 * AK_RESTRICT pAudioBufEnd = pAudioBuf + (in_pAudioBuffer.MaxFrames() / 4);

				while ( pAudioBuf < pAudioBufEnd )
				{
					AKSIMD_V4F32 temp = AKSIMD_MADD_V4F32( *pModSrcBuf, vScale, vOffset );
					*pAudioBuf = AKSIMD_MUL_V4F32(*pAudioBuf, temp );
					pAudioBuf++;
					pModSrcBuf++;
				}
			}
		}

		// Takes and audio buffer, an array of volume automation buffers, and an array of transforms, and applies each 
		//	transformed volume automation to the audio buffer.
		static inline void ApplyVolAutomation(AkAudioBuffer& in_pAudioBuffer, AkModulatorXfrm* in_arrayXfrms, AkReal32** in_pArrayAutmBufs, AkUInt32 in_uNumModulators )
		{
			for (AkUInt32 i = 0; i< in_uNumModulators; ++i )
			{
				const AkModulatorXfrm& xfrm = in_arrayXfrms[i];
				const AkReal32* pAutmBuf = in_pArrayAutmBufs[i];
				ApplyVolAutomation(in_pAudioBuffer, xfrm, pAutmBuf );
			}
		}

	} // namespace DSP
} // namespace AK

#endif // _AKVOLUMEAUTOMATION_H_
