//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKFXPARAMETERCHANGEHANDLER_H_
#define _AKFXPARAMETERCHANGEHANDLER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

/// Can be used to track individual parameter changes to avoid costly computations when they remain constant
/// This class is designed to use only the lower bit information of the parameter IDs
namespace AK
{

	template <AkUInt32 T_MAXNUMPARAMS> 
	class AkFXParameterChangeHandler
	{
	public:

		inline AkFXParameterChangeHandler()
		{
			ResetAllParamChanges( );
		}

		inline void SetParamChange( AkPluginParamID in_ID )
		{
			AKASSERT( in_ID <= T_MAXNUMPARAMS );
			const AkUInt32 uByteIndex = in_ID/8;
			const AkUInt32 uBitMask = 1<<(in_ID-uByteIndex*8);
			m_uParamBitArray[uByteIndex] |= uBitMask;
		}

		inline bool HasChanged( AkPluginParamID in_ID )
		{
			AKASSERT( in_ID <= T_MAXNUMPARAMS );
			const AkUInt32 uByteIndex = in_ID/8;
			const AkUInt32 uBitMask = 1<<(in_ID-uByteIndex*8);
			return (m_uParamBitArray[uByteIndex] & uBitMask) > 0;
		}

		inline bool HasAnyChanged()
		{
			AkUInt32 uByteIndex = 0;
			do
			{
				if ( m_uParamBitArray[uByteIndex] > 0 )
					return true;
				++uByteIndex;
			} while (uByteIndex < (((T_MAXNUMPARAMS) + ((8)-1)) & ~((8)-1))/8 );
			return false;
		}

		inline void ResetParamChange( AkPluginParamID in_ID )
		{
			AKASSERT( in_ID <= T_MAXNUMPARAMS );
			const AkUInt32 uByteIndex = in_ID/8;
			const AkUInt32 uBitMask = 1<<(in_ID-uByteIndex*8);
			m_uParamBitArray[uByteIndex] &= ~uBitMask;
		}

		inline void ResetAllParamChanges( )
		{
			AkZeroMemSmall( m_uParamBitArray, sizeof(m_uParamBitArray) );
		}

		inline void SetAllParamChanges( )
		{
			AKPLATFORM::AkMemSet( m_uParamBitArray, 0xFF, sizeof(m_uParamBitArray) );
		}	

	protected:

		// Minimum storage in bits aligned to next byte boundary
		AkUInt8 m_uParamBitArray[(((T_MAXNUMPARAMS) + ((8)-1)) & ~((8)-1))/8]; 

	};

} // namespace AK

#endif // _AKFXPARAMETERCHANGEHANDLER_H_
