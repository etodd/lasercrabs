//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Audiokinetic platform checks. This is where we detect which platform
/// is being compiled, and where we define the corresponding AK-specific
/// symbols.

#pragma once

#if defined( NN_PLATFORM_CTR )

	#include <AK/SoundEngine/Platforms/3DS/AkTypes.h>

#elif _XBOX_VER >= 200 // Check Xbox before WIN32 because WIN32 might also be defined in some cases in Xbox 360 projects
 
	#include <AK/SoundEngine/Platforms/XBox360/AkTypes.h>

#elif defined( _XBOX_ONE )

	#include <AK/SoundEngine/Platforms/XboxOne/AkTypes.h>

#elif defined( WIN32 ) || defined ( WIN64 ) || defined( WINAPI_FAMILY )

	#include <AK/SoundEngine/Platforms/Windows/AkTypes.h>

#elif defined( __APPLE__ )

	#include <AK/SoundEngine/Platforms/Mac/AkTypes.h>

#elif defined (__PPU__) || defined (__SPU__)

	#include <AK/SoundEngine/Platforms/PS3/AkTypes.h>

#elif defined( CAFE ) || defined( RVL_OS )

	#include <AK/SoundEngine/Platforms/WiiFamily/AkTypes.h>

#elif defined( __SCE__ ) && defined( __arm__ )

	#include <AK/SoundEngine/Platforms/Vita/AkTypes.h>
	
#elif defined( __ORBIS__ )

	#include <AK/SoundEngine/Platforms/PS4/AkTypes.h>

#elif defined( __ANDROID__ )

	#include <AK/SoundEngine/Platforms/Android/AkTypes.h>

#elif defined( __native_client__ )

	#include <AK/SoundEngine/Platforms/nacl/AkTypes.h>

#elif defined( __linux__ )

	#include <AK/SoundEngine/Platforms/Linux/AkTypes.h>

#elif defined( __QNX__ )

	#include <AK/SoundEngine/Platforms/QNX/AkTypes.h>

#else

	#error Unsupported platform, or platform-specific symbols not defined

#endif
