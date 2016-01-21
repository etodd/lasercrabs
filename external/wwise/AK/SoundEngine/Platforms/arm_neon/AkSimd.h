//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// AKSIMD - arm_neon implementation

#ifndef _AKSIMD_ARM_NEON_H_
#define _AKSIMD_ARM_NEON_H_

#include <arm_neon.h>
#include <AK/SoundEngine/Common/AkTypes.h>

// Platform specific defines for prefetching

/*
// ??????
#define AKSIMD_ARCHCACHELINESIZE	(64)				///< Assumed cache line width for architectures on this platform
// ??????
#define AKSIMD_ARCHMAXPREFETCHSIZE	(512) 				///< Use this to control how much prefetching maximum is desirable (assuming 8-way cache)		
/// Cross-platform memory prefetch of effective address assuming non-temporal data
// ??????
#define AKSIMD_PREFETCHMEMORY( __offset__, __add__ ) _mm_prefetch(((char *)(__add__))+(__offset__), _MM_HINT_NTA ) 
*/

////////////////////////////////////////////////////////////////////////
/// @name Platform specific memory size alignment for allocation purposes
//@{

#define AKSIMD_ALIGNSIZE( __Size__ ) (((__Size__) + 15) & ~15)

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD types
//@{

typedef int32x4_t		AKSIMD_V4I32;		///< Vector of 4 32-bit signed integers
typedef int16x8_t		AKSIMD_V8I16;		///< Vector of 8 16-bit signed integers
typedef int16x4_t		AKSIMD_V4I16;		///< Vector of 4 16-bit signed integers
typedef uint32x4_t		AKSIMD_V4UI32;		///< Vector of 4 32-bit unsigned signed integers
typedef uint32x2_t		AKSIMD_V2UI32;		///< Vector of 2 32-bit unsigned signed integers
typedef int32x2_t		AKSIMD_V2I32;		///< Vector of 2 32-bit signed integers
typedef float32_t		AKSIMD_F32;			///< 32-bit float
typedef float32x2_t		AKSIMD_V2F32;		///< Vector of 2 32-bit floats
typedef float32x4_t		AKSIMD_V4F32;		///< Vector of 4 32-bit floats

typedef uint32x4_t		AKSIMD_V4COND;		///< Vector of 4 comparison results
typedef uint32x4_t		AKSIMD_V4ICOND;		///< Vector of 4 comparison results
typedef uint32x4_t		AKSIMD_V4FCOND;		///< Vector of 4 comparison results

#if defined(AK_CPU_ARM_NEON)
typedef float32x2x2_t	AKSIMD_V2F32X2;
typedef float32x4x2_t	AKSIMD_V4F32X2;
typedef float32x4x4_t	AKSIMD_V4F32X4;
#endif

#define AKSIMD_V4F32_SUPPORTED


//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD loading / setting
//@{

/// Loads four single-precision, floating-point values (see _mm_load_ps)
#define AKSIMD_LOAD_V4F32( __addr__ ) vld1q_f32( (float32_t*)(__addr__) )

/// Loads four single-precision floating-point values from unaligned
/// memory (see _mm_loadu_ps)
#define AKSIMD_LOADU_V4F32( __addr__ ) vld1q_f32( (float32_t*)(__addr__) )

/// Loads a single single-precision, floating-point value, copying it into
/// all four words (see _mm_load1_ps, _mm_load_ps1)
#define AKSIMD_LOAD1_V4F32( __scalar__ ) vld1q_dup_f32( (float32_t*)(&(__scalar__)) )

/// Sets the four single-precision, floating-point values to __scalar__ (see
/// _mm_set1_ps, _mm_set_ps1)
#define AKSIMD_SET_V4F32( __scalar__ ) vdupq_n_f32( __scalar__ )

/// Sets the four integer values to __scalar__
#define AKSIMD_SET_V4I32( __scalar__ ) vdupq_n_s32( __scalar__ )

/// Sets the four single-precision, floating-point values to zero (see
/// _mm_setzero_ps)
#define AKSIMD_SETZERO_V4F32() AKSIMD_SET_V4F32( 0 )

/// Loads a single-precision, floating-point value into the low word
/// and clears the upper three words.
/// r0 := *p; r1 := 0.0 ; r2 := 0.0 ; r3 := 0.0 (see _mm_load_ss)
#define AKSIMD_LOAD_SS_V4F32( __addr__ ) vld1q_lane_f32( (float32_t*)(__addr__), AKSIMD_SETZERO_V4F32(), 0 );

/// Loads four 32-bit signed integer values (aligned)
#define AKSIMD_LOAD_V4I32( __addr__ ) vld1q_s32( (const int32_t*)(__addr__) )

/// Loads 8 16-bit signed integer values (aligned)
#define AKSIMD_LOAD_V8I16( __addr__ ) vld1q_s16( (const int16_t*)(__addr__) )

/// Loads 4 16-bit signed integer values (aligned)
#define AKSIMD_LOAD_V4I16( __addr__ ) vld1_s16( (const int16_t*)(__addr__) )

/// Loads unaligned 128-bit value (see _mm_loadu_si128)
#if defined(AK_VITA) 
// Due to a compiler bug in sony sdk 1.8 and 2.0, this workaround is required. Removed when fixed.
#define AKSIMD_LOADU_V4I32( __addr__ ) *__addr__ 
#else
#define AKSIMD_LOADU_V4I32( __addr__ ) vld1q_s32( (const int32_t*)(__addr__))
#endif
/// Sets the four 32-bit integer values to zero (see _mm_setzero_si128)
#define AKSIMD_SETZERO_V4I32() vdupq_n_s32( 0 )

/// Loads two single-precision, floating-point values
#define AKSIMD_LOAD_V2F32( __addr__ ) vld1_f32( (float32_t*)(__addr__) )
#define AKSIMD_LOAD_V2F32_LANE( __addr__, __vec__, __lane__ ) vld1_lane_f32( (float32_t*)(__addr__), (__vec__), (__lane__) );

/// Sets the two single-precision, floating-point values to __scalar__
#define AKSIMD_SET_V2F32( __scalar__ ) vdup_n_f32( __scalar__ )

/// Sets the two single-precision, floating-point values to zero
#define AKSIMD_SETZERO_V2F32() AKSIMD_SET_V2F32( 0 )

/// Loads data from memory and de-interleaves
#define AKSIMD_LOAD_V4F32X2( __addr__ ) vld2q_f32( (float32_t*)(__addr__) )
#define AKSIMD_LOAD_V2F32X2( __addr__ ) vld2_f32( (float32_t*)(__addr__) )

/// Loads data from memory and de-interleaves; only selected lane
#define AKSIMD_LOAD_V2F32X2_LANE( __addr__, __vec__, __lane__ ) vld2_lane_f32( (float32_t*)(__addr__), (__vec__), (__lane__) );
#define AKSIMD_LOAD_V4F32X4_LANE( __addr__, __vec__, __lane__ ) vld4q_lane_f32( (float32_t*)(__addr__), (__vec__), (__lane__) );

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD storing
//@{

/// Stores four single-precision, floating-point values. The address must be 16-byte aligned
#define AKSIMD_STORE_V4F32( __addr__, __vName__ ) vst1q_f32( (float32_t*)(__addr__), (__vName__) )

/// Stores four single-precision, floating-point values. The address does not need to be 16-byte aligned.
#define AKSIMD_STOREU_V4F32( __addr__, __vec__ ) vst1q_f32( (float32_t*)(__addr__), (__vec__) )

/// Stores the lower single-precision, floating-point value.
/// *p := a0 (see _mm_store_ss)
#define AKSIMD_STORE1_V4F32( __addr__, __vec__ ) vst1q_lane_f32( (float32_t*)(__addr__), (__vec__), 0 )

/// Stores four 32-bit integer values. The address must be 16-byte aligned.
#define AKSIMD_STORE_V4I32( __addr__, __vec__ ) vst1q_s32( (int32_t*)(__addr__), (__vec__) )

/// Stores four 32-bit integer values. The address does not need to be 16-byte aligned.
#define AKSIMD_STOREU_V4I32( __addr__, __vec__ ) vst1q_s32( (int32_t*)(__addr__), (__vec__) )

/// Stores four 32-bit unsigned integer values. The address does not need to be 16-byte aligned.
#define AKSIMD_STOREU_V4UI32( __addr__, __vec__ ) vst1q_u32( (uint32_t*)(__addr__), (__vec__) )

/// Stores two single-precision, floating-point values. The address must be 16-byte aligned.
#define AKSIMD_STORE_V2F32( __addr__, __vName__ ) vst1_f32( (AkReal32*)(__addr__), (__vName__) )

/// Stores data by interleaving into memory
#define AKSIMD_STORE_V4F32X2( __addr__, __vName__ ) vst2q_f32( (float32_t*)(__addr__), (__vName__) )
#define AKSIMD_STORE_V2F32X2( __addr__, __vName__ ) vst2_f32( (float32_t*)(__addr__), (__vName__) )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD conversion
//@{

/// Converts the four signed 32-bit integer values of a to single-precision,
/// floating-point values (see _mm_cvtepi32_ps)
#define AKSIMD_CONVERT_V4I32_TO_V4F32( __vec__ ) vcvtq_f32_s32( __vec__ )

/// Converts the four single-precision, floating-point values of a to signed
/// 32-bit integer values (see _mm_cvtps_epi32)
#define AKSIMD_CONVERT_V4F32_TO_V4I32( __vec__ ) vcvtq_s32_f32( __vec__ )

/// Converts the four single-precision, floating-point values of a to signed
/// 32-bit integer values by truncating (see _mm_cvttps_epi32)
#define AKSIMD_TRUNCATE_V4F32_TO_V4I32( __vec__ ) vcvtq_s32_f32( (__vec__) )

/// Converts the two single-precision, floating-point values of a to signed
/// 32-bit integer values
#define AKSIMD_CONVERT_V2F32_TO_V2I32( __vec__ ) vcvt_s32_f32( __vec__ )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD logical operations
//@{

/// Computes the bitwise AND of the 128-bit value in a and the
/// 128-bit value in b (see _mm_and_si128)
#define AKSIMD_AND_V4I32( __a__, __b__ ) vandq_s32( (__a__), (__b__) )

/// Compares the 8 signed 16-bit integers in a and the 8 signed
/// 16-bit integers in b for greater than (see _mm_cmpgt_epi16)
#define AKSIMD_CMPGT_V8I16( __a__, __b__ ) \
	vreinterpretq_s32_u16( vcgtq_s16( vreinterpretq_s16_s32(__a__), vreinterpretq_s16_s32(__b__) ) )

/// Compares for less than or equal (see _mm_cmple_ps)
#define AKSIMD_CMPLE_V4F32( __a__, __b__ ) vcleq_f32( (__a__), (__b__) )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD shifting
//@{

/// Shifts the 4 signed or unsigned 32-bit integers in a left by
/// in_shiftBy bits while shifting in zeros (see _mm_slli_epi32)
#define AKSIMD_SHIFTLEFT_V4I32( __vec__, __shiftBy__ ) \
	vshlq_n_s32( (__vec__), (__shiftBy__) )

/// Shifts the 4 signed 32-bit integers in a right by in_shiftBy
/// bits while shifting in the sign bit (see _mm_srai_epi32)
#define AKSIMD_SHIFTRIGHTARITH_V4I32( __vec__, __shiftBy__ ) \
	vrshrq_n_s32( (__vec__), (__shiftBy__) )

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD shuffling
//@{

// Macro for combining two vector of 2 elements into one vector of
// 4 elements.
#define AKSIMD_COMBINE_V2F32( a, b ) vcombine_f32( a, b )

// Macro for shuffle parameter for AKSIMD_SHUFFLE_V4F32() (see _MM_SHUFFLE)
#define AKSIMD_SHUFFLE( fp3, fp2, fp1, fp0 ) \
	(((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

/// Selects four specific single-precision, floating-point values from
/// a and b, based on the mask i (see _mm_shuffle_ps)
// Usage: AKSIMD_SHUFFLE_V4F32( vec1, vec2, AKSIMD_SHUFFLE( z, y, x, w ) )
// If you get a link error, it's probably because the required
// _AKSIMD_LOCAL::SHUFFLE_V4F32< zyxw > is not implemented in
// <AK/SoundEngine/Platforms/arm_neon/AkSimdShuffle.h>.
#define AKSIMD_SHUFFLE_V4F32( a, b, zyxw ) \
	_AKSIMD_LOCAL::SHUFFLE_V4F32< zyxw >( a, b )

// Various combinations of zyxw for _AKSIMD_LOCAL::SHUFFLE_V4F32< zyxw > are
// implemented in a separate header file to keep this one cleaner:
#include <AK/SoundEngine/Platforms/arm_neon/AkSimdShuffle.h>

/// Moves the upper two single-precision, floating-point values of b to
/// the lower two single-precision, floating-point values of the result.
/// The upper two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := a3; r2 := a2; r1 := b3; r0 := b2 (see _mm_movehl_ps)
inline AKSIMD_V4F32 AKSIMD_MOVEHL_V4F32( const AKSIMD_V4F32 abcd, const AKSIMD_V4F32 xyzw ) 
{
		//return akshuffle_zwcd( xyzw, abcd );
		AKSIMD_V2F32 zw = vget_high_f32( xyzw );
		AKSIMD_V2F32 cd = vget_high_f32( abcd );
		AKSIMD_V4F32 zwcd = vcombine_f32( zw , cd );
		return zwcd;
}

/// Moves the lower two single-precision, floating-point values of b to
/// the upper two single-precision, floating-point values of the result.
/// The lower two single-precision, floating-point values of a are passed
/// through to the result.
/// r3 := b1 ; r2 := b0 ; r1 := a1 ; r0 := a0 (see _mm_movelh_ps)
inline AKSIMD_V4F32 AKSIMD_MOVELH_V4F32( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
{
	return vcombine_f32( vget_low_f32( xyzw ) , vget_low_f32( abcd ) );
}

/// Swap the 2 lower floats together and the 2 higher floats together.	
//#define AKSIMD_SHUFFLE_BADC( __a__ ) AKSIMD_SHUFFLE_V4F32( (__a__), (__a__), AKSIMD_SHUFFLE(2,3,0,1))
#define AKSIMD_SHUFFLE_BADC( __a__ ) vrev64q_f32( __a__ )

/// Swap the 2 lower floats with the 2 higher floats.	
//#define AKSIMD_SHUFFLE_CDAB( __a__ ) AKSIMD_SHUFFLE_V4F32( (__a__), (__a__), AKSIMD_SHUFFLE(1,0,3,2))
#define AKSIMD_SHUFFLE_CDAB( __a__ ) vcombine_f32( vget_high_f32(__a__), vget_low_f32(__a__) )

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
#define AKSIMD_SUB_V4F32( __a__, __b__ ) vsubq_f32( (__a__), (__b__) )

/// Subtracts the two single-precision, floating-point values of
/// a and b
#define AKSIMD_SUB_V2F32( __a__, __b__ ) vsub_f32( (__a__), (__b__) )

/// Subtracts the lower single-precision, floating-point values of a and b.
/// The upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 - b0 ; r1 := a1 ; r2 := a2 ; r3 := a3 (see _mm_sub_ss)
#define AKSIMD_SUB_SS_V4F32( __a__, __b__ ) \
	vsubq_f32( (__a__), vsetq_lane_f32( AKSIMD_GETELEMENT_V4F32( (__b__), 0 ), AKSIMD_SETZERO_V4F32(), 0 ) );

/// Adds the four single-precision, floating-point values of
/// a and b (see _mm_add_ps)
#define AKSIMD_ADD_V4F32( __a__, __b__ ) vaddq_f32( (__a__), (__b__) )

/// Adds the two single-precision, floating-point values of
/// a and b
#define AKSIMD_ADD_V2F32( __a__, __b__ ) vadd_f32( (__a__), (__b__) )

/// Adds the four integers of a and b
#define AKSIMD_ADD_V4I32( __a__, __b__ ) vaddq_s32( (__a__), (__b__) )

/// Compare the content of four single-precision, floating-point values of
/// a and b
#define AKSIMD_COMP_V4F32( __a__, __b__ ) vceqq_f32( (__a__), (__b__) )

/// Compare the content of two single-precision, floating-point values of
/// a and b
#define AKSIMD_COMP_V2F32( __a__, __b__ ) vceq_f32( (__a__), (__b__) )

/// Adds the lower single-precision, floating-point values of a and b; the
/// upper three single-precision, floating-point values are passed through from a.
/// r0 := a0 + b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
#define AKSIMD_ADD_SS_V4F32( __a__, __b__ ) \
	vaddq_f32( (__a__), vsetq_lane_f32( AKSIMD_GETELEMENT_V4F32( (__b__), 0 ), AKSIMD_SETZERO_V4F32(), 0 ) )

/// Multiplies the four single-precision, floating-point values
/// of a and b (see _mm_mul_ps)
#define AKSIMD_MUL_V4F32( __a__, __b__ ) vmulq_f32( (__a__), (__b__) )

/// Multiplies the four single-precision, floating-point values of a
/// by the single-precision, floating-point scalar b
#define AKSIMD_MUL_V4F32_SCALAR( __a__, __b__ ) vmulq_n_f32( (__a__), (__b__) )

/// Rough estimation of division
AkForceInline AKSIMD_V4F32 AKSIMD_DIV_V4F32( AKSIMD_V4F32 a, AKSIMD_V4F32 b ) 
{
	AKSIMD_V4F32 inv = vrecpeq_f32(b);
	AKSIMD_V4F32 restep = vrecpsq_f32(b, inv);
	inv = vmulq_f32(restep, inv);
	return vmulq_f32(a, inv);
}

/// Multiplies the two single-precision, floating-point values
/// of a and b
#define AKSIMD_MUL_V2F32( __a__, __b__ ) vmul_f32( (__a__), (__b__) )

/// Multiplies the two single-precision, floating-point values of a
/// by the single-precision, floating-point scalar b
#define AKSIMD_MUL_V2F32_SCALAR( __a__, __b__ ) vmul_n_f32( (__a__), (__b__) )

/// Multiplies the lower single-precision, floating-point values of
/// a and b; the upper three single-precision, floating-point values
/// are passed through from a.
/// r0 := a0 * b0; r1 := a1; r2 := a2; r3 := a3 (see _mm_add_ss)
#define AKSIMD_MUL_SS_V4F32( __a__, __b__ ) \
	vmulq_f32( (__a__), vsetq_lane_f32( AKSIMD_GETELEMENT_V4F32( (__b__), 0 ), AKSIMD_SETZERO_V4F32(), 0 ) )

/// Vector multiply-add operation.
#define AKSIMD_MADD_V4F32( __a__, __b__, __c__ ) \
	AKSIMD_ADD_V4F32( AKSIMD_MUL_V4F32( (__a__), (__b__) ), (__c__) )

#define AKSIMD_MSUB_V4F32( __a__, __b__, __c__ ) \
	AKSIMD_SUB_V4F32( AKSIMD_MUL_V4F32( (__a__), (__b__) ), (__c__) )

#define AKSIMD_MADD_V2F32( __a__, __b__, __c__ ) \
	AKSIMD_ADD_V2F32( AKSIMD_MUL_V2F32( (__a__), (__b__) ), (__c__) )

#define AKSIMD_MSUB_V2F32( __a__, __b__, __c__ ) \
	AKSIMD_SUB_V2F32( AKSIMD_MUL_V2F32( (__a__), (__b__) ), (__c__) )

#define AKSIMD_MADD_V4F32_INST( __a__, __b__, __c__ ) vmlaq_f32( (__c__), (__a__), (__b__) )
#define AKSIMD_MADD_V2F32_INST( __a__, __b__, __c__ ) vmla_f32( (__c__), (__a__), (__b__) )
//#define AKSIMD_MSUB_V4F32( __a__, __b__, __c__ ) vmlsq_f32( (__c__), (__a__), (__b__) )
//#define AKSIMD_MSUB_V2F32( __a__, __b__, __c__ ) vmls_f32( (__c__), (__a__), (__b__) )

#define AKSIMD_MADD_V4F32_SCALAR( __a__, __b__, __c__ ) vmlaq_n_f32( (__c__), (__a__), (__b__) )
#define AKSIMD_MADD_V2F32_SCALAR( __a__, __b__, __c__ ) vmla_n_f32( (__c__), (__a__), (__b__) )

/// Vector multiply-add operation.
AkForceInline AKSIMD_V4F32 AKSIMD_MADD_SS_V4F32( const AKSIMD_V4F32& __a__, const AKSIMD_V4F32& __b__, const AKSIMD_V4F32& __c__ )
{
	return AKSIMD_ADD_SS_V4F32( AKSIMD_MUL_SS_V4F32( __a__, __b__ ), __c__ );
}

/// Computes the minima of the four single-precision, floating-point
/// values of a and b (see _mm_min_ps)
#define AKSIMD_MIN_V4F32( __a__, __b__ ) vminq_f32( (__a__), (__b__) )

/// Computes the minima of the two single-precision, floating-point
/// values of a and b
#define AKSIMD_MIN_V2F32( __a__, __b__ ) vmin_f32( (__a__), (__b__) )

/// Computes the maximums of the four single-precision, floating-point
/// values of a and b (see _mm_max_ps)
#define AKSIMD_MAX_V4F32( __a__, __b__ ) vmaxq_f32( (__a__), (__b__) )

/// Computes the maximums of the two single-precision, floating-point
/// values of a and b
#define AKSIMD_MAX_V2F32( __a__, __b__ ) vmax_f32( (__a__), (__b__) )

/// Returns absolute value
#define AKSIMD_ABS_V4F32( __a__ ) vabsq_f32((__a__))

/// Changes the sign
#define AKSIMD_NEG_V2F32( __a__ ) vneg_f32( (__a__) )
#define AKSIMD_NEG_V4F32( __a__ ) vnegq_f32( (__a__) )

/// Square root (4 floats)
#define AKSIMD_SQRT_V4F32( __vec__ ) vrecpeq_f32( vrsqrteq_f32( __vec__ ) )

/// Square root (2 floats)
#define AKSIMD_SQRT_V2F32( __vec__ ) vrecpe_f32( vrsqrte_f32( __vec__ ) )

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

#if defined(AK_IOS) || defined(AK_VITA)

// V2 implementation (faster 'cause ARM processors actually have an x2 pipeline)

static AkForceInline AKSIMD_V4F32 AKSIMD_COMPLEXMUL( AKSIMD_V4F32 vCIn1, AKSIMD_V4F32 vCIn2 )
{
	static const AKSIMD_V2F32 vSign = { -1.f, 1.f }; 

	AKSIMD_V2F32 vCIn1a = vget_low_f32( vCIn1 );
	AKSIMD_V2F32 vCIn2a = vget_low_f32( vCIn2 );
	AKSIMD_V2F32 vTmpa0 = vmul_n_f32( vCIn2a, vCIn1a[0] );
	AKSIMD_V2F32 vTmpa1 = vmul_n_f32( vCIn2a, vCIn1a[1] );
	vTmpa1 = vrev64_f32( vTmpa1 );
	vTmpa1 = vmul_f32( vTmpa1, vSign );
	vTmpa0 = vadd_f32( vTmpa0, vTmpa1 );

	AKSIMD_V2F32 vCIn1b = vget_high_f32( vCIn1 );
	AKSIMD_V2F32 vCIn2b = vget_high_f32( vCIn2 );
	AKSIMD_V2F32 vTmpb0 = vmul_n_f32( vCIn2b, vCIn1b[0] );
	AKSIMD_V2F32 vTmpb1 = vmul_n_f32( vCIn2b, vCIn1b[1] );
	vTmpb1 = vrev64_f32( vTmpb1 );
	vTmpb1 = vmul_f32( vTmpb1, vSign );
	vTmpb0 = vadd_f32( vTmpb0, vTmpb1 );

	return vcombine_f32( vTmpa0, vTmpb0 );
}

#else

// V4 implementation (kept in case future ARM processors actually have an x4 pipeline)

static AkForceInline AKSIMD_V4F32 AKSIMD_COMPLEXMUL( AKSIMD_V4F32 vCIn1, AKSIMD_V4F32 vCIn2 )
{
#ifdef AKSIMD_DECLARE_V4F32
	static const AKSIMD_DECLARE_V4F32( vSign, 1.f, -1.f, 1.f, -1.f ); 
#else
	static const AKSIMD_V4F32 vSign = { 1.f, -1.f, 1.f, -1.f }; 
#endif

	AKSIMD_V4F32 vTmp1 = AKSIMD_SHUFFLE_V4F32( vCIn1, vCIn1, AKSIMD_SHUFFLE(2,2,0,0)); 
	vTmp1 = AKSIMD_MUL_V4F32( vTmp1, vCIn2 );
	AKSIMD_V4F32 vTmp2 = AKSIMD_SHUFFLE_V4F32( vCIn1, vCIn1, AKSIMD_SHUFFLE(3,3,1,1)); 
	vTmp2 = AKSIMD_MUL_V4F32( vTmp2, vSign );
	vTmp2 = AKSIMD_MUL_V4F32( vTmp2, vCIn2 );
	vTmp2 = AKSIMD_SHUFFLE_BADC( vTmp2 ); 
	vTmp2 = AKSIMD_ADD_V4F32( vTmp2, vTmp1 );
	return vTmp2;
}

#endif

//@}
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
/// @name AKSIMD packing / unpacking
//@{

/// Interleaves the lower 4 signed or unsigned 16-bit integers in a with
/// the lower 4 signed or unsigned 16-bit integers in b (see _mm_unpacklo_epi16)
#define AKSIMD_UNPACKLO_VECTOR8I16( __a__, __b__ ) vreinterpretq_s32_s16( vzipq_s16( vreinterpretq_s16_s32(__a__), vreinterpretq_s16_s32(__b__) ).val[0] )

/// Interleaves the upper 4 signed or unsigned 16-bit integers in a with
/// the upper 4 signed or unsigned 16-bit integers in b (see _mm_unpackhi_epi16)
#define AKSIMD_UNPACKHI_VECTOR8I16( __a__, __b__ ) vreinterpretq_s32_s16( vzipq_s16( vreinterpretq_s16_s32(__a__), vreinterpretq_s16_s32(__b__) ).val[1] )

/// Selects and interleaves the lower two single-precision, floating-point
/// values from a and b (see _mm_unpacklo_ps)
AkForceInline AKSIMD_V4F32 AKSIMD_UNPACKLO_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	// sce_vectormath_xayb(in_vec1, in_vec2)
	float32x2_t xy = vget_low_f32( in_vec1 /*xyzw*/ );
	float32x2_t ab = vget_low_f32( in_vec2 /*abcd*/ );
	float32x2x2_t xa_yb = vtrn_f32( xy, ab );
	AKSIMD_V4F32 xayb = vcombine_f32( xa_yb.val[0], xa_yb.val[1] );
	return xayb;
}

/// Selects and interleaves the upper two single-precision, floating-point
/// values from a and b (see _mm_unpackhi_ps)
AkForceInline AKSIMD_V4F32 AKSIMD_UNPACKHI_V4F32( const AKSIMD_V4F32& in_vec1, const AKSIMD_V4F32& in_vec2 )
{
	//return sce_vectormath_zcwd( in_vec1, in_vec2 );
	float32x2_t zw = vget_high_f32( in_vec1 /*xyzw*/ );
	float32x2_t cd = vget_high_f32( in_vec2 /*abcd*/ );
	float32x2x2_t zc_wd = vtrn_f32( zw, cd );
	AKSIMD_V4F32 zcwd = vcombine_f32( zc_wd.val[0], zc_wd.val[1] );
	return zcwd;
}

/// Packs the 8 signed 32-bit integers from a and b into signed 16-bit
/// integers and saturates (see _mm_packs_epi32)
AkForceInline AKSIMD_V4I32 AKSIMD_PACKS_V4I32( const AKSIMD_V4I32& in_vec1, const AKSIMD_V4I32& in_vec2 )
{
	int16x4_t	vec1_16 = vqmovn_s32( in_vec1 );
	int16x4_t	vec2_16 = vqmovn_s32( in_vec2 );
	int16x8_t result = vcombine_s16( vec1_16, vec2_16 );
	return vreinterpretq_s32_s16( result );
}

/// V1 = {a,b}   =>   VR = {b,c}
/// V2 = {c,d}   =>
#define AKSIMD_HILO_V2F32( in_vec1, in_vec2 ) vreinterpret_f32_u32( vext_u32( vreinterpret_u32_f32( in_vec1 ), vreinterpret_u32_f32( in_vec2 ), 1 ) )

/// V1 = {a,b}   =>   V1 = {a,c}
/// V2 = {c,d}   =>   V2 = {b,d}
#define AKSIMD_TRANSPOSE_V2F32( in_vec1, in_vec2 ) vtrn_f32( in_vec1, in_vec2 )

#define AKSIMD_TRANSPOSE_V4F32( in_vec1, in_vec2 ) vtrnq_f32( in_vec1, in_vec2 )

/// V1 = {a,b}   =>   VR = {b,a}
#define AKSIMD_SWAP_V2F32( in_vec ) vrev64_f32( in_vec )

//@}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
/// @name AKSIMD vector comparison
/// Apart from AKSIMD_SEL_GTEQ_V4F32, these implementations are limited to a few platforms. 
//@{

#define AKSIMD_CMP_CTRLMASK uint32x4_t

/// Compare each float element and return control mask.
#define AKSIMD_GTEQ_V4F32( __a__, __b__ ) vcgeq_f32( (__a__), (__b__))

/// Compare each integer element and return control mask.
#define AKSIMD_GTEQ_V4I32( __a__, __b__ ) vcgeq_s32( (__a__), (__b__))

/// Compare each float element and return control mask.
#define AKSIMD_EQ_V4F32( __a__, __b__ ) vceqq_f32( (__a__), (__b__))

/// Compare each integer element and return control mask.
#define AKSIMD_EQ_V4I32( __a__, __b__ ) vceqq_s32( (__a__), (__b__))

/// Return a when control mask is 0, return b when control mask is non zero, control mask is in c and usually provided by above comparison operations
#define AKSIMD_VSEL_V4F32( __a__, __b__, __c__ ) vbslq_f32( (__c__), (__b__), (__a__) )

// (cond1 >= cond2) ? b : a.
#define AKSIMD_SEL_GTEQ_V4F32( __a__, __b__, __cond1__, __cond2__ ) AKSIMD_VSEL_V4F32( __a__, __b__, AKSIMD_GTEQ_V4F32( __cond1__, __cond2__ ) )

// a >= 0 ? b : c ... Written, like, you know, the normal C++ operator syntax.
#define AKSIMD_SEL_GTEZ_V4F32( __a__, __b__, __c__ ) AKSIMD_VSEL_V4F32( (__c__), (__b__), AKSIMD_GTEQ_V4F32( __a__, AKSIMD_SETZERO_V4F32() ) )

#define AKSIMD_SPLAT_V4F32(var, idx) vmovq_n_f32(vgetq_lane_f32(var, idx))

//@}
////////////////////////////////////////////////////////////////////////

#endif //_AKSIMD_ARM_NEON_H_

