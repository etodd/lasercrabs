//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_VECTORVALUERAMP_H_
#define _AK_VECTORVALUERAMP_H_

#include <AK/SoundEngine/Common/AkSimd.h>

/// Tool for computing a ramp using SIMD types.
/// Implementation using AKSIMD_V4F32
class CAkVectorValueRampV4
{
public:

	AkForceInline AKSIMD_V4F32 Setup( AkReal32 in_fStartValue, AkReal32 in_fStopValue, AkUInt32 in_uNumFrames )
	{
		const AkReal32 fIncrement = (in_fStopValue-in_fStartValue)/in_uNumFrames;
		const AkReal32 f4xInc = 4.f*fIncrement;
		vIncrement = AKSIMD_LOAD1_V4F32( f4xInc);
		AK_ALIGN_SIMD( AkReal32 fVal[4] );
		fVal[0] = in_fStartValue;
		fVal[1] = fVal[0] + fIncrement;
		fVal[2] = fVal[1] + fIncrement;
		fVal[3] = fVal[2] + fIncrement;
		vValueRamp = AKSIMD_LOAD_V4F32( fVal );
		return vValueRamp;
	}

	AkForceInline AKSIMD_V4F32 Tick( )
	{
		vValueRamp = AKSIMD_ADD_V4F32( vValueRamp, vIncrement );
		return vValueRamp;
	}

private:
	AKSIMD_V4F32 vIncrement;
	AKSIMD_V4F32 vValueRamp;
};


#ifdef AKSIMD_V2F32_SUPPORTED
/// Tool for computing a ramp using SIMD types.
/// Implementation using AKSIMD_V2F32
class CAkVectorValueRampV2
{
public:

	AkForceInline AKSIMD_V2F32 Setup( AkReal32 in_fStartValue, AkReal32 in_fStopValue, AkUInt32 in_uNumFrames )
	{
		const AkReal32 fIncrement = (in_fStopValue-in_fStartValue)/in_uNumFrames;
		const AkReal32 f2xInc = 2.f*fIncrement;
		vIncrement = AKSIMD_SET_V2F32( f2xInc );
		AKSIMD_BUILD_V2F32( const AKSIMD_V2F32 vStartOffset, 0.f, fIncrement );
		AKSIMD_V2F32 l_vValueRamp = AKSIMD_SET_V2F32( in_fStartValue );
		l_vValueRamp = AKSIMD_ADD_V2F32( l_vValueRamp, vStartOffset );
		vValueRamp = l_vValueRamp;
		return l_vValueRamp;
	}

	AkForceInline AKSIMD_V2F32 Tick( )
	{
		vValueRamp = AKSIMD_ADD_V2F32( vValueRamp, vIncrement );
		return vValueRamp;
	}

private:
	AKSIMD_V2F32 vIncrement;
	AKSIMD_V2F32 vValueRamp;
};
#endif

// By default, CAkVectorValueRamp uses the V4 implementation.
typedef CAkVectorValueRampV4 CAkVectorValueRamp;

#endif //_AK_VECTORVALUERAMP_H_
