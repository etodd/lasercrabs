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

  Version: v2017.2.1  Build: 6524
  Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AK_ALLPLUGINSFACTORIES_H_
#define _AK_ALLPLUGINSFACTORIES_H_

#include <AK/AkPlatforms.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

// Effect plug-ins
#include <AK/Plugin/AkCompressorFXFactory.h>					// Compressor
#include <AK/Plugin/AkDelayFXFactory.h>							// Delay
#include <AK/Plugin/AkMatrixReverbFXFactory.h>					// Matrix reverb
#include <AK/Plugin/AkMeterFXFactory.h>							// Meter
#include <AK/Plugin/AkExpanderFXFactory.h>						// Expander
#include <AK/Plugin/AkParametricEQFXFactory.h>					// Parametric equalizer
#include <AK/Plugin/AkGainFXFactory.h>							// Gain
#include <AK/Plugin/AkPeakLimiterFXFactory.h>					// Peak limiter
#include <AK/Plugin/AkSoundSeedImpactFXFactory.h>				// SoundSeed Impact
#include <AK/Plugin/AkRoomVerbFXFactory.h>						// RoomVerb
#include <AK/Plugin/AkGuitarDistortionFXFactory.h>				// Guitar distortion
#include <AK/Plugin/AkStereoDelayFXFactory.h>					// Stereo delay
#include <AK/Plugin/AkPitchShifterFXFactory.h>					// Pitch shifter
#include <AK/Plugin/AkTimeStretchFXFactory.h>					// Time stretch
#include <AK/Plugin/AkFlangerFXFactory.h>						// Flanger
#include <AK/Plugin/AkTremoloFXFactory.h>						// Tremolo
#include <AK/Plugin/AkHarmonizerFXFactory.h>					// Harmonizer
#include <AK/Plugin/AkRecorderFXFactory.h>						// Recorder

// Platform specific
#ifdef AK_PS4
#include <AK/Plugin/SceAudio3dEngineFactory.h>					// SCE Audio3d
#endif

// Sources plug-ins
#include <AK/Plugin/AkSilenceSourceFactory.h>					// Silence generator
#include <AK/Plugin/AkSineSourceFactory.h>						// Sine wave generator
#include <AK/Plugin/AkToneSourceFactory.h>						// Tone generator
#include <AK/Plugin/AkAudioInputSourceFactory.h>				// Audio input
#include <AK/Plugin/AkSoundSeedWooshSourceFactory.h>			// SoundSeed Woosh
#include <AK/Plugin/AkSoundSeedWindSourceFactory.h>				// SoundSeed Wind
#include <AK/Plugin/AkSynthOneSourceFactory.h>					// SynthOne
#include <AK/Plugin/AkMotionGeneratorSourceFactory.h>

// Required by codecs plug-ins
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#ifdef AK_APPLE
#include <AK/Plugin/AkAACFactory.h>			// Note: Useable only on Apple devices. Ok to include it on other platforms as long as it is not referenced.
#endif
#ifdef AK_NX
#include <AK/Plugin/AkOpusFactory.h>		// Note: Useable only on NX. Ok to include it on other platforms as long as it is not referenced.
#endif

#if (defined AK_WIN || defined AK_PS4 || defined AK_XBOXONE || defined AK_NX)
#include <AK/Plugin/AkMotionSinkFactory.h>
#endif

#endif // _AK_ALLPLUGINSFACTORIES_H_
