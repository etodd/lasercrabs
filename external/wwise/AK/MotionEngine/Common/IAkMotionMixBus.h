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

// IAkMotionMixBus.h

#ifndef _IMOTIONBUS_H
#define _IMOTIONBUS_H

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

class IAkMotionMixBus : public AK::IAkPlugin
{
public:
	virtual AKRESULT 	Init(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pContext, AkPlatformInitSettings * io_pPDSettings, AkUInt8 in_iPlayer, void * in_pDevice = NULL) = 0;

	virtual AKRESULT	MixAudioBuffer( AkAudioBuffer &io_rBuffer ) = 0;
	virtual AKRESULT	MixFeedbackBuffer( AkAudioBuffer &io_rBuffer, AkReal32 in_fVolume ) = 0;
	virtual AKRESULT	RenderData() = 0;
	virtual void		CommandTick() = 0;
	virtual void		Stop() = 0;

	virtual AkReal32	GetPeak() = 0;
	virtual bool		IsStarving() = 0;
	virtual bool		IsActive() = 0;
	virtual AkChannelMask GetMixingFormat() = 0;
	virtual void		SetMasterVolume(AkReal32 in_fVol) = 0;

	virtual void		StartOutputCapture(const AkOSChar* in_CaptureFileName) = 0;
	virtual void		StopOutputCapture() = 0;
};
#endif
