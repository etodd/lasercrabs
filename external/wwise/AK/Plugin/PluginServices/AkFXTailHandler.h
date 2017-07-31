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
 
#ifndef _AKFXTAILHANDLER_H_
#define _AKFXTAILHANDLER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

/// Default value when effect has not enterred tail mode yet.
#define AKFXTAILHANDLER_NOTINTAIL 0xFFFFFFFF

/// Effect tail handling utility class.
/// Handles varying number of tail frames from frame to frame (i.e. based on RTPC parameters).
/// Handles effect revived (quit tail) and reenters etc.
class AkFXTailHandler
{
public:
	/// Constructor
	inline AkFXTailHandler() 
		: uTailFramesRemaining( AKFXTAILHANDLER_NOTINTAIL )
		, uTotalTailFrames(0) {}

	/// Handle FX tail and zero pads AkAudioBuffer if necessary
	inline void HandleTail(	
		AkAudioBuffer * io_pBuffer, 
		AkUInt32 in_uTotalTailFrames )
	{
		bool bPreStop = io_pBuffer->eState == AK_NoMoreData;
		if ( bPreStop )
		{	
			// Tail not yet finished processing
			if ( uTailFramesRemaining > 0  
				|| io_pBuffer->uValidFrames > 0 // <-- there are valid frames, so last (maybe partially filled) buffer.
				)
			{
				// Not previously in tail, compute tail time
				if (uTailFramesRemaining == AKFXTAILHANDLER_NOTINTAIL  
					|| io_pBuffer->uValidFrames > 0 // <- ANY valid frames in the buffer should reset the tail.
					)
				{
					uTailFramesRemaining = in_uTotalTailFrames;
					uTotalTailFrames	 = in_uTotalTailFrames;
				}
				// Tail time changed, augment if necessary but preserve where we are so that effect will 
				// still finish when constantly changing this based on RTPC parameters
				else if ( in_uTotalTailFrames > uTotalTailFrames )
				{
					AkUInt32 uFramesElapsed = uTotalTailFrames - uTailFramesRemaining;
					uTailFramesRemaining = in_uTotalTailFrames - uFramesElapsed;
					uTotalTailFrames	 = in_uTotalTailFrames;
				}
				// Always full buffers while in tail
				AkUInt32 uNumTailFrames = (AkUInt32)(io_pBuffer->MaxFrames()-io_pBuffer->uValidFrames); 
				uTailFramesRemaining -= AkMin( uTailFramesRemaining, uNumTailFrames ); 
				io_pBuffer->ZeroPadToMaxFrames();
				if ( uTailFramesRemaining > 0 )
					io_pBuffer->eState = AK_DataReady;
			}
		}
		else
		{
			// Reset tail mode for next time if exits tail mode (on bus only)
			uTailFramesRemaining = AKFXTAILHANDLER_NOTINTAIL;
		}
	}

protected:

	AkUInt32	uTailFramesRemaining; // AKFXTAILHANDLER_NOTINTAIL, otherwise value represents number of frames remaining in tail
	AkUInt32	uTotalTailFrames;
	
} AK_ALIGN_DMA;





#endif // _AKFXTAILHANDLER_H_
