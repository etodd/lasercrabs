//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkTypes.h

/// \file 
/// Data type definitions.

#pragma once

#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>

#define AK_RESTRICT		__restrict				///< Refers to the __restrict compilation flag available on some platforms
#define AK_EXPECT_FALSE( _x )	(_x)
#define AkForceInline	inline					///< Force inlining
#define AkNoInline		__attribute ((__noinline__))			///< Disable inlining

#define AK_SIMD_ALIGNMENT	16					///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_SIMD( __Declaration__ ) __Declaration__ __attribute__((aligned(AK_SIMD_ALIGNMENT))) ///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_DMA							///< Platform-specific data alignment for DMA transfers
#define AK_ALIGN_FASTDMA 						///< Platform-specific data alignment for faster DMA transfers
#define AK_ALIGN_SIZE_FOR_DMA( __Size__ ) (__Size__) ///< Used to align sizes to next 16 byte boundary on platfroms that require it
#define AK_BUFFER_ALIGNMENT AK_SIMD_ALIGNMENT

typedef uint8_t			AkUInt8;				///< Unsigned 8-bit integer
typedef uint16_t		AkUInt16;				///< Unsigned 16-bit integer
typedef uint32_t		AkUInt32;				///< Unsigned 32-bit integer
typedef uint64_t		AkUInt64;				///< Unsigned 64-bit integer
typedef uintptr_t		AkUIntPtr;
typedef int8_t			AkInt8;					///< Signed 8-bit integer
typedef int16_t			AkInt16;				///< Signed 16-bit integer
typedef int32_t   		AkInt32;				///< Signed 32-bit integer
typedef int64_t			AkInt64;				///< Signed 64-bit integer
typedef intptr_t		AkIntPtr;

typedef char			AkOSChar;				///< Generic character string

typedef float			AkReal32;				///< 32-bit floating point
typedef double          AkReal64;				///< 64-bit floating point

typedef pthread_t		AkThread;				///< Thread handle
typedef pthread_t		AkThreadID;				///< Thread ID
typedef void* 			(*AkThreadRoutine)(	void* lpThreadParameter	);	///< Thread routine
#ifndef AK_APPLE
typedef sem_t 			AkEvent;				///< Event handle
#endif
typedef FILE*			AkFileHandle;			///< File handle

typedef AkUInt16		AkUtf16;				///< Type for 2 byte chars. Used for communication
												///< with the authoring tool.

#define AK_UINT_MAX		UINT_MAX

// For strings.
#define AK_MAX_PATH     260						///< Maximum path length.

typedef AkUInt32			AkFourcc;			///< Riff chunk

/// Create Riff chunk
#define AkmmioFOURCC( ch0, ch1, ch2, ch3 )									    \
		( (AkFourcc)(AkUInt8)(ch0) | ( (AkFourcc)(AkUInt8)(ch1) << 8 ) |		\
		( (AkFourcc)(AkUInt8)(ch2) << 16 ) | ( (AkFourcc)(AkUInt8)(ch3) << 24 ) )

#define AK_BANK_PLATFORM_DATA_ALIGNMENT	(16)	///< Required memory alignment for bank loading by memory address (see LoadBank())
#define AK_BANK_PLATFORM_ALLOC_TYPE		AkMalloc


#define AKTEXT(x) x


