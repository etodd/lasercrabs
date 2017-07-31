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

#ifndef _AK_ACOUSTICTEXTURE_H_
#define _AK_ACOUSTICTEXTURE_H_

#include <AK/SoundEngine/Common/AkTypes.h>

struct AkAcousticTexture
{
	AkAcousticTexture(){}

	//Constructor
	AkAcousticTexture(
		AkUInt32					in_ID, 
		AkReal32					in_fAbsorptionOffset,
		AkReal32					in_fAbsorptionLow,
		AkReal32					in_fAbsorptionMidLow,
		AkReal32					in_fAbsorptionMidHigh,
		AkReal32					in_fAbsorptionHigh,
		AkReal32					in_fScattering) 
	:ID(in_ID)
	,fAbsorptionOffset(in_fAbsorptionOffset)
	,fAbsorptionLow(in_fAbsorptionLow)
	,fAbsorptionMidLow(in_fAbsorptionMidLow)
	,fAbsorptionMidHigh(in_fAbsorptionMidHigh)
	,fAbsorptionHigh(in_fAbsorptionHigh)
	,fScattering(in_fScattering)
	{
	}

	AkUInt32						ID;

	AkReal32						fAbsorptionOffset;
	AkReal32						fAbsorptionLow;
	AkReal32						fAbsorptionMidLow;
	AkReal32						fAbsorptionMidHigh;
	AkReal32						fAbsorptionHigh;
	AkReal32						fScattering;
};

#endif
