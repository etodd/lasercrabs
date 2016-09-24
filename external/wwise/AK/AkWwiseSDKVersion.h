//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKWWISESDKVERSION_H_
#define _AKWWISESDKVERSION_H_

/// \file 
/// Audiokinetic Wwise SDK version, build number and copyright defines. These
/// are used by sample projects to display the version and to include it in DLL or
/// EXE resources. They can also be used by games or tools to display the current
/// version and build number of the Wwise Sound Engine.

/// @name Wwise SDK Version - Numeric values
//@{

/// Wwise SDK major version
#define AK_WWISESDK_VERSION_MAJOR				2016

/// Wwise SDK minor version
#define AK_WWISESDK_VERSION_MINOR				1

/// Wwise SDK sub-minor version
#define AK_WWISESDK_VERSION_SUBMINOR			2

/// Wwise SDK build number
#define AK_WWISESDK_VERSION_BUILD				5863

/// Wwise SDK build date (year)
#define AK_WWISESDK_BUILD_YEAR					2016

/// Wwise SDK build date (month)
#define AK_WWISESDK_BUILD_MONTH					8

/// Wwise SDK build date (day)
#define AK_WWISESDK_BUILD_DAY					29

//@}

/// @name Wwise SDK Version - String values
//@{

/// Macro that "converts" a numeric define to a string
/// \sa
/// - \ref AK_WWISESDK_NUM2STRING
#define _AK_WWISESDK_NUM2STRING( n )			#n

/// Macro that "converts" a numeric define to a string
#define AK_WWISESDK_NUM2STRING( n )				_AK_WWISESDK_NUM2STRING( n )

/// Macro to determine if there's a subminor version number to add to the full version name
#if AK_WWISESDK_VERSION_SUBMINOR > 0
	#define AK_WWISESDK_VERSION_SUBMINOR_POSTFIX		"."	AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_SUBMINOR )
#else
	#define AK_WWISESDK_VERSION_SUBMINOR_POSTFIX
#endif

/// Macro to determine if there's a nickname to add to the full version name
#if defined( AK_WWISESDK_VERSION_NICKNAME )
	#define AK_WWISESDK_VERSION_NICKNAME_POSTFIX		"_"	AK_WWISESDK_VERSION_NICKNAME
#else
	#define AK_WWISESDK_VERSION_NICKNAME_POSTFIX
#endif

/// String representing the Wwise SDK version
#define AK_WWISESDK_VERSIONNAME				"v" AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_MAJOR ) \
											"."	AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_MINOR ) \
											AK_WWISESDK_VERSION_SUBMINOR_POSTFIX \
											AK_WWISESDK_VERSION_NICKNAME_POSTFIX

/// Wwise SDK branch
#define AK_WWISESDK_BRANCH					"wwise_v2016.1"

/// @name Wwise SDK Copyright Notice

//@{
	/// Wwise SDK copyright notice
	#define AK_WWISESDK_COPYRIGHT 				"\xA9 2006-2016. Audiokinetic Inc. All rights reserved."
	/// Wwise SDK copyright notice
	#define AK_WWISESDK_COPYRIGHT_CONSOLE 		"(C) 2006-2016. Audiokinetic Inc. All rights reserved."
//@}

#define AK_WWISESDK_VERSION_COMBINED ((AK_WWISESDK_VERSION_MAJOR<<8) | AK_WWISESDK_VERSION_MINOR)

#endif // _AKWWISESDKVERSION_H_

