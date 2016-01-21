//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// Length of delay line is mapped on 4 frames boundary (i.e. may not be suited for reverberation for example)
// Handling code for triple buffering processing on position independent code platforms is provided
// This is not a delay line implementation, but rather just some services for memory managment related 
// to specific delay line execution needs as detailed by clients
#include <AK/AkPlatforms.h>
#ifdef AK_PS3
#include "PS3/AkDelayLineMemoryStream.h"
#else
//Default implementation

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
#endif //AK_PS3