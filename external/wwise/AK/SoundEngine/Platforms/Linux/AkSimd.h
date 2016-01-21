//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// AkSimd - Linux implementation

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>

#if defined AK_CPU_ARM_NEON

	/// Retrieve scalar element from vector.
	#define AKSIMD_GETELEMENT_V4F32( __vName, __num__ )				(__vName)[(__num__)]
	#define AKSIMD_GETELEMENT_V2F32( __vName, __num__ )				(__vName)[(__num__)]
	#define AKSIMD_GETELEMENT_V4I32( __vName, __num__ )				(__vName)[(__num__)]

	#include <AK/SoundEngine/Platforms/arm_neon/AkSimd.h>

#elif defined AK_CPU_X86 || defined AK_CPU_X86_64
	
	#include <AK/SoundEngine/Platforms/SSE/AkSimd.h>

#else

	#include <AK/SoundEngine/Platforms/Generic/AkSimd.h>

#endif

#ifndef AKSIMD_GETELEMENT_V4F32
	/// Get the element at index __num__ in vector __vName
	#define AKSIMD_GETELEMENT_V4F32( __vName, __num__ )			((float*)&(__vName))[(__num__)]							///< Retrieve scalar element from vector.
#endif

#ifndef AKSIMD_GETELEMENT_V2F32
	#define AKSIMD_GETELEMENT_V2F32( __vName, __num__ )			((float*)&(__vName))[(__num__)]							///< Retrieve scalar element from vector.
#endif

#ifndef AKSIMD_GETELEMENT_V4I32
/// Get the element at index __num__ in vector __vName
#define AKSIMD_GETELEMENT_V4I32( __vName, __num__ )			((int*)&(__vName))[(__num__)]							///< Retrieve scalar element from vector.
#endif
