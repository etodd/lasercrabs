//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkTypes.h

/// \file 
/// Data type definitions.

#pragma once

#define AK_LINUX

#if defined __x86_64
	#define AK_CPU_X86_64
#elif defined __i386
	#define AK_CPU_X86
#endif

#define AK_71AUDIO
#define AK_LFECENTER
#define AK_REARCHANNELS

#define AK_SUPPORT_WCHAR						///< Can support wchar

#include <AK/SoundEngine/Platforms/POSIX/AkTypes.h>

