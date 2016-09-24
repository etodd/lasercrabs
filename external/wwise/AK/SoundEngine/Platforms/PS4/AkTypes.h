//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkTypes.h

/// \file 
/// Data type definitions.

#pragma once

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <libdbg.h>
#include <sceconst.h>
#include <scetypes.h>
#include <kernel\eventflag.h>
#include <fios2.h>
#include <kernel.h>

#define AK_PS4									///< Compiling for PS4
	
#define AK_MOTION								///< Internal use
#define AK_LFECENTER							///< Internal use
#define AK_REARCHANNELS							///< Internal use
#define AK_SUPPORT_WCHAR						///< Can support wchar
#define AK_71FROM51MIXER						///< Internal use
#define AK_71FROMSTEREOMIXER					///< Internal use
#define AK_71AUDIO								///< Internal use
//#define AK_OS_WCHAR	

//#define AK_ENABLE_RAZOR_PROFILING

#ifndef AK_OPTIMIZED
	// Define AK_ENABLE_VITACODECENGINEPERFMONITORING to enable "DSP Usage" monitoring (must also be enabled on the app side for Vita)
	//#define AK_ENABLE_VITACODECENGINEPERFMONITORING
#endif // AK_OPTIMIZED

#define AK_CPU_X86_64

#define AK_RESTRICT				__restrict								///< Refers to the __restrict compilation flag available on some platforms
#define AK_EXPECT_FALSE( _x )	( _x )
#define AkRegister				
#define AkForceInline			inline __attribute__((always_inline))	///< Force inlining
#define AkNoInline				__attribute__((noinline))

#define AK_SIMD_ALIGNMENT	16					///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_SIMD( __Declaration__ ) __Declaration__ __attribute__((aligned(AK_SIMD_ALIGNMENT))) ///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_DMA							///< Platform-specific data alignment for DMA transfers
#define AK_ALIGN_FASTDMA 						///< Platform-specific data alignment for faster DMA transfers
#define AK_ALIGN_SIZE_FOR_DMA( __Size__ ) (__Size__) ///< Used to align sizes to next 16 byte boundary on platfroms that require it
#define AK_BUFFER_ALIGNMENT AK_SIMD_ALIGNMENT
#define AKSIMD_V4F32_SUPPORTED

#define AK_DLLEXPORT __declspec(dllexport)
#define AK_DLLIMPORT __declspec(dllimport)	

typedef uint8_t			AkUInt8;				///< Unsigned 8-bit integer
typedef uint16_t		AkUInt16;				///< Unsigned 16-bit integer
typedef uint32_t		AkUInt32;				///< Unsigned 32-bit integer
typedef uint64_t		AkUInt64;				///< Unsigned 64-bit integer

#ifdef AK_CPU_X86_64
typedef int64_t			AkIntPtr;
typedef uint64_t		AkUIntPtr;
#else 
typedef int				AkIntPtr;
typedef unsigned int	AkUIntPtr;
#endif

typedef int8_t			AkInt8;					///< Signed 8-bit integer
typedef int16_t			AkInt16;				///< Signed 16-bit integer
typedef int32_t   		AkInt32;				///< Signed 32-bit integer
typedef int64_t			AkInt64;				///< Signed 64-bit integer

typedef char			AkOSChar;				///< Generic character string

typedef float			AkReal32;				///< 32-bit floating point
typedef double          AkReal64;				///< 64-bit floating point

typedef ScePthread				AkThread;				///< Thread handle
typedef ScePthread				AkThreadID;				///< Thread ID
typedef void* 					(*AkThreadRoutine)(	void* lpThreadParameter	);		///< Thread routine
typedef SceKernelEventFlag		AkEvent;				///< Event handle

typedef SceFiosFH				AkFileHandle;			///< File handle

typedef wchar_t					AkUtf16;				///< Type for 2 byte chars. Used for communication
														///< with the authoring tool.

#define AK_CAPTURE_TYPE_FLOAT							///< Capture type is float.
typedef AkReal32				AkCaptureType;			///< Capture type is float.

#define AK_UINT_MAX				UINT_MAX

// For strings.
#define AK_MAX_PATH				SCE_FIOS_PATH_MAX		///< Maximum path length (each file/dir name is max 255 char)

typedef AkUInt32				AkFourcc;				///< Riff chunk

/// Create Riff chunk
#define AkmmioFOURCC( ch0, ch1, ch2, ch3 )									    \
		( (AkFourcc)(AkUInt8)(ch0) | ( (AkFourcc)(AkUInt8)(ch1) << 8 ) |		\
		( (AkFourcc)(AkUInt8)(ch2) << 16 ) | ( (AkFourcc)(AkUInt8)(ch3) << 24 ) )

#define AK_BANK_PLATFORM_DATA_ALIGNMENT				(256)	///< Required memory alignment for bank loading by memory address for ATRAC9 (see LoadBank())
#define AK_BANK_PLATFORM_DATA_NON_ATRAC9_ALIGNMENT	(16)	///< Required memory alignment for bank loading by memory address for non-ATRAC9 formats (see LoadBank())

#define AK_BANK_PLATFORM_ALLOC_TYPE		AkMalloc

/// Macro that takes a string litteral and changes it to an AkOSChar string at compile time
/// \remark This is similar to the TEXT() and _T() macros that can be used to turn string litterals into Unicode strings
/// \remark Usage: AKTEXT( "Some Text" )
#define AKTEXT(x) x
