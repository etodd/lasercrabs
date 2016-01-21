//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// AKSIMD - SSE implementation

#ifndef _AK_SIMD_SSE_H_
#define _AK_SIMD_SSE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <xmmintrin.h>

////////////////////////////////////////////////////////////////////////
/// @name Platform specific defines for prefetching
//@{

#define AKSIMD_ARCHCACHELINESIZE	(64)				///< Assumed cache line width for architectures on this platform
#define AKSIMD_ARCHMAXPREFETCHSIZE	(512) 				///< Use this to control how much prefetching maximum is desirable (assuming 8-way cache)		
/// Cross-platform memory prefetch of effective address assuming non-temporal data
#define AKSIMD_PREFETCHMEMORY( __offset__, __add__ ) _mm_prefetch(((char *)(__add__))+(__offset__), _MM_HINT_NTA ) 

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name Platform specific memory size alignment for allocation purposes
//@{
#define AKSIMD_ALIGNSIZE( __Size__ ) (((__Size__) + 15) & ~15)
//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD types
//@{

typedef float	AKSIMD_F32;		///< 32-bit float
typedef __m128	AKSIMD_V4F32;	///< Vector of 4 32-bit floats
typedef AKSIMD_V4F32 AKSIMD_V4COND;	 ///< Vector of 4 comparison results
typedef AKSIMD_V4F32 AKSIMD_V4FCOND;	 ///< Vector of 4 comparison results
#define AKSIMD_V4F32_SUPPORTED

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD loading / setting
//@{

/// Loads four single-precision, floating-point values (see _mm_load_ps)
#define AKSIMD_LOAD_V4F32( __addr__ ) _mm_load_ps( (AkReal32*)(__addr__) )

/// Loads four single-precision floating-point values from unaligned
/// memory (see _mm_loadu_ps)
#define AKSIMD_LOADU_V4F32( __addr__ ) _mm_loadu_ps( (__addr__) )

/// Loads a single single-precision, floating-point value, copying it into
/// all four words (see _mm_load1_ps, _mm_load_ps1)
#define AKSIMD_LOAD1_V4F32( __scalar__ ) _mm_load1_ps( &(__scalar__) )

/// Sets the four single-precision, floating-point values to in_value (see
/// _mm_set1_ps, _mm_set_ps1)
#define AKSIMD_SET_V4F32( __scalar__ ) _mm_set_ps1( (__scalar__) )

/// Sets the four single-precision, floating-point values to zero (see
/// _mm_setzero_ps)
#define AKSIMD_SETZERO_V4F32() _mm_setzero_ps()

/// Loads a single-precision, floating-point value into the low word
/// and clears the upper three words.
/// r0 := *p; r1 := 0.0 ; r2 := 0.0 ; r3 := 0.0 (see _mm_load_ss)
#define AKSIMD_LOAD_SS_V4F32( __addr__ ) _mm_load_ss( (__addr__) )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD storing
//@{

/// Stores four single-precision, floating-point values. The address
/// must be 16-byte aligned (see _mm_store_ps)
#define AKSIMD_STORE_V4F32( __addr__, __vec__ ) _mm_store_ps( (AkReal32*)(__addr__), (__vec__) )

/// Stores four single-precision, floating-point values. The address
/// does not need to be 16-byte aligned (see _mm_storeu_ps).
#define AKSIMD_STOREU_V4F32( __addr__, __vec__ ) _mm_storeu_ps( (AkReal32*)(__addr__), (__vec__) )

/// Stores the lower single-precision, floating-point value.
/// *p := a0 (see _mm_store_ss)
#define AKSIMD_STORE1_V4F32( __addr__, __vec__ ) _mm_store_ss( (AkReal32*)(__addr__), (__vec__) )

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD shuffling
//@{

// Macro for shuffle parameter for AKSIMD_SHUFFLE_V4F32() (see _MM_SHUFFLE)
#define AKSIMD_SHUFFLE( fp3, fp2, fp1, fp0 ) _MM_SHUFFLE( (fp3), (fp2), (fp1), (fp0) )

/// Selects four specific single-precision, floating-point values from
/// a and b, based on the mask i (see _mm_shuffle_ps)
// Usage: AKSIMD_SHUFFLE_V4F32( vec1, vec2, AKSIMD_SHUFFLE( z, y, x, w ) )
#define AKSIMD_SHUFFLE_V4F32( a, b, i ) _mm_shuffle_ps( a, b, i )

/// Moves the upper two single-precision, floating-point values of b to
/// the lower two single-precision, floating-point values of the result.
/// The upper two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := a3; r2 := a2; r1 := b3; r0 := b2 (see _mm_movehl_ps)
#define AKSIMD_MOVEHL_V4F32( a, b ) _mm_movehl_ps( a, b )

/// Moves the lower two single-precision, floating-point values of b to
/// the upper two single-precision, floating-point values of the result.
/// The lower two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := b1 ; r2 := b0 ; r1 := a1 ; r0 := a0 (see _mm_movelh_ps)
#define AKSIMD_MOVELH_V4F32( a, b ) _mm_movelh_ps( a, b )

/// Swap the 2 lower floats together and the 2 higher floats together.	
#define AKSIMD_SHUFFLE_BADC( __a__ ) _mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(2,3,0,1))

/// Swap the 2 lower floats with the 2 higher floats.	
#define AKSIMD_SHUFFLE_CDAB( __a__ ) _mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(1,0,3,2))

/// Barrel-shift all floats by one.
#define AKSIMD_SHUFFLE_BCDA( __a__ ) AKSIMD_SHUFFLE_V4F32( (__a__), (__a__), _MM_SHUFFLE(0,3,2,1))

/// Duplicates the odd items into the even items (d c b a -> d d b b )
#define AKSIMD_DUP_ODD(__vv) AKSIMD_SHUFFLE_V4F32(__vv, __vv, AKSIMD_SHUFFLE(3,3,1,1))

/// Duplicates the even items into the odd items (d c b a -> c c a a )
#define AKSIMD_DUP_EVEN(__vv) AKSIMD_SHUFFLE_V4F32(__vv, __vv, AKSIMD_SHUFFLE(2,2,0,0))

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD arithmetic
//@{

/// Subtracts the four single-precision, floating-point values of
/// a and b (a - b) (see _mm_sub_ps)
#define AKSIMD_SUB_V4F32( a, b ) _mm_sub_ps( a, b )

/// Subtracts the lower single-precision, floating-point values of a and b.
/// The upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 - b0 ; r1 := a1 ; r2 := a2 ; r3 := a3 (see _mm_sub_ss)
#define AKSIMD_SUB_SS_V4F32( a, b ) _mm_sub_ss( a, b )

/// Adds the four single-precision, floating-point values of
/// a and b (see _mm_add_ps)
#define AKSIMD_ADD_V4F32( a, b ) _mm_add_ps( a, b )

/// Adds the lower single-precision, floating-point values of a and b; the
/// upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 + b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
#define AKSIMD_ADD_SS_V4F32( a, b ) _mm_add_ss( a, b )

/// Multiplies the four single-precision, floating-point values
/// of a and b (see _mm_mul_ps)
#define AKSIMD_MUL_V4F32( a, b ) _mm_mul_ps( a, b )

#define AKSIMD_DIV_V4F32( a, b ) _mm_div_ps( a, b )

/// Multiplies the lower single-precision, floating-point values of
/// a and b; the upper three single-precision, floating-point values
/// are passed through from a.
/// r0 := a0 * b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
#define AKSIMD_MUL_SS_V4F32( a, b ) _mm_mul_ss( a, b )

/// Vector multiply-add operation.
#define AKSIMD_MADD_V4F32( __a__, __b__, __c__ ) _mm_add_ps( _mm_mul_ps( (__a__), (__b__) ), (__c__) )
#define AKSIMD_MSUB_V4F32( __a__, __b__, __c__ ) _mm_sub_ps( _mm_mul_ps( (__a__), (__b__) ), (__c__) )

/// Vector multiply-add operation.
#define AKSIMD_MADD_SS_V4F32( __a__, __b__, __c__ ) _mm_add_ss( _mm_mul_ss( (__a__), (__b__) ), (__c__) )

/// Computes the minima of the four single-precision, floating-point
/// values of a and b (see _mm_min_ps)
#define AKSIMD_MIN_V4F32( a, b ) _mm_min_ps( a, b )

/// Computes the maximums of the four single-precision, floating-point
/// values of a and b (see _mm_max_ps)
#define AKSIMD_MAX_V4F32( a, b ) _mm_max_ps( a, b )

/// Computes the absolute value
#define AKSIMD_ABS_V4F32( a ) _mm_andnot_ps(_mm_set1_ps(-0.f), a)

/// Changes the sign
#define AKSIMD_NEG_V4F32( __a__ ) _mm_xor_ps(_mm_set1_ps(-0.f), __a__)

/// Vector square root aproximation (see _mm_sqrt_ps)
#define AKSIMD_SQRT_V4F32( __a__ ) _mm_sqrt_ps( (__a__) )

/// Faked in-place vector horizontal add. 
/// \akwarning 
/// Don't expect this to be very efficient. 
/// /endakwarning
static AkForceInline void AKSIMD_HORIZONTALADD(AKSIMD_V4F32 & vVec)
{   
	__m128 vHighLow = _mm_movehl_ps(vVec, vVec);
	vVec = _mm_add_ps(vVec, vHighLow);
	vHighLow = _mm_shuffle_ps(vVec, vVec, 0x55);
	vVec = _mm_add_ps(vVec, vHighLow);
} 

static AkForceInline AKSIMD_V4F32 AKSIMD_DOTPRODUCT( AKSIMD_V4F32 & vVec, const AKSIMD_V4F32 & vfSigns )
{
	AKSIMD_V4F32 vfDotProduct = AKSIMD_MUL_V4F32( vVec, vfSigns );
	AKSIMD_HORIZONTALADD( vfDotProduct );
	return AKSIMD_SHUFFLE_V4F32( vfDotProduct, vfDotProduct, AKSIMD_SHUFFLE(0,0,0,0) );
}

/// Cross-platform SIMD multiplication of 2 complex data elements with interleaved real and imaginary parts
static AkForceInline AKSIMD_V4F32 AKSIMD_COMPLEXMUL( const AKSIMD_V4F32 vCIn1, const AKSIMD_V4F32 vCIn2 )
{
	static const AKSIMD_V4F32 vSign = { -1.f, 1.f, -1.f, 1.f }; 

	AKSIMD_V4F32 vTmp1 = _mm_shuffle_ps( vCIn1, vCIn1, _MM_SHUFFLE(2,2,0,0)); 
	vTmp1 = AKSIMD_MUL_V4F32( vTmp1, vCIn2 );
	AKSIMD_V4F32 vTmp2 = _mm_shuffle_ps( vCIn1, vCIn1, _MM_SHUFFLE(3,3,1,1)); 
	vTmp2 = AKSIMD_MUL_V4F32( vTmp2, vSign );
	vTmp2 = AKSIMD_MADD_V4F32( vTmp2, AKSIMD_SHUFFLE_BADC( vCIn2 ), vTmp1 );
	return vTmp2;
}

#ifdef AK_SSE3

#include <pmmintrin.h>

/// Cross-platform SIMD multiplication of 2 complex data elements with interleaved real and imaginary parts
static AKSIMD_V4F32 AKSIMD_COMPLEXMUL_SSE3( const AKSIMD_V4F32 vCIn1, const AKSIMD_V4F32 vCIn2 )
{
	AKSIMD_V4F32 vXMM0 = _mm_moveldup_ps(vCIn1);	// multiplier real  (a1,   a1,   a0,   a0) 
	vXMM0 = AKSIMD_MUL_V4F32(vXMM0, vCIn2);			// temp1            (a1d1, a1c1, a0d0, a0c0) 
	AKSIMD_V4F32 xMM1 = _mm_shuffle_ps(vCIn2, vCIn2, 0xB1);	// shuf multiplicand(c1,   d1,   c0,   d0)  
	AKSIMD_V4F32 xMM2 = _mm_movehdup_ps(vCIn1);		// multiplier imag  (b1,   b1,   b0,   b0) 
	xMM2 = AKSIMD_MUL_V4F32( xMM2, xMM1);			// temp2            (b1c1, b1d1, b0c0, b0d0) 
	AKSIMD_V4F32 vCOut = _mm_addsub_ps(vXMM0, xMM2);		// b1c1+a1d1, a1c1-b1d1, a0d0+b0d0, a0c0-b0c0 
	return vCOut;
}

#endif

#if defined _MSC_VER && ( _MSC_VER <= 1600 )
	#define AKSIMD_ASSERTFLUSHZEROMODE	AKASSERT( _MM_GET_FLUSH_ZERO_MODE(dummy) == _MM_FLUSH_ZERO_ON )
#else
	#define AKSIMD_ASSERTFLUSHZEROMODE	AKASSERT( _MM_GET_FLUSH_ZERO_MODE() == _MM_FLUSH_ZERO_ON )
#endif

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD integer arithmetic
//@{

/// Adds the four integer values of a and b
#define AKSIMD_ADD_V4I32( a, b ) _mm_add_epi32( a, b )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD packing / unpacking
//@{

/// Selects and interleaves the lower two single-precision, floating-point
/// values from a and b (see _mm_unpacklo_ps)
#define AKSIMD_UNPACKLO_V4F32( a, b ) _mm_unpacklo_ps( a, b )

/// Selects and interleaves the upper two single-precision, floating-point
/// values from a and b (see _mm_unpackhi_ps)
#define AKSIMD_UNPACKHI_V4F32( a, b ) _mm_unpackhi_ps( a, b )

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD vector comparison
/// Apart from AKSIMD_SEL_GTEQ_V4F32, these implementations are limited to a few platforms. 
//@{

#define AKSIMD_CMP_CTRLMASK __m128

/// Vector "<=" operation (see _mm_cmple_ps)
#define AKSIMD_LTEQ_V4F32( __a__, __b__ ) _mm_cmple_ps( (__a__), (__b__) )

/// Vector ">=" operation (see _mm_cmple_ps)
#define AKSIMD_GTEQ_V4F32( __a__, __b__ ) _mm_cmpge_ps( (__a__), (__b__) )

/// Vector "==" operation (see _mm_cmpeq_ps)
#define AKSIMD_EQ_V4F32( __a__, __b__ ) _mm_cmpeq_ps( (__a__), (__b__) )

/// Return a when control mask is 0, return b when control mask is non zero, control mask is in c and usually provided by above comparison operations
static AkForceInline AKSIMD_V4F32 AKSIMD_VSEL_V4F32( AKSIMD_V4F32 vA, AKSIMD_V4F32 vB, AKSIMD_V4F32 vMask )
{
    vB = _mm_and_ps( vB, vMask );
    vA= _mm_andnot_ps( vMask, vA );
    return _mm_or_ps( vA, vB );
}

// (cond1 >= cond2) ? b : a.
#define AKSIMD_SEL_GTEQ_V4F32( __a__, __b__, __cond1__, __cond2__ ) AKSIMD_VSEL_V4F32( __a__, __b__, AKSIMD_GTEQ_V4F32( __cond1__, __cond2__ ) )

// a >= 0 ? b : c ... Written, like, you know, the normal C++ operator syntax.
#define AKSIMD_SEL_GTEZ_V4F32( __a__, __b__, __c__ ) AKSIMD_VSEL_V4F32( (__c__), (__b__), AKSIMD_GTEQ_V4F32( __a__, _mm_set1_ps(0) ) )

#define AKSIMD_SPLAT_V4F32(var, idx) AKSIMD_SHUFFLE_V4F32(var,var, AKSIMD_SHUFFLE(idx,idx,idx,idx))

//@}
////////////////////////////////////////////////////////////////////////

#include <emmintrin.h>

typedef __m128i	AKSIMD_V4I32;	///< Vector of 4 32-bit signed integers

typedef AKSIMD_V4I32 AKSIMD_V4ICOND;

/// Loads unaligned 128-bit value (see _mm_loadu_si128)
#define AKSIMD_LOADU_V4I32( __addr__ ) _mm_loadu_si128( (__addr__) )

/// Loads aligned 128-bit value (see _mm_loadu_si128)
#define AKSIMD_LOAD_V4I32( __addr__ ) _mm_load_si128( (__addr__) )

/// Sets the four 32-bit integer values to zero (see _mm_setzero_si128)
#define AKSIMD_SETZERO_V4I32() _mm_setzero_si128()

#define AKSIMD_SET_V4I32( __scalar__ ) _mm_set1_epi32( (__scalar__) )

#define AKSIMD_SETV_V4I32( _d, _c, _b, _a ) _mm_set_epi32( (_d), (_c), (_b), (_a) )

/// Stores four 32-bit integer values. 
#define AKSIMD_STORE_V4I32( __addr__, __vec__ ) _mm_store_si128( (__addr__), (__vec__) )

/// Stores four 32-bit integer values. The address
/// does not need to be 16-byte aligned (see _mm_storeu_si128).
#define AKSIMD_STOREU_V4I32( __addr__, __vec__ ) _mm_storeu_si128( (__addr__), (__vec__) )

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD conversion
//@{

/// Converts the four signed 32-bit integer values of a to single-precision,
/// floating-point values (see _mm_cvtepi32_ps)
#define AKSIMD_CONVERT_V4I32_TO_V4F32( __vec__ ) _mm_cvtepi32_ps( (__vec__) )

/// Converts the four single-precision, floating-point values of a to signed
/// 32-bit integer values by rounding (see _mm_cvtps_epi32)
#define AKSIMD_CONVERT_V4F32_TO_V4I32( __vec__ ) _mm_cvtps_epi32( (__vec__) )

/// Converts the four single-precision, floating-point values of a to signed
/// 32-bit integer values by truncating (see _mm_cvttps_epi32)
#define AKSIMD_TRUNCATE_V4F32_TO_V4I32( __vec__ ) _mm_cvttps_epi32( (__vec__) )

/// Computes the bitwise AND of the 128-bit value in a and the
/// 128-bit value in b (see _mm_and_si128)
#define AKSIMD_AND_V4I32( __a__, __b__ ) _mm_and_si128( (__a__), (__b__) )

/// Compares the 8 signed 16-bit integers in a and the 8 signed
/// 16-bit integers in b for greater than (see _mm_cmpgt_epi16)
#define AKSIMD_CMPGT_V8I16( __a__, __b__ ) _mm_cmpgt_epi16( (__a__), (__b__) )

//@}
////////////////////////////////////////////////////////////////////////

/// Interleaves the lower 4 signed or unsigned 16-bit integers in a with
/// the lower 4 signed or unsigned 16-bit integers in b (see _mm_unpacklo_epi16)
#define AKSIMD_UNPACKLO_VECTOR8I16( a, b ) _mm_unpacklo_epi16( a, b )

/// Interleaves the upper 4 signed or unsigned 16-bit integers in a with
/// the upper 4 signed or unsigned 16-bit integers in b (see _mm_unpackhi_epi16)
#define AKSIMD_UNPACKHI_VECTOR8I16( a, b ) _mm_unpackhi_epi16( a, b )

/// Packs the 8 signed 32-bit integers from a and b into signed 16-bit
/// integers and saturates (see _mm_packs_epi32)
#define AKSIMD_PACKS_V4I32( a, b ) _mm_packs_epi32( a, b )

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD shifting
//@{

/// Shifts the 4 signed or unsigned 32-bit integers in a left by
/// in_shiftBy bits while shifting in zeros (see _mm_slli_epi32)
#define AKSIMD_SHIFTLEFT_V4I32( __vec__, __shiftBy__ ) \
	_mm_slli_epi32( (__vec__), (__shiftBy__) )

/// Shifts the 4 signed 32-bit integers in a right by in_shiftBy
/// bits while shifting in the sign bit (see _mm_srai_epi32)
#define AKSIMD_SHIFTRIGHTARITH_V4I32( __vec__, __shiftBy__ ) \
	_mm_srai_epi32( (__vec__), (__shiftBy__) )

//@}
////////////////////////////////////////////////////////////////////////

#if defined( AK_CPU_X86 ) && !defined(AK_IOS)	/// MMX

typedef __m64	AKSIMD_V2F32;	///< Vector of 2 32-bit floats

#endif


#endif //_AK_SIMD_SSE_H_
