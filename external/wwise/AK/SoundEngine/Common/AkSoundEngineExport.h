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

#ifndef AK_DLLEXPORT
#define AK_DLLEXPORT
#endif

#ifndef AK_DLLIMPORT
#define AK_DLLIMPORT
#endif

#ifdef AKSOUNDENGINE_DLL
	#ifdef AKSOUNDENGINE_EXPORTS
	/// Sound Engine API import/export definition
	#define AKSOUNDENGINE_API AK_DLLEXPORT
	#else
	/// Sound Engine API import/export definition		
	#define AKSOUNDENGINE_API AK_DLLIMPORT
	#endif // Export
#else
	#define AKSOUNDENGINE_API	
#endif

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
