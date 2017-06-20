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

  Version: v2016.2.4  Build: 6097
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

// SceAudio3dMixerFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  SCE Audio3d
/// <br><b>Library file:</b> SceAudioAudio3dEngine.lib

#ifndef _SCE_AUDIO_3D_MIXER_FACTORY_H_
#define _SCE_AUDIO_3D_MIXER_FACTORY_H_

#if defined AK_PS4
AK_STATIC_LINK_PLUGIN(SceAudio3dMixer)
AK_STATIC_LINK_PLUGIN(SceAudio3dMixerAttachment)
AK_STATIC_LINK_PLUGIN(SceAudio3dSinkEffect)
#endif


#endif // _SCE_AUDIO_3D_MIXER_FACTORY_H_

