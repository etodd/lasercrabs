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

  Version: v2016.2.4  Build: 6097
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AK_PLATFORM_FUNCS_H_
#define _AK_PLATFORM_FUNCS_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include <sce_atomic.h>
#include <sceerror.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <kernel\eventflag.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>


//-----------------------------------------------------------------------------
// Platform-specific thread properties definition.
//-----------------------------------------------------------------------------
struct AkThreadProperties
{
    int						nPriority;		///< Thread priority
    SceKernelCpumask		dwAffinityMask;	///< Affinity mask
	size_t					uStackSize;		///< Thread stack size
	int						uSchedPolicy;	///< Thread scheduling policy
};

//-----------------------------------------------------------------------------
// External variables.
//-----------------------------------------------------------------------------
// g_fFreqRatio is used by time helpers to return time values in milliseconds.
// It is declared and updated by the sound engine.
namespace AK
{
    extern AkReal32 g_fFreqRatio;
}

//-----------------------------------------------------------------------------
// Defines for PS4.
//-----------------------------------------------------------------------------
#define AK_DECLARE_THREAD_ROUTINE( FuncName )   void* FuncName(void* lpParameter)
#define AK_THREAD_RETURN( _param_ )				return (_param_);
#define AK_THREAD_ROUTINE_PARAMETER             lpParameter
#define AK_GET_THREAD_ROUTINE_PARAMETER_PTR(type) reinterpret_cast<type*>( AK_THREAD_ROUTINE_PARAMETER )

#define AK_RETURN_THREAD_OK                     0x00000000
#define AK_RETURN_THREAD_ERROR                  0x00000001
#define AK_DEFAULT_STACK_SIZE                   (65536)
#define AK_THREAD_DEFAULT_SCHED_POLICY			SCE_KERNEL_SCHED_FIFO
#define AK_THREAD_PRIORITY_NORMAL				SCE_KERNEL_PRIO_FIFO_DEFAULT
#define AK_THREAD_PRIORITY_ABOVE_NORMAL			SCE_KERNEL_PRIO_FIFO_HIGHEST
#define AK_THREAD_PRIORITY_BELOW_NORMAL			SCE_KERNEL_PRIO_FIFO_LOWEST

#define AK_THREAD_AFFINITY_ALL					63; // from binary 111111 setting the 6 available core to true. (ex: 4 << 1)
#define	AK_THREAD_AFFINITY_DEFAULT				AK_THREAD_AFFINITY_ALL

// On PS4 this needs to be called regularly.
#define	AK_RELEASE_GPU_OFFLINE_FRAME			sce::Gnm::submitDone();

// NULL objects
#define AK_NULL_THREAD                          NULL

#define AK_INFINITE                             (AK_UINT_MAX)

#define AkMax(x1, x2)	(((x1) > (x2))? (x1): (x2))
#define AkMin(x1, x2)	(((x1) < (x2))? (x1): (x2))
#define AkClamp(x, min, max)  ((x) < (min)) ? (min) : (((x) > (max) ? (max) : (x)))

namespace AKPLATFORM
{
#ifndef AK_OPTIMIZED
	/// Output a debug message on the console (Ansi string)
	AkForceInline void OutputDebugMsg( const char* in_pszMsg )
	{
		fputs( in_pszMsg, stderr );
	}
	/// Output a debug message on the console (Unicode string)
	AkForceInline void OutputDebugMsg( const wchar_t* in_pszMsg )
	{
		fputws( in_pszMsg, stderr );
	}
#else
	inline void OutputDebugMsg( const wchar_t* ){}
	inline void OutputDebugMsg( const char* ){}
#endif


	// Simple automatic event API
    // ------------------------------------------------------------------
	
	/// Platform Independent Helper
	AkForceInline void AkClearEvent( AkEvent & out_event )
    {		
		out_event = NULL;
	}

	AkForceInline AKRESULT AkCreateNamedEvent( AkEvent & out_event, const char* in_szName )
    {
		// NOTE: AkWaitForEvent uses the SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT flag
		// to get the same behavior as an auto-reset Win32 event
        sceKernelCreateEventFlag(
			&out_event,
			in_szName,
			SCE_KERNEL_EVF_ATTR_MULTI,
			0 /* not signalled by default */,
			NULL /* No optional params */ );

		AKASSERT(out_event >= 0);

		if ( out_event >= 0 )
			return AK_Success;

		AkClearEvent( out_event );
		return AK_Fail;
	}

	/// Platform Independent Helper
	AkForceInline AKRESULT AkCreateEvent( AkEvent & out_event )
    {
		return AkCreateNamedEvent( out_event, "AkEvent" );
	}

	/// Platform Independent Helper
	AkForceInline void AkDestroyEvent( AkEvent & io_event )
	{
		sceKernelDeleteEventFlag(io_event);
		AkClearEvent( io_event );
	}

	/// Platform Independent Helper
	AkForceInline void AkWaitForEvent( AkEvent & in_event )
	{
		AKVERIFY( sceKernelWaitEventFlag(
			in_event,
			1,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
			SCE_NULL,
			SCE_NULL) == 0 );
	}

	/// Platform Independent Helper
	AkForceInline void AkSignalEvent( const AkEvent & in_event )
	{
		AKVERIFY( sceKernelSetEventFlag( in_event, 1 ) == 0 );
	}

	AkForceInline bool AkIsValidEvent( const AkEvent & in_event )
	{
		return ( in_event >= 0 );
	}


	// Atomic Operations
    // ------------------------------------------------------------------

	/// Platform Independent Helper
	AkForceInline AkInt32 AkInterlockedIncrement(AkAtomic32 * pValue)
	{
		return sceAtomicIncrement32( (volatile SceInt32 *) pValue ) + 1;
	}

	/// Platform Independent Helper
	AkForceInline AkInt32 AkInterlockedDecrement(AkAtomic32 * pValue)
	{
		return sceAtomicDecrement32( (volatile SceInt32 *) pValue ) - 1;
	}

	AkForceInline bool AkInterlockedCompareExchange(volatile AkAtomic64* io_pDest, AkInt64 in_newValue, AkInt64 in_expectedOldVal)
	{
		return sceAtomicCompareAndSwap64(io_pDest, in_expectedOldVal, in_newValue) == in_expectedOldVal;
	}

	AkForceInline bool AkInterlockedCompareExchange(volatile AkAtomic32* io_pDest, AkInt32 in_newValue, AkInt32 in_expectedOldVal)
	{
		return sceAtomicCompareAndSwap32(io_pDest, in_expectedOldVal, in_newValue) == in_expectedOldVal;
	}

	AkForceInline void AkMemoryBarrier()
	{
		__asm("sfence");
	}

    // Threads
    // ------------------------------------------------------------------

	/// Platform Independent Helper
	AkForceInline bool AkIsValidThread( AkThread * in_pThread )
	{
		return ( *in_pThread != AK_NULL_THREAD );
	}

	/// Platform Independent Helper
	AkForceInline void AkClearThread( AkThread * in_pThread )
	{
		*in_pThread = AK_NULL_THREAD;
	}

	/// Platform Independent Helper
    AkForceInline void AkCloseThread( AkThread * in_pThread )
    {
        AKASSERT( in_pThread );
        AKASSERT( *in_pThread );

		// #define KILL_THREAD(t) do { void *ret; scePthreadJoin(t,&ret); } while(false)
		// AKVERIFY( SCE_OK == sceKernelDeleteThread( *in_pThread ) );
        AkClearThread( in_pThread );
    }

	#define AkExitThread( _result ) return _result; // ?????

	/// Platform Independent Helper
	AkForceInline void AkGetDefaultThreadProperties( AkThreadProperties & out_threadProperties )
	{
		out_threadProperties.uStackSize		= AK_DEFAULT_STACK_SIZE;
		out_threadProperties.uSchedPolicy	= AK_THREAD_DEFAULT_SCHED_POLICY;
		out_threadProperties.nPriority		= AK_THREAD_PRIORITY_NORMAL;
		out_threadProperties.dwAffinityMask = AK_THREAD_AFFINITY_DEFAULT;
	}

	/// Platform Independent Helper
	inline void AkCreateThread( 
		AkThreadRoutine pStartRoutine,					// Thread routine.
		void * pParams,									// Routine params.
		const AkThreadProperties & in_threadProperties,	// Properties. NULL for default.
		AkThread * out_pThread,							// Returned thread handle.
		const char * in_szThreadName )				// Opt thread name.
    {
		AKASSERT( out_pThread != NULL );
		
		ScePthreadAttr  attr;
		
		// Create the attr
		AKVERIFY(!scePthreadAttrInit(&attr));
		// Set the stack size
		AKVERIFY(!scePthreadAttrSetstacksize(&attr,in_threadProperties.uStackSize));
		AKVERIFY(!scePthreadAttrSetdetachstate(&attr, SCE_PTHREAD_CREATE_JOINABLE));
		AKVERIFY(!scePthreadAttrSetinheritsched(&attr, SCE_PTHREAD_EXPLICIT_SCHED));
		AKVERIFY(!scePthreadAttrSetaffinity(&attr,in_threadProperties.dwAffinityMask)); 
		
		// Try to set the thread policy
		int sched_policy = in_threadProperties.uSchedPolicy;
		if( scePthreadAttrSetschedpolicy( &attr, sched_policy )  )
		{
			AKASSERT( !"AKCreateThread invalid sched policy, will automatically set it to FIFO scheduling" );
			sched_policy = AK_THREAD_DEFAULT_SCHED_POLICY;
			AKVERIFY( !scePthreadAttrSetschedpolicy( &attr, sched_policy ));
		}

		int minPriority, maxPriority;
		minPriority = SCE_KERNEL_PRIO_FIFO_HIGHEST;
		maxPriority = SCE_KERNEL_PRIO_FIFO_LOWEST;
		
		// Set the thread priority if valid
		AKASSERT( in_threadProperties.nPriority >= minPriority && in_threadProperties.nPriority <= maxPriority );
		if(  in_threadProperties.nPriority >= minPriority && in_threadProperties.nPriority <= maxPriority )
		{
			SceKernelSchedParam schedParam;
			AKVERIFY( scePthreadAttrGetschedparam(&attr, &schedParam) == 0 );
			schedParam.sched_priority = in_threadProperties.nPriority;
			AKVERIFY( scePthreadAttrSetschedparam(&attr, &schedParam) == 0 );
		}

		// Create the tread
		int threadError = scePthreadCreate(out_pThread, &attr, pStartRoutine, pParams, in_szThreadName);
		AKASSERT( threadError == 0 );
		AKVERIFY(!scePthreadAttrDestroy(&attr));
		
		if( threadError != 0 )
		{
			AkClearThread( out_pThread );
			return;
		}
		
		// ::CreateThread() return NULL if it fails.
        if ( !*out_pThread )
        {
			AkClearThread( out_pThread );
            return;
        }		
    }

	/// Platform Independent Helper
    AkForceInline void AkWaitForSingleThread( AkThread * in_pThread )
    {
        AKASSERT( in_pThread );
        AKASSERT( *in_pThread );
		AKVERIFY(!scePthreadJoin( *in_pThread, NULL ));
    }

	inline AkThreadID CurrentThread()
	{
		return scePthreadSelf();
	}

	/// Platform Independent Helper
    AkForceInline void AkSleep( AkUInt32 in_ulMilliseconds )
    {
		usleep( in_ulMilliseconds * 1000 );
    }

	// Optimized memory functions
	// --------------------------------------------------------------------

	/// Platform Independent Helper
	AkForceInline void AkMemCpy( void * pDest, const void * pSrc, AkUInt32 uSize )
	{
		memcpy( pDest, pSrc, uSize );
	}

	/// Platform Independent Helper
	AkForceInline void AkMemSet( void * pDest, AkInt32 iVal, AkUInt32 uSize )
	{
		memset( pDest, iVal, uSize );
	}

    // Time functions
    // ------------------------------------------------------------------

	/// Platform Independent Helper
    AkForceInline void PerformanceCounter( AkInt64 * out_piLastTime )
	{
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		*out_piLastTime = (AkInt64)ts.tv_sec * CLOCKS_PER_SEC + (AkInt64)ts.tv_nsec / 1000;
	}

	/// Frequency of the PerformanceCounter() (ticks per second)
	AkForceInline void PerformanceFrequency( AkInt64 * out_piFreq )
	{
		*out_piFreq = CLOCKS_PER_SEC;
	}

	/// Platform Independent Helper
    AkForceInline void UpdatePerformanceFrequency()
	{
        AkInt64 iFreq;
        PerformanceFrequency( &iFreq );
		AK::g_fFreqRatio = (AkReal32)( iFreq / 1000 );
	}

	/// Returns a time range in milliseconds, using the sound engine's updated count->milliseconds ratio.
    AkForceInline AkReal32 Elapsed( const AkInt64 & in_iNow, const AkInt64 & in_iStart )
    {
        return ( in_iNow - in_iStart ) / AK::g_fFreqRatio;
    }

	/// String conversion helper
	AkForceInline AkInt32 AkWideCharToChar(	const wchar_t*	in_pszUnicodeString,
											AkUInt32		in_uiOutBufferSize,
											char*		io_pszAnsiString )
	{
		AKASSERT( io_pszAnsiString != NULL );

		mbstate_t state;
		memset (&state, '\0', sizeof (state));

		return (AkInt32)wcsrtombs(io_pszAnsiString,		// destination
							&in_pszUnicodeString,	// source
							in_uiOutBufferSize,		// destination length
							&state);				// 

	}
	
	/// String conversion helper
	AkForceInline AkInt32 AkCharToWideChar(	const char*	in_pszAnsiString,
											AkUInt32			in_uiOutBufferSize,
											void*			io_pvUnicodeStringBuffer )
	{
		AKASSERT( io_pvUnicodeStringBuffer != NULL );

		mbstate_t state;
		memset (&state, '\0', sizeof (state));

		return (AkInt32)mbsrtowcs((wchar_t*)io_pvUnicodeStringBuffer,	// destination
									&in_pszAnsiString,					// source
									in_uiOutBufferSize,					// destination length
									&state);							// 
	}

	AkForceInline AkInt32 AkUtf8ToWideChar( const char*	in_pszUtf8String,
									 AkUInt32		in_uiOutBufferSize,
									 void*			io_pvUnicodeStringBuffer )
	{
		return AkCharToWideChar( in_pszUtf8String, in_uiOutBufferSize, (wchar_t*)io_pvUnicodeStringBuffer );
	}

	/// Safe unicode string copy.
	AkForceInline void SafeStrCpy( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t uSizeCopy = AkMin( in_uDestMaxNumChars - 1, wcslen( in_pSrc ) + 1 );
		wcsncpy( in_pDest, in_pSrc, uSizeCopy );
		in_pDest[uSizeCopy] = '\0';
	}

	/// Safe ansi string copy.
	AkForceInline void SafeStrCpy( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t uSizeCopy = AkMin( in_uDestMaxNumChars - 1, strlen( in_pSrc ) + 1 );
		strncpy( in_pDest, in_pSrc, uSizeCopy );
		in_pDest[uSizeCopy] = '\0';
	}

	/// Safe unicode string concatenation.
	AkForceInline void SafeStrCat( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t uAvailableSize = ( in_uDestMaxNumChars - wcslen( in_pDest ) - 1 );
		wcsncat( in_pDest, in_pSrc, AkMin( uAvailableSize, wcslen( in_pSrc ) ) );
	}

	/// Safe ansi string concatenation.
	AkForceInline void SafeStrCat( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t uAvailableSize = ( in_uDestMaxNumChars - strlen( in_pDest ) - 1 );
		strncat( in_pDest, in_pSrc, AkMin( uAvailableSize, strlen( in_pSrc ) ) );
	}

	/// Stack allocations.
	#define AkAlloca( _size_ ) alloca( _size_ )	

	

	/// Converts a wchar_t string to an AkOSChar string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_WIDE_TO_OSCHAR( _wstring_, _oscharstring_ ) \
		   _oscharstring_ = (AkOSChar*)AkAlloca( (1 + wcslen( _wstring_ )) * sizeof(AkOSChar) ); \
		   AKPLATFORM::AkWideCharToChar( _wstring_ , (AkUInt32)(1 + wcslen( _wstring_ )), (AkOSChar*)( _oscharstring_ ) )


	/// Converts a char string to an AkOSChar string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_CHAR_TO_OSCHAR( _astring_, _oscharstring_ ) ( _oscharstring_ ) = (AkOSChar*)( _astring_ )

	/// Converts a AkOSChar string into wide char string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_OSCHAR_TO_WIDE( _osstring_, _wstring_ ) \
		_wstring_ = (wchar_t*)AkAlloca((1+strlen(_osstring_)) * sizeof(wchar_t)); \
		AKPLATFORM::AkCharToWideChar( _osstring_, (AkUInt32)(1 + strlen(_osstring_ )), _wstring_ )

	/// Converts a AkOSChar string into char string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_OSCHAR_TO_CHAR( _osstring_, _astring_ ) _astring_ = (char*)_osstring_

	/// Get the length, in characters, of a NULL-terminated AkUtf16 string
	/// \return The length, in characters, of the specified string (excluding terminating NULL)
	AkForceInline size_t AkUtf16StrLen( const AkUtf16* in_pStr )
	{
		return ( wcslen( in_pStr ) );
	}

	/// Get the length, in characters, of a NULL-terminated AkOSChar string
	/// \return The length, in characters, of the specified string (excluding terminating NULL)
	AkForceInline size_t OsStrLen( const AkOSChar* in_pszString )
	{
		return ( strlen( in_pszString ) );
	}

	/// AkOSChar version of sprintf().
	#define AK_OSPRINTF snprintf

	/// Compare two NULL-terminated AkOSChar strings
	/// \return
	/// - \< 0 if in_pszString1 \< in_pszString2
	/// -    0 if the two strings are identical
	/// - \> 0 if in_pszString1 \> in_pszString2
	/// \remark The comparison is case-sensitive
	AkForceInline int OsStrCmp( const AkOSChar* in_pszString1, const AkOSChar* in_pszString2 )
	{
		return ( strcmp( in_pszString1,  in_pszString2 ) );
	}
	
	#define AK_UTF16_TO_WCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )
	#define AK_WCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )
	#define AK_UTF16_TO_OSCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkWideCharToChar( in_pSrc, in_MaxSize, in_pdDest )
	#define AK_UTF16_TO_CHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkWideCharToChar( in_pSrc, in_MaxSize, in_pdDest )
	#define AK_CHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkCharToWideChar( in_pSrc, in_MaxSize, in_pdDest )		
	#define AK_OSCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkCharToWideChar( in_pSrc, in_MaxSize, in_pdDest )	

	// Use with AkOSChar.
	#define AK_PATH_SEPARATOR	("/")

}

#ifdef AK_ENABLE_INSTRUMENT

#include <perf.h>
#include <sdk_version.h>
#if SCE_ORBIS_SDK_VERSION >= 0x04500000
	#include <razorcpu.h>
	#ifndef SCE_RAZOR_MARKER_DISABLE_HUD
		#define SCE_RAZOR_MARKER_DISABLE_HUD 0
	#endif
#endif

class AkInstrumentScope
{
public:
	inline AkInstrumentScope( const char *in_pszZoneName ) 
	{
		sceRazorCpuPushMarkerStatic( in_pszZoneName, 0, SCE_RAZOR_MARKER_DISABLE_HUD );
	}

	inline ~AkInstrumentScope()
	{
		sceRazorCpuPopMarker();
	}
};

#define AK_INSTRUMENT_BEGIN( _zone_name_ ) sceRazorCpuPushMarkerStatic( text, 0, SCE_RAZOR_MARKER_DISABLE_HUD )
#define AK_INSTRUMENT_BEGIN_C( _color_, _zone_name_ ) sceRazorCpuPushMarkerStatic( text, _color_, SCE_RAZOR_MARKER_DISABLE_HUD )
#define AK_INSTRUMENT_END( _zone_name_ ) sceRazorCpuPopMarker()
#define AK_INSTRUMENT_SCOPE( _zone_name_ ) AkInstrumentScope akInstrumentScope_##__LINE__(_zone_name_)

#define AK_INSTRUMENT_IDLE_BEGIN( _zone_name_ )
#define AK_INSTRUMENT_IDLE_END( _zone_name_ )
#define AK_INSTRUMENT_IDLE_SCOPE( _zone_name_ )

#define AK_INSTRUMENT_STALL_BEGIN( _zone_name_ )
#define AK_INSTRUMENT_STALL_END( _zone_name_ )
#define AK_INSTRUMENT_STALL_SCOPE( _zone_name_ )

#define AK_INSTRUMENT_THREAD_START( _thread_name_ )

#endif // AK_ENABLE_INSTRUMENT

#endif  // _AK_PLATFORM_FUNCS_H_