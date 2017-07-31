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

/// \file 
/// Floating point performance utilities.

#ifndef _AK_FP_UTILS_H_
#define _AK_FP_UTILS_H_

#include <AK/SoundEngine/Common/AkTypes.h>

#define AK_FSEL( __a__, __b__, __c__) (((__a__) >= 0) ? (__b__) : (__c__))

/// Branchless (where available) version returning minimum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMin( AkReal32 fA, AkReal32 fB )
{   
	return (fA < fB ? fA : fB);
} 

/// Branchless (where available) version returning maximum value between two AkReal32 values
static AkForceInline AkReal32 AK_FPMax( AkReal32 fA, AkReal32 fB )
{   
	return (fA > fB ? fA : fB);
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than 2nd argument.
static AkForceInline void AK_FPSetValGT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA > in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is greater than equal 2nd argument.
static AkForceInline void AK_FPSetValGTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA >= in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than 2nd argument.
static AkForceInline void AK_FPSetValLT( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA < in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

/// Branchless comparison (where available) setting 3rd argument to 4th argument if 1st argument is less than equal 2nd argument.
static AkForceInline void AK_FPSetValLTE( AkReal32 in_fComparandA, AkReal32 in_fComparandB, AkReal32 & io_fVariableToSet, AkReal32 in_fValueIfTrue )
{   
	if ( in_fComparandA <= in_fComparandB )
		io_fVariableToSet = in_fValueIfTrue;
}

#endif //_AK_FP_UTILS_H_

