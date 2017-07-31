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

  Version: v2017.1.0  Build: 6301
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/AkTypes.h>

#include <mach/task.h>
#include <mach/semaphore.h>
#include <CoreFoundation/CFString.h>
#include <libkern/OSAtomic.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <wchar.h>

namespace AKPLATFORM
{
    extern inline size_t AkUtf16StrLen( const AkUtf16* in_pStr );
	// Simple automatic event API
    // ------------------------------------------------------------------
	
	/// Platform Independent Helper
	inline void AkClearEvent( AkEvent & out_event )
    {
		out_event = 0;
	}
	
	/// Platform Independent Helper
	inline AKRESULT AkCreateEvent( AkEvent & out_event )
    {
		kern_return_t ret = semaphore_create(	
							mach_task_self(),
							&out_event,
							SYNC_POLICY_FIFO,
							0 );
		
		return ( ret == noErr  ) ? AK_Success : AK_Fail;
	}

	inline AKRESULT AkCreateSemaphore( AkSemaphore & out_semaphore, AkUInt32 in_initialCount )
	{
		kern_return_t ret = semaphore_create(	
							mach_task_self(),
							&out_semaphore,
							SYNC_POLICY_FIFO,
							in_initialCount );
		
		return ( ret == noErr  ) ? AK_Success : AK_Fail;	
	}
	
	/// Platform Independent Helper
	inline void AkDestroyEvent( AkEvent & io_event )
	{
		if( io_event != 0 )
		{
			AKVERIFY( semaphore_destroy( mach_task_self(), io_event ) == noErr);
		}
		io_event = 0;
	}

	/// Platform Independent Helper
	inline void AkDestroySemaphore( AkSemaphore & io_semaphore )
	{
		if( io_semaphore != 0 )
		{
			AKVERIFY( semaphore_destroy( mach_task_self(), io_semaphore ) == noErr);
		}
		io_semaphore = 0;
	}	
	
	/// Platform Independent Helper
	inline void AkWaitForEvent( AkEvent & in_event )
	{
		AKVERIFY( semaphore_wait( in_event ) == noErr );
	}
	
	inline void AkWaitForSemaphore( AkSemaphore & in_semaphore )
	{
		AKVERIFY( semaphore_wait( in_semaphore ) == noErr );
	}

	/// Platform Independent Helper
	inline void AkSignalEvent( const AkEvent & in_event )
	{
		AKVERIFY( semaphore_signal( in_event ) == noErr );
	}
	
	inline void AkReleaseSemaphore( const AkSemaphore & in_event )
	{
		AKVERIFY( semaphore_signal( in_event ) == noErr );
	}
	
	// Atomic Operations
    // ------------------------------------------------------------------

	/// Platform Independent Helper
	inline AkInt32 AkInterlockedIncrement( AkAtomic32 * pValue )
	{
#ifdef AK_USE_STD_ATOMIC
        return __c11_atomic_fetch_add( pValue,1,__ATOMIC_SEQ_CST );
#else
		return OSAtomicIncrement32( pValue );
#endif
	}

	/// Platform Independent Helper
	inline AkInt32 AkInterlockedDecrement( AkAtomic32 * pValue )
	{
#ifdef AK_USE_STD_ATOMIC
        return __c11_atomic_fetch_sub( pValue,1,__ATOMIC_SEQ_CST );
#else
		return OSAtomicDecrement32( pValue );
#endif
	}

	inline bool AkInterlockedCompareExchange( volatile AkAtomic32* io_pDest, AkInt32 in_newValue, AkInt32 in_expectedOldVal )
	{
#ifdef AK_USE_STD_ATOMIC
        return __c11_atomic_compare_exchange_strong( io_pDest, &in_expectedOldVal, in_newValue, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
#else
		return OSAtomicCompareAndSwapInt(in_expectedOldVal, in_newValue, io_pDest);
#endif
	}

	inline bool AkInterlockedCompareExchange( volatile AkAtomic64* io_pDest, AkInt64 in_newValue, AkInt64 in_expectedOldVal )
	{
#ifdef AK_USE_STD_ATOMIC
        return __c11_atomic_compare_exchange_strong( io_pDest, &in_expectedOldVal, in_newValue, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
#else
		return OSAtomicCompareAndSwap64(in_expectedOldVal, in_newValue, io_pDest);
#endif
	}

	inline bool AkInterlockedCompareExchange( volatile AkAtomicPtr* io_pDest, AkIntPtr in_newValue, AkIntPtr in_expectedOldVal )
	{
#ifdef AK_USE_STD_ATOMIC
	#ifndef AK_IOS
		if (sizeof(io_pDest) == 8)
			return AkInterlockedCompareExchange( (volatile AkAtomic64*)io_pDest, (AkInt64)in_newValue, (AkInt64)in_expectedOldVal );
	#endif // #ifndef AK_IOS
		return AkInterlockedCompareExchange( (volatile AkAtomic32*)io_pDest, (AkInt32)in_newValue, (AkInt32)in_expectedOldVal );
#else
	#ifndef AK_IOS
		if (sizeof(io_pDest) == 8)
			return OSAtomicCompareAndSwap64(( AkInt64)in_expectedOldVal, (AkInt64)in_newValue, (volatile AkInt64*)io_pDest);
	#endif // #ifndef AK_IOS
		return OSAtomicCompareAndSwapInt((AkInt32)in_expectedOldVal, (AkInt32)in_newValue, (AkInt32*)io_pDest);
#endif
	}

	inline void AkMemoryBarrier()
	{
#ifdef AK_USE_STD_ATOMIC
        atomic_thread_fence( __ATOMIC_SEQ_CST );
#else
		OSMemoryBarrier();
#endif
	}
	
    // Time functions
    // ------------------------------------------------------------------

	/// Platform Independent Helper
    inline void PerformanceCounter( AkInt64 * out_piLastTime )
	{
		*out_piLastTime = mach_absolute_time();
	}

	/// Platform Independent Helper
	inline void PerformanceFrequency( AkInt64 * out_piFreq )
	{
		static mach_timebase_info_data_t    sTimebaseInfo;
		mach_timebase_info(&sTimebaseInfo);
		if ( sTimebaseInfo.numer !=0 )
		{
			*out_piFreq = AkInt64((1E9 * sTimebaseInfo.denom) / sTimebaseInfo.numer );
		}
		else
		{
			*out_piFreq = 0;
		}
	}


	template<class destType, class srcType>
	inline size_t AkMacConvertString( destType* in_pdDest, const srcType* in_pSrc, size_t in_MaxSize, size_t destStrLen(const destType *),  size_t srcStrLen(const srcType *) )
	{ 
		CFStringBuiltInEncodings dstEncoding;		
		CFStringBuiltInEncodings srcEncoding;
		switch(sizeof(destType))
		{
			case 1:
				dstEncoding = kCFStringEncodingUTF8;
				break;
			case 2:
				dstEncoding = kCFStringEncodingUTF16LE;
				break;
			case 4:
				dstEncoding = kCFStringEncodingUTF32LE;
				break;
			default:
				AKASSERT(!"Invalid Char size");
		}
		
		switch(sizeof(srcType))
		{
			case 1:
				srcEncoding = kCFStringEncodingUTF8;
				break;
			case 2:
				srcEncoding = kCFStringEncodingUTF16LE;
				break;
			case 4:
				srcEncoding = kCFStringEncodingUTF32LE;
				break;
			default:
				AKASSERT(!"Invalid Char size");
		}
		
		CFStringRef strRef;
		strRef = CFStringCreateWithBytes(	nil, 
										 (UInt8 *) in_pSrc,
										 (srcStrLen( in_pSrc ) + 1) * sizeof(srcType),
										 srcEncoding,
										 false );
		CFRange rangeToProcess = CFRangeMake(0, CFStringGetLength(strRef));
		CFIndex sizeConverted = CFStringGetBytes(strRef, rangeToProcess, dstEncoding, '?', false, (UInt8 *)in_pdDest , in_MaxSize * sizeof(destType), NULL);
        
        //WG-28497 Memory leak when converting strings on Mac & iOS
        CFRelease(strRef);
        
        return sizeConverted;
	}

	#define CONVERT_UTF16_TO_WCHAR( _astring_, _wcharstring_ ) \
		_wcharstring_ = (wchar_t*)AkAlloca( (1 + AKPLATFORM::AkUtf16StrLen((const AkUtf16*)_astring_)) * sizeof(wchar_t) ); \
		AK_UTF16_TO_WCHAR(	_wcharstring_, (const AkUtf16*)_astring_, AKPLATFORM::AkUtf16StrLen((const AkUtf16*)_astring_)+1 )
	
	#define CONVERT_WCHAR_TO_UTF16( _astring_, _utf16string_ ) \
		_utf16string_ = (AkUtf16*)AkAlloca( (1 + wcslen(_astring_)) * sizeof(AkUtf16) ); \
		AK_WCHAR_TO_UTF16(	_utf16string_, (const wchar_t*)_astring_, wcslen(_astring_)+1 )

	#define CONVERT_OSCHAR_TO_UTF16( _astring_, _utf16string_ ) \
		_utf16string_ = (AkUtf16*)AkAlloca( (1 + strlen(_astring_)) * sizeof(AkUtf16) ); \
		AK_OSCHAR_TO_UTF16(	_utf16string_, (const AkOSChar*)_astring_, strlen(_astring_)+1 )

	#define CONVERT_UTF16_TO_OSCHAR( _astring_, _oscharstring_ ) \
		_oscharstring_ = (AkOSChar*)AkAlloca( (1 + AKPLATFORM::AkUtf16StrLen((const AkUtf16*)_astring_)) * sizeof(AkOSChar) ); \
		AK_UTF16_TO_OSCHAR(	_oscharstring_, (const AkUtf16*)_astring_, AKPLATFORM::AkUtf16StrLen((const AkUtf16*)_astring_)+1 ) 

	#define AK_UTF16_TO_WCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<wchar_t, AkUtf16>(	in_pdDest, in_pSrc, in_MaxSize, &wcslen , &AKPLATFORM::AkUtf16StrLen)
	#define AK_WCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<AkUtf16, wchar_t>(	in_pdDest, in_pSrc, in_MaxSize, &AKPLATFORM::AkUtf16StrLen, &wcslen )
	#define AK_UTF16_TO_OSCHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<AkOSChar, AkUtf16>(	in_pdDest, in_pSrc, in_MaxSize, strlen, AKPLATFORM::AkUtf16StrLen )
	#define AK_UTF16_TO_CHAR(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<char, AkUtf16>(	in_pdDest, in_pSrc, in_MaxSize, strlen, AKPLATFORM::AkUtf16StrLen )
	#define AK_CHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<AkUtf16, char>(	in_pdDest, in_pSrc, in_MaxSize, AKPLATFORM::AkUtf16StrLen, strlen)	
	#define AK_OSCHAR_TO_UTF16(	in_pdDest, in_pSrc, in_MaxSize )	AKPLATFORM::AkMacConvertString<AkUtf16, AkOSChar>(	in_pdDest, in_pSrc, in_MaxSize, AKPLATFORM::AkUtf16StrLen, strlen)	
	
	/// Stack allocations.
	#define AkAlloca( _size_ ) alloca( _size_ )
}