//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// Length of delay line is mapped on 4 frames boundary (i.e. may not be suited for reverberation for example)
// This is not a delay line implementation, but rather just some services for memory managment related 
// to specific delay line execution needs as detailed by clients

#ifndef _AKDSP_DELAYLINEMEMORY_
#define _AKDSP_DELAYLINEMEMORY_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#define AK_ALIGN_TO_NEXT_BOUNDARY( __num__, __boundary__ ) (((__num__) + ((__boundary__)-1)) & ~((__boundary__)-1))
namespace AK
{
	namespace DSP
	{
		template < class T_SAMPLETYPE, AkUInt32 T_MAXNUMCHANNELS > 
		class CAkDelayLineMemory
		{
		public:
			
			CAkDelayLineMemory( )
				: m_uDelayLineLength( 0 )
				, m_uOffset( 0 )
				, m_uNumChannels( 0 )
			{
				AkZeroMemSmall( m_pDelay, T_MAXNUMCHANNELS*sizeof(T_SAMPLETYPE *) );
			}

			AKRESULT Init( 
				AK::IAkPluginMemAlloc * in_pAllocator, 
				AkUInt32 in_uDelayLineLength,  
				AkUInt32 in_uNumChannels )
			{
				m_uNumChannels = in_uNumChannels;
				// Align delay length on 4 frame boundary to simplify DMA and SIMD alignement
				m_uDelayLineLength = AK_ALIGN_TO_NEXT_BOUNDARY( in_uDelayLineLength, 4 ); 
				m_uOffset = 0;
				if ( m_uDelayLineLength )
				{
					for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
					{
						m_pDelay[i] = (T_SAMPLETYPE*)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( sizeof(T_SAMPLETYPE) * m_uDelayLineLength ) );
						if ( m_pDelay[i] == NULL )
							return AK_InsufficientMemory;
					}
				}
				return AK_Success;
			}

			void Term( AK::IAkPluginMemAlloc * in_pAllocator )
			{
				for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
				{
					if ( m_pDelay[i] )
					{
						AK_PLUGIN_FREE( in_pAllocator, m_pDelay[i] );
						m_pDelay[i] = NULL;
					}
				}
				m_uDelayLineLength = 0;
			}

			void Reset( )
			{
				if ( m_uDelayLineLength )
				{
					for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
					{
						if (m_pDelay[i])
							AkZeroMemLarge( (void*) m_pDelay[i], m_uDelayLineLength*sizeof(T_SAMPLETYPE) );
					}
				}
				m_uOffset = 0;
			}

			AkForceInline AkUInt32 GetCurrentOffset()
			{
				return m_uOffset;
			}

			AkForceInline void SetCurrentOffset( AkUInt32 in_uOffset )
			{
				m_uOffset = in_uOffset;
			}

			AkForceInline AkUInt32 GetDelayLength()
			{
				return m_uDelayLineLength;
			}

			T_SAMPLETYPE * GetCurrentPointer( AkUInt32 in_uOffset, AkUInt32 in_uChannelIndex )
			{
				return m_pDelay[in_uChannelIndex] + in_uOffset;
			}

		public:

			T_SAMPLETYPE *	m_pDelay[T_MAXNUMCHANNELS];	// Delay lines for each channel
			AkUInt32		m_uDelayLineLength;			// Total delay line length    
			AkUInt32		m_uOffset;					// Current delay line write position
			AkUInt32		m_uNumChannels;				// Number of delayed channels
		};

	} // namespace DSP
} // namespace AK

#endif // _AKDSP_DELAYLINEMEMORY_