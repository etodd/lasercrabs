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

  Version: v2016.2.4  Build: 6098
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

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
#define AK_WWISESDK_VERSION_MINOR				2

/// Wwise SDK sub-minor version
#define AK_WWISESDK_VERSION_SUBMINOR			4

/// Wwise SDK build number
#define AK_WWISESDK_VERSION_BUILD				6098

/// Wwise SDK build date (year)
#define AK_WWISESDK_BUILD_YEAR					2017

/// Wwise SDK build date (month)
#define AK_WWISESDK_BUILD_MONTH					5

/// Wwise SDK build date (day)
#define AK_WWISESDK_BUILD_DAY					30

//@}

/// @name Wwise SDK Version - String values
//@{

/// Macro that "converts" a numeric define to a string
/// \sa
/// - \ref AK_WWISESDK_NUM2STRING
#define _AK_WWISESDK_NUM2STRING( n )			#n

/// Macro that "converts" a numeric define to a string
#define AK_WWISESDK_NUM2STRING( n )				_AK_WWISESDK_NUM2STRING( n )

/// Macro to determine if there's a nickname to add to the full version name
#if defined( AK_WWISESDK_VERSION_NICKNAME )
	#define AK_WWISESDK_VERSION_NICKNAME_POSTFIX		"_"	AK_WWISESDK_VERSION_NICKNAME
#else
	#define AK_WWISESDK_VERSION_NICKNAME_POSTFIX
#endif

/// String representing the Wwise SDK version without the nickname postfix
#define AK_WWISESDK_VERSIONNAME_SHORT		"v" AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_MAJOR ) \
											"."	AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_MINOR ) \
											"."	AK_WWISESDK_NUM2STRING( AK_WWISESDK_VERSION_SUBMINOR )

/// String representing the Wwise SDK version
#define AK_WWISESDK_VERSIONNAME				AK_WWISESDK_VERSIONNAME_SHORT \
											AK_WWISESDK_VERSION_NICKNAME_POSTFIX

/// Wwise SDK branch
#define AK_WWISESDK_BRANCH					"wwise_v2016.2"

/// @name Wwise SDK Copyright Notice

//@{
	/// Wwise SDK copyright notice
	#define AK_WWISESDK_COPYRIGHT 				"\xA9 2006-2017. Audiokinetic Inc. All rights reserved."
	/// Wwise SDK copyright notice
	#define AK_WWISESDK_COPYRIGHT_CONSOLE 		"(C) 2006-2017. Audiokinetic Inc. All rights reserved."
//@}

#define AK_WWISESDK_VERSION_COMBINED ((AK_WWISESDK_VERSION_MAJOR<<8) | AK_WWISESDK_VERSION_MINOR)

#endif // _AKWWISESDKVERSION_H_

