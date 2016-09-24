//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkTypes.h

/// \file 
/// Data type definitions.

#pragma once

#include <mach/semaphore.h>

#include <TargetConditionals.h>

#define AK_APPLE									///< Compiling for an Apple platform

#if (TARGET_OS_IPHONE || TARGET_OS_TV) && !TARGET_OS_SIMULATOR
	#define AK_IOS									///< Compiling for iOS or tvOS (iPhone, iPad, iPod, Apple TV...)
	#define AK_CPU_ARM								///< Compiling for ARM CPU
	#if defined __ARM_NEON__
		#define AK_CPU_ARM_NEON						///< Compiling for ARM CPU with Neon
		#define AKSIMD_V4F32_SUPPORTED
	#endif

	#if __LP64__
		#define AK_CPU_ARM_64
	#endif // #if __LP64__
#elif (TARGET_OS_IPHONE || TARGET_OS_TV) && TARGET_OS_SIMULATOR
	#define AK_IOS									///< Compiling for iOS or tvOS (iPhone, iPad, iPod, Apple TV...)
	#if __LP64__
		#define AK_CPU_X86_64
	#else
		#define AK_CPU_X86								///< Compiling for 32-bit x86 CPU
	#endif // #if __LP64__
#elif !TARGET_OS_EMBEDDED
	#define AK_MAC_OS_X								///< Compiling for Mac OS X
	#define AKSIMD_V4F32_SUPPORTED
	#if  TARGET_CPU_X86_64
		#define AK_CPU_X86_64						///< Compiling for 64-bit x86 CPU
	#elif TARGET_CPU_X86
		#define AK_CPU_X86							///< Compiling for 32-bit x86 CPU
	#endif
#endif

	#define AK_LFECENTER							///< Internal use
	#define AK_REARCHANNELS							///< Internal use
	#define AK_71AUDIO								///< Internal use

#ifdef AK_MAC_OS_X
	#define AK_71FROMSTEREOMIXER
	#define AK_51FROMSTEREOMIXER
#endif

#define AK_SUPPORT_WCHAR						///< Can support wchar

typedef semaphore_t		AkEvent;
typedef semaphore_t		AkSemaphore;

// Note: POSIX-type include has to stay at the bottom 
// otherwise AK_APPLE and other defines won't work for the include.
#include <AK/SoundEngine/Platforms/POSIX/AkTypes.h>