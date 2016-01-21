//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_ALLPLUGINSFACTORIES_H_
#define _AK_ALLPLUGINSFACTORIES_H_

#include <AK/AkPlatforms.h>
#include <AK/SoundEngine/Common/AkTypes.h>

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

// Platform specific
#if defined AK_WII_FAMILY
#include <AK/Plugin/WiiPluginsFXFactory.h>						// All Wii plug-ins
#endif
#ifdef AK_3DS
#include <AK/Plugin/3DSPluginsFXFactory.h>						// All 3DS plug-ins
#endif
#ifdef AK_VITA
#include <AK/Plugin/VitaPluginsFXFactory.h>						// All Vita plug-ins
#endif
#ifdef AK_PS4
#include <AK/Plugin/SceAudio3dEngineFactory.h>					// SCE Audio3d
#endif

// McDSP plug-ins
#include <AK/Plugin/McDSPFutzBoxFXFactory.h>					// FutzBox
#include <AK/Plugin/McDSPLimiterFXFactory.h>					// ML1 Limiter

// GenAudio plug-ins
#include <AK/Plugin/AstoundSoundRTIFXFactory.h>					// RTI
#include <AK/Plugin/AstoundSoundExpanderFXFactory.h>			// Expander
#include <AK/Plugin/AstoundSoundFolddownFXFactory.h>			// folddown

// iZotope plug-ins
#include <AK/Plugin/iZHybridReverbFXFactory.h>					// Hybrid Reverb
#include <AK/Plugin/iZTrashMultibandDistortionFXFactory.h>		// Trash MultibandDistortion
#include <AK/Plugin/iZTrashBoxModelerFXFactory.h>				// Trash BoxModeler
#include <AK/Plugin/iZTrashDelayFXFactory.h>					// Trash Delay
#include <AK/Plugin/iZTrashDistortionFXFactory.h>				// Trash Distortion
#include <AK/Plugin/iZTrashDynamicsFXFactory.h>					// Trash Dynamics
#include <AK/Plugin/iZTrashFiltersFXFactory.h>					// Trash Filters

// Crankcase plug-ins
#include <AK/Plugin/CrankcaseAudioREVModelPlayerFactory.h>					// Trash Filters

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
#ifdef AK_WIN
#include <AK/Plugin/AkMP3SourceFactory.h>						// MP3 source Note: Useable only on PC. Ok to include it on other platforms as long as it is not referenced.
#endif
#include <AK/Plugin/AkSynthOneFactory.h>						// SynthOne

// Required by codecs plug-ins
#include <AK/Plugin/AkVorbisFactory.h>
#ifdef AK_XBOX360
#include <AK/Plugin/AkXWMAFactory.h>		// Note: Useable only on Xbox 360. Ok to include it on other platforms as long as it is not referenced.
#endif
#ifdef AK_APPLE
#include <AK/Plugin/AkAACFactory.h>			// Note: Useable only on Apple devices. Ok to include it on other platforms as long as it is not referenced.
#endif
#ifdef AK_VITA
#include <AK/Plugin/AkATRAC9Factory.h>		// Note: Useable only on Vita. Ok to include it on other platforms as long as it is not referenced.
#endif

// Mixer plugins
#include <AK/Plugin/IOSONOProximityMixerFactory.h>					// IOSONO Proximity

// Rumble support
#if defined AK_MOTION
#include <AK/Plugin/AkRumbleFactory.h>
#include <AK/Plugin/AkMotionGeneratorFactory.h>
#endif

#endif // _AK_ALLPLUGINSFACTORIES_H_
