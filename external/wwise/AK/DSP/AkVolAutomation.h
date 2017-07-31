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
