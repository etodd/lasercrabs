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

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AK_AKASSERT_H_
#define _AK_AKASSERT_H_

#if defined( _DEBUG ) && !(defined AK_DISABLE_ASSERTS)
	#ifndef AK_ENABLE_ASSERTS
		#define AK_ENABLE_ASSERTS
	#endif
#endif

#if !defined( AKASSERT )

	#include <AK/SoundEngine/Common/AkTypes.h> //For AK_Fail/Success
	#include <AK/SoundEngine/Common/AkSoundEngineExport.h>

	#if defined( AK_ENABLE_ASSERTS )

		#ifndef AK_ASSERT_HOOK
			AK_CALLBACK( void, AkAssertHook)( 
									const char * in_pszExpression,	///< Expression
									const char * in_pszFileName,	///< File Name
									int in_lineNumber				///< Line Number
									);
			#define AK_ASSERT_HOOK
		#endif

		extern AKSOUNDENGINE_API AkAssertHook g_pAssertHook;

		// These platforms use a built-in g_pAssertHook (and do not fall back to the regular assert macro)
		#define AKASSERT(Condition) ((Condition) ? ((void) 0) : g_pAssertHook( #Condition, __FILE__, __LINE__) )


		#define AKVERIFY AKASSERT

		#ifdef _DEBUG
			#define AKASSERTD AKASSERT
		#else
			#define AKASSERTD(Condition) ((void)0)
		#endif		

	#else //  defined( AK_ENABLE_ASSERTS )

		#define AKASSERT(Condition) ((void)0)
		#define AKASSERTD(Condition) ((void)0)
		#define AKVERIFY(x) ((void)(x))		

	#endif //  defined( AK_ENABLE_ASSERTS )

	#define AKASSERT_RANGE(Value, Min, Max) (AKASSERT(((Value) >= (Min)) && ((Value) <= (Max))))

	#define AKASSERTANDRETURN( __Expression, __ErrorCode )\
		if (!(__Expression))\
		{\
			AKASSERT(__Expression);\
			return __ErrorCode;\
		}\

	#define AKASSERTPOINTERORFAIL( __Pointer ) AKASSERTANDRETURN( __Pointer != NULL, AK_Fail )
	#define AKASSERTSUCCESSORRETURN( __akr ) AKASSERTANDRETURN( __akr == AK_Success, __akr )

	#define AKASSERTPOINTERORRETURN( __Pointer ) \
		if ((__Pointer) == NULL)\
		{\
			AKASSERT((__Pointer) == NULL);\
			return ;\
		}\

	#if defined( AK_WIN ) && ( _MSC_VER >= 1600 )
		// Compile-time assert
		#define AKSTATICASSERT( __expr__, __msg__ ) static_assert( (__expr__), (__msg__) )
	#else
		// Compile-time assert
		#define AKSTATICASSERT( __expr__, __msg__ ) typedef char __AKSTATICASSERT__[(__expr__)?1:-1]
	#endif	

#endif // ! defined( AKASSERT )

#ifdef AK_ENABLE_ASSERTS


//Do nothing. This is a dummy function, so that g_pAssertHook is never NULL.
#define DEFINEDUMMYASSERTHOOK void AkAssertHookFunc( \
const char* in_pszExpression,\
const char* in_pszFileName,\
int in_lineNumber)\
{\
\
}\
AkAssertHook g_pAssertHook = AkAssertHookFunc;
#else
#define DEFINEDUMMYASSERTHOOK

#endif

// Compile-time assert.  Usage:
// AkStaticAssert<[your boolean expression here]>::Assert();
// Example: 
// AkStaticAssert<sizeof(MyStruct) == 20>::Assert();	//If you hit this, you changed the size of MyStruct!
// Empty default template
template <bool b>
struct AkStaticAssert {};

// Template specialized on true
template <>
struct AkStaticAssert<true>
{
	static void Assert() {}
};

#endif

