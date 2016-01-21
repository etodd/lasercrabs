//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_AKASSERT_H_
#define _AK_AKASSERT_H_

#if defined( _DEBUG ) && !(defined AK_DISABLE_ASSERTS)
	#ifndef AK_ENABLE_ASSERTS
		#define AK_ENABLE_ASSERTS
	#endif
#endif

#if ! defined( AKASSERT )

	#include <AK/SoundEngine/Common/AkTypes.h> //For AK_Fail/Success
	#include <AK/SoundEngine/Common/AkSoundEngineExport.h>

	#if defined( AK_ENABLE_ASSERTS )

		#if defined( __SPU__ )

			#if defined ( _DEBUG )
				// Note: No assert hook on SPU
				#include "spu_printf.h"
				#include "libsn_spu.h"
				#define AKASSERT(Condition)																\
					if ( !(Condition) )																	\
					{																					\
						spu_printf( "Assertion triggered in file %s at line %d\n", __FILE__, __LINE__ );\
						/*snPause();*/																	\
					}																	
			#else
				#define AKASSERT(Condition)
			#endif

		#else // defined( __SPU__ )

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
			#if defined( AK_APPLE ) || defined( AK_ANDROID ) || defined( AK_WII_FAMILY )

				#define AKASSERT(Condition) ((Condition) ? ((void) 0) : g_pAssertHook( #Condition, __FILE__, __LINE__) )

			#else
				
				#include <assert.h>

				#define _AkAssertHook(_Expression) (void)( (_Expression) || (g_pAssertHook( #_Expression, __FILE__, __LINE__), 0) )

				#define AKASSERT(Condition) if ( g_pAssertHook )          \
												_AkAssertHook(Condition); \
											else                          \
												assert(Condition)

			#endif // defined( AK_WII )

		#endif // defined( __SPU__ )

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

#endif //_AK_AKASSERT_H_

