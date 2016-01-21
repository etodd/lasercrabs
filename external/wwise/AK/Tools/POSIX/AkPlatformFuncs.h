//////////////////////////////////////////////////////////////////////
//
// AkPlatformFuncs.h 
//
// Audiokinetic platform-dependent functions definition.
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/AkTypes.h>

#include <pthread.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

#define AK_POSIX_NO_ERR 0
#define AK_POSIX

//-----------------------------------------------------------------------------
// Platform-specific thread properties definition.
//-----------------------------------------------------------------------------
struct AkThreadProperties
{
    int                 nPriority;		///< Thread priority
	size_t				uStackSize;		///< Thread stack size
	int					uSchedPolicy;	///< Thread scheduling policy
	AkUInt32			dwAffinityMask;	///< Affinity mask
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

#ifdef AK_NACL
#define sched_get_priority_max( _a ) 0
#define sched_get_priority_min( _a ) 0
#endif

//-----------------------------------------------------------------------------
// Defines for POSIX (Mac, iOS, Android)
//-----------------------------------------------------------------------------
#define AK_DECLARE_THREAD_ROUTINE( FuncName )   void* FuncName(void* lpParameter)
#define AK_THREAD_RETURN( _param_ )				return (_param_);
#define AK_THREAD_ROUTINE_PARAMETER             lpParameter
#define AK_GET_THREAD_ROUTINE_PARAMETER_PTR(type) reinterpret_cast<type*>( AK_THREAD_ROUTINE_PARAMETER )
#define AK_RETURN_THREAD_OK                     0x00000000
#define AK_RETURN_THREAD_ERROR                  0x00000001

// WG-21467
// For GameSim to play AAC soundbanks on Mac, stacksize has to be bigger to avoid crashing.
#ifdef AK_APPLE
	#define AK_DEFAULT_STACK_SIZE                   (32768*2)
#else
	#define AK_DEFAULT_STACK_SIZE                   (32768)
#endif

#define AK_THREAD_DEFAULT_SCHED_POLICY			SCHED_FIFO
#define AK_THREAD_PRIORITY_NORMAL				(((sched_get_priority_max( SCHED_FIFO ) - sched_get_priority_min( SCHED_FIFO )) / 2) + sched_get_priority_min( SCHED_FIFO ))
#define AK_THREAD_PRIORITY_ABOVE_NORMAL			sched_get_priority_max( SCHED_FIFO )
#define AK_THREAD_PRIORITY_BELOW_NORMAL			sched_get_priority_min( SCHED_FIFO )
#define AK_THREAD_AFFINITY_DEFAULT				0xFFFF


// NULL objects
#define AK_NULL_THREAD                          0

#define AK_INFINITE                             (AK_UINT_MAX)

#define AkMakeLong(a,b)							MAKELONG((a),(b))

#define AkMax(x1, x2)	(((x1) > (x2))? (x1): (x2))
#define AkMin(x1, x2)	(((x1) < (x2))? (x1): (x2))
#define AkClamp(x, min, max)  ((x) < (min)) ? (min) : (((x) > (max) ? (max) : (x)))

#pragma GCC visibility push(hidden)

namespace AKPLATFORM
{
	// Simple automatic event API
    // ------------------------------------------------------------------
#ifndef AK_APPLE	
	/// Platform Independent Helper
	inline void AkClearEvent( AkEvent & out_event )
    {
		memset(&out_event,0,sizeof(AkEvent));
	}
	
	/// Platform Independent Helper
	inline AKRESULT AkCreateEvent( AkEvent & out_event )
    {
		int ret = sem_init(	
							&out_event,
							0,
							0 );
		
		return ( ret == AK_POSIX_NO_ERR  ) ? AK_Success : AK_Fail;
	}

	/// Platform Independent Helper
	inline void AkDestroyEvent( AkEvent & io_event )
	{
		AKVERIFY( sem_destroy( &io_event ) == AK_POSIX_NO_ERR);
		AkClearEvent(io_event); 
	}

	/// Platform Independent Helper
	inline void AkWaitForEvent( AkEvent & in_event )
	{
		AKVERIFY( sem_wait( &in_event ) == AK_POSIX_NO_ERR );
	
	}

	/// Platform Independent Helper
	inline void AkSignalEvent( AkEvent & in_event )
	{
		AKVERIFY( sem_post( &in_event ) == AK_POSIX_NO_ERR );
	}
#endif	
    // Threads
    // ------------------------------------------------------------------

	/// Platform Independent Helper
	inline bool AkIsValidThread( AkThread * in_pThread )
	{
		return (*in_pThread != AK_NULL_THREAD);
	}

	/// Platform Independent Helper
	inline void AkClearThread( AkThread * in_pThread )
	{
		*in_pThread = AK_NULL_THREAD;
	}

	/// Platform Independent Helper
    inline void AkCloseThread( AkThread * in_pThread )
    {
        AKASSERT( in_pThread );
        AKASSERT( *in_pThread );
		
        AkClearThread( in_pThread );
    }

#define AkExitThread( _result ) return _result;

	/// Platform Independent Helper
	inline void AkGetDefaultThreadProperties( AkThreadProperties & out_threadProperties )
	{
		out_threadProperties.uStackSize		= AK_DEFAULT_STACK_SIZE;
		out_threadProperties.uSchedPolicy	= AK_THREAD_DEFAULT_SCHED_POLICY;
		out_threadProperties.nPriority		= AK_THREAD_PRIORITY_NORMAL;
		out_threadProperties.dwAffinityMask = AK_THREAD_AFFINITY_DEFAULT;	
	}

#ifndef AK_ANDROID
	/// Platform Independent Helper
	inline void AkCreateThread( 
		AkThreadRoutine pStartRoutine,					// Thread routine.
		void * pParams,									// Routine params.
		const AkThreadProperties & in_threadProperties,	// Properties. NULL for default.
		AkThread * out_pThread,							// Returned thread handle.
		const char * /*in_szThreadName*/ )				// Opt thread name.
    {
		AKASSERT( out_pThread != NULL );
		
		pthread_attr_t  attr;
		
		// Create the attr
		AKVERIFY(!pthread_attr_init(&attr));
		// Set the stack size
		AKVERIFY(!pthread_attr_setstacksize(&attr,in_threadProperties.uStackSize));
		
		AKVERIFY(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
		
#ifndef AK_NACL		
		// Try to set the thread policy
		int sched_policy = in_threadProperties.uSchedPolicy;
		if( pthread_attr_setschedpolicy( &attr, sched_policy )  )
		{
			AKASSERT( !"AKCreateThread invalid sched policy, will automatically set it to FIFO scheduling" );
			sched_policy = AK_THREAD_DEFAULT_SCHED_POLICY;
			AKVERIFY( !pthread_attr_setschedpolicy( &attr, sched_policy ));
		}

		// Get the priority for the policy
		int minPriority, maxPriority;
		minPriority = sched_get_priority_min(sched_policy);
		maxPriority = sched_get_priority_max(sched_policy);
		
		// Set the thread priority if valid
		AKASSERT( in_threadProperties.nPriority >= minPriority && in_threadProperties.nPriority <= maxPriority );
		if(  in_threadProperties.nPriority >= minPriority && in_threadProperties.nPriority <= maxPriority )
		{
			sched_param schedParam;
			AKVERIFY( !pthread_attr_getschedparam(&attr, &schedParam));
			schedParam.sched_priority = in_threadProperties.nPriority;
			AKVERIFY( !pthread_attr_setschedparam( &attr, &schedParam ));
		}
#endif
#ifdef AK_APPLE
		int inherit;
		pthread_attr_getinheritsched(&attr, &inherit );
		pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED );
#endif
		// Create the tread
		int     threadError = pthread_create( out_pThread, &attr, pStartRoutine, pParams);
		AKASSERT( threadError == 0 );
		AKVERIFY(!pthread_attr_destroy(&attr));
		
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
#endif

	/// Platform Independent Helper
    inline void AkWaitForSingleThread( AkThread * in_pThread )
    {
        AKASSERT( in_pThread );
        AKASSERT( *in_pThread );
		AKVERIFY(!pthread_join( *in_pThread, NULL ));
    }

	/// Returns the calling thread's ID.
	inline AkThreadID CurrentThread()
	{
		return pthread_self();
	}

	/// Platform Independent Helper
    inline void AkSleep( AkUInt32 in_ulMilliseconds )
    {
		// usleep in micro second
		usleep( in_ulMilliseconds * 1000 );
    }

	// Optimized memory functions
	// --------------------------------------------------------------------

	/// Platform Independent Helper
	inline void AkMemCpy( void * pDest, const void * pSrc, AkUInt32 uSize )
	{
		memcpy( pDest, pSrc, uSize );
	}

	/// Platform Independent Helper
	inline void AkMemSet( void * pDest, AkInt32 iVal, AkUInt32 uSize )
	{
		memset( pDest, iVal, uSize );
	}

	/// Platform Independent Helper
    inline void UpdatePerformanceFrequency()
	{
        AkInt64 iFreq;
        PerformanceFrequency( &iFreq );
        AK::g_fFreqRatio = (AkReal32)( iFreq / 1000 );
	}

	/// Returns a time range in milliseconds, using the sound engine's updated count->milliseconds ratio.
    inline AkReal32 Elapsed( const AkInt64 & in_iNow, const AkInt64 & in_iStart )
    {
        return ( in_iNow - in_iStart ) / AK::g_fFreqRatio;
    }

	/// String conversion helper
	inline AkInt32 AkWideCharToChar( const wchar_t*	in_pszUnicodeString,
									 AkUInt32	in_uiOutBufferSize,
									 char*	io_pszAnsiString )
	{
		AKASSERT( io_pszAnsiString != NULL );
		io_pszAnsiString[0] = 0;

		AkInt32 ConvRet = (AkInt32) wcstombs(io_pszAnsiString,  in_pszUnicodeString, in_uiOutBufferSize);

		return ConvRet;
	}

#ifdef AK_SUPPORT_WCHAR	
	/// String conversion helper
	inline AkInt32 AkCharToWideChar( const char*	in_pszAnsiString,
									 AkUInt32		in_uiOutBufferSize,
									 wchar_t*			io_pvUnicodeStringBuffer )
	{
		AKASSERT( io_pvUnicodeStringBuffer != NULL );
		io_pvUnicodeStringBuffer[0] = 0;

		AkInt32 ConvRet = (AkInt32) mbstowcs((wchar_t *)io_pvUnicodeStringBuffer, in_pszAnsiString, in_uiOutBufferSize );

		return ConvRet;
	}

	inline AkInt32 AkUtf8ToWideChar( const char*	in_pszUtf8String,
									 AkUInt32		in_uiOutBufferSize,
									 void*			io_pvUnicodeStringBuffer )
	{
		return AkCharToWideChar( in_pszUtf8String, in_uiOutBufferSize, (wchar_t*)io_pvUnicodeStringBuffer );
	}
#endif
	/// Safe unicode string copy.
	inline void SafeStrCpy( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iSizeCopy = AkMin( in_uDestMaxNumChars - 1, wcslen( in_pSrc ) + 1 );
		wcsncpy( in_pDest, in_pSrc, iSizeCopy );
		in_pDest[iSizeCopy] = '\0';
	}

	/// Safe ansi string copy.
	inline void SafeStrCpy( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iSizeCopy = AkMin( in_uDestMaxNumChars - 1, strlen( in_pSrc ) + 1 );
		strncpy( in_pDest, in_pSrc, iSizeCopy );
		in_pDest[iSizeCopy] = '\0';
	}

	/// Safe unicode string concatenation.
	inline void SafeStrCat( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iAvailableSize = in_uDestMaxNumChars - wcslen( in_pDest ) - 1;
		wcsncat( in_pDest, in_pSrc, AkMin( iAvailableSize, wcslen( in_pSrc ) ) );
	}

	/// Safe ansi string concatenation.
	inline void SafeStrCat( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iAvailableSize = in_uDestMaxNumChars - strlen( in_pDest ) - 1;
		strncat( in_pDest, in_pSrc, AkMin( iAvailableSize, strlen( in_pSrc ) ) );
	}
	
	/// Get the length, in characters, of a NULL-terminated AkUtf16 string
	/// \return The length, in characters, of the specified string (excluding terminating NULL)
	inline size_t AkUtf16StrLen( const AkUtf16* in_pStr )
	{
		size_t len = 0;
		while( *in_pStr != 0 )
		{
			in_pStr++;
			len++;
		}
		return len;
	}

#ifndef AK_ANDROID
	#ifndef AK_OPTIMIZED	
		/// Output a debug message on the console (Unicode string)
		inline void OutputDebugMsg( const wchar_t* in_pszMsg )
		{
			fputws( in_pszMsg, stderr );
		}

		/// Output a debug message on the console (Ansi string)
		inline void OutputDebugMsg( const char* in_pszMsg )
		{
			fputs( in_pszMsg, stderr );
		}
	#else
	inline void OutputDebugMsg( const wchar_t* ){}
	inline void OutputDebugMsg( const char* ){}
	#endif
#endif

	/// Converts a wchar_t string to an AkOSChar string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_WIDE_TO_OSCHAR( _astring_, _oscharstring_ ) \
		_oscharstring_ = (AkOSChar*)AkAlloca( (1 + wcslen( _astring_ )) * sizeof(AkOSChar)); \
		AKPLATFORM::AkWideCharToChar( _astring_, (AkUInt32)(1 + wcslen(_astring_ )), (AkOSChar*)( _oscharstring_ ) )
	
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
	
	inline size_t OsStrLen( const AkOSChar* in_pszString )
	{
		return ( strlen( in_pszString ) );
	}

	/// AkOSChar version of snprintf().
	#define AK_OSPRINTF snprintf

	inline int OsStrCmp( const AkOSChar* in_pszString1, const AkOSChar* in_pszString2 )
	{
		return ( strcmp( in_pszString1,  in_pszString2 ) );
	}

	// Use with AkOSChar.
	#define AK_PATH_SEPARATOR	("/")
	
}

#pragma GCC visibility pop

