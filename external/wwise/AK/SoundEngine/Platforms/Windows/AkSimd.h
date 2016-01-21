//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// AKSIMD - Windows implementation

#pragma once

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
	#define AKSIMD_GETELEMENT_V4I32( __vName, __num__ )			(__vName).m128i_i32[(__num__)]	///< Retrieve scalar element from vector.
	#define AKSIMD_GETELEMENT_V4F32( __vName, __num__ )			(__vName).m128_f32[(__num__)]	///< Retrieve scalar element from vector.
	#define AKSIMD_GETELEMENT_V2F32( __vName, __num__ )			(__vName).m64_f32[(__num__)]	///< Retrieve scalar element from vector.
	
	#include <AK/SoundEngine/Platforms/SSE/AkSimd.h>
#endif

#if defined AK_CPU_ARM_NEON
	#define AKSIMD_GETELEMENT_V4I32( __vName, __num__ )			(__vName).n128_i32[(__num__)]	///< Retrieve scalar element from vector.
	#define AKSIMD_GETELEMENT_V4F32( __vName, __num__ )			(__vName).n128_f32[(__num__)]	///< Retrieve scalar element from vector.
	#define AKSIMD_GETELEMENT_V2F32( __vName, __num__ )			(__vName).n64_f32[(__num__)]	///< Retrieve scalar element from vector.
	
	#define AKSIMD_DECLARE_V4F32_TYPE							DirectX::XMVECTORF32
	#define AKSIMD_DECLARE_V4F32( _x, _a, _b, _c, _d )			AKSIMD_DECLARE_V4F32_TYPE _x = { _a, _b, _c, _d }

	#define AKSIMD_DECLARE_V4I32_TYPE							DirectX::XMVECTORI32
	#define AKSIMD_DECLARE_V4I32( _x, _a, _b, _c, _d )			AKSIMD_DECLARE_V4I32_TYPE _x = { _a, _b, _c, _d }

	#include <DirectXMath.h>
	#include <AK/SoundEngine/Platforms/arm_neon/AkSimd.h>
#endif
