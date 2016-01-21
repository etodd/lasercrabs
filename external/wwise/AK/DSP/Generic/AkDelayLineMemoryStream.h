//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// Length of delay line is mapped on 4 frames boundary (i.e. may not be suited for reverberation for example)
// Handling code for triple buffering processing on position independent code platforms is provided
// This is not a delay line implementation, but rather just some services for memory managment related 
// to specific delay line execution needs as detailed by clients

#ifndef _AKDSP_DELAYLINEMEMORYSTREAM_
#define _AKDSP_DELAYLINEMEMORYSTREAM_

#include <AK/DSP/AkDelayLineMemory.h>

namespace AK
{
	namespace DSP
	{
		template < class T_SAMPLETYPE, AkUInt32 T_MAXNUMCHANNELS > 
		class CAkDelayLineMemoryStream : public CAkDelayLineMemory< T_SAMPLETYPE, T_MAXNUMCHANNELS > 
		{
		public:

#ifdef __PPU__
			// Use this to DMA data from PPU with job scheduling to avoid blocking job upon entrance
			AkUInt32 GetPrimeDelayMemory( 
				AkUInt32 in_uMaxFrames, 
				T_SAMPLETYPE * out_pAddressToDMA[2], 
				AkUInt32 out_uDMASize[2], 
				AkUInt32 & out_uOutputBufferSize )
			{
				AkZeroMemSmall( out_pAddressToDMA, 2*sizeof(T_SAMPLETYPE*) );
				AkZeroMemSmall( out_uDMASize, 2*sizeof(AkUInt32) );
				m_uMaxStreamBufferLength = in_uMaxFrames;
				AkUInt32 uNumDMA = 0;
				out_uOutputBufferSize = AkMin((this->m_uNumChannels-1),2)*AkMin(this->m_uDelayLineLength,in_uMaxFrames)*sizeof(T_SAMPLETYPE);
				if ( this->m_uNumChannels )
				{
					if ( this->m_uDelayLineLength <= in_uMaxFrames )
					{
						// Small delay may wrap multiple times per execution, send whole delay line over
						out_pAddressToDMA[0] = this->m_pDelay[0];
						out_uDMASize[0] = this->m_uDelayLineLength*sizeof(T_SAMPLETYPE);
						++uNumDMA;
					}
					else
					{
						// Large delay, only DMA necessary section for this execution
						const AkUInt32 uNumFramesFirstDMA = AkMin( this->m_uDelayLineLength - this->m_uOffset, in_uMaxFrames );		
						out_pAddressToDMA[0] = this->m_pDelay[0] + this->m_uOffset;	
						out_uDMASize[0] = uNumFramesFirstDMA*sizeof(T_SAMPLETYPE);
						++uNumDMA;
						if ( uNumFramesFirstDMA < in_uMaxFrames )
						{
							// Large DMA will wrap during execution, 2 DMAs are necessary
							out_pAddressToDMA[1] = this->m_pDelay[0];
							out_uDMASize[1] = (in_uMaxFrames-uNumFramesFirstDMA)*sizeof(T_SAMPLETYPE);
							++uNumDMA;
						}
					}
				}
				return uNumDMA;
			}

#endif	// #ifdef __PPU__

#ifdef __SPU__
			// Setup delay memory triple buffering
			void InitSPU( AkReal32 * in_pPrimeDataDMABuffer, AkReal32 * in_pOutputBuffer )
			{
				m_uStreamIndex = 0;
				AkZeroMemSmall( m_pfSreamedData, 3*sizeof(T_SAMPLETYPE*) );
				if ( this->m_uNumChannels >= 1 )
					m_pfSreamedData[0] = in_pPrimeDataDMABuffer;
				if ( this->m_uNumChannels >= 2 ) 
					m_pfSreamedData[1] = in_pOutputBuffer;
				if ( this->m_uNumChannels >= 3 ) 
					m_pfSreamedData[2] = &in_pOutputBuffer[m_uMaxStreamBufferLength];
			}

			void GetChannel( AkUInt32 in_uChannelIndex, AkUInt32 in_uDMATag )
			{
				if ( (in_uChannelIndex < this->m_uNumChannels) && (in_uChannelIndex >= 1) )
				{
					const AkUInt32 uNextBuffer = (m_uStreamIndex+1)%3;
					AKASSERT( m_pfSreamedData[uNextBuffer] != NULL );
					if ( this->m_uDelayLineLength <= m_uMaxStreamBufferLength )
					{
						// Small delay may wrap multiple times per execution, send whole delay line over
						T_SAMPLETYPE * pAddressToGet = this->m_pDelay[in_uChannelIndex];
						AkUInt32 uDataSize = this->m_uDelayLineLength*sizeof(T_SAMPLETYPE);
						AkDmaGet( "DelayLineMemoryStream::DelayMem", m_pfSreamedData[uNextBuffer], (std::uint64_t)pAddressToGet, uDataSize, in_uDMATag, 0, 0);
					}
					else
					{
						// Large delay, only DMA necessary section for this execution
						T_SAMPLETYPE * pAddressToGet = this->m_pDelay[in_uChannelIndex] + this->m_uOffset;
						AkUInt32 uNumFramesFirstDMA = AkMin( this->m_uDelayLineLength - this->m_uOffset, m_uMaxStreamBufferLength );
						AkUInt32 uDataSize = uNumFramesFirstDMA*sizeof(T_SAMPLETYPE);
						AkDmaGet( "DelayLineMemoryStream::DelayMem(1)", m_pfSreamedData[uNextBuffer], (std::uint64_t)pAddressToGet, uDataSize, in_uDMATag, 0, 0);
						if ( uNumFramesFirstDMA < m_uMaxStreamBufferLength )
						{
							// Large DMA will wrap during execution, 2 DMAs are necessary
							pAddressToGet = this->m_pDelay[in_uChannelIndex];
							uDataSize = (m_uMaxStreamBufferLength-uNumFramesFirstDMA)*sizeof(T_SAMPLETYPE);
							AkDmaGet( "DelayLineMemoryStream::DelayMem(2)", m_pfSreamedData[uNextBuffer]+uNumFramesFirstDMA, (std::uint64_t)pAddressToGet, uDataSize, in_uDMATag, 0, 0);
						}
					}
				}
			}

			void Wait( AkUInt32 in_uDMATag )
			{
				// no more than 1 (set) DMA in flight (wait on previous DMA completion)
				if ( this->m_uNumChannels > 1 )
					AkDmaWait(1<<in_uDMATag);  
			}

			void PutChannel( AkUInt32 in_uChannelIndex, AkUInt32 in_uDMATag )
			{	
				AKASSERT( m_pfSreamedData[m_uStreamIndex] != NULL );
				if ( this->m_uDelayLineLength <= m_uMaxStreamBufferLength )
				{
					// Small delay may wrap multiple times per execution, send whole delay line over
					T_SAMPLETYPE * pAddressToPut = this->m_pDelay[in_uChannelIndex];
					AkUInt32 uDataSize = this->m_uDelayLineLength*sizeof(T_SAMPLETYPE);
					AkDmaPut( "DelayLineMemoryStream::DelayMem", m_pfSreamedData[m_uStreamIndex], (std::uint64_t)pAddressToPut, uDataSize, in_uDMATag, 0, 0);
				}
				else
				{
					// Large delay, only DMA necessary section for this execution
					T_SAMPLETYPE * pAddressToPut = this->m_pDelay[in_uChannelIndex] + this->m_uOffset;
					AkUInt32 uNumFramesFirstDMA = AkMin( this->m_uDelayLineLength - this->m_uOffset, m_uMaxStreamBufferLength );
					AkUInt32 uDataSize = uNumFramesFirstDMA*sizeof(T_SAMPLETYPE);
					AkDmaPut( "DelayLineMemoryStream::DelayMem(1)", m_pfSreamedData[m_uStreamIndex], (std::uint64_t)pAddressToPut, uDataSize, in_uDMATag, 0, 0);

					if ( uNumFramesFirstDMA < m_uMaxStreamBufferLength )
					{
						// Large DMA will wrap during execution, 2 DMAs are necessary
						pAddressToPut = this->m_pDelay[in_uChannelIndex];
						uDataSize = (m_uMaxStreamBufferLength-uNumFramesFirstDMA)*sizeof(T_SAMPLETYPE);
						AkDmaPut( "DelayLineMemoryStream::DelayMem(2)", m_pfSreamedData[m_uStreamIndex]+uNumFramesFirstDMA, (std::uint64_t)pAddressToPut, uDataSize, in_uDMATag, 0, 0);
					}
				}

				m_uStreamIndex = ++m_uStreamIndex % 3;
			}
#endif // #ifdef __SPU__

			T_SAMPLETYPE * GetCurrentPointer( AkUInt32 in_uOffset, AkUInt32 in_uChannelIndex )
			{
#ifndef __SPU__
				return this->m_pDelay[in_uChannelIndex] + in_uOffset;
#else	
				// Use triple buffer local storage memory
				// m_pfSreamedData[m_uStreamIndex] points to the proper (ready) channel
				T_SAMPLETYPE * pfReadyData = m_pfSreamedData[m_uStreamIndex];
				AKASSERT( pfReadyData != NULL );

				if ( this->m_uDelayLineLength <= m_uMaxStreamBufferLength )
				{
					// Small delays (which can wrap multiple times per execution) are always retrieved entirely, use absolute offset
					pfReadyData += in_uOffset;
				}
				else
				{		
					// Large delays are always retrieved partially, use relative offset
					if ( in_uOffset >= this->m_uOffset )
					{
						// delay has not wrapped, simply consider relative offset
						AkUInt32 uDMAOffset = in_uOffset - this->m_uOffset;
						pfReadyData += uDMAOffset;
					}
					else
					{
						// Delay has wrapped, need to offset by the size of the first DMA
						pfReadyData += AkMin( this->m_uDelayLineLength - this->m_uOffset, m_uMaxStreamBufferLength ) + in_uOffset;
					}
				}

				return pfReadyData;
#endif
			}

		protected:

			T_SAMPLETYPE *	m_pfSreamedData[3];
			AkUInt32		m_uStreamIndex;
			AkUInt32		m_uMaxStreamBufferLength;

		};

	} // namespace DSP
} // namespace AK

#endif // _AKDSP_DELAYLINEMEMORYSTREAM_