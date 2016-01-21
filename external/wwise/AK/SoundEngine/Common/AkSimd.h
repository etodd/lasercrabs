//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// Simd definitions.

#ifndef _AK_SIMD_H_
#define _AK_SIMD_H_

#include <AK/SoundEngine/Common/AkTypes.h>

// Platform-specific section.
//----------------------------------------------------------------------------------------------------

#if defined( AK_WIN ) || defined( AK_XBOXONE )
	
	#include <AK/SoundEngine/Platforms/Windows/AkSimd.h>

#elif defined( AK_APPLE )

	#include <TargetConditionals.h>
	#if TARGET_OS_IPHONE
		#include <AK/SoundEngine/Platforms/iOS/AkSimd.h>
	#else
	#include <AK/SoundEngine/Platforms/Mac/AkSimd.h>
	#endif

#elif defined( AK_XBOX360 )

	#include <AK/SoundEngine/Platforms/XBox360/AkSimd.h>

#elif defined (AK_PS3)

	#include <AK/SoundEngine/Platforms/PS3/AkSimd.h>
#elif defined( AK_WII )

	#include <AK/SoundEngine/Platforms/Generic/AkSimd.h>
	
#elif defined( AK_VITA )

	#include <AK/SoundEngine/Platforms/Vita/AkSimd.h>

#elif defined( AK_3DS )

	#include <AK/SoundEngine/Platforms/3DS/AkSimd.h>

#elif defined( AK_ANDROID )

	#include <AK/SoundEngine/Platforms/Android/AkSimd.h>

#elif defined( AK_NACL )

	#include <AK/SoundEngine/Platforms/nacl/AkSimd.h>
	
#elif defined( AK_WIIU )

	#include <AK/SoundEngine/Platforms/WiiFamily/AkSimd.h>

#elif defined( AK_PS4 )

	#include <AK/SoundEngine/Platforms/PS4/AkSimd.h>
	
#elif defined( AK_LINUX )

	#include <AK/SoundEngine/Platforms/Linux/AkSimd.h>
	
#elif defined( AK_QNX )

	#include <AK/SoundEngine/Platforms/QNX/AkSimd.h>

#else
	#error Unsupported platform, or platform-specific SIMD not defined
#endif

#ifndef AKSIMD_ASSERTFLUSHZEROMODE
	#define AKSIMD_ASSERTFLUSHZEROMODE
#endif

#ifndef AKSIMD_DECLARE_V4F32_TYPE
	#define AKSIMD_DECLARE_V4F32_TYPE AKSIMD_V4F32
#endif

#ifndef AKSIMD_DECLARE_V4I32_TYPE
	#define AKSIMD_DECLARE_V4I32_TYPE AKSIMD_V4I32
#endif

#ifndef AKSIMD_DECLARE_V4F32
	#define AKSIMD_DECLARE_V4F32( _x, _a, _b, _c, _d ) AKSIMD_DECLARE_V4F32_TYPE _x = { _a, _b, _c, _d }
#endif

#ifndef AKSIMD_DECLARE_V4I32
	#define AKSIMD_DECLARE_V4I32( _x, _a, _b, _c, _d ) AKSIMD_DECLARE_V4I32_TYPE _x = { _a, _b, _c, _d }
#endif

#ifndef AKSIMD_SETELEMENT_V4F32
	#define AKSIMD_SETELEMENT_V4F32( __vName__, __num__, __value__ ) ( AKSIMD_GETELEMENT_V4F32( __vName__, __num__ ) = (__value__) )
#endif

#endif  //_AK_DATA_TYPES_H_
