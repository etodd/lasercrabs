//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Floating point performance utilities.

#ifndef _AK_FP_UTILS_H_
#define _AK_FP_UTILS_H_

#include <AK/SoundEngine/Common/AkTypes.h>

// Note: In many case you can use AK_FPSetValXX instead of FSEL. This saves a subtraction on platforms that do not have FSEL instructions.
#if defined(__PPU__)
#include <ppu_intrinsics.h>
#define AK_FSEL( __a__, __b__, __c__ ) ( __fsels((__a__),(__b__),(__c__) ) )
#elif defined(AK_XBOX360)
#include "ppcintrinsics.h"
#define AK_FSEL( __a__, __b__, __c__ ) ( (AkReal32)__fsel((__a__),(__b__),(__c__) ) )
#else
#define AK_FSEL( __a__, __b__, __c__) (((__a__) >= 0) ? (__b__) : (__c__))
#endif

#if defined(AK_XBOX360) || defined (__PPU__) || defined(AK_WIIU_SOFTWARE)

/// Branchless (where available) version returning minimum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMin( AkReal32 fA, AkReal32 fB )
{   
	return AK_FSEL(fA-fB,fB,fA);
} 

/// Branchless (where available) version returning maximum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMax( AkReal32 fA, AkReal32 fB )
{   
	return AK_FSEL(fA-fB,fA,fB);
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than 2nd argument.
static AkForceInline void AK_FPSetValGT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	io_fVariableToSet = AK_FSEL( (in_fComparandB-in_fComparandA), io_fVariableToSet, in_fValueIfTrue );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than equal 2nd argument.
static AkForceInline void AK_FPSetValGTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	io_fVariableToSet = AK_FSEL( (in_fComparandA-in_fComparandB), in_fValueIfTrue, io_fVariableToSet );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than 2nd argument.
static AkForceInline void AK_FPSetValLT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	io_fVariableToSet = AK_FSEL( (in_fComparandA-in_fComparandB), io_fVariableToSet, in_fValueIfTrue );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than equal 2nd argument.
static AkForceInline void AK_FPSetValLTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	io_fVariableToSet = AK_FSEL( (in_fComparandB-in_fComparandA), in_fValueIfTrue, io_fVariableToSet );
}

#elif defined(__SPU__)

// Note: spu_insert() and spu_promote should not compile to actual instructions on the SPU where everything is vector types

/// Branchless (where available) version returning minimum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMin( AkReal32 fA, AkReal32 fB )
{   
	vec_float4 vA = spu_promote( fA, 0 );
	vec_float4 vB = spu_promote( fB, 0 );
	vec_float4 vSel = spu_sel(vA, vB, spu_cmpgt(vA, vB));
	return spu_extract( vSel, 0 );
} 

/// Branchless (where available) version returning maximum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMax( AkReal32 fA, AkReal32 fB )
{   
	vec_float4 vA = spu_promote( fA, 0 );
	vec_float4 vB = spu_promote( fB, 0 );
	vec_float4 vSel = spu_sel(vB, vA, spu_cmpgt(vA, vB));
	return spu_extract( vSel, 0 );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than 2nd argument.
static AkForceInline void AK_FPSetValGT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	vec_float4 vA = spu_promote( in_fComparandA, 0 );
	vec_float4 vB = spu_promote( in_fComparandB, 0 );
	vec_float4 vVTS = spu_promote( io_fVariableToSet, 0 );
	vec_float4 vVIT = spu_promote( in_fValueIfTrue, 0 );
	vVTS = spu_sel(vVTS, vVIT, spu_cmpgt(vA, vB));
	io_fVariableToSet = spu_extract( vVTS, 0 );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than equal 2nd argument.
/// Use Greater Than version instead if you can (more performant).
static AkForceInline void AK_FPSetValGTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	vec_float4 vA = spu_promote( in_fComparandA, 0 );
	vec_float4 vB = spu_promote( in_fComparandB, 0 );
	vec_float4 vVTS = spu_promote( io_fVariableToSet, 0 );
	vec_float4 vVIT = spu_promote( in_fValueIfTrue, 0 );
	vec_uint4 vCmp = spu_cmpgt(vA, vB);
	vCmp = spu_or( vCmp, spu_cmpeq( vA, vB ) );
	vVTS = spu_sel(vVTS, vVIT, vCmp);
	io_fVariableToSet = spu_extract( vVTS, 0 );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than 2nd argument.
/// Use Less Than EQUAL version instead if you can (more performant).
static AkForceInline void AK_FPSetValLT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	vec_float4 vA = spu_promote( in_fComparandA, 0 );
	vec_float4 vB = spu_promote( in_fComparandB, 0 );
	vec_float4 vVTS = spu_promote( io_fVariableToSet, 0 );
	vec_float4 vVIT = spu_promote( in_fValueIfTrue, 0 );
	vec_uint4 vCmp = spu_cmpgt(vA, vB);
	vCmp = spu_nor( vCmp, spu_cmpeq( vA, vB ) );
	vVTS = spu_sel(vVTS, vVIT, vCmp);
	io_fVariableToSet = spu_extract( vVTS, 0 );
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than equal 2nd argument.
static AkForceInline void AK_FPSetValLTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	vec_float4 vA = spu_promote( in_fComparandA, 0 );
	vec_float4 vB = spu_promote( in_fComparandB, 0 );
	vec_float4 vVTS = spu_promote( io_fVariableToSet, 0 );
	vec_float4 vVIT = spu_promote( in_fValueIfTrue, 0 );
	vec_uint4 vCtl = spu_cmpgt(vA, vB);
	vCtl = spu_nand(vCtl,vCtl);
	vVTS = spu_sel(vVTS, vVIT, vCtl);
	io_fVariableToSet = spu_extract( vVTS, 0 );
}

#else

/// Branchless (where available) version returning minimum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMin( AkReal32 fA, AkReal32 fB )
{   
	return (fA < fB ? fA : fB);
} 

/// Branchless (where available) version returning maximum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMax( AkReal32 fA, AkReal32 fB )
{   
	return (fA > fB ? fA : fB);
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than 2nd argument.
static AkForceInline void AK_FPSetValGT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA > in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than equal 2nd argument.
static AkForceInline void AK_FPSetValGTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA >= in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than 2nd argument.
static AkForceInline void AK_FPSetValLT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA < in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than equal 2nd argument.
static AkForceInline void AK_FPSetValLTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA <= in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

#endif

#endif //_AK_FP_UTILS_H_

