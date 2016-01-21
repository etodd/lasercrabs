//////////////////////////////////////////////////////////////////////
//
// AkPlatformFuncs.h
//
// Audiokinetic platform-dependent functions definition.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_PLATFORM_FUNCS_H_
#define _AK_PLATFORM_FUNCS_H_

#include "malloc.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <windows.h>
//#define AK_ENABLE_PERF_RECORDING
#if defined(AK_ENABLE_PERF_RECORDING)
#include <stdio.h>
#endif

#ifdef AK_USE_THREAD_EMULATION
	#include <AK/Tools/Win32/ThreadEmulation.h>
#endif

#if defined(_WIN64)
// on 64 bit, removes warning C4985: 'ceil': attributes not present on previous declaration.
// see http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=294649
#include <math.h>
#endif // _WIN64
#include <intrin.h>

//-----------------------------------------------------------------------------
// Platform-specific thread properties definition.
//-----------------------------------------------------------------------------
struct AkThreadProperties
{
    int                 nPriority;		///< Thread priority
    AkUInt32            dwAffinityMask;	///< Affinity mask
	AkUInt32			uStackSize;		///< Thread stack size.
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
// Defines for Win32.
//-----------------------------------------------------------------------------
#define AK_DECLARE_THREAD_ROUTINE( FuncName )   DWORD WINAPI FuncName(LPVOID lpParameter)
#define AK_THREAD_RETURN( _param_ )				return (_param_);
#define AK_THREAD_ROUTINE_PARAMETER             lpParameter
#define AK_GET_THREAD_ROUTINE_PARAMETER_PTR(type) reinterpret_cast<type*>( AK_THREAD_ROUTINE_PARAMETER )
#define AK_RETURN_THREAD_OK                     0x00000000
#define AK_RETURN_THREAD_ERROR                  0x00000001
#if defined AK_CPU_X86_64
#define AK_DEFAULT_STACK_SIZE					(65536)
#else
#define AK_DEFAULT_STACK_SIZE                   (32768)
#endif
#define AK_THREAD_PRIORITY_NORMAL				THREAD_PRIORITY_NORMAL
#define AK_THREAD_PRIORITY_ABOVE_NORMAL			THREAD_PRIORITY_ABOVE_NORMAL

// NULL objects
#define AK_NULL_THREAD                          NULL

#define AK_INFINITE                             INFINITE

#define AkMakeLong(a,b)							MAKELONG((a),(b))

#define AkMax(x1, x2)	(((x1) > (x2))? (x1): (x2))
#define AkMin(x1, x2)	(((x1) < (x2))? (x1): (x2))
#define AkClamp(x, min, max)  ((x) < (min)) ? (min) : (((x) > (max) ? (max) : (x)))

namespace AKPLATFORM
{
	// Simple automatic event API
    // ------------------------------------------------------------------
	
	/// Platform Independent Helper
	inline void AkClearEvent( AkEvent & out_event )
    {
		out_event = NULL;
	}

	/// Platform Independent Helper
	inline AKRESULT AkCreateEvent( AkEvent & out_event )
    {
#ifdef AK_USE_METRO_API
		out_event = CreateEventEx(nullptr, nullptr, 0, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE);
#else
		out_event = ::CreateEvent( NULL,					// No security attributes
                                    false,					// Reset type: automatic
                                    false,					// Initial signaled state: not signaled
                                    NULL                    // No name
                                   );
#endif
		return ( out_event ) ? AK_Success : AK_Fail;
	}

	/// Platform Independent Helper
	inline void AkDestroyEvent( AkEvent & io_event )
	{
		if ( io_event )
			::CloseHandle( io_event );
		io_event = NULL;
	}

	/// Platform Independent Helper
	inline void AkWaitForEvent( AkEvent & in_event )
	{
#ifdef AK_USE_METRO_API
#ifdef AK_ENABLE_ASSERTS
		DWORD dwWaitResult = 
#endif // AK_ENABLE_ASSERTS
			::WaitForSingleObjectEx( in_event, INFINITE, FALSE );
        AKASSERT( dwWaitResult == WAIT_OBJECT_0 );
#else
        AKVERIFY( ::WaitForSingleObject( in_event, INFINITE ) == WAIT_OBJECT_0 );
#endif
	}

	/// Platform Independent Helper
	inline void AkSignalEvent( const AkEvent & in_event )
	{
		AKVERIFY( ::SetEvent( in_event ) );
	}

	
	// Atomic Operations
    // ------------------------------------------------------------------

	/// Platform Independent Helper
	inline AkInt32 AkInterlockedIncrement( AkInt32 * pValue )
	{
		return InterlockedIncrement( pValue );
	}

	/// Platform Independent Helper
	inline AkInt32 AkInterlockedDecrement( AkInt32 * pValue )
	{
		return InterlockedDecrement( pValue );
	}

#ifdef AK_CPU_X86_64
	inline bool AkInterlockedCompareExchange( volatile AkInt64* io_pDest, AkInt64 in_newValue, AkInt64 in_expectedOldVal )
	{
		return _InterlockedCompareExchange64(io_pDest, in_newValue, in_expectedOldVal) == in_expectedOldVal;
	}
#endif

	inline bool AkInterlockedCompareExchange( volatile AkInt32* io_pDest, AkInt32 in_newValue, AkInt32 in_expectedOldVal )
	{
		return InterlockedCompareExchange(io_pDest, in_newValue, in_expectedOldVal) == in_expectedOldVal;
	}

#if defined AK_CPU_X86 || defined AK_CPU_ARM
	inline bool AkInterlockedCompareExchange( volatile AkIntPtr* io_pDest, AkIntPtr in_newValue, AkIntPtr in_expectedOldVal )
	{
		return InterlockedCompareExchange((volatile LONG_PTR*)io_pDest, (LONG_PTR)in_newValue, (LONG_PTR)in_expectedOldVal) == in_expectedOldVal;
	}
#endif		

	//Ensure that all write operations are complete.  Necessary only on platforms that don't garentee the order of writes.
	inline void AkMemoryBarrier() 
	{
		_ReadWriteBarrier();
	}

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
        AKVERIFY( ::CloseHandle( *in_pThread ) );
        AkClearThread( in_pThread );
    }

#define AkExitThread( _result ) return _result;

	/// Platform Independent Helper
	inline void AkGetDefaultThreadProperties( AkThreadProperties & out_threadProperties )
	{
		out_threadProperties.nPriority = AK_THREAD_PRIORITY_NORMAL;
		out_threadProperties.uStackSize= AK_DEFAULT_STACK_SIZE;
		out_threadProperties.dwAffinityMask = 0;
	}

	/// Set the name of a thread: see http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
	inline void AkSetThreadName( DWORD in_dwThreadID, LPCSTR in_szThreadName )
	{
#if defined AK_USE_THREAD_EMULATION
		UNREFERENCED_PARAMETER( in_dwThreadID );
		UNREFERENCED_PARAMETER( in_szThreadName );
#else
		const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
		typedef struct tagTHREADNAME_INFO
		{
			DWORD dwType;
			LPCSTR szName;
			DWORD dwThreadID;
			DWORD dwFlags;
		} THREADNAME_INFO;
#pragma pack(pop)

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = in_szThreadName;
		info.dwThreadID = in_dwThreadID;
		info.dwFlags = 0;

		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
		}
#pragma warning(suppress: 6312)
		__except(EXCEPTION_CONTINUE_EXECUTION)
		{
		}
#endif
	}

	/// Platform Independent Helper
	inline void AkCreateThread( 
		AkThreadRoutine pStartRoutine,					// Thread routine.
		void * pParams,									// Routine params.
		const AkThreadProperties & in_threadProperties,	// Properties. NULL for default.
		AkThread * out_pThread,							// Returned thread handle.
		const char * in_szThreadName )						// Opt thread name.
    {
		AKASSERT( out_pThread != NULL );
		AKASSERT( (in_threadProperties.nPriority >= THREAD_PRIORITY_LOWEST && in_threadProperties.nPriority <= THREAD_PRIORITY_HIGHEST)
			|| ( in_threadProperties.nPriority == THREAD_PRIORITY_TIME_CRITICAL ) );

#ifdef AK_USE_THREAD_EMULATION
		UNREFERENCED_PARAMETER( in_threadProperties );
		UNREFERENCED_PARAMETER( in_szThreadName );
       *out_pThread = AK::ThreadEmulation::CreateThread( pStartRoutine,                   // Thread start routine
                                       pParams,                         // Thread function parameter
                                       0 );								// Creation flags: create running
#else
		DWORD dwThreadID;
        *out_pThread = ::CreateThread( NULL,							// No security attributes
                                       in_threadProperties.uStackSize,	// StackSize (0 uses system default)
                                       pStartRoutine,                   // Thread start routine
                                       pParams,                         // Thread function parameter
                                       0,								// Creation flags: create running
                                       &dwThreadID );

		// ::CreateThread() return NULL if it fails.
        if ( !*out_pThread )
        {
			AkClearThread( out_pThread );
            return;
        }

        // Set thread name.
        AkSetThreadName( dwThreadID, in_szThreadName );

		// Set properties.
		if ( !::SetThreadPriority( *out_pThread, in_threadProperties.nPriority ) )
        {
            AKASSERT( !"Failed setting IO thread priority" );
			AkCloseThread( out_pThread );
            return;
        }
		if ( in_threadProperties.dwAffinityMask )
        {
#ifndef AK_XBOXONE_ADK
			if ( !::SetThreadAffinityMask( *out_pThread, in_threadProperties.dwAffinityMask ) )
            {
                AKASSERT( !"Failed setting IO thread affinity mask" );
				AkCloseThread( out_pThread );
            }
#endif
		}
#endif
	}

	/// Platform Independent Helper
    inline void AkWaitForSingleThread( AkThread * in_pThread )
    {
        AKASSERT( in_pThread );
        AKASSERT( *in_pThread );
#ifdef AK_USE_METRO_API
        ::WaitForSingleObjectEx( *in_pThread, INFINITE, FALSE );
#else
        ::WaitForSingleObject( *in_pThread, INFINITE );
#endif
    }

	/// Returns the calling thread's ID.
	inline AkThreadID CurrentThread()
	{
		return ::GetCurrentThreadId();
	}

	/// Platform Independent Helper
    inline void AkSleep( AkUInt32 in_ulMilliseconds )
    {
#ifdef AK_USE_THREAD_EMULATION
		AK::ThreadEmulation::Sleep( in_ulMilliseconds );
#else
		::Sleep( in_ulMilliseconds );
#endif
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

    // Time functions
    // ------------------------------------------------------------------

	/// Platform Independent Helper
    inline void PerformanceCounter( AkInt64 * out_piLastTime )
	{
		::QueryPerformanceCounter( (LARGE_INTEGER*)out_piLastTime );
	}

	/// Platform Independent Helper
	inline void PerformanceFrequency( AkInt64 * out_piFreq )
	{
		::QueryPerformanceFrequency( (LARGE_INTEGER*)out_piFreq );
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
		int iWritten = ::WideCharToMultiByte(CP_ACP,																	// code page
									0,																		// performance and mapping flags
									in_pszUnicodeString,													// wide-character string
									(int)AkMin( ( (AkUInt32)wcslen( in_pszUnicodeString )), in_uiOutBufferSize-1 ),	// number of chars in string : -1 = NULL terminated string.
									io_pszAnsiString,														// buffer for new string
									in_uiOutBufferSize,														// size of buffer
									NULL,																	// default for unmappable chars
									NULL);																	// set when default char used
		io_pszAnsiString[iWritten] = 0;
		return iWritten;
	}

	/// String conversion helper
	inline AkInt32 AkCharToWideChar( const char*	in_pszAnsiString,
									 AkUInt32		in_uiOutBufferSize,
									 void*			io_pvUnicodeStringBuffer )
	{
		return ::MultiByteToWideChar(	CP_ACP,								// code page
										0,									// performance and mapping flags
										in_pszAnsiString,					// wide-character string
										-1,									// number of chars in string : -1 = NULL terminated string.
										(wchar_t*)io_pvUnicodeStringBuffer,	// buffer for new string
										in_uiOutBufferSize);				// size of buffer
	}

	/// String conversion helper
	inline AkInt32 AkUtf8ToWideChar( const char*	in_pszUtf8String,
									 AkUInt32		in_uiOutBufferSize,
									 void*			io_pvUnicodeStringBuffer )
	{
		return ::MultiByteToWideChar(	CP_UTF8,							// code page
										0,									// performance and mapping flags
										in_pszUtf8String,					// wide-character string
										-1,									// number of chars in string : -1 = NULL terminated string.
										(wchar_t*)io_pvUnicodeStringBuffer,	// buffer for new string
										in_uiOutBufferSize);				// size of buffer
	}

	/// Safe unicode string copy.
	inline void SafeStrCpy( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iSizeCopy = AkMin( in_uDestMaxNumChars - 1, wcslen( in_pSrc ) + 1 );
		wcsncpy_s( in_pDest, in_uDestMaxNumChars, in_pSrc, iSizeCopy );
		in_pDest[iSizeCopy] = '\0';
	}

	/// Safe string copy.
	inline void SafeStrCpy( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		size_t iSizeCopy = AkMin( in_uDestMaxNumChars - 1, strlen( in_pSrc ) + 1 );
		strncpy_s( in_pDest, in_uDestMaxNumChars, in_pSrc, iSizeCopy );
		in_pDest[iSizeCopy] = '\0';
	}

	/// Safe unicode string concatenation.
	inline void SafeStrCat( wchar_t * in_pDest, const wchar_t* in_pSrc, size_t in_uDestMaxNumChars )
	{
		int iAvailableSize = (int)( in_uDestMaxNumChars - wcslen( in_pDest ) - 1 );
		wcsncat_s( in_pDest, in_uDestMaxNumChars, in_pSrc, AkMin( iAvailableSize, (int)wcslen( in_pSrc ) ) );
	}

	/// Safe string concatenation.
	inline void SafeStrCat( char * in_pDest, const char* in_pSrc, size_t in_uDestMaxNumChars )
	{
		int iAvailableSize = (int)( in_uDestMaxNumChars - strlen( in_pDest ) - 1 );
		strncat_s( in_pDest, in_uDestMaxNumChars, in_pSrc, AkMin( iAvailableSize, (int)strlen( in_pSrc ) ) );
	}

	/// Stack allocations.
	#define AkAlloca( _size_ ) _alloca( _size_ )

	/// Output a debug message on the console
#if ! ( defined(AK_USE_METRO_API) || defined(AK_OPTIMIZED) )
	inline void OutputDebugMsg( const wchar_t* in_pszMsg )
	{
		OutputDebugStringW( in_pszMsg );
	}

	/// Output a debug message on the console
	inline void OutputDebugMsg( const char* in_pszMsg )
	{
		OutputDebugStringA( in_pszMsg );
	}
#else
	inline void OutputDebugMsg( const wchar_t* ){}
	inline void OutputDebugMsg( const char* ){}
#endif

	/// Converts a wchar_t string to an AkOSChar string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_WIDE_TO_OSCHAR( _wstring_, _oscharstring_ ) ( _oscharstring_ ) = (AkOSChar*)( _wstring_ )

	/// Converts a char string to an AkOSChar string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_CHAR_TO_OSCHAR( _astring_, _oscharstring_ ) \
		   _oscharstring_ = (AkOSChar*)AkAlloca( (1 + strlen( _astring_ )) * sizeof(AkOSChar)); \
		   AKPLATFORM::AkCharToWideChar( _astring_, (AkUInt32)(1 + strlen(_astring_ )), (AkOSChar*)( _oscharstring_ ) )

	/// Converts a AkOSChar string into wide char string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_OSCHAR_TO_WIDE( _osstring_, _wstring_ ) _wstring_ = _osstring_

	/// Converts a AkOSChar string into char string.
	/// \remark On some platforms the AkOSChar string simply points to the same string,
	/// on others a new buffer is allocated on the stack using AkAlloca. This means
	/// you must make sure that:
	/// - The source string stays valid and unmodified for as long as you need the
	///   AkOSChar string (for cases where they point to the same string)
	/// - The AkOSChar string is used within this scope only -- for example, do NOT
	///   return that string from a function (for cases where it is allocated on the stack)
	#define CONVERT_OSCHAR_TO_CHAR( _osstring_, _astring_ ) \
			_astring_ = (char*)AkAlloca( 1 + wcslen( _osstring_ )); \
			AKPLATFORM::AkWideCharToChar( _osstring_, AkUInt32(1 + wcslen( _osstring_ )), _astring_ );

	/// Get the length, in characters, of a NULL-terminated AkUtf16 string
	/// \return The length, in characters, of the specified string (excluding terminating NULL)
	inline size_t AkUtf16StrLen( const AkUtf16* in_pStr )
	{
		return ( wcslen( in_pStr ) );
	}

	/// Get the length, in characters, of a NULL-terminated AkOSChar string
	/// \return The length, in characters, of the specified string (excluding terminating NULL)
	inline size_t OsStrLen( const AkOSChar* in_pszString )
	{
		return ( wcslen( in_pszString ) );
	}

	/// AkOSChar version of sprintf().
	#define AK_OSPRINTF swprintf_s

	/// Compare two NULL-terminated AkOSChar strings
	/// \return
	/// - \< 0 if in_pszString1 \< in_pszString2
	/// -    0 if the two strings are identical
	/// - \> 0 if in_pszString1 \> in_pszString2
	/// \remark The comparison is case-sensitive
	inline int OsStrCmp( const AkOSChar* in_pszString1, const AkOSChar* in_pszString2 )
	{
		return ( wcscmp( in_pszString1,  in_pszString2 ) );
	}
	
	#define AK_UTF16_TO_WCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )
	#define AK_WCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )
	#define AK_UTF16_TO_OSCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )
	#define AK_UTF16_TO_CHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkWideCharToChar( in_pSrc, in_MaxSize, in_pdDest )
	#define AK_CHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkCharToWideChar( in_pSrc, in_MaxSize, in_pdDest )
	#define AK_OSCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::SafeStrCpy(		in_pdDest, in_pSrc, in_MaxSize )

	// Use with AkOSChar.
	#define AK_PATH_SEPARATOR	(L"\\")

	#if defined(AK_ENABLE_PERF_RECORDING)
	
		static AkUInt32 g_uAkPerfRecExecCount = 0;	
		static AkReal32 g_fAkPerfRecExecTime = 0.f;

		#define AK_PERF_RECORDING_RESET()	\
			AKPLATFORM::g_uAkPerfRecExecCount = 0;\
			AKPLATFORM::g_fAkPerfRecExecTime = 0.f;

		#define AK_PERF_RECORDING_START( __StorageName__, __uExecutionCountStart__, __uExecutionCountStop__ )						\
			AkInt64 iAkPerfRecTimeBefore;																							\
			if ( (AKPLATFORM::g_uAkPerfRecExecCount >= (__uExecutionCountStart__)) && (AKPLATFORM::g_uAkPerfRecExecCount <= (__uExecutionCountStop__)) )	\
				AKPLATFORM::PerformanceCounter( &iAkPerfRecTimeBefore );

		#define AK_PERF_RECORDING_STOP( __StorageName__, __uExecutionCountStart__, __uExecutionCountStop__ )						\
		if ( (AKPLATFORM::g_uAkPerfRecExecCount >= (__uExecutionCountStart__)) && (AKPLATFORM::g_uAkPerfRecExecCount <= (__uExecutionCountStop__)) )	\
			{																														\
				AkInt64 iAkPerfRecTimeAfter;																						\
				AKPLATFORM::PerformanceCounter( &iAkPerfRecTimeAfter );																\
				AKPLATFORM::g_fAkPerfRecExecTime += AKPLATFORM::Elapsed( iAkPerfRecTimeAfter, iAkPerfRecTimeBefore );							\
				if ( AKPLATFORM::g_uAkPerfRecExecCount == (__uExecutionCountStop__) )															\
				{																													\
					AkReal32 fAverageExecutionTime = AKPLATFORM::g_fAkPerfRecExecTime/((__uExecutionCountStop__)-(__uExecutionCountStart__));	\
					char str[256];																									\
					sprintf_s(str, 256, "%s average execution time: %f\n", __StorageName__, fAverageExecutionTime);					\
					AKPLATFORM::OutputDebugMsg( str );																				\
				}																													\
			}																														\
			AKPLATFORM::g_uAkPerfRecExecCount++;
	#endif // AK_ENABLE_PERF_RECORDING
}

#endif  // _AK_PLATFORM_FUNCS_H_