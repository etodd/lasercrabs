/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

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
	
#elif defined( AK_VITA )

	#include <AK/SoundEngine/Platforms/Vita/AkSimd.h>

#elif defined( AK_ANDROID )

	#include <AK/SoundEngine/Platforms/Android/AkSimd.h>

#elif defined( AK_PS4 )

	#include <AK/SoundEngine/Platforms/PS4/AkSimd.h>
	
#elif defined( AK_LINUX )

	#include <AK/SoundEngine/Platforms/Linux/AkSimd.h>

#elif defined( AK_EMSCRIPTEN )

	#include <AK/SoundEngine/Platforms/Emscripten/AkSimd.h>

#elif defined( AK_QNX )

	#include <AK/SoundEngine/Platforms/QNX/AkSimd.h>

#elif defined( AK_NX )

	#include <AK/SoundEngine/Platforms/NX/AkSimd.h>

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
