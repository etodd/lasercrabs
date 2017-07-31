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
#include <AK/Plugin/AkConvolutionReverbFXFactory.h>				// Convolution reverb
#include <AK/Plugin/AkTremoloFXFactory.h>						// Tremolo
#include <AK/Plugin/AkHarmonizerFXFactory.h>					// Harmonizer
#include <AK/Plugin/AkRecorderFXFactory.h>						// Recorder
#include <AK/Plugin/AkReflectFXFactory.h>						// Reflect

// Platform specific
#ifdef AK_VITA
#include <AK/Plugin/AkVitaPluginFXFactory.h>						// All Vita plug-ins
#endif
#ifdef AK_PS4
#include <AK/Plugin/SceAudio3dEngineFactory.h>					// SCE Audio3d
#endif

// McDSP plug-ins
#include <AK/Plugin/McDSPFutzBoxFXFactory.h>					// FutzBox
#include <AK/Plugin/McDSPLimiterFXFactory.h>					// ML1 Limiter

// iZotope plug-ins
#include <AK/Plugin/iZHybridReverbFXFactory.h>					// Hybrid Reverb
#include <AK/Plugin/iZTrashMultibandDistortionFXFactory.h>		// Trash MultibandDistortion
#include <AK/Plugin/iZTrashBoxModelerFXFactory.h>				// Trash BoxModeler
#include <AK/Plugin/iZTrashDelayFXFactory.h>					// Trash Delay
#include <AK/Plugin/iZTrashDistortionFXFactory.h>				// Trash Distortion
#include <AK/Plugin/iZTrashDynamicsFXFactory.h>					// Trash Dynamics
#include <AK/Plugin/iZTrashFiltersFXFactory.h>					// Trash Filters

// Crankcase plug-ins
#include <AK/Plugin/CrankcaseAudioREVModelPlayerFXFactory.h>					// Trash Filters

// Auro plug-ins
#include <AK/Plugin/AuroHeadphoneFXFactory.h>
#include <AK/Plugin/AuroPannerMixerFactory.h>

// Sources plug-ins
#include <AK/Plugin/AkSilenceSourceFactory.h>					// Silence generator
#include <AK/Plugin/AkSineSourceFactory.h>						// Sine wave generator
#include <AK/Plugin/AkToneSourceFactory.h>						// Tone generator
#include <AK/Plugin/AkAudioInputSourceFactory.h>				// Audio input
#include <AK/Plugin/AkSoundSeedWooshFactory.h>					// SoundSeed Woosh
#include <AK/Plugin/AkSoundSeedWindFactory.h>					// SoundSeed Wind
#include <AK/Plugin/AkSynthOneFactory.h>						// SynthOne

// Required by codecs plug-ins
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#ifdef AK_APPLE
#include <AK/Plugin/AkAACFactory.h>			// Note: Useable only on Apple devices. Ok to include it on other platforms as long as it is not referenced.
#endif
#ifdef AK_VITA
#include <AK/Plugin/AkATRAC9Factory.h>		// Note: Useable only on Vita. Ok to include it on other platforms as long as it is not referenced.
#endif
#ifdef AK_NX
#include <AK/Plugin/AkOpusFactory.h>		// Note: Useable only on NX. Ok to include it on other platforms as long as it is not referenced.
#endif

// Rumble support
#if defined AK_MOTION
#include <AK/Plugin/AkRumbleFactory.h>
#include <AK/Plugin/AkMotionGeneratorFactory.h>
#endif

#endif // _AK_ALLPLUGINSFACTORIES_H_
