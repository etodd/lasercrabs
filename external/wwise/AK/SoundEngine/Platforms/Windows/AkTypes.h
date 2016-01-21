//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkTypes.h

/// \file 
/// Data type definitions.

#ifndef _AK_DATA_TYPES_PLATFORM_H_
#define _AK_DATA_TYPES_PLATFORM_H_

#include <limits.h>

#ifndef __cplusplus
	#include <wchar.h> // wchar_t not a built-in type in C
#endif

#define AK_WIN										///< Compiling for Windows
	
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0602
#endif						
	
#if defined _M_IX86
	#define AK_CPU_X86								///< Compiling for 32-bit x86 CPU
#elif defined _M_AMD64
	#define AK_CPU_X86_64							///< Compiling for 64-bit x86 CPU
#elif defined _M_ARM
	#define AK_CPU_ARM
	#define AK_CPU_ARM_NEON
#endif
	
#ifdef WINAPI_FAMILY
	#include <winapifamily.h>
	#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
		#define AK_USE_METRO_API
		#define AK_USE_THREAD_EMULATION
		#if defined(WINAPI_PARTITION_PHONE) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE)
			#define AK_WINPHONE
		#endif
	#endif
#endif

#ifndef AK_WINPHONE
#define AK_MOTION								///< Internal use
#define AK_71AUDIO
#endif

#define AK_LFECENTER							///< Internal use
#define AK_REARCHANNELS							///< Internal use

#define AK_SUPPORT_WCHAR						///< Can support wchar
#define AK_OS_WCHAR								///< Use wchar natively

#define AK_RESTRICT		__restrict				///< Refers to the __restrict compilation flag available on some platforms
#define AK_EXPECT_FALSE( _x )	(_x)
#define AkForceInline	__forceinline			///< Force inlining
#define AkNoInline		__declspec(noinline)	///< Disable inlining

#define AK_SIMD_ALIGNMENT	16					///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_SIMD( __Declaration__ ) __declspec(align(AK_SIMD_ALIGNMENT)) __Declaration__ ///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_DMA							///< Platform-specific data alignment for DMA transfers
#define AK_ALIGN_FASTDMA 						///< Platform-specific data alignment for faster DMA transfers
#define AK_ALIGN_SIZE_FOR_DMA( __Size__ ) (__Size__) ///< Used to align sizes to next 16 byte boundary on platfroms that require it
#define AK_BUFFER_ALIGNMENT AK_SIMD_ALIGNMENT
#define AK_XAUDIO2_FLAGS 0

#define AKSOUNDENGINE_CALL __cdecl				///< Calling convention for the Wwise API

typedef unsigned char		AkUInt8;			///< Unsigned 8-bit integer
typedef unsigned short		AkUInt16;			///< Unsigned 16-bit integer
typedef unsigned long		AkUInt32;			///< Unsigned 32-bit integer
typedef unsigned __int64	AkUInt64;			///< Unsigned 64-bit integer

#if defined(_WIN64)
typedef __int64 AkIntPtr;						///< Integer type for pointers
typedef unsigned __int64 AkUIntPtr;				///< Integer (unsigned) type for pointers
#else
typedef __w64 int AkIntPtr;						///< Integer type for pointers
typedef __w64 unsigned int AkUIntPtr;			///< Integer (unsigned) type for pointers
#endif

typedef char			AkInt8;					///< Signed 8-bit integer
typedef short			AkInt16;				///< Signed 16-bit integer
typedef long   			AkInt32;				///< Signed 32-bit integer
typedef __int64			AkInt64;				///< Signed 64-bit integer

typedef wchar_t			AkOSChar;				///< Generic character string

typedef float			AkReal32;				///< 32-bit floating point
typedef double          AkReal64;				///< 64-bit floating point

typedef void *					AkThread;		///< Thread handle
typedef AkUInt32				AkThreadID;		///< Thread ID
typedef AkUInt32 (__stdcall *AkThreadRoutine)(	void* lpThreadParameter	); ///< Thread routine
typedef void *					AkEvent;		///< Event handle
typedef void *					AkFileHandle;	///< File handle
typedef wchar_t			AkUtf16;				///< Type for 2 byte chars. Used for communication
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

/// Macro that takes a string litteral and changes it to an AkOSChar string at compile time
/// \remark This is similar to the TEXT() and _T() macros that can be used to turn string litterals into wchar_t strings
/// \remark Usage: AKTEXT( "Some Text" )
#define AKTEXT(x) L ## x

/// Memory pool attributes.
/// Block allocation type determines the method used to allocate
/// a memory pool. Block management type determines the
/// method used to manage memory blocks. Note that
/// the list of values in this enum is platform-dependent.
/// \sa
/// - AkMemoryMgr::CreatePool()
/// - AK::Comm::DEFAULT_MEMORY_POOL_ATTRIBUTES
enum AkMemPoolAttributes
{
	AkNoAlloc		= 0,	///< CreatePool will not allocate memory.  You need to allocate the buffer yourself.
	AkMalloc		= 1<<0,	///< CreatePool will use AK::AllocHook() to allocate the memory block.

	AkVirtualAlloc	= 1<<1,	///< CreatePool will use AK::VirtualAllocHook() to allocate the memory block (Windows & Xbox360 only).
	AkAllocMask		= AkNoAlloc | AkMalloc | AkVirtualAlloc,	///< Block allocation type mask.		

	AkFixedSizeBlocksMode	= 1<<3,			///< Block management type: Fixed-size blocks. Get blocks through GetBlock/ReleaseBlock API.  If not specified, use AkAlloc/AkFree.
	AkBlockMgmtMask	= AkFixedSizeBlocksMode	///< Block management type mask.
};
#define AK_MEMPOOLATTRIBUTES

#ifdef __cplusplus
	namespace AK
	{
		/// External allocation hook for the Memory Manager. Called by the Audiokinetic 
		/// implementation of the Memory Manager when creating a pool of type AkVirtualAlloc.
		/// \aknote This needs to be defined by the client, who must allocate memory using VirtualAlloc. \endaknote
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		/// \sa 
		/// - \ref memorymanager
		/// - AK::AllocHook()
		/// - AK::FreeHook()
		/// - AK::VirtualFreeHook()
		extern void * AKSOUNDENGINE_CALL VirtualAllocHook( 
			void * in_pMemAddress,		///< Parameter for VirtualAlloc
			size_t in_size,				///< Number of bytes to allocate
			AkUInt32 in_dwAllocationType,	///< Parameter for VirtualAlloc
			AkUInt32 in_dwProtect			///< Parameter for VirtualAlloc
			);
	
		/// External deallocation hook for the Memory Manager. Called by the Audiokinetic 
		/// implementation of the Memory Manager when destroying a pool of type AkVirtualAlloc.
		/// \aknote This needs to be defined by the client, who must deallocate memory using VirtualFree. \endaknote
		/// \sa 
		/// - \ref memorymanager
		/// - AK::VirtualAllocHook()
		/// - AK::AllocHook()
		/// - AK::FreeHook()
		extern void AKSOUNDENGINE_CALL VirtualFreeHook( 
			void * in_pMemAddress,	///< Pointer to the start of memory allocated with VirtualAllocHook
			size_t in_size,			///< Parameter for VirtualFree
			AkUInt32 in_dwFreeType		///< Parameter for VirtualFree
			);
	}
#endif

#endif //_AK_DATA_TYPES_PLATFORM_H_

