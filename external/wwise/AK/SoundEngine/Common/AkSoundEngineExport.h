//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSoundEngineExport.h

/// \file 
/// Export/calling convention definitions.

#ifndef _AK_SOUNDENGINE_EXPORT_H_
#define _AK_SOUNDENGINE_EXPORT_H_

#include <AK/AkPlatforms.h>

#if defined ( AK_WIN ) && defined ( AKSOUNDENGINE_DLL )

    // DLL exports.

    // Sound Engine
    #ifdef AKSOUNDENGINE_EXPORTS
		/// Sound Engine API import/export definition
        #define AKSOUNDENGINE_API __declspec(dllexport)
    #else
		/// Sound Engine API import/export definition
        #define AKSOUNDENGINE_API __declspec(dllimport)
    #endif // Export
#elif defined( AK_LINUX )

    #ifdef AKSOUNDENGINE_EXPORTS
		/// Sound Engine API import/export definition
        #define AKSOUNDENGINE_API __attribute__ ((visibility ("default")))
	#else
		#define AKSOUNDENGINE_API
    #endif  
#else // defined ( AK_WIN ) && defined ( AKSOUNDENGINE_DLL )

    // Static libs.

	/// Sound Engine API import/export definition
    #define AKSOUNDENGINE_API	

#endif // defined ( AK_WIN ) && defined ( AKSOUNDENGINE_DLL )

#ifndef AKSOUNDENGINE_CALL
	#define AKSOUNDENGINE_CALL	
#endif

/// Declare a function
/// \param __TYPE__	Return type of the function
/// \param __NAME__ Name of the function
/// \remarks This must be followed by the parentheses containing the function arguments declaration
#define AK_FUNC( __TYPE__, __NAME__ ) __TYPE__ AKSOUNDENGINE_CALL __NAME__

/// Declare an extern function
/// \param __TYPE__	Return type of the function
/// \param __NAME__ Name of the function
/// \remarks This must be followed by the parentheses containing the function arguments declaration
#define AK_EXTERNFUNC( __TYPE__, __NAME__ ) extern __TYPE__ AKSOUNDENGINE_CALL __NAME__

/// Declare an extern function that is exported/imported
/// \param __TYPE__	Return type of the function
/// \param __NAME__ Name of the function
/// \remarks This must be followed by the parentheses containing the function arguments declaration
#define AK_EXTERNAPIFUNC( __TYPE__, __NAME__ ) extern AKSOUNDENGINE_API __TYPE__ AKSOUNDENGINE_CALL __NAME__

/// Declare a callback function type
/// \param __TYPE__	Return type of the function
/// \param __NAME__ Name of the function
/// \remarks This must be followed by the parentheses containing the function arguments declaration
#define AK_CALLBACK( __TYPE__, __NAME__ ) typedef __TYPE__ ( AKSOUNDENGINE_CALL *__NAME__ )

#endif  //_AK_SOUNDENGINE_EXPORT_H_
