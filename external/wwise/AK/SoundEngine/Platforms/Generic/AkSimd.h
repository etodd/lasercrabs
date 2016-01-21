//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// AKSIMD - Generic (no SIMD support) implementation

#ifndef _AKSIMD_GENERIC_H_
#define _AKSIMD_GENERIC_H_

#include <math.h>
#include <string.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD types
//@{
typedef AkInt32	AKSIMD_I32;									///< 32-bit signed integer
typedef struct { AkInt32 m_data[4]; } AKSIMD_V4I32;			///< Vector of 4 32-bit signed integers
typedef struct { AkUInt32 m_data[4]; } AKSIMD_V4UI32;		///< Vector of 4 32-bit signed integers
typedef AkReal32 AKSIMD_F32;								///< 32-bit float
typedef struct { AkReal32 m_data[2]; } AKSIMD_V2F32;		///< Vector of 2 32-bit floats
typedef struct { AkReal32 m_data[4]; } AKSIMD_V4F32;		///< Vector of 4 32-bit floats
typedef AKSIMD_V4UI32	AKSIMD_V4COND;						///< Vector of 4 comparison results


typedef struct { AkInt32 m_data[4]; }  __attribute__((__packed__)) AKSIMD_V4I32_UNALIGNED;		///< Unaligned Vector of 4 32-bit signed integers
typedef struct { AkUInt32 m_data[4]; } __attribute__((__packed__)) AKSIMD_V4UI32_UNALIGNED;		///< Unaligned Vector of 4 32-bit signed integers
typedef struct { AkReal32 m_data[2]; } __attribute__((__packed__)) AKSIMD_V2F32_UNALIGNED;		///< Unaligned Vector of 2 32-bit floats
typedef struct { AkReal32 m_data[4]; } __attribute__((__packed__)) AKSIMD_V4F32_UNALIGNED;		///< Unaligned Vector of 4 32-bit floats

//@}
////////////////////////////////////////////////////////////////////////

#ifndef AKSIMD_GETELEMENT_V4F32
#define AKSIMD_GETELEMENT_V4F32( __vName, __num__ )				(__vName).m_data[(__num__)]
#endif

#ifndef AKSIMD_GETELEMENT_V2F32
#define AKSIMD_GETELEMENT_V2F32( __vName, __num__ )				(__vName).m_data[(__num__)]
#endif

#ifndef AKSIMD_GETELEMENT_V4I32
#define AKSIMD_GETELEMENT_V4I32( __vName, __num__ )				(__vName).m_data[(__num__)]
#endif

////////////////////////////////////////////////////////////////////////
/// @name Platform specific memory size alignment for allocation purposes
//@{
#define AKSIMD_ALIGNSIZE( __Size__ ) (((__Size__) + 15) & ~15)
//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD loading / setting
//@{
#define AKSIMD_LOADU_V4I32( in_pData ) (*(in_pData))

#define AKSIMD_LOADU_V4F32( in_pValue ) (*(AKSIMD_V4F32*)(in_pValue))

#define AKSIMD_LOAD_V4F32( in_pValue ) (*(AKSIMD_V4F32*)(in_pValue))

AkForceInline AKSIMD_V4F32 AKSIMD_LOAD1_V4F32( AKSIMD_F32 in_value )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = in_value;
	vector.m_data[1] = in_value;
	vector.m_data[2] = in_value;
	vector.m_data[3] = in_value;
	
	return vector;
}

// _mm_set_ps1
AkForceInline AKSIMD_V4F32 AKSIMD_SET_V4F32( AKSIMD_F32 in_value )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = in_value;
	vector.m_data[1] = in_value;
	vector.m_data[2] = in_value;
	vector.m_data[3] = in_value;
	
	return vector;
}


AkForceInline AKSIMD_V2F32 AKSIMD_SET_V2F32( AKSIMD_F32 in_value )
{
	AKSIMD_V2F32 vector;
	vector.m_data[0] = in_value;
	vector.m_data[1] = in_value;
	
	return vector;
}

// _mm_setzero_ps()
AkForceInline AKSIMD_V4F32 AKSIMD_SETZERO_V4F32()
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = 0.f;
	vector.m_data[1] = 0.f;
	vector.m_data[2] = 0.f;
	vector.m_data[3] = 0.f;
	
	return vector;
}

AkForceInline AKSIMD_V2F32 AKSIMD_SETZERO_V2F32()
{
	AKSIMD_V2F32 vector;
	vector.m_data[0] = 0.f;
	vector.m_data[1] = 0.f;
	
	return vector;
}
// _mm_setzero_si128()
AkForceInline AKSIMD_V4I32 AKSIMD_SETZERO_V4I32()
{
	AKSIMD_V4I32 vector;
	vector.m_data[0] = 0;
	vector.m_data[1] = 0;
	vector.m_data[2] = 0;
	vector.m_data[3] = 0;
	
	return vector;
}


/// Loads a single-precision, floating-point value into the low word
/// and clears the upper three words.
/// r0 := *p; r1 := 0.0 ; r2 := 0.0 ; r3 := 0.0 (see _mm_load_ss)
AkForceInline AKSIMD_V4F32 AKSIMD_LOAD_SS_V4F32( const AKSIMD_F32* in_pData )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = *in_pData;
	vector.m_data[1] = 0.f;
	vector.m_data[2] = 0.f;
	vector.m_data[3] = 0.f;
	
	return vector;
}

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD storing
//@{

// _mm_storeu_ps -- The address does not need to be 16-byte aligned.
#define AKSIMD_STOREU_V4F32( in_pTo, in_vec ) (*(AKSIMD_V4F32*)(in_pTo)) = (in_vec)

// _mm_store_ps -- The address must be 16-byte aligned.
// ????? _mm_storeu_ps vs _mm_store_ps ?????
#define AKSIMD_STORE_V4F32( __addr__, __vName__ ) AKSIMD_STOREU_V4F32(__addr__, __vName__)

// _mm_storeu_si128
#define AKSIMD_STOREU_V4I32( in_pTo, in_vec ) (*(AKSIMD_V4I32*)(in_pTo)) = (in_vec)

/// Stores the lower single-precision, floating-point value.
/// *p := a0 (see _mm_store_ss)
AkForceInline void AKSIMD_STORE1_V4F32( AKSIMD_F32* in_pTo, const AKSIMD_V4F32& in_vec )
{
	((AKSIMD_V4F32*)in_pTo)->m_data[0] = in_vec.m_data[0];
}

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD conversion
//@{

// _mm_cvtepi32_ps
AkForceInline AKSIMD_V4F32 AKSIMD_CONVERT_V4I32_TO_V4F32( const AKSIMD_V4I32& in_from )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = (AkReal32)in_from.m_data[0];
	vector.m_data[1] = (AkReal32)in_from.m_data[1];
	vector.m_data[2] = (AkReal32)in_from.m_data[2];
	vector.m_data[3] = (AkReal32)in_from.m_data[3];
	
	return vector;
}
// _mm_cvtps_epi32
AkForceInline AKSIMD_V4I32 AKSIMD_CONVERT_V4F32_TO_V4I32( const AKSIMD_V4F32& in_from )
{
	AKSIMD_V4I32 vector;
	vector.m_data[0] = (AkInt32)in_from.m_data[0];
	vector.m_data[1] = (AkInt32)in_from.m_data[1];
	vector.m_data[2] = (AkInt32)in_from.m_data[2];
	vector.m_data[3] = (AkInt32)in_from.m_data[3];
	
	return vector;
}

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD logical operations
//@{

// _mm_and_si128
AkForceInline AKSIMD_V4I32 AKSIMD_AND_V4I32( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	AKSIMD_V4I32 vector;
	vector.m_data[0] = in_vec1.m_data[0] & in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] & in_vec2.m_data[1];
	vector.m_data[2] = in_vec1.m_data[2] & in_vec2.m_data[2];
	vector.m_data[3] = in_vec1.m_data[3] & in_vec2.m_data[3];
	
	return vector;
}

/// Compares the 8 signed 16-bit integers in a and the 8 signed
/// 16-bit integers in b for greater than (see _mm_cmpgt_epi16)
AkForceInline AKSIMD_V4I32 AKSIMD_CMPGT_V8I16( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	AKSIMD_V4I32 vector;
	
	AkInt16 *pVec1,*pVec2,*pVec3;
	pVec1 = (AkInt16*)&in_vec1;
	pVec2 = (AkInt16*)&in_vec2;
	pVec3 = (AkInt16*)&vector;
	
	pVec3[0] = (pVec1[0] > pVec2[0]) ? 0xffff : 0x0;
	pVec3[1] = (pVec1[1] > pVec2[1]) ? 0xffff : 0x0;
	pVec3[2] = (pVec1[2] > pVec2[2]) ? 0xffff : 0x0;
	pVec3[3] = (pVec1[3] > pVec2[3]) ? 0xffff : 0x0;
	pVec3[4] = (pVec1[4] > pVec2[4]) ? 0xffff : 0x0;
	pVec3[5] = (pVec1[5] > pVec2[5]) ? 0xffff : 0x0;
	pVec3[6] = (pVec1[6] > pVec2[6]) ? 0xffff : 0x0;
	pVec3[7] = (pVec1[7] > pVec2[7]) ? 0xffff : 0x0;

	return vector;
}

/// Compares for less than or equal (see _mm_cmple_ps)
AkForceInline AKSIMD_V4UI32 AKSIMD_CMPLE_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4UI32 vector;
	
	vector.m_data[0] = (in_vec1.m_data[0] <= in_vec2.m_data[0]) ? 0xffffffff : 0x0;
	vector.m_data[1] = (in_vec1.m_data[1] <= in_vec2.m_data[1]) ? 0xffffffff : 0x0;
	vector.m_data[2] = (in_vec1.m_data[2] <= in_vec2.m_data[2]) ? 0xffffffff : 0x0;
	vector.m_data[3] = (in_vec1.m_data[3] <= in_vec2.m_data[3]) ? 0xffffffff : 0x0;
	
	return vector;
}


AkForceInline AKSIMD_V4I32 AKSIMD_SHIFTLEFT_V4I32( AKSIMD_V4I32 in_vector, int in_shiftBy)
{
	in_vector.m_data[0] <<= in_shiftBy;
	in_vector.m_data[1] <<= in_shiftBy;
	in_vector.m_data[2] <<= in_shiftBy;
	in_vector.m_data[3] <<= in_shiftBy;
	
	return in_vector;
}

AkForceInline AKSIMD_V4I32 AKSIMD_SHIFTRIGHTARITH_V4I32( AKSIMD_V4I32 in_vector, int in_shiftBy)
{
	in_vector.m_data[0] >>= in_shiftBy;
	in_vector.m_data[1] >>= in_shiftBy;
	in_vector.m_data[2] >>= in_shiftBy;
	in_vector.m_data[3] >>= in_shiftBy;
	
	return in_vector;
}

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD arithmetic
//@{
// _mm_sub_ps
AkForceInline AKSIMD_V4F32 AKSIMD_SUB_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] - in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] - in_vec2.m_data[1];
	vector.m_data[2] = in_vec1.m_data[2] - in_vec2.m_data[2];
	vector.m_data[3] = in_vec1.m_data[3] - in_vec2.m_data[3];
	
	return vector;
}

/// Subtracts the lower single-precision, floating-point values of a and b.
/// The upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 - b0 ; r1 := a1 ; r2 := a2 ; r3 := a3 (see _mm_sub_ss)

AkForceInline AKSIMD_V4F32 AKSIMD_SUB_SS_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] - in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1];
	vector.m_data[2] = in_vec1.m_data[2];
	vector.m_data[3] = in_vec1.m_data[3];
	
	return vector;
}

// _mm_add_ps
AkForceInline AKSIMD_V4F32 AKSIMD_ADD_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] + in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] + in_vec2.m_data[1];
	vector.m_data[2] = in_vec1.m_data[2] + in_vec2.m_data[2];
	vector.m_data[3] = in_vec1.m_data[3] + in_vec2.m_data[3];
	
	return vector;
}

AkForceInline AKSIMD_V2F32 AKSIMD_ADD_V2F32( const AKSIMD_V2F32& in_vec1, const AKSIMD_V2F32& in_vec2 )
{
	AKSIMD_V2F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] + in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] + in_vec2.m_data[1];
	
	return vector;
}

/// Adds the lower single-precision, floating-point values of a and b; the
/// upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 + b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
AkForceInline AKSIMD_V4F32 AKSIMD_ADD_SS_V4F32( const AKSIMD_V4F32& a, const AKSIMD_V4F32& b )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = a.m_data[0] + b.m_data[0];
	vector.m_data[1] = a.m_data[1];
	vector.m_data[2] = a.m_data[2];
	vector.m_data[3] = a.m_data[3];
	
	return vector;
}

// _mm_mul_ps
AkForceInline AKSIMD_V4F32 AKSIMD_MUL_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] * in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] * in_vec2.m_data[1];
	vector.m_data[2] = in_vec1.m_data[2] * in_vec2.m_data[2];
	vector.m_data[3] = in_vec1.m_data[3] * in_vec2.m_data[3];
	
	return vector;
}

AkForceInline AKSIMD_V2F32 AKSIMD_MUL_V2F32( const AKSIMD_V2F32& in_vec1, const AKSIMD_V2F32& in_vec2 )
{
	AKSIMD_V2F32 vector;
	
	vector.m_data[0] = in_vec1.m_data[0] * in_vec2.m_data[0];
	vector.m_data[1] = in_vec1.m_data[1] * in_vec2.m_data[1];
	
	return vector;
}

/// Multiplies the lower single-precision, floating-point values of
/// a and b; the upper three single-precision, floating-point values
/// are passed through from a.
/// r0 := a0 * b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
AkForceInline AKSIMD_V4F32 AKSIMD_MUL_SS_V4F32( const AKSIMD_V4F32& a, const AKSIMD_V4F32& b )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = a.m_data[0] * b.m_data[0];
	vector.m_data[1] = a.m_data[1];
	vector.m_data[2] = a.m_data[2];
	vector.m_data[3] = a.m_data[3];
	
	return vector;
}

/// Vector multiply-add operation.
#define AKSIMD_MADD_V4F32( __a__, __b__, __c__ ) AKSIMD_ADD_V4F32( AKSIMD_MUL_V4F32( (__a__), (__b__) ), (__c__) )
#define AKSIMD_MSUB_V4F32( __a__, __b__, __c__ ) AKSIMD_SUB_V4F32( AKSIMD_MUL_V4F32( (__a__), (__b__) ), (__c__) )

/// Vector multiply-add operation.
#define AKSIMD_MADD_SS_V4F32( __a__, __b__, __c__ ) AKSIMD_ADD_SS_V4F32( AKSIMD_MUL_SS_V4F32( (__a__), (__b__) ), (__c__) )

// _mm_min_ps
AkForceInline AKSIMD_V4F32 AKSIMD_MIN_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = AkMin(in_vec1.m_data[0], in_vec2.m_data[0]);
	vector.m_data[1] = AkMin(in_vec1.m_data[1], in_vec2.m_data[1]);
	vector.m_data[2] = AkMin(in_vec1.m_data[2], in_vec2.m_data[2]);
	vector.m_data[3] = AkMin(in_vec1.m_data[3], in_vec2.m_data[3]);
	
	return vector;
}

AkForceInline AKSIMD_V2F32 AKSIMD_MIN_V2F32( const AKSIMD_V2F32& in_vec1, const AKSIMD_V2F32& in_vec2 )
{
	AKSIMD_V2F32 vector;
	
	vector.m_data[0] = AkMin(in_vec1.m_data[0], in_vec2.m_data[0]);
	vector.m_data[1] = AkMin(in_vec1.m_data[1], in_vec2.m_data[1]);
	
	return vector;
}

// _mm_max_ps
AkForceInline AKSIMD_V4F32 AKSIMD_MAX_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	
	vector.m_data[0] = AkMax(in_vec1.m_data[0], in_vec2.m_data[0]);
	vector.m_data[1] = AkMax(in_vec1.m_data[1], in_vec2.m_data[1]);
	vector.m_data[2] = AkMax(in_vec1.m_data[2], in_vec2.m_data[2]);
	vector.m_data[3] = AkMax(in_vec1.m_data[3], in_vec2.m_data[3]);
	
	return vector;
}

AkForceInline AKSIMD_V2F32 AKSIMD_MAX_V2F32( const AKSIMD_V2F32& in_vec1, const AKSIMD_V2F32& in_vec2 )
{
	AKSIMD_V2F32 vector;
	
	vector.m_data[0] = AkMax(in_vec1.m_data[0], in_vec2.m_data[0]);
	vector.m_data[1] = AkMax(in_vec1.m_data[1], in_vec2.m_data[1]);
	
	return vector;
}

AkForceInline AKSIMD_V4F32 AKSIMD_ABS_V4F32( const AKSIMD_V4F32& in_vec1 )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = fabs(in_vec1.m_data[0]);
	vector.m_data[1] = fabs(in_vec1.m_data[1]);
	vector.m_data[2] = fabs(in_vec1.m_data[2]);
	vector.m_data[3] = fabs(in_vec1.m_data[3]);
	return vector;
}

AkForceInline AKSIMD_V4F32 AKSIMD_NEG_V4F32( const AKSIMD_V4F32& in_vec1 )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = -in_vec1.m_data[0];
	vector.m_data[1] = -in_vec1.m_data[1];
	vector.m_data[2] = -in_vec1.m_data[2];
	vector.m_data[3] = -in_vec1.m_data[3];
	return vector;
}

// _mm_sqrt_ps
AkForceInline AKSIMD_V4F32 AKSIMD_SQRT_V4F32( const AKSIMD_V4F32& in_vec )
{
		AKSIMD_V4F32 vCompare;
		AKSIMD_GETELEMENT_V4F32(vCompare,0) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,0) );
		AKSIMD_GETELEMENT_V4F32(vCompare,1) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,1) );
		AKSIMD_GETELEMENT_V4F32(vCompare,2) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,2) );
		AKSIMD_GETELEMENT_V4F32(vCompare,3) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,3) );

		//AKSIMD_V4F32 res = vrecpeq_f32( vrsqrteq_f32( in_vec ) );

		return vCompare /*res*/;
}

AkForceInline AKSIMD_V2F32 AKSIMD_SQRT_V2F32( const AKSIMD_V2F32& in_vec )
{
	AKSIMD_V2F32 vCompare;
	AKSIMD_GETELEMENT_V4F32(vCompare,0) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,0) );
	AKSIMD_GETELEMENT_V4F32(vCompare,1) = sqrt( AKSIMD_GETELEMENT_V4F32(in_vec,1) );
	
	//AKSIMD_V4F32 res = vrecpeq_f32( vrsqrteq_f32( in_vec ) );
	
	return vCompare /*res*/;
}

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD packing / unpacking
//@{

//
// _mm_unpacklo_epi16
// r0 := a0
// r1 := b0
// r2 := a1
// r3 := b1
// r4 := a2
// r5 := b2
// r6 := a3
// r7 := b3
AkForceInline AKSIMD_V4I32 AKSIMD_UNPACKLO_VECTOR8I16( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	AKSIMD_V4I32 vector;
	AkInt16 *pVec1,*pVec2,*pDest;
	pVec1 = (AkInt16*)&in_vec1;
	pVec2 = (AkInt16*)&in_vec2;
	pDest = (AkInt16*)&vector;
	
	pDest[0] = pVec1[0];
	pDest[1] = pVec2[0];	
	pDest[2] = pVec1[1];	
	pDest[3] = pVec2[1];
	pDest[4] = pVec1[2];
	pDest[5] = pVec2[2];
	pDest[6] = pVec1[3];
	pDest[7] = pVec2[3];
	
	return vector;
}

// _mm_unpackhi_epi16
AkForceInline AKSIMD_V4I32 AKSIMD_UNPACKHI_VECTOR8I16( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	AKSIMD_V4I32 vector;
	AkInt16 *pVec1,*pVec2,*pDest;
	pVec1 = (AkInt16*)&in_vec1;
	pVec2 = (AkInt16*)&in_vec2;
	pDest = (AkInt16*)&vector;
	
	pDest[0] = pVec1[4];
	pDest[1] = pVec2[4];	
	pDest[2] = pVec1[5];	
	pDest[3] = pVec2[5];
	pDest[4] = pVec1[6];
	pDest[5] = pVec2[6];
	pDest[6] = pVec1[7];
	pDest[7] = pVec2[7];
	
	return vector;
}

// _mm_unpacklo_ps
AkForceInline AKSIMD_V4F32 AKSIMD_UNPACKLO_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = in_vec1.m_data[0];
	vector.m_data[1] = in_vec2.m_data[0];
	vector.m_data[2] = in_vec1.m_data[1];
	vector.m_data[3] = in_vec2.m_data[1];
	
	return vector;
}

// _mm_unpackhi_ps
AkForceInline AKSIMD_V4F32 AKSIMD_UNPACKHI_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = in_vec1.m_data[2];
	vector.m_data[1] = in_vec2.m_data[2];
	vector.m_data[2] = in_vec1.m_data[3];
	vector.m_data[3] = in_vec2.m_data[3];
	
	return vector;
}

// _mm_packs_epi32
AkForceInline AKSIMD_V4I32 AKSIMD_PACKS_V4I32( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	AKSIMD_V4I32 vector;
	AkInt16 *pDest = (AkInt16*)&vector;
	
	pDest[0] = AkClamp( in_vec1.m_data[0], -32768, 32767);
	pDest[1] = AkClamp( in_vec1.m_data[1], -32768, 32767);	
	pDest[2] = AkClamp( in_vec1.m_data[2], -32768, 32767);	
	pDest[3] = AkClamp( in_vec1.m_data[3], -32768, 32767);
	pDest[4] = AkClamp( in_vec2.m_data[0], -32768, 32767);
	pDest[5] = AkClamp( in_vec2.m_data[1], -32768, 32767);
	pDest[6] = AkClamp( in_vec2.m_data[2], -32768, 32767);
	pDest[7] = AkClamp( in_vec2.m_data[3], -32768, 32767);
	
	return vector;
}

//@}
////////////////////////////////////////////////////////////////////////


//#define AKSIMD_GET_ITEM( vec, index ) vec[index]




////////////////////////////////////////////////////////////////////////
/// @name AKSIMD shuffling
//@{

// See _MM_SHUFFLE
#define AKSIMD_SHUFFLE( fp3, fp2, fp1, fp0 ) \
	(((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

// See _mm_shuffle_ps
// Usage: AKSIMD_SHUFFLE_V4F32( vec1, vec2, AKSIMD_SHUFFLE( z, y, x, w ) )
//#define AKSIMD_SHUFFLE_V4F32( a, b, zyxw )

 AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_V4F32( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd, int mask )
{
	AKSIMD_V4F32 vector;
	vector.m_data[0] = xyzw.m_data[(mask) & 0x3];
	vector.m_data[1] = xyzw.m_data[(mask >> 2) & 0x3];
	vector.m_data[2] = abcd.m_data[(mask >> 4) & 0x3];
	vector.m_data[3] = abcd.m_data[(mask >> 6) & 0x3];
	
	return vector;
}


/// Moves the upper two single-precision, floating-point values of b to
/// the lower two single-precision, floating-point values of the result.
/// The upper two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := a3; r2 := a2; r1 := b3; r0 := b2 (see _mm_movehl_ps)
#define AKSIMD_MOVEHL_V4F32( a, b ) \
	AKSIMD_SHUFFLE_V4F32( (b), (a), AKSIMD_SHUFFLE(3, 2, 3, 2) )

/// Moves the lower two single-precision, floating-point values of b to
/// the upper two single-precision, floating-point values of the result.
/// The lower two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := b1 ; r2 := b0 ; r1 := a1 ; r0 := a0 (see _mm_movelh_ps)
#define AKSIMD_MOVELH_V4F32( a, b ) \
	AKSIMD_SHUFFLE_V4F32( (a), (b), AKSIMD_SHUFFLE(1, 0, 1, 0) )

/// Swap the 2 lower floats together and the 2 higher floats together.	
#define AKSIMD_SHUFFLE_BADC( __a__ ) AKSIMD_SHUFFLE_V4F32( (__a__), (__a__), AKSIMD_SHUFFLE(2,3,0,1));

/// Swap the 2 lower floats with the 2 higher floats.	
#define AKSIMD_SHUFFLE_CDAB( __a__ ) AKSIMD_SHUFFLE_V4F32( (__a__), (__a__), AKSIMD_SHUFFLE(1,0,3,2));

 /// Duplicates the odd items into the even items (d c b a -> d d b b )
#define AKSIMD_DUP_ODD(__vv) AKSIMD_SHUFFLE_V4F32(__vv, __vv, AKSIMD_SHUFFLE(3,3,1,1))

 /// Duplicates the even items into the odd items (d c b a -> c c a a )
#define AKSIMD_DUP_EVEN(__vv) AKSIMD_SHUFFLE_V4F32(__vv, __vv, AKSIMD_SHUFFLE(2,2,0,0))


//#include <AK/SoundEngine/Platforms/Generic/AkSimdShuffle.h>

//@}
////////////////////////////////////////////////////////////////////////

// Old AKSIMD -- will search-and-replace later
#define AkReal32Vector AKSIMD_V4F32
#define AKSIMD_LOAD1( __scalar__ ) AKSIMD_LOAD1_V4F32( &__scalar__ )
#define AKSIMD_LOADVEC(v) AKSIMD_LOAD_V4F32((const AKSIMD_F32*)((v)))
#define AKSIMD_MUL AKSIMD_MUL_V4F32
#define AKSIMD_STOREVEC AKSIMD_STORE_V4F32

/// Faked in-place vector horizontal add. 
/// \akwarning 
/// Don't expect this to be very efficient. 
/// /endakwarning
static AkForceInline void AKSIMD_HORIZONTALADD( AKSIMD_V4F32 & vVec )
{   
	AKSIMD_V4F32 vHighLow = AKSIMD_MOVEHL_V4F32(vVec, vVec);
	vVec = AKSIMD_ADD_V4F32(vVec, vHighLow);
	vHighLow = AKSIMD_SHUFFLE_V4F32(vVec, vVec, 0x55);
	vVec = AKSIMD_ADD_V4F32(vVec, vHighLow);
} 

/// Cross-platform SIMD multiplication of 2 complex data elements with interleaved real and imaginary parts
static AkForceInline AKSIMD_V4F32 AKSIMD_COMPLEXMUL( const AKSIMD_V4F32 vCIn1, const AKSIMD_V4F32 vCIn2 )
{
	static const AKSIMD_V4F32 vSign = { 1.f, -1.f, 1.f, -1.f }; 

	AKSIMD_V4F32 vTmp1 = AKSIMD_SHUFFLE_V4F32( vCIn1, vCIn1, AKSIMD_SHUFFLE(2,2,0,0)); 
	vTmp1 = AKSIMD_MUL_V4F32( vTmp1, vCIn2 );
	AKSIMD_V4F32 vTmp2 = AKSIMD_SHUFFLE_V4F32( vCIn1, vCIn1, AKSIMD_SHUFFLE(3,3,1,1)); 
	vTmp2 = AKSIMD_MUL_V4F32( vTmp2, vSign );
	vTmp2 = AKSIMD_MUL_V4F32( vTmp2, vCIn2 );
	vTmp2 = AKSIMD_SHUFFLE_BADC( vTmp2 ); 
	vTmp2 = AKSIMD_ADD_V4F32( vTmp2, vTmp1 );
	return vTmp2;
}

#define AKSIMD_SPLAT_V4F32(var, idx) AKSIMD_SHUFFLE_V4F32(var,var, AKSIMD_SHUFFLE(idx,idx,idx,idx))

#endif //_AKSIMD_GENERIC_H_

