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

#ifndef _AK_ACOUSTICTEXTURE_H_
#define _AK_ACOUSTICTEXTURE_H_

#include <AK/SoundEngine/Common/AkTypes.h>

struct AkAcousticTexture
{
	AkAcousticTexture(){}

	//Constructor
	AkAcousticTexture(
		AkUInt32					in_ID,
		AkUInt16					in_OnOffBand1,
		AkUInt16					in_OnOffBand2,
		AkUInt16					in_OnOffBand3,
		AkUInt16					in_FilterTypeBand1,
		AkUInt16					in_FilterTypeBand2,
		AkUInt16					in_FilterTypeBand3,
		AkReal32					in_FrequencyBand1,
		AkReal32					in_FrequencyBand2,
		AkReal32					in_FrequencyBand3,
		AkReal32					in_QFactorBand1,
		AkReal32					in_QFactorBand2,
		AkReal32					in_QFactorBand3,
		AkReal32					in_GainBand1,
		AkReal32					in_GainBand2,
		AkReal32					in_GainBand3,
		AkReal32					in_OutputGain) :
		ID(in_ID),
		OnOffBand1(in_OnOffBand1 != 0),
		OnOffBand2(in_OnOffBand2 != 0),
		OnOffBand3(in_OnOffBand3 != 0),
		FilterTypeBand1(in_FilterTypeBand1),
		FilterTypeBand2(in_FilterTypeBand2),
		FilterTypeBand3(in_FilterTypeBand3),
		FrequencyBand1(in_FrequencyBand1),
		FrequencyBand2(in_FrequencyBand2),
		FrequencyBand3(in_FrequencyBand3),
		QFactorBand1(in_QFactorBand1),
		QFactorBand2(in_QFactorBand2),
		QFactorBand3(in_QFactorBand3), 
		GainBand1(in_GainBand1),
		GainBand2(in_GainBand2),
		GainBand3(in_GainBand3),
		OutputGain(in_OutputGain)
	{
	}

	AkUInt32						ID;

	bool							OnOffBand1;
	bool							OnOffBand2;
	bool							OnOffBand3;

	AkUInt16						FilterTypeBand1;
	AkUInt16						FilterTypeBand2;
	AkUInt16						FilterTypeBand3;

	AkReal32						FrequencyBand1;
	AkReal32						FrequencyBand2;
	AkReal32						FrequencyBand3;

	AkReal32						QFactorBand1;
	AkReal32						QFactorBand2;
	AkReal32						QFactorBand3;
	
	AkReal32						GainBand1;
	AkReal32						GainBand2;
	AkReal32						GainBand3;
	
	AkReal32						OutputGain;
};


struct AkDiffuseReverberator
{
	AkDiffuseReverberator(){}

	//Constructor
	AkDiffuseReverberator(
		AkUInt32					in_ID,
		AkReal32					in_Time,
		AkReal32					in_HFRatio,
		AkReal32					in_DryLevel,
		AkReal32					in_WetLevel,
		AkReal32					in_Spread,
		AkReal32					in_Density,
		AkUInt32					in_Quality,
		AkReal32					in_Diffusion,
		AkReal32					in_Scale):
		ID(in_ID),
		Time(in_Time),
		HFRatio(in_HFRatio),
		DryLevel(in_DryLevel),
		WetLevel(in_WetLevel),
		Spread(in_Spread),
		Density(in_Density),
		Quality(in_Quality),
		Diffusion(in_Diffusion),
		Scale(in_Scale)
	{
	}


	AkUInt32						ID;
	AkReal32						Time;
	AkReal32						HFRatio;
	AkReal32						DryLevel;

	AkReal32						WetLevel;
	AkReal32						Spread;
	AkReal32						Density;

	AkUInt32						Quality;
	AkReal32						Diffusion;
	AkReal32						Scale;


};

#endif
