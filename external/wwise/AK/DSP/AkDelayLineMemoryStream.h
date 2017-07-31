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

// Length of delay line is mapped on 4 frames boundary (i.e. may not be suited for reverberation for example)
// Handling code for triple buffering processing on position independent code platforms is provided
// This is not a delay line implementation, but rather just some services for memory managment related 
// to specific delay line execution needs as detailed by clients
#include <AK/AkPlatforms.h>

#ifndef _AKDSP_DELAYLINEMEMORYSTREAM_
#define _AKDSP_DELAYLINEMEMORYSTREAM_

#include <AK/DSP/AkDelayLineMemory.h>

namespace AK
{
	namespace DSP
	{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
		template < class T_SAMPLETYPE > 
		class CAkDelayLineMemoryStream : public CAkDelayLineMemory< T_SAMPLETYPE > 
		{
		public:

			T_SAMPLETYPE * GetCurrentPointer( AkUInt32 in_uOffset, AkUInt32 in_uChannelIndex )
			{
				return this->m_ppDelay[in_uChannelIndex] + in_uOffset;
			}
		};
#else
		template < class T_SAMPLETYPE, AkUInt32 T_MAXNUMCHANNELS > 
		class CAkDelayLineMemoryStream : public CAkDelayLineMemory< T_SAMPLETYPE, T_MAXNUMCHANNELS > 
		{
		public:

			T_SAMPLETYPE * GetCurrentPointer( AkUInt32 in_uOffset, AkUInt32 in_uChannelIndex )
			{
				return this->m_pDelay[in_uChannelIndex] + in_uOffset;
			}
		};
#endif
	} // namespace DSP
} // namespace AK
#endif // _AKDSP_DELAYLINEMEMORYSTREAM_
