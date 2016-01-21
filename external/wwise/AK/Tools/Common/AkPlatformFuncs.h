//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkPlatformFuncs.h

/// \file 
/// Platform-dependent functions definition.

#ifndef _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
#define _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H

#include <AK/SoundEngine/Common/AkTypes.h>

#if defined(AK_WIN) || defined(AK_XBOXONE)
#include <AK/Tools/Win32/AkPlatformFuncs.h>

#elif defined (AK_XBOX360)
#include <AK/Tools/XBox360/AkPlatformFuncs.h>

#elif defined (AK_PS3)
#include <AK/Tools/PS3/AkPlatformFuncs.h>

#elif defined (AK_WII)
#include <AK/Tools/Wii/AkPlatformFuncs.h>

#elif defined (AK_WIIU)
#include <AK/Tools/WiiU/AkPlatformFuncs.h>

#elif defined (AK_APPLE)
#include <AK/Tools/Mac/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined (AK_VITA)
#include <AK/Tools/Vita/AkPlatformFuncs.h>

#elif defined (AK_3DS)
#include <AK/Tools/3DS/AkPlatformFuncs.h>

#elif defined (AK_ANDROID)
#include <AK/Tools/Android/AkPlatformFuncs.h>

#elif defined (AK_NACL)
#include <AK/Tools/nacl/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined (AK_PS4)
#include <AK/Tools/PS4/AkPlatformFuncs.h>

#elif defined (AK_LINUX)
#include <AK/Tools/Linux/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined (AK_QNX)
#include <AK/Tools/QNX/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#else
#error AkPlatformFuncs.h: Undefined platform
#endif

#ifndef AkPrefetchZero
#define AkPrefetchZero(___Dest, ___Size)
#endif

#ifndef AkPrefetchZeroAligned
#define AkPrefetchZeroAligned(___Dest, ___Size)
#endif

#ifndef AkZeroMemAligned
#define AkZeroMemAligned(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif
#ifndef AkZeroMemLarge
#define AkZeroMemLarge(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif
#ifndef AkZeroMemSmall
#define AkZeroMemSmall(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif

#ifndef AK_THREAD_INIT_CODE
#define AK_THREAD_INIT_CODE(_threadProperties)
#endif

/// Utility functions
namespace AK
{
	/// Count non-zero bits.
	/// \return Number of channels.
	AkForceInline AkUInt32 GetNumNonZeroBits( AkUInt32 in_uWord )
	{
		AkUInt32 num = 0;
		while( in_uWord ){ ++num; in_uWord &= in_uWord-1; }
		return num;
	}
}


#ifndef AK_PERF_RECORDING_RESET
#define AK_PERF_RECORDING_RESET()
#endif
#ifndef AK_PERF_RECORDING_START
#define AK_PERF_RECORDING_START( __StorageName__, __uExecutionCountStart__, __uExecutionCountStop__ )
#endif
#ifndef AK_PERF_RECORDING_STOP
#define AK_PERF_RECORDING_STOP( __StorageName__, __uExecutionCountStart__, __uExecutionCountStop__ )	
#endif

#endif // _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
