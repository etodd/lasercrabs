//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_ALLPLUGINSREGISTRATIONHELPERS_H_
#define _AK_ALLPLUGINSREGISTRATIONHELPERS_H_

#include <AK/Plugin/AllPluginsFactories.h>
#if defined AK_MOTION
#include <AK/MotionEngine/Common/AkMotionEngine.h>
#endif


namespace AK
{
	namespace SoundEngine
	{

		#if !defined (AK_WII_FAMILY_HW) && !defined(AK_3DS) && !defined (AK_VITA_HW)
			#define AK_SOFTWARE_EFFECT_PLATFORM
		#endif

		/// Early return if anything goes wrong
		#define AK_CHECK_ERROR( __FUNCCALL__ )	\
		{											\
			AKRESULT eResult = (__FUNCCALL__);		\
			if ( eResult != AK_Success )			\
				eFinal = eResult;					\
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllEffectPlugins()
		{
			AKRESULT eFinal = AK_Success;
#if defined(AK_SOFTWARE_EFFECT_PLATFORM)

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_COMPRESSOR,
				CreateCompressorFX,
				CreateCompressorFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_EXPANDER,
				CreateExpanderFX,
				CreateExpanderFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_PEAKLIMITER,
				CreatePeakLimiterFX,
				CreatePeakLimiterFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_MATRIXREVERB,
				CreateMatrixReverbFX,
				CreateMatrixReverbFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_ROOMVERB,
				CreateRoomVerbFX,
				CreateRoomVerbFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_DELAY,
				CreateDelayFX,
				CreateDelayFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_FLANGER,
				CreateFlangerFX,
				CreateFlangerFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_TREMOLO,
				CreateTremoloFX,
				CreateTremoloFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_GUITARDISTORTION,
				CreateGuitarDistortionFX,
				CreateGuitarDistortionFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_PARAMETRICEQ,
				CreateParametricEQFX,
				CreateParametricEQFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_METER,
				CreateMeterFX,
				CreateMeterFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_STEREODELAY,
				CreateStereoDelayFX,
				CreateStereoDelayFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_PITCHSHIFTER,
				CreatePitchShifterFX,
				CreatePitchShifterFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_TIMESTRETCH,
				CreateTimeStretchFX,
				CreateTimeStretchFXParams ) );
			
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_HARMONIZER,
				CreateHarmonizerFX,
				CreateHarmonizerFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_GAIN,
				CreateGainFX,
				CreateGainFXParams ) );

#elif defined(AK_WII_FAMILY)

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_WII_STANDARD_REVERB,
				CreateWiiReverbStd,
				CreateWiiReverbStdFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_WII_HIGH_QUALITY_REVERB,
				CreateWiiReverbHi,
				CreateWiiReverbHiFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_WII_CHORUS,
				CreateWiiChorus,
				CreateWiiChorusFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_WII_DELAY,
				CreateWiiDelay,
				CreateWiiDelayFXParams ) );

#elif defined(AK_3DS)

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_3DS_REVERB,
				Create3DSReverb,
				Create3DSReverbFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_3DS_DELAY,
				Create3DSDelay,
				Create3DSDelayFXParams ) );
#endif

#if defined(AK_VITA_HW)
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin(
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_VITA_REVERB,
				CreateVitaReverbFX,
				CreateVitaReverbFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_VITA_COMPRESSOR,
				CreateVitaCompressorFX,
				CreateVitaCompressorFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_VITA_DELAY,
				CreateVitaDelayFX,
				CreateVitaDelayFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_VITA_DISTORTION,
				CreateVitaDistortionFX,
				CreateVitaDistortionFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_VITA_EQ,
				CreateVitaEQFX,
				CreateVitaEQFXParams ) );
#endif

#if defined(AK_PS4)
			// SCE Audio 3D
			AK::SoundEngine::RegisterPlugin( AkPluginTypeMixer, 
											 AKCOMPANYID_AUDIOKINETIC, 
											 AKEFFECTID_SCE_AUDIO3D,
											 CreateSceAudio3dMixer,
											 CreateSceAudio3dMixerParams );

			AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
											 AKCOMPANYID_AUDIOKINETIC, 
											 AKEFFECTID_SCE_AUDIO3D_ATTACHMENT, 
											 NULL, 
											 CreateSceAudio3dAttachment );

			AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
											 AKCOMPANYID_AUDIOKINETIC, 
											 AKEFFECTID_SCE_AUDIO3D_SINK_EFFECT, 
											 CreateSceAudio3dSinkEffect, 
											 CreateSceAudio3dSinkEffectParams );

#endif

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllSourcePlugins()
		{
			AKRESULT eFinal = AK_Success;
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_SILENCE,
				CreateSilenceSource,
				CreateSilenceSourceParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_SINE,
				CreateSineSource,
				CreateSineSourceParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_TONE,
				CreateToneSource,
				CreateToneSourceParams ) );

#if defined( AK_WIN ) && !defined( AK_USE_METRO_API )
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_MP3,
				CreateMP3Source,
				CreateMP3SourceParams ) );
#endif

#if !defined( AK_VITA ) && !defined( AK_3DS )
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin(
				AkPluginTypeSource,
				AKCOMPANYID_AUDIOKINETIC,
				AKSOURCEID_AUDIOINPUT,
				CreateAudioInputSource,
				CreateAudioInputSourceParams ) );
#endif

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_SYNTHONE,
				CreateSynthOne,
				CreateSynthOneParams ) );

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterConvolutionReverbPlugin()
		{
			AKRESULT eFinal = AK_Success;
#ifdef AK_SOFTWARE_EFFECT_PLATFORM
		AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_CONVOLUTIONREVERB,
				CreateConvolutionReverbFX,
				CreateConvolutionReverbFXParams ) );
#endif
			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterMcDSPPlugins()
		{
			AKRESULT eFinal = AK_Success;
#ifdef AK_SOFTWARE_EFFECT_PLATFORM
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_MCDSP, 
				AKEFFECTID_MCDSPFUTZBOX,
				CreateMcDSPFutzBoxFX,
				CreateMcDSPFutzBoxFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_MCDSP, 
				AKEFFECTID_MCDSPML1,
				CreateMcDSPML1FX,
				CreateMcDSPML1FXParams ) );
#endif

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAstoundSoundPlugins()
		{
			AKRESULT eFinal = AK_Success;
#if ( defined( AK_WIN ) && !defined( AK_USE_METRO_API ) ) || defined( AK_PS4 ) || defined( AK_XBOXONE ) || defined( AK_ANDROID ) || defined( AK_IOS ) || defined( AK_MAC_OS_X )
			
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_GENAUDIO, 
				AKEFFECTID_GENAUDIORTI,
				CreateAstoundSoundRTIFX,
				CreateAstoundSoundRTIFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_GENAUDIO, 
				AKEFFECTID_GENAUDIOEXPANDER,
				CreateAstoundSoundExpanderFX,
				CreateAstoundSoundExpanderFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_GENAUDIO, 
				AKEFFECTID_GENAUDIOFOLDDOWN,
				CreateAstoundSoundFolddownFX,
				CreateAstoundSoundFolddownFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeMixer, 
				AKCOMPANYID_GENAUDIO, 
				AKEFFECTID_GENAUDIORTI_MIXER,
				CreateAstoundSoundRTIMixer,
				CreateAstoundSoundRTIMixerParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_GENAUDIO,
				AKEFFECTID_GENAUDIORTI_MIXERATTACHABLE, 
				NULL,
				CreateAstoundSoundRTIMixerAttachmentParams ) );
#endif

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisteriZotopePlugins()
		{
			AKRESULT eFinal = AK_Success;
#if ( defined( AK_WIN ) && !defined( AK_USE_METRO_API ) ) || defined( AK_XBOX360 ) || defined( AK_PS3 ) || defined( AK_XBOXONE ) || defined( AK_PS4 )
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHMULTIBANDDISTORTION,
				CreateTrashMultibandDistortionFX,
				CreateTrashMultibandDistortionFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHDISTORTION,
				CreateTrashDistortionFX,
				CreateTrashDistortionFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHFILTERS,
				CreateTrashFiltersFX,
				CreateTrashFiltersFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHDYNAMICS,
                CreateTrashDynamicsFX,
                CreateTrashDynamicsFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHDELAY,
                CreateTrashDelayFX,
                CreateTrashDelayFXParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_TRASHBOXMODELER,
                CreateTrashBoxModelerFX,
                CreateTrashBoxModelerFXParams ) );
#endif
#if ( defined( AK_WIN ) && !defined( AK_USE_METRO_API ) ) || defined( AK_XBOX360 ) || defined( AK_XBOXONE ) || defined( AK_PS4 )

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IZOTOPE, 
				IZEFFECTID_HYBRIDREVERB,
				CreateHybridReverbFX,
				CreateHybridReverbFXParams ) );
#endif

			return eFinal;
		}

		static AKRESULT RegisterCrankcaseAudioPlugins()
		{
			AKRESULT eFinal = AK_Success;

#if ( defined( AK_WIN ) && !defined( AK_USE_METRO_API ) ) || defined( AK_PS4 ) || defined( AK_XBOXONE ) || defined( AK_XBOX360 )

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource,
				AKCOMPANYID_CRANKCASEAUDIO,
				AKSOURCEID_REV_MODEL_PLAYER,
				CreateREVModelPlayerSource,
				CreateREVModelPlayerSourceParams ) );
#endif
			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterSoundSeedPlugins()
		{
			AKRESULT eFinal = AK_Success;
#ifdef AK_SOFTWARE_EFFECT_PLATFORM
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKEFFECTID_SOUNDSEEDIMPACT,
				CreateAkSoundSeedImpactFX,
				CreateAkSoundSeedImpactFXParams ) );
#endif

#ifndef AK_3DS
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_SOUNDSEEDWIND,
				CreateSoundSeedWind,
				CreateSoundSeedWindParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeSource, 
				AKCOMPANYID_AUDIOKINETIC, 
				AKSOURCEID_SOUNDSEEDWOOSH,
				CreateSoundSeedWoosh,
				CreateSoundSeedWooshParams ) );
#endif

			return eFinal;
		}

		static AKRESULT RegisterIOSONOPlugins()
		{
			AKRESULT eFinal = AK_Success;

			// IOSONO Proximity
#if (defined(AK_WIN) && !defined(AK_USE_METRO_API)) || defined(AK_XBOXONE) || defined(AK_PS4)
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeMixer, 
				AKCOMPANYID_IOSONO, 
				IOSONOEFFECTID_PROXIMITY,
				CreateIOSONOProximityMixer,
				CreateIOSONOProximityMixerParams ) );

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_IOSONO,
				IOSONOEFFECTID_PROXIMITY_ATTACHMENT, 
				NULL,
				CreateIOSONOProximityAttachmentParams ) );
#endif
			return eFinal;
		}

		static AKRESULT RegisterAuroPlugins()
		{
			AKRESULT eFinal = AK_Success;

#if defined(AK_WIN) && !defined(AK_USE_METRO_API)
			// Auro Headphone
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUROTECHNOLOGIES, 
				AKEFFECTID_AUROHEADPHONE,
				CreateAuroHeadphoneFX,
				CreateAuroHeadphoneFXParams ) );
			
			// Auro Panner
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeMixer, 
				AKCOMPANYID_AUROTECHNOLOGIES, 
				AKEFFECTID_AUROPANNER,
				CreateAuroPannerMixer,
				CreateAuroPannerMixerParams ) );
			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeEffect, 
				AKCOMPANYID_AUROTECHNOLOGIES,
				AKEFFECTID_AUROPANNER_ATTACHMENT, 
				NULL,
				CreateAuroPannerMixerAttachmentParams ) );
#endif

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllCodecPlugins()
		{
			AKRESULT eFinal = AK_Success;
#ifndef AK_3DS
			AK::SoundEngine::RegisterCodec(
				AKCOMPANYID_AUDIOKINETIC,
				AKCODECID_VORBIS,
				CreateVorbisFilePlugin,
				CreateVorbisBankPlugin );
#endif

#ifdef AK_XBOX360
			AK::SoundEngine::RegisterCodec(
				AKCOMPANYID_AUDIOKINETIC,
				AKCODECID_XWMA,
				CreateXWMAFilePlugin,
				CreateXWMABankPlugin );
#endif

#ifdef AK_APPLE
			AK::SoundEngine::RegisterCodec(
				AKCOMPANYID_AUDIOKINETIC,
				AKCODECID_AAC,
				CreateAACFilePlugin,
				CreateAACBankPlugin );
#endif

#ifdef AK_VITA
			AK::SoundEngine::RegisterCodec(
				AKCOMPANYID_AUDIOKINETIC,
				AKCODECID_ATRAC9,
				CreateATRAC9FilePlugin,
				CreateATRAC9BankPlugin );
#endif

			return eFinal;

		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllRumblePlugins()
		{
			AKRESULT eFinal = AK_Success;
#if defined AK_MOTION

			AK_CHECK_ERROR( AK::SoundEngine::RegisterPlugin( 
				AkPluginTypeMotionSource,
				AKCOMPANYID_AUDIOKINETIC,
				AKSOURCEID_MOTIONGENERATOR,
				AkCreateMotionGenerator,
				AkCreateMotionGeneratorParams ) );

			AK::MotionEngine::RegisterMotionDevice( 
				AKCOMPANYID_AUDIOKINETIC, 
				AKMOTIONDEVICEID_RUMBLE,
				AkCreateRumblePlugin );

#endif

			return eFinal;
		}

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllBuiltInPlugins()
		{
			AKRESULT eFinal = AK_Success;
			AK_CHECK_ERROR( RegisterAllCodecPlugins() );
			AK_CHECK_ERROR( RegisterAllEffectPlugins() );
			AK_CHECK_ERROR( RegisterAllSourcePlugins() );
			AK_CHECK_ERROR( RegisterAllRumblePlugins() );
			return eFinal;
		}	

		/// Note: This a convenience method for rapid prototyping. 
		/// To reduce executable code size register/link only the plug-ins required by your game 
		static AKRESULT RegisterAllPlugins()
		{
			AKRESULT eFinal = AK_Success;
			// Built-in products
			AK_CHECK_ERROR( RegisterAllBuiltInPlugins() );
			// Products which require additional license
			AK_CHECK_ERROR( RegisterConvolutionReverbPlugin() );
			AK_CHECK_ERROR( RegisterSoundSeedPlugins() );
			AK_CHECK_ERROR( RegisterMcDSPPlugins() );
			AK_CHECK_ERROR( RegisterAstoundSoundPlugins() );
			AK_CHECK_ERROR( RegisteriZotopePlugins() );
			AK_CHECK_ERROR( RegisterCrankcaseAudioPlugins() );
			AK_CHECK_ERROR( RegisterIOSONOPlugins() )
			AK_CHECK_ERROR( RegisterAuroPlugins() )
			return eFinal;
		}

	} // SoundEngine
} // AK

#endif // _AK_ALLPLUGINSREGISTRATIONHELPERS_H_
