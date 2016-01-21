//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSoundEngine.h

/// \file 
/// The main sound engine interface.


#ifndef _AK_SOUNDENGINE_H_
#define _AK_SOUNDENGINE_H_

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/AkCallback.h>

#ifdef AK_WIN
#include <AK/SoundEngine/Platforms/Windows/AkWinSoundEngine.h>

#elif defined (AK_MAC_OS_X)
#include <AK/SoundEngine/Platforms/Mac/AkMacSoundEngine.h>

#elif defined (AK_IOS)
#include <AK/SoundEngine/Platforms/iOS/AkiOSSoundEngine.h>

#elif defined (AK_XBOX360)
#include <AK/SoundEngine/Platforms/XBox360/AkXBox360SoundEngine.h>

#elif defined (AK_XBOXONE)
#include <AK/SoundEngine/Platforms/XboxOne/AkXboxOneSoundEngine.h>

#elif defined (__PPU__) || defined (__SPU__)
#include <AK/SoundEngine/Platforms/PS3/AkPs3SoundEngine.h>

#elif defined (AK_WII_FAMILY)
#include <AK/SoundEngine/Platforms/WiiFamily/AkWiiSoundEngine.h>

#elif defined (AK_VITA)
#include <AK/SoundEngine/Platforms/Vita/AkVitaSoundEngine.h>

#elif defined( AK_3DS )
#include <AK/SoundEngine/Platforms/3DS/Ak3DSSoundEngine.h>

#elif defined( AK_ANDROID )
#include <AK/SoundEngine/Platforms/Android/AkAndroidSoundEngine.h>

#elif defined( AK_NACL )
#include <AK/SoundEngine/Platforms/nacl/AkNaclSoundEngine.h>

#elif defined (AK_PS4)
#include <AK/SoundEngine/Platforms/PS4/AkPS4SoundEngine.h>

#elif defined( AK_LINUX )
#include <AK/SoundEngine/Platforms/Linux/AkLinuxSoundEngine.h>

#elif defined( AK_QNX  )
#include <AK/SoundEngine/Platforms/QNX/AkQNXSoundEngine.h>

#else
#error AkSoundEngine.h: Undefined platform
#endif

#ifndef AK_ASSERT_HOOK
	/// Function called on assert handling, optional
	/// \sa 
	/// - AkInitSettings
	AK_CALLBACK( void, AkAssertHook)( 
		const char * in_pszExpression,	///< Expression
		const char * in_pszFileName,	///< File Name
		int in_lineNumber				///< Line Number
		);
	#define AK_ASSERT_HOOK
#endif

/// Callback function prototype used for when another app starts or stops playing its audio. 
///	It is useful for reacting to user music playback.
/// \remark
/// - Under an unmixable audio session category, e.g. AkAudioSessionCategorySoloAmbient, an audio session interruption occurs when another app's audio starts playing, and the interruption callback AudioInterruptionCallbackFunc is called at this time. When another app's audio stops playing while the app remains foreground the whole time, the interruption callback is not called by Apple's design and this callback is called instead. Under a mixable audio session category, e.g. this callback is called both when another app starts and stops playing.
/// - When the audio sessoin category is under AkAudioSessionCategoryAmbient, this callback behaves much like the AVFoundation callback for the notification AVAudioSessionSilenceSecondaryAudioHintNotification. In fact, this callback is called via an observer of the notification on devices running iOS 8 and later, after the sound engine handles the muting/unmuting.
/// - Before calling this callback, the sound engine checks the playback status of other audio status change and mutes/unmutes registered busses according to that status. When other audio is playing, the sound engine mutes the specified busses, and unmutes them when other audio stops playing.
/// - To make the app's audio mutually exclusive (unmixable) with another app's audio, it is recommended to use an unmixable audio session category with AudioInterruptionCallbackFunc instead of this callback.
///
/// \sa
/// - \ref AkGlobalCallbackFunc
/// - \ref AudioInterruptionCallbackFunc
/// - \ref AkPlatformInitSettings
/// - \ref background_music_and_dvr
///
typedef AKRESULT ( * AkAudioSourceChangeCallbackFunc )(
	bool in_bOtherAudioPlaying,	///< Flag indicating whether or not audio of another app is playing.
	void* in_pCookie ///< User-provided data, e.g. a user structure.
	);

/// Platform-independent initialization settings of output devices.
struct AkOutputSettings
{
	AkPanningRule	ePanningRule;	///< Rule for 3D panning of signals routed to a stereo bus. In AkPanningRule_Speakers mode, the angle of the front loudspeakers 
									///< (uSpeakerAngles[0]) is used. In AkPanningRule_Headphones mode, the speaker angles are superseded with constant power panning
									///< between two virtual microphones spaced 180 degrees apart.
	AkChannelConfig	channelConfig;	///< Channel configuration for this output. Call AkChannelConfig::Clear() to let the engine use the default output configuration.  
									///< Hardware might not support the selected configuration.

	AkCreatePluginCallback pfSinkPluginFactory;	///< Custom sink plugin factory. Pass the creation function of your implementation of AK::IAkSinkPlugin. 
												///< Pass NULL to use built-in sinks (default).
};

/// Platform-independent initialization settings of the sound engine
/// \sa 
/// - AK::SoundEngine::Init()
/// - AK::SoundEngine::GetDefaultInitSettings()
/// - \ref soundengine_integration_init_advanced
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
struct AkInitSettings
{
    AkAssertHook        pfnAssertHook;				///< External assertion handling function (optional)

    AkUInt32            uMaxNumPaths;				///< Maximum number of paths for positioning
    AkUInt32            uMaxNumTransitions;			///< Maximum number of transitions
    AkUInt32            uDefaultPoolSize;			///< Size of the default memory pool, in bytes
	AkReal32            fDefaultPoolRatioThreshold;	///< 0.0f to 1.0f value: The percentage of occupied memory where the sound engine should enter in Low memory Mode. \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
	AkUInt32            uCommandQueueSize;			///< Size of the command queue, in bytes
	AkMemPoolId			uPrepareEventMemoryPoolID;	///< Memory pool where data allocated by AK::SoundEngine::PrepareEvent() and AK::SoundEngine::PrepareGameSyncs() will be done. 
	bool				bEnableGameSyncPreparation;	///< Set to true to enable AK::SoundEngine::PrepareGameSync usage.
	AkUInt32			uContinuousPlaybackLookAhead;	///< Number of quanta ahead when continuous containers should instantiate a new voice before which next sounds should start playing. This look-ahead time allows I/O to occur, and is especially useful to reduce the latency of continuous containers with trigger rate or sample-accurate transitions. 
													///< Default is 1 audio quantum (20.3 ms on most platforms). This value is adequate if you have a RAM-based streaming device which completes transfers within 20 ms: with 1 look-ahead quantum, voices spawned by continuous containers are more likely to be ready when they are required to play, thus improving the overall timing precision sounds scheduling. If your device completes transfers in 30 ms instead, you might consider increasing this value to 2, as it will grant new voices 2 audio quanta (~41 ms) during which they can fetch data. 

	AkUInt32			uNumSamplesPerFrame;		///< Number of samples per audio frame (256, 512, 1024 or 2048).

    AkUInt32            uMonitorPoolSize;			///< Size of the monitoring pool, in bytes. This parameter is not used in Release build.
    AkUInt32            uMonitorQueuePoolSize;		///< Size of the monitoring queue pool, in bytes. This parameter is not used in Release build.

	AkAudioAPI			eMainOutputType;			///< API used for audio output, default = AkAPI_Default.
	AkOutputSettings	settingsMainOutput;			///< Main output device settings.
	AkUInt32			uMaxHardwareTimeoutMs;		///< Amount of time to wait for HW devices to trigger an audio interrupt.  If there is no interrupt after that time, the sound engine will revert to  silent mode and continue operating until the HW finally comes back.  Default value: 2000 (2 seconds)

	bool				bUseSoundBankMgrThread;		///< Use a separate thread for loading sound banks. Allows asynchronous operations.
	bool				bUseLEngineThread;			///< Use a separate thread for processing audio. If set to false, audio processing will occur in RenderAudio(). Ignored on 3DS, Vita, Wii-U, and Xbox 360 platforms. \ref goingfurther_eventmgrthread

	AkAudioSourceChangeCallbackFunc sourceChangeCallback; ///< Application-defined audio source change event callback function.
	void* sourceChangeCallbackCookie; ///< Application-defined user data for the audio source change event callback function.
};

/// Necessary settings for setting externally-loaded sources
struct AkSourceSettings
{
	AkUniqueID	sourceID;							///< Source ID (available in the soundbank content files)
	AkUInt8*	pMediaMemory;						///< Pointer to the data to be set for the source
	AkUInt32	uMediaSize;							///< Size, in bytes, of the data to be set for the source
};

/// Audiokinetic namespace
namespace AK
{
	/// Audiokinetic sound engine namespace
	/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
	namespace SoundEngine
	{
        ///////////////////////////////////////////////////////////////////////
		/// @name Initialization
		//@{

		/// Query whether or not the sound engine has been successfully initialized.
		/// \warning This function is not thread-safe. It should not be called at the same time as SoundEngine::Init() or SoundEngine::Term().
		/// \return True if the sound engine has been initialized, False otherwise
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::Term()
		AK_EXTERNAPIFUNC( bool, IsInitialized )();

		/// Initialize the sound engine.
		/// \warning This function is not thread-safe.
		/// \remark The initial settings should be initialized using AK::SoundEngine::GetDefaultInitSettings()
		///			and AK::SoundEngine::GetDefaultPlatformInitSettings() to fill the structures with their 
		///			default settings. This is not mandatory, but it helps avoid backward compatibility problems.
		///		
		/// \return 
		/// - AK_Success if the initialization was successful
		/// - AK_MemManagerNotInitialized if the memory manager is not available or not properly initialized
		/// - AK_StreamMgrNotInitialized if the stream manager is not available or not properly initialized
		/// - AK_SSEInstructionsNotSupported if the machine does not support SSE instruction (only on the PC)
		/// - AK_InsufficientMemory or AK_Fail if there is not enough memory available to initialize the sound engine properly
		/// - AK_InvalidParameter if some parameters are invalid
		/// - AK_Fail if the sound engine is already initialized, or if the provided settings result in insufficient 
		/// resources for the initialization.
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Term()
		/// - AK::SoundEngine::GetDefaultInitSettings()
		/// - AK::SoundEngine::GetDefaultPlatformInitSettings()
		/// \if WiiU
		/// - \ref soundengine_WiiURequirements_Initialization
		/// \endif 
        AK_EXTERNAPIFUNC( AKRESULT, Init )(
            AkInitSettings *			in_pSettings,   		///< Initialization settings (can be NULL, to use the default values)
            AkPlatformInitSettings *	in_pPlatformSettings  	///< Platform-specific settings (can be NULL, to use the default values)
		    );

		/// Get the default values of the platform-independent initialization settings.
		/// \warning This function is not thread-safe.
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::GetDefaultPlatformInitSettings()
		AK_EXTERNAPIFUNC( void, GetDefaultInitSettings )(
            AkInitSettings &			out_settings   			///< Returned default platform-independent sound engine settings
		    );

		/// Get the default values of the platform-specific initialization settings.
		///
		/// Windows Specific:
		///		When initializing for Windows platform, the HWND value returned in the 
		///		AkPlatformInitSettings structure is the foreground HWND at the moment of the 
		///		initialization of the sound engine and may not be the correct one for your need.
		///		Each game must specify the HWND that will be passed to DirectSound initialization.
		///		It is required that each game provides the correct HWND to be used or it could cause
		///		one of the following problem:
		///				- Random Sound engine initialization failure.
		///				- Audio focus to be located on the wrong window.
		///
		/// \warning This function is not thread-safe.
		/// \sa 
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::GetDefaultInitSettings()
		AK_EXTERNAPIFUNC( void, GetDefaultPlatformInitSettings )(
            AkPlatformInitSettings &	out_platformSettings  	///< Returned default platform-specific sound engine settings
		    );
		
        /// Terminate the sound engine.
		/// If some sounds are still playing or events are still being processed when this function is 
		///	called, they will be stopped.
		/// \warning This function is not thread-safe.
		/// \warning Before calling Term, you must ensure that no other thread is accessing the sound engine.
		/// \sa 
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
        AK_EXTERNAPIFUNC( void, Term )();

		/// Get the output speaker configuration of the specified output.
		/// Call this function to get the speaker configuration of the output (which may not correspond
		/// to the physical output format of the platform, in the case of downmixing provided by the platform itself). 
		/// You may initialize the sound engine with a user-specified configuration, but the resulting 
		/// configuration is determined by the sound engine, based on the platform, output type and
		/// platform settings (for e.g. system menu or control panel option). 
		/// 
		/// \warning Call this function only after the sound engine has been properly initialized.
		/// \return The output configuration. An empty AkChannelConfig (!AkChannelConfig::IsValid()) if device does not exist.
		/// \sa 
		/// - AkSpeakerConfig.h
		/// - AkOutputSettings
		AK_EXTERNAPIFUNC( AkChannelConfig, GetSpeakerConfiguration )(
			AkAudioOutputType 	in_eSinkType = AkOutput_Main,	///< Output sink type. Pass AkOutput_Main for main output.
			AkUInt32 			in_iOutputID = 0			///< Player number or device-unique identifier. Pass 0 for main.
			);

		/// Get the panning rule of the specified output.
		/// \warning Call this function only after the sound engine has been properly initialized.
		/// \return One of the supported configuration: 
		/// - AkPanningRule_Speakers
		/// - AkPanningRule_Headphone
		/// \sa 
		/// - AkSpeakerConfig.h
		AK_EXTERNAPIFUNC( AKRESULT, GetPanningRule )(
			AkPanningRule &		out_ePanningRule,			///< Returned panning rule (AkPanningRule_Speakers or AkPanningRule_Headphone) for given output.
			AkAudioOutputType 	in_eSinkType = AkOutput_Main,	///< Output sink type. Pass AkOutput_Main for main output.
			AkUInt32 			in_iOutputID = 0			///< Player number or device-unique identifier. Pass 0 for main.
			);

		/// Set the panning rule of the specified output.
		/// This may be changed anytime once the sound engine is initialized.
		/// \warning This function posts a message through the sound engine's internal message queue, whereas GetPanningRule() queries the current panning rule directly.
		AK_EXTERNAPIFUNC( AKRESULT, SetPanningRule )( 
			AkPanningRule		in_ePanningRule,			///< Panning rule.
			AkAudioOutputType	in_eSinkType = AkOutput_Main,	///< Output sink type. Pass AkOutput_Main for main output.
			AkUInt32 			in_iOutputID = 0			///< Player number or device-unique identifier. Pass 0 for main.
			);

		/// Get speaker angles of the specified device. Speaker angles are used for 3D positioning of sounds over standard configurations.
		/// Note that the current version of Wwise only supports positioning on the plane.
		/// The speaker angles are expressed as an array of loudspeaker pairs, in degrees, relative to azimuth ]0,180].
		/// Supported loudspeaker setups are always symmetric; the center speaker is always in the middle and thus not specified by angles.
		/// Angles must be set in ascending order. 
		/// You may call this function with io_pfSpeakerAngles set to NULL to get the expected number of angle values in io_uNumAngles, 
		/// in order to allocate your array correctly. You may also obtain this number by calling
		/// AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ).
		/// If io_pfSpeakerAngles is not NULL, the array is filled with up to io_uNumAngles.
		/// Typical usage:
		/// - AkUInt32 uNumAngles;
		/// - GetSpeakerAngles( NULL, uNumAngles, AkOutput_Main );
		/// - AkReal32 * pfSpeakerAngles = AkAlloca( uNumAngles * sizeof(AkReal32) );
		/// - GetSpeakerAngles( pfSpeakerAngles, uNumAngles, AkOutput_Main );
		/// \warning Call this function only after the sound engine has been properly initialized.
		/// \return AK_Success if device exists.
		/// \sa SetSpeakerAngles()
		AK_EXTERNAPIFUNC( AKRESULT, GetSpeakerAngles )(
			AkReal32 *			io_pfSpeakerAngles,			///< Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
			AkUInt32 &			io_uNumAngles,				///< Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ), or just the latter if io_pfSpeakerAngles is NULL.
			AkReal32 &			out_fHeightAngle,			///< Elevation of the height layer, in degrees relative to the plane [-90,90].
			AkAudioOutputType	in_eSinkType = AkOutput_Main,	///< Output sink type. Pass AkOutput_Main for main output.
			AkUInt32 			in_iOutputID = 0			///< Player number or device-unique identifier. Pass 0 for main.
			);
		
		/// Set speaker angles of the specified device. Speaker angles are used for 3D positioning of sounds over standard configurations.
		/// Note that the current version of Wwise only supports positioning on the plane.
		/// The speaker angles are expressed as an array of loudspeaker pairs, in degrees, relative to azimuth ]0,180].
		/// Supported loudspeaker setups are always symmetric; the center speaker is always in the middle and thus not specified by angles.
		/// Angles must be set in ascending order. 
		/// Typical usage: 
		/// - Initialize the sound engine and/or add secondary output(s).
		/// - Get number of speaker angles and their value into an array using GetSpeakerAngles().
		/// - Modify the angles and call SetSpeakerAngles().
		/// This function posts a message to the audio thread through the command queue, so it is thread safe. However the result may not be immediately read with GetSpeakerAngles().
		/// \warning This function only applies to configurations (or subset of these configurations) that are standard and whose speakers are on the plane (2D).
		/// \return AK_NotCompatible if the channel configuration of the device is not standard (AK_ChannelConfigType_Standard), 
		/// AK_Success if successful (device exists and angles are valid), AK_Fail otherwise.
		/// \sa GetSpeakerAngles()
		AK_EXTERNAPIFUNC( AKRESULT, SetSpeakerAngles )(
			AkReal32 *			in_pfSpeakerAngles,			///< Array of loudspeaker pair angles, in degrees relative to azimuth [0,180].
			AkUInt32			in_uNumAngles,				///< Number of elements in in_pfSpeakerAngles. It must correspond to AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ) (the value returned by GetSpeakerAngles()).
			AkReal32 			in_fHeightAngle,			///< Elevation of the height layer, in degrees relative to the plane  [-90,90].
			AkAudioOutputType	in_eSinkType = AkOutput_Main,	///< Output sink type. Pass AkOutput_Main for main output.
			AkUInt32 			in_iOutputID = 0			///< Player number or device-unique identifier. Pass 0 for main.
			);

		/// Allows the game to set the volume threshold to be used by the sound engine to determine if a voice must go virtual.
		/// This may be changed anytime once the sound engine was initialized.
		/// If this function is not called, the used value will be the value specified in the platform specific project settings.
		/// \return 
		/// - AK_InvalidParameter if the threshold was not between 0 and -96.3 dB.
		/// - AK_Success if successful
		AK_EXTERNAPIFUNC( AKRESULT, SetVolumeThreshold )( 
			AkReal32 in_fVolumeThresholdDB ///< Volume Threshold, must be a value between 0 and -96.3 dB
			);

		/// Allows the game to set the maximum number of non virtual voices to be played simultaneously.
		/// This may be changed anytime once the sound engine was initialized.
		/// If this function is not called, the used value will be the value specified in the platform specific project settings.
		/// \return 
		/// - AK_InvalidParameter if the threshold was not between 1 and MaxUInt16.
		/// - AK_Success if successful
		AK_EXTERNAPIFUNC( AKRESULT, SetMaxNumVoicesLimit )( 
			AkUInt16 in_maxNumberVoices ///< Maximun number of non-virtual voices.
			);
				
        //@}

		////////////////////////////////////////////////////////////////////////
		/// @name Rendering Audio
		//@{

		/// Process all events in the sound engine's queue.
		/// This method has to be called periodically (usually once per game frame).
		/// \sa 
		/// - \ref concept_events
		/// - \ref soundengine_events
		/// - AK::SoundEngine::PostEvent()
		/// \return Always returns AK_Success
        AK_EXTERNAPIFUNC( AKRESULT, RenderAudio )();

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Component Registration
		//@{
		
		/// Register a plug-in with the sound engine and set the callback functions to create the 
		/// plug-in and its parameter node.
		/// \sa
		/// - \ref register_effects
		/// - \ref plugin_xml
		/// \return AK_Success if successful, AK_InvalidParameter if invalid parameters were provided or Ak_Fail otherwise. Possible reasons for an AK_Fail result are:
		/// - Insufficient memory to register the plug-in
		/// - Plug-in ID already registered
		/// \remarks
		/// Codecs and plug-ins must be registered before loading banks that use them.\n
		/// Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
        AK_EXTERNAPIFUNC( AKRESULT, RegisterPlugin )( 
			AkPluginType in_eType,						///< Plug-in type (for example, source or effect)
			AkUInt32 in_ulCompanyID,					///< Company identifier (as declared in the plug-in description XML file)
			AkUInt32 in_ulPluginID,						///< Plug-in identifier (as declared in the plug-in description XML file)
			AkCreatePluginCallback in_pCreateFunc,		///< Pointer to the plug-in's creation function
            AkCreateParamCallback in_pCreateParamFunc	///< Pointer to the plug-in's parameter node creation function
            );
		
		/// Register a codec type with the sound engine and set the callback functions to create the 
		/// codec's file source and bank source nodes.
		/// \sa 
		/// - \ref register_effects
		/// \return AK_Success if successful, AK_InvalidParameter if invalid parameters were provided, or Ak_Fail otherwise. Possible reasons for an AK_Fail result are:
		/// - Insufficient memory to register the codec
		/// - Codec ID already registered
		/// \remarks
		/// Codecs and plug-ins must be registered before loading banks that use them.\n
		/// Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
        AK_EXTERNAPIFUNC( AKRESULT, RegisterCodec )( 
			AkUInt32 in_ulCompanyID,						///< Company identifier (as declared in the plug-in description XML file)
			AkUInt32 in_ulCodecID,							///< Codec identifier (as declared in the plug-in description XML file)
			AkCreateFileSourceCallback in_pFileCreateFunc,	///< Pointer to the codec's file source node creation function
            AkCreateBankSourceCallback in_pBankCreateFunc	///< Pointer to the codec's bank source node creation function
            );

		/// Register a global callback function. This function will be called from the main audio thread for each rendered
		/// audio frame. This function will also be called from the thread calling AK::SoundEngine::Term with the bLastCall
		/// parameter set to true.
		/// \remarks
		/// It is illegal to call this function while already inside of a global callback.
		/// This function might stall for several milliseconds before returning.
		/// \sa UnregisterGlobalCallback
        AK_EXTERNAPIFUNC( AKRESULT, RegisterGlobalCallback )(
			AkGlobalCallbackFunc in_pCallback				///< Function to register as a global callback.
			);

		/// Unregister a global callback function, previously registered using RegisterGlobalCallback.
		/// \remarks
		/// It is legal to call this function while already inside of a global callback, If it is unregistering itself and not
		/// another callback.
		/// This function might stall for several milliseconds before returning.
		/// \sa RegisterGlobalCallback
        AK_EXTERNAPIFUNC( AKRESULT, UnregisterGlobalCallback )(
			AkGlobalCallbackFunc in_pCallback				///< Function to unregister as a global callback.
			);

		//@}

#ifdef AK_SUPPORT_WCHAR
		////////////////////////////////////////////////////////////////////////
		/// @name Getting ID from strings
		//@{

		/// Universal converter from unicode string to ID for the sound engine.
		/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
		/// Note:
		///		This function does return a AkUInt32, which is totally compatible with:
		///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, and so on...
		/// \sa
		/// - AK::SoundEngine::PostEvent
		/// - AK::SoundEngine::SetRTPCValue
		/// - AK::SoundEngine::SetSwitch
		/// - AK::SoundEngine::SetState
		/// - AK::SoundEngine::PostTrigger
		/// - AK::SoundEngine::SetGameObjectAuxSendValues
		/// - AK::SoundEngine::LoadBank
		/// - AK::SoundEngine::UnloadBank
		/// - AK::SoundEngine::PrepareEvent
		/// - AK::SoundEngine::PrepareGameSyncs
		AK_EXTERNAPIFUNC( AkUInt32, GetIDFromString )( const wchar_t* in_pszString );
#endif //AK_SUPPORT_WCHAR

		/// Universal converter from string to ID for the sound engine.
		/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
		/// Note:
		///		This function does return a AkUInt32, which is totally compatible with:
		///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, and so on...
		/// \sa
		/// - AK::SoundEngine::PostEvent
		/// - AK::SoundEngine::SetRTPCValue
		/// - AK::SoundEngine::SetSwitch
		/// - AK::SoundEngine::SetState
		/// - AK::SoundEngine::PostTrigger
		/// - AK::SoundEngine::SetGameObjectAuxSendValues
		/// - AK::SoundEngine::LoadBank
		/// - AK::SoundEngine::UnloadBank
		/// - AK::SoundEngine::PrepareEvent
		/// - AK::SoundEngine::PrepareGameSyncs
		AK_EXTERNAPIFUNC( AkUInt32, GetIDFromString )( const char* in_pszString );

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Event Management
		//@{

		/// Asynchronously post an event to the sound engine (by event ID).
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event. 
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        AK_EXTERNAPIFUNC( AkPlayingID, PostEvent )(
	        AkUniqueID in_eventID,							///< Unique ID of the event
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information
			AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL,///< Optional array of external source resolution information
			AkPlayingID	in_PlayingID = AK_INVALID_PLAYING_ID///< Optional (advanced users only) Specify the playing ID to target with the event. Will Cause active actions in this event to target an existing Playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any for normal playback.
	        );

#ifdef AK_SUPPORT_WCHAR
		/// Post an event to the sound engine (by event name), using callbacks.
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event. 
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        AK_EXTERNAPIFUNC( AkPlayingID, PostEvent )(
	        const wchar_t* in_pszEventName,					///< Name of the event
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information.
			AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL,///< Optional array of external source resolution information
			AkPlayingID	in_PlayingID = AK_INVALID_PLAYING_ID///< Optional (advanced users only) Specify the playing ID to target with the event. Will Cause active actions in this event to target an existing Playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any for normal playback.
	        );
#endif //AK_SUPPORT_WCHAR

		/// Post an event to the sound engine (by event name), using callbacks.
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event.  P
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        AK_EXTERNAPIFUNC( AkPlayingID, PostEvent )(
	        const char* in_pszEventName,					///< Name of the event
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information.
			AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL,///< Optional array of external source resolution information
			AkPlayingID	in_PlayingID = AK_INVALID_PLAYING_ID///< Optional (advanced users only) Specify the playing ID to target with the event. Will Cause active actions in this event to target an existing Playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any for normal playback.
	        );

		
		/// AkActionOnEventType
		/// \sa
		/// - AK::SoundEngine::ExecuteActionOnEvent()
		enum AkActionOnEventType
		{
			AkActionOnEventType_Stop	= 0,			///< Stop
			AkActionOnEventType_Pause	= 1,			///< Pause
			AkActionOnEventType_Resume	= 2,			///< Resume
			AkActionOnEventType_Break	= 3,			///< Break
			AkActionOnEventType_ReleaseEnvelope	= 4		///< Release envelope
		};

		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		AK_EXTERNAPIFUNC( AKRESULT, ExecuteActionOnEvent )(
			AkUniqueID in_eventID,												///< Unique ID of the event
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the transition
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID					///< Associated PlayingID
			);

#ifdef AK_SUPPORT_WCHAR
		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		AK_EXTERNAPIFUNC( AKRESULT, ExecuteActionOnEvent )(
			const wchar_t* in_pszEventName,										///< Name of the event
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the transition
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID					///< Associated PlayingID
			);
#endif //AK_SUPPORT_WCHAR

		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		AK_EXTERNAPIFUNC( AKRESULT, ExecuteActionOnEvent )(
			const char* in_pszEventName,										///< Name of the event
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the transition
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID					///< Associated PlayingID
			);


		/// Start streaming the first part of all steamed files referenced by an event into a cache buffer.  Caching streams are services when no other streams require the 
		/// available bandwidth. The files will remain cached until UnpinEventInStreamCache is called, or a higher priority pinned file needs the space and the limit set by 
		/// uMaxCachePinnedBytes is exceeded.  The amount of data from the start of the file that will be pinned to cache in is determined by the prefetch size,
		/// which can be set via the authoring tool and stored in the sound banks, or via the low level IO.  
		/// If this function is called multiple time successively with the same event, then the priority of the caching streams are updated.  Note however that priority is passed down
		/// to the stream manager on a file-by-file basis, and if another event is pinned to cache that references the same file but with a 
		/// different priority, then the first priority will be updated with the most recent value.
		/// If the event references file that are chosen based on a state group (via a switch container), all files in all states will be cached. Those in the current active state
		/// will get cached with active priority, while all other files will get cached with inactive priority
		/// \sa
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, PinEventInStreamCache )(
			AkUniqueID in_eventID,											///< Unique ID of the event
			AkPriority in_uActivePriority,									///< Priority of active stream caching I/O
			AkPriority in_uInactivePriority 								///< Priority of inactive stream caching I/O
			);

#ifdef AK_SUPPORT_WCHAR
		/// Start streaming the first part of all steamed files referenced by an event into a cache buffer.  Caching streams are services when no other streams require the 
		/// available bandwidth. The files will remain cached until UnpinEventInStreamCache is called, or a higher priority pinned file needs the space and the limit set by 
		/// uMaxCachePinnedBytes is exceeded.  The amount of data from the start of the file that will be pinned to cache in is determined by the prefetch size,
		/// which can be set via the authoring tool and stored in the sound banks, or via the low level IO.  
		/// If this function is called multiple time successively with the same event, then the priority of the caching streams are updated.  Note however that priority is passed down
		/// to the stream manager on a file-by-file basis, and if another event is pinned to cache that references the same file but with a 
		/// different priority, then the first priority will be updated with the most recent value.
		/// If the event references file that are chosen based on a state group (via a switch container), all files in all states will be cached. Those in the current active state
		/// will get cached with active priority, while all other files will get cached with inactive priority
		/// \sa
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, PinEventInStreamCache )(
			const wchar_t* in_pszEventName,									///< Name of the event
			AkPriority in_uActivePriority,									///< Priority of active stream caching I/O
			AkPriority in_uInactivePriority 								///< Priority of inactive stream caching I/O
			);
#endif //AK_SUPPORT_WCHAR

		/// Start streaming the first part of all steamed files referenced by an event into a cache buffer.  Caching streams are services when no other streams require the 
		/// available bandwidth. The files will remain cached until UnpinEventInStreamCache is called, or a higher priority pinned file needs the space and the limit set by 
		/// uMaxCachePinnedBytes is exceeded.  The amount of data from the start of the file that will be pinned to cache in is determined by the prefetch size,
		/// which can be set via the authoring tool and stored in the sound banks, or via the low level IO.  
		/// If this function is called multiple time successively with the same event, then the priority of the caching streams are updated.  Note however that priority is passed down
		/// to the stream manager on a file-by-file basis, and if another event is pinned to cache that references the same file but with a 
		/// different priority, then the first priority will be updated with the most recent value.
		/// If the event references file that are chosen based on a state group (via a switch container), all files in all states will be cached. Those in the current active state
		/// will get cached with active priority, while all other files will get cached with inactive priority
		/// \sa
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, PinEventInStreamCache )(
			const char* in_pszEventName,									///< Name of the event
			AkPriority in_uActivePriority,									///< Priority of active stream caching I/O
			AkPriority in_uInactivePriority 								///< Priority of inactive stream caching I/O
			);

		/// Release the set of files that were previouly requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().  The file may still remain in stream cache
		/// after AK::SoundEngine::UnpinEventInStreamCache() is called, until the memory is reused by the streaming memory manager in accordance with to its cache management algorithm.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		AK_EXTERNAPIFUNC( AKRESULT, UnpinEventInStreamCache )(
			AkUniqueID in_eventID											///< Unique ID of the event
			);	

#ifdef AK_SUPPORT_WCHAR
		/// Release the set of files that were previouly requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().  The file may still remain in stream cache
		/// after AK::SoundEngine::UnpinEventInStreamCache() is called, until the memory is reused by the streaming memory manager in accordance with to its cache management algorithm.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		AK_EXTERNAPIFUNC( AKRESULT, UnpinEventInStreamCache )(
			const wchar_t* in_pszEventName									///< Name of the event
			);
#endif //AK_SUPPORT_WCHAR

		/// Release the set of files that were previouly requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().  The file may still remain in stream cache
		/// after AK::SoundEngine::UnpinEventInStreamCache() is called, until the memory is reused by the streaming memory manager in accordance with to its cache management algorithm.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
		AK_EXTERNAPIFUNC( AKRESULT, UnpinEventInStreamCache )(
			const char* in_pszEventName										///< Name of the event
			);

		/// Return information about an event that was requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().
		/// Retrieves the smallest buffer fill-percentage for each file referenced by the event, and whether 
		/// the cache-pinned memory limit is preventing any of the files from filling up their buffer.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, GetBufferStatusForPinnedEvent )(
			AkUniqueID in_eventID,											///< Unique ID of the event
			AkReal32& out_fPercentBuffered,									///< Fill-percentage (out of 100) of requested buffer size for least buffered file in the event.
			bool& out_bCachePinnedMemoryFull								///< True if at least one file can not complete buffering because the cache-pinned memory limit would be exceeded.
			);

		/// Return information about an event that was requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().
		/// Retrieves the smallest buffer fill-percentage for each file referenced by the event, and whether 
		/// the cache-pinned memory limit is preventing any of the files from filling up their buffer.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, GetBufferStatusForPinnedEvent )(
			const char* in_pszEventName,									///< Name of the event
			AkReal32& out_fPercentBuffered,									///< Fill-percentage (out of 100) of requested buffer size for least buffered file in the event.
			bool& out_bCachePinnedMemoryFull								///< True if at least one file can not complete buffering because the cache-pinned memory limit would be exceeded.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Return information about an event that was requested to be pinned into cache via AK::SoundEngine::PinEventInStreamCache().
		/// Retrieves the smallest buffer fill-percentage for each file referenced by the event, and whether 
		/// the cache-pinned memory limit is preventing any of the files from filling up their buffer.
		/// \sa
		/// - AK::SoundEngine::PinEventInStreamCache
		/// - AK::SoundEngine::UnpinEventInStreamCache
		AK_EXTERNAPIFUNC( AKRESULT, GetBufferStatusForPinnedEvent )(
			const wchar_t* in_pszEventName,									///< Name of the event
			AkReal32& out_fPercentBuffered,									///< Fill-percentage (out of 100) of requested buffer size for least buffered file in the event.
			bool& out_bCachePinnedMemoryFull								///< True if at least one file can not complete buffering because the cache-pinned memory limit would be exceeded.
			);
#endif

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time.  
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			AkUniqueID in_eventID,										///< Unique ID of the event
			AkGameObjectID in_gameObjectID,								///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition,										///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			const wchar_t* in_pszEventName,								///< Name of the event
			AkGameObjectID in_gameObjectID,								///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition,										///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.	
			);
#endif //AK_SUPPORT_WCHAR

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			const char* in_pszEventName,								///< Name of the event
			AkGameObjectID in_gameObjectID,								///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition,										///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see note above)	
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.	
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			AkUniqueID in_eventID,										///< Unique ID of the event
			AkGameObjectID in_gameObjectID ,							///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent,										///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see note above)	
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.	
			);

#ifdef AK_SUPPORT_WCHAR
		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			const wchar_t* in_pszEventName,								///< Name of the event
			AkGameObjectID in_gameObjectID ,							///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent ,										///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see note above)	
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.	
			);
#endif //AK_SUPPORT_WCHAR

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments and music switch containers. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// Notes specific to music switch containers:
		///		- Seeking triggers a music transition towards the current (or target) segment.
		///			This transition is subject to the container's transition rule that matches the current and defined in the container,
		///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
		///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
		///		- If the specified time is greater than the destination segment's length, the modulo is taken.
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		AK_EXTERNAPIFUNC( AKRESULT, SeekOnEvent )( 
			const char* in_pszEventName,								///< Name of the event
			AkGameObjectID in_gameObjectID,								///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent,										///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false,						///< If true, the final seeking position will be made equal to the nearest marker (see notes above).	
			AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID			///< Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any, to seek on all active actions of this event ID.	
			);

		/// Cancel all event callbacks associated with a specific callback cookie.\n
		/// \sa 
		/// - AK::SoundEngine::PostEvent()
		AK_EXTERNAPIFUNC( void, CancelEventCallbackCookie )( 
			void * in_pCookie 							///< Callback cookie to be cancelled
			);

		/// Cancel all event callbacks for a specific playing ID.
		/// \sa 
		/// - AK::SoundEngine::PostEvent()
		AK_EXTERNAPIFUNC( void, CancelEventCallback )( 
			AkPlayingID in_playingID 					///< Playing ID of the event that must not use callbacks
			);

		/// Get the current position of the source associated with this playing ID, obtained from PostEvent().
		/// Notes:
		/// - You need to pass AK_EnableGetSourcePlayPosition to PostEvent() in order to use this function, otherwise
		/// 	it returns AK_Fail, even if the playing ID is valid.
		/// - The source's position is updated at every audio frame, and the time at which this occurs is stored. 
		///		When you call this function from your thread, you therefore query the position that was updated in the previous audio frame.
		///		If in_bExtrapolate is true (default), the returned position is extrapolated using the elapsed time since last 
		///		sound engine update and the source's playback rate.
		/// \return AK_Success if successful.
		///			It returns AK_InvalidParameter if the provided pointer is not valid.
		///			It returns AK_Fail if the playing ID is invalid (not playing yet, or finished playing).
		/// \sa 
		/// - \ref soundengine_query_pos
		/// - \ref concept_events
		AK_EXTERNAPIFUNC( AKRESULT, GetSourcePlayPosition )(
			AkPlayingID		in_PlayingID,				///< Playing ID returned by AK::SoundEngine::PostEvent()
			AkTimeMs*		out_puPosition,				///< Position of the source (in ms) associated with that playing ID
			bool			in_bExtrapolate = true		///< Position is extrapolated based on time elapsed since last sound engine update.
			);

		/// Get the stream buffering of the sources associated with this playing ID, obtained from PostEvent().
		/// Notes:
		/// - You need to pass AK_EnableGetSourceStreamBuffering to PostEvent() in order to use this function, otherwise
		/// 	it returns AK_Fail, even if the playing ID is valid.
		/// - The sources stream buffering is updated at every audio frame. If there are multiple sources associated with this playing ID,
		///		the value returned corresponds to the least buffered source. 
		/// - The returned buffering status out_bIsBuffering will be true If any of the sources associated with the playing ID are actively being buffered.
		///		It will be false if all of them have reached the end of file, or have reached a state where they are buffered enough and streaming is temporarily idle.
		/// - Purely in-memory sources are excluded from this database. If all sources are in-memory, GetSourceStreamBuffering() will return AK_Fail.
		/// - The returned buffering amount and state is not completely accurate with some hardware-accelerated codecs. In such cases, the amount of stream buffering is generally underestimated.
		///		On the other hand, it is not guaranteed that the source will be ready to produce data at the next audio frame even if out_bIsBuffering has turned to false.
		/// \return 
		///	- AK_Success if successful.
		/// - AK_Fail if the source data associated with this playing ID is not found, for example if PostEvent() was not called with AK_EnableGetSourceStreamBuffering, or if the header was not parsed.
		/// \sa 
		/// - \ref concept_events
		AK_EXTERNAPIFUNC( AKRESULT, GetSourceStreamBuffering )(
			AkPlayingID		in_PlayingID,				///< Playing ID returned by AK::SoundEngine::PostEvent()
			AkTimeMs &		out_buffering,				///< Returned amount of buffering (in ms) of the source (or one of the sources) associated with that playing ID
			bool &			out_bIsBuffering			///< Returned buffering status of the source(s) associated with that playing ID
			);

		/// Stop the current content playing associated to the specified game object ID.
		/// If no game object is specified, all sounds will be stopped.
		AK_EXTERNAPIFUNC( void, StopAll )( 
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< (Optional)Specify a game object to stop only playback associated to the provided game object ID.
			);

		/// Stop the current content playing associated to the specified playing ID.
		AK_EXTERNAPIFUNC( void, StopPlayingID )( 
			AkPlayingID in_playingID,											///< Playing ID to be stopped.
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
			);

		/// Set the random seed value. Can be used to synchronize randomness
		/// across instances of the Sound Engine.
		/// \remark This seeds the number generator used for all container randomizations;
		/// 		since it acts globally, this should be called right before any PostEvent
		///			call where randomness synchronization is required, and cannot guarantee
		///			similar results for continuous containers.
		AK_EXTERNAPIFUNC( void, SetRandomSeed )( 
			AkUInt32 in_uSeed													///< Random seed.
			);

		/// Mute/Unmute the busses tagged as background music.  
		/// This is automatically called for platforms that have user-music support.
		/// This function is provided to give the same behavior on platforms that don't have user-music support.
		AK_EXTERNAPIFUNC( void, MuteBackgroundMusic ) (
			bool in_bMute ///< Set true to mute, false to unmute.
			);
		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Game Objects
		//@{
		
        /// Register a game object.
		/// \return
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 and -1 are invalid)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered.
		/// \sa 
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        AK_EXTERNAPIFUNC( AKRESULT, RegisterGameObj )(
	        AkGameObjectID in_gameObjectID,				///< ID of the game object to be registered			
			AkUInt32 in_uListenerMask = 0x01			///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
	        );

		/// Register a game object.
		/// \return
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 and -1 are invalid)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered.
		/// \sa 
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        AK_EXTERNAPIFUNC( AKRESULT, RegisterGameObj )(
	        AkGameObjectID in_gameObjectID,				///< ID of the game object to be registered
			const char * in_pszObjName,					///< Name of the game object (for monitoring purpose)
			AkUInt32 in_uListenerMask = 0x01			///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
	        );

        /// Unregister a game object.
		/// \return 
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 is an invalid ID)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered. Unregistering a game object while it is 
		///			in use is allowed, but the control over the parameters of this game object is lost.
		///			For example, say a sound associated with this game object is a 3D moving sound. This sound will 
		///			stop moving when the game object is unregistered, and there will be no way to regain control over the game object.
		/// \sa 
		/// - AK::SoundEngine::RegisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        AK_EXTERNAPIFUNC( AKRESULT, UnregisterGameObj )(
	        AkGameObjectID in_gameObjectID				///< ID of the game object to be unregistered. Use 
	        											/// AK_INVALID_GAME_OBJECT to unregister all game objects.
	        );

        /// Unregister all game objects.
		/// \return Always returns AK_Success
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered. Unregistering a game object while it is 
		///			in use is allowed, but the control over the parameters of this game object is lost.
		///			For example, if a sound associated with this game object is a 3D moving sound, it will 
		///			stop moving once the game object is unregistered, and there will be no way to recover 
		///			the control over this game object.
		/// \sa 
		/// - AK::SoundEngine::RegisterGameObj()
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - \ref concept_gameobjects
        AK_EXTERNAPIFUNC( AKRESULT, UnregisterAllGameObj )();

       	/// Set the position of a game object.
		/// \warning The object's orientation vector (in_Position.Orientation) must be normalized.
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if parameters are not valid.
		/// \sa 
		/// - \ref soundengine_3dpositions
        AK_EXTERNAPIFUNC( AKRESULT, SetPosition )( 
			AkGameObjectID in_GameObjectID,		///< Game object identifier
			const AkSoundPosition & in_Position	///< Position to set; in_Position.Orientation must be normalized.
		    );

		/// Set multiple positions to a single game object.
		/// Setting multiple position on a single game object is a way to simulate multiple emission sources while using the ressources of only one voice.
		/// This can be used to simulate wall openings, area sounds, or multiple objects emiting the same sound in the same area.
		/// \aknote Calling AK::SoundEngine::SetMultiplePositions() with only one position is the same than calling AK::SoundEngine::SetPosition() \endaknote
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if parameters are not valid.
		/// \sa 
		/// - \ref soundengine_3dpositions
		/// - \ref soundengine_3dpositions_multiplepos
		/// - \ref AK::SoundEngine::MultiPositionType
        AK_EXTERNAPIFUNC( AKRESULT, SetMultiplePositions )( 
			AkGameObjectID in_GameObjectID,						///< Game object identifier.
			const AkSoundPosition * in_pPositions,				///< Array of positions to apply.
			AkUInt16 in_NumPositions,							///< Number of positions specified in the provided array.
			MultiPositionType in_eMultiPositionType = MultiPositionType_MultiDirections ///< \ref AK::SoundEngine::MultiPositionType
		    );

        /// Set the scaling factor of a game object.
		/// Modify the attenuation computations on this game object to simulate sounds with a a larger or smaller area of effect.
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if the scaling factor specified was 0 or negative.
		AK_EXTERNAPIFUNC( AKRESULT, SetAttenuationScalingFactor )(
			AkGameObjectID in_GameObjectID,				///< Game object identifier
			AkReal32 in_fAttenuationScalingFactor		///< Scaling Factor, 1 means 100%, 0.5 means 50%, 2 means 200%, and so on.
			);

        /// Set the scaling factor for a listener.
		/// A larger factor means that the listener will hear sounds with less attenuation based on distance.
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if the scaling factor specified was 0 or negative.
		AK_EXTERNAPIFUNC( AKRESULT, SetListenerScalingFactor )(
			AkUInt32 in_uListenerIndex,				///< ListenerIndex
			AkReal32 in_fListenerScalingFactor		///< Scaling Factor, 1 means 100%, 0.5 means 50%, 2 means 200%, and so on.
			);

        //@}

		////////////////////////////////////////////////////////////////////////
		/// @name Bank Management
		//@{

		/// Unload all currently loaded banks.
		/// It also internally calls ClearPreparedEvents() since at least one bank must have been loaded to allow preparing events.
		/// \return 
		/// - AK_Success if successful
		///	- AK_Fail if the sound engine was not correctly initialized or if there is not enough memory to handle the command
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		AK_EXTERNAPIFUNC( AKRESULT, ClearBanks )();

		/// Set the I/O settings of the bank load and prepare event processes.
        /// The sound engine uses default values unless explicitly set by calling this method.
		/// \warning This function must be called before loading banks.
		/// \return 
		/// - AK_Success if successful
		/// - AK_Fail if the sound engine was not correctly initialized
		/// - AK_InvalidParameter if some parameters are invalid
		/// \sa 
		/// - \ref soundengine_banks
        /// - \ref streamingdevicemanager
        AK_EXTERNAPIFUNC( AKRESULT, SetBankLoadIOSettings )(
            AkReal32            in_fThroughput,         ///< Average throughput of bank data streaming (bytes/ms) (the default value is AK_DEFAULT_BANK_THROUGHPUT)
            AkPriority          in_priority             ///< Priority of bank streaming (the default value is AK_DEFAULT_PRIORITY)
            );

#ifdef AK_SUPPORT_WCHAR
		/// Load a bank synchronously (by unicode string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::SoundEngine::GetIDFromString
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        const wchar_t*      in_pszString,		    ///< Name of the bank to load
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            AkBankID &          out_bankID				///< Returned bank ID
	        );
#endif //AK_SUPPORT_WCHAR

		/// Load a bank synchronously.\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::SoundEngine::GetIDFromString
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        const char*         in_pszString,		    ///< Name of the bank to load
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank synchronously (by ID).\n
		/// The bank ID is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        AkBankID			in_bankID,              ///< Bank ID of the bank to load
            AkMemPoolId         in_memPoolId			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            );

		/// Load a bank synchronously (from in-memory data, in-place).\n
		///
		/// IMPORTANT: Banks loaded from memory with in-place data MUST be unloaded using the UnloadBank function
		/// providing the same memory pointer. Make sure you are using the correct UnloadBank(...) overload
		///
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine.
		/// In-memory loading is in-place: *** the memory must be valid until the bank is unloaded. ***
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is stored in the first few bytes of the bank file. You may use this 
		/// ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - (Xbox360 only): If the bank may contain XMA in memory data, the memory must be allocated using the Physical memory allocator.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by using synchrone versions
		/// of the UnloadBank function.
		/// - Avoid using this function for banks containing a lot of events or structure data.  
		///	  This data will be loaded in the Default Pool anyway thus duplicating memory (one copy in the Default Pool 
		///   and one in the block you provided).  For event/structure-only banks, prefer the other versions of LoadBank().
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load (pointer is stored in sound engine, memory must remain valid)
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
            AkBankID &          out_bankID				///< Returned bank ID
	        );

		/// Load a bank synchronously (from in-memory data, out-of-place).\n
		///
		/// NOTE: Banks loaded from in-memory with out-of-place data must be unloaded using the standard UnloadBank function
		/// (with no memory pointer). Make sure you are using the correct UnloadBank(...) overload
		///
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine, the media section of the bank will be copied into the 
		/// specified memory pool.  
		/// In-memory loading is out-of-place: the buffer can be release as soon as the function returns. The advantage of using this
		/// over the in-place version is that there is no duplication of bank structures.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is stored in the first few bytes of the bank file. You may use this 
		/// ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - (Xbox360 only): If the bank may contain XMA in memory data, the memory must be allocated using the Physical memory allocator.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by using synchrone versions
		/// of the UnloadBank function.
		/// - Avoid using this function for banks containing a lot of events or structure data.  
		///	  This data will be loaded in the Default Pool anyway thus duplicating memory (one copy in the Default Pool 
		///   and one in the block you provided).  For event/structure-only banks, prefer the other versions of LoadBank().
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
			const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load (pointer is not stored in sound engine, memory can be released after return)
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
			AkMemPoolId			in_uPoolForBankMedia,	///< Memory pool to copy the media section of the bank to (the pool is created if AK_DEFAULT_POOL_ID is passed).
			AkBankID &          out_bankID				///< Returned bank ID
			);

#ifdef AK_SUPPORT_WCHAR
        /// Load a bank asynchronously (by unicode string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager (AK::IAkStreamMgr::CreateStd()), or in the Low-Level I/O module 
		/// (AK::StreamMgr::IAkFileLocationResolver::Open()) if you use the default Stream Manager's implementation.
		/// - The cookie (in_pCookie) is passed to the Low-Level I/O module for your convenience, in AK::StreamMgr::IAkFileLocationResolver::Open() 
		// as AkFileSystemFlags::pCustomParam.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        const wchar_t*      in_pszString,           ///< Name/path of the bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function, and also to  AK::StreamMgr::IAkFileLocationResolver::Open() as AkFileSystemFlags::pCustomParam)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
			AkBankID &          out_bankID				///< Returned bank ID
	        );
#endif //AK_SUPPORT_WCHAR

        /// Load a bank asynchronously.\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager (AK::IAkStreamMgr::CreateStd()), or in the Low-Level I/O module 
		/// (AK::StreamMgr::IAkFileLocationResolver::Open()) if you use the default Stream Manager's implementation.
		/// - The cookie (in_pCookie) is passed to the Low-Level I/O module for your convenience, in AK::StreamMgr::IAkFileLocationResolver::Open() 
		// as AkFileSystemFlags::pCustomParam.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        const char*         in_pszString,			///< Name/path of the bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function, and also to  AK::StreamMgr::IAkFileLocationResolver::Open() as AkFileSystemFlags::pCustomParam)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
			AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank asynchronously (by ID).\n
		/// The bank ID is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The file path should be resolved in your implementation of the Stream Manager, or in the Low-Level I/O module if 
		/// you use the default Stream Manager's implementation. The ID overload of AK::IAkStreamMgr::CreateStd() and AK::StreamMgr::IAkFileLocationResolver::Open() are called.
		/// - The cookie (in_pCookie) is passed to the Low-Level I/O module for your convenience, in AK::StreamMgr::IAkFileLocationResolver::Open() 
		// as AkFileSystemFlags::pCustomParam.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
	        AkBankID			in_bankID,				///< Bank ID of the bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function, and also to  AK::StreamMgr::IAkFileLocationResolver::Open() as AkFileSystemFlags::pCustomParam)
            AkMemPoolId         in_memPoolId			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
	        );

		/// Load a bank asynchronously (from in-memory data, in-place).\n
		///
		/// IMPORTANT: Banks loaded from memory with in-place data MUST be unloaded using the UnloadBank function
		/// providing the same memory pointer. Make sure you are using the correct UnloadBank(...) overload
		///
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine.
		/// In-memory loading is in-place: *** the memory must be valid until the bank is unloaded. ***
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise, or AK_InvalidParameter if memory alignment is not correct.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - (Xbox360 only): If the bank may contain XMA in memory data, the memory must be allocated using the Physical memory allocator.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
			const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load (pointer is stored in sound engine, memory must remain valid)
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie
			AkBankID &          out_bankID				///< Returned bank ID
	        );

		/// Load a bank asynchronously (from in-memory data, out-of-place).\n
		///
		/// NOTE: Banks loaded from in-memory with out-of-place data must be unloaded using the standard UnloadBank function
		/// (with no memory pointer). Make sure you are using the correct UnloadBank(...) overload
		///
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine, the media section of the bank will be copied into the 
		/// specified memory pool.  
		/// In-memory loading is out-of-place: the buffer can be released after the callback function is called. The advantage of using this
		/// over the in-place version is that there is no duplication of bank structures.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise, or AK_InvalidParameter if memory alignment is not correct.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - (Xbox360 only): If the bank may contain XMA in memory data, the memory must be allocated using the Physical memory allocator.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, LoadBank )(
			const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load (pointer is not stored in sound engine, memory can be released after callback)
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie
			AkMemPoolId			in_uPoolForBankMedia,	///< Memory pool to copy the media section of the bank to (the pool is created if AK_DEFAULT_POOL_ID is passed).
			AkBankID &          out_bankID				///< Returned bank ID
			);

#ifdef AK_SUPPORT_WCHAR
        /// Unload a bank synchronously (by unicode string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// - If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        const wchar_t*      in_pszString,           ///< Name of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated toa  memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
	        AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );
#endif //AK_SUPPORT_WCHAR

        /// Unload a bank synchronously.\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// - If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        const char*         in_pszString,           ///< Name of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated toa  memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
	        AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );

		/// Unload a bank synchronously (by ID and memory pointer).\n
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        AkBankID            in_bankID,              ///< ID of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated to a memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
            AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );

#ifdef AK_SUPPORT_WCHAR
		/// Unload a bank asynchronously (by unicode string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        const wchar_t*      in_pszString,           ///< Name of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated toa  memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie 				///< Callback cookie (reserved to user, passed to the callback function)
	        );
#endif //AK_SUPPORT_WCHAR

		/// Unload a bank asynchronously.\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        const char*         in_pszString,           ///< Name of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated toa  memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie 				///< Callback cookie (reserved to user, passed to the callback function)
	        );

		/// Unload a bank asynchronously (by ID and memory pointer).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		AK_EXTERNAPIFUNC( AKRESULT, UnloadBank )(
	        AkBankID            in_bankID,				///< ID of the bank to unload
			const void *		in_pInMemoryBankPtr,	///< Memory pointer from where the bank was initially loaded from. (REQUIRED to determine which bank associated toa  memory pointer must be unloaded). Pass NULL only if NULL was passed when loading the bank.
			AkBankCallbackFunc  in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
	        );

		/// Cancel all event callbacks associated with a specific callback cookie specified while loading Banks of preparing events.\n
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		AK_EXTERNAPIFUNC( void, CancelBankCallbackCookie )( 
			void * in_pCookie 							///< Callback cookie to be cancelled
			);

		/// Preparation type.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::PrepareGameSyncs()
		/// - AK::SoundEngine::PrepareBank()
		enum PreparationType
		{
			Preparation_Load,	///< PrepareEvent will load required information to play the specified event.
			Preparation_Unload	///< PrepareEvent will unload required information to play the specified event.
		};

		/// Parameter to be passed to AK::SoundEngine::PrepareBank().
		/// Use AkBankContent_All to load both the media and structural content form the bank. 
		/// Use AkBankContent_StructureOnly to load only the structural content, including events, from the bank and then later use the PrepareEvent() functions to load media on demand from loose files on the disk.
		/// \sa 
		/// - AK::SoundEngine::PrepareBank()
		/// - \ref soundengine_banks_preparingbanks
		enum AkBankContent
		{
			AkBankContent_StructureOnly,	///< Use AkBankContent_All to load both the media and structural content.
			AkBankContent_All				///< Use AkBankContent_StructureOnly to load only the structural content, including events, and then later use the PrepareEvent() functions to load media on demand from loose files on the disk.
		};

#ifdef AK_SUPPORT_WCHAR
		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank; but, as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///	for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*        in_pszString,								///< Name of the bank to Prepare/Unprepare.
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);
#endif //AK_SUPPORT_WCHAR

		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank; but, as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///		for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*           in_pszString,								///< Name of the bank to Prepare/Unprepare.
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);

		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank, but as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///		for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,									///< ID of the bank to Prepare/Unprepare.
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);

#ifdef AK_SUPPORT_WCHAR
		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank, but as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///		for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*      in_pszString,								///< Name of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,							///< Callback function
			void *              in_pCookie,									///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);
#endif //AK_SUPPORT_WCHAR

		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank, but as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///		for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*         in_pszString,								///< Name of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,							///< Callback function
			void *              in_pCookie,									///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);

		/// This function will load the events, structural content, and optionally, the media content from the SoundBank. If the flag AkBankContent_All is specified, PrepareBank() will load the media content from 
		/// the bank, but as opposed to LoadBank(), the media will be loaded one media item at a time instead of in one contiguous memory block. Using PrepareBank(), alone or in combination with PrepareEvent(), 
		/// will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.  
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		/// \remarks
		/// PrepareBank(), when called with the flag AkBankContent_StructureOnly, requires additional calls to PrepareEvent() to load the media for each event. PrepareEvent(), however, is unable to 
		///		access media content contained within SoundBanks and requires that the media be available as loose files in the file system. This flag may be useful to implement multiple loading configurations;
		///		for example, a game may have a tool mode that uses PrepareEvent() to load loose files on-demand and, also, a game mode that uses LoadBank() to load the bank in entirety.
		AK_EXTERNAPIFUNC( AKRESULT, PrepareBank )(
			AK::SoundEngine::PreparationType		in_PreparationType,				///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,						///< ID of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,				///< Callback function
			void *              in_pCookie,						///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent		in_uFlags = AkBankContent_All	///< Structures only (including events) or all content.
			);
		
		/// Clear all previously prepared events.\n
		/// \return
		/// - AK_Success if successful.
		///	- AK_Fail if the sound engine was not correctly initialized or if there is not enough memory to handle the command.
		/// \remarks
		/// The function ClearBanks() also clears all prepared events.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearBanks()
		AK_EXTERNAPIFUNC( AKRESULT, ClearPreparedEvents )();

#ifdef AK_SUPPORT_WCHAR
		/// Prepare or un-prepare events synchronously (by unicode string).\n
		/// The events are identified by strings, and converted to IDs internally
		/// (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t**		in_ppszString,			///< Array of event names
			AkUInt32			in_uNumEvent			///< Number of events in the array
			);
#endif //AK_SUPPORT_WCHAR

		/// Prepare or un-prepare events synchronously.\n
		/// The events are identified by strings, and converted to IDs internally
		/// (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char**		in_ppszString,			///< Array of event names
			AkUInt32			in_uNumEvent			///< Number of events in the array
			);

		/// Prepare or un-prepare events synchronously (by ID).
		/// The events are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkUniqueID*			in_pEventID,			///< Array of event IDs
			AkUInt32			in_uNumEvent			///< Number of event IDs in the array
			);

#ifdef AK_SUPPORT_WCHAR
		/// Prepare or un-prepare an event asynchronously (by unicode string).
		/// The events are identified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t**		in_ppszString,			///< Array of event names
			AkUInt32			in_uNumEvent,			///< Number of events in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);
#endif //AK_SUPPORT_WCHAR

		/// Prepare or un-prepare an event asynchronously.
		/// The events are identified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char**		in_ppszString,			///< Array of event names 
			AkUInt32			in_uNumEvent,			///< Number of events in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare events asynchronously (by ID).\n
		/// The events are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareEvent )( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkUniqueID*			in_pEventID,			///< Array of event IDs
			AkUInt32			in_uNumEvent,			///< Number of event IDs in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Indicate the location of a specific Media ID in memory
		/// The sources are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// \return AK_Success if operation was successful, AK_InvalidParameter if in_pSourceSettings is invalid, and AK_Fail otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, SetMedia )( 
			AkSourceSettings *	in_pSourceSettings,		///< Array of Source Settings
			AkUInt32			in_uNumSourceSettings	///< Number of Source Settings in the array
			);

		/// Remove the specified source from the list of loaded media.
		/// The sources are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// \return AK_Success if operation was successful, AK_InvalidParameter if in_pSourceSettings is invalid, and AK_Fail otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, UnsetMedia )( 
			AkSourceSettings *	in_pSourceSettings,		///< Array of Source Settings
			AkUInt32			in_uNumSourceSettings	///< Number of Source Settings in the array
			);

#ifdef AK_SUPPORT_WCHAR
		/// Prepare or un-prepare game syncs synchronously (by unicode string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			const wchar_t*	in_pszGroupName,			///< The state group Name or the Switch Group Name.
			const wchar_t**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support.
			AkUInt32		in_uNumGameSyncs			///< The number of game sync in the string array.
			);
#endif //AK_SUPPORT_WCHAR

		/// Prepare or un-prepare game syncs synchronously.\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			const char*		in_pszGroupName,			///< The state group Name or the Switch Group Name.
			const char**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support.
			AkUInt32		in_uNumGameSyncs			///< The number of game sync in the string array.
			);

		/// Prepare or un-prepare game syncs synchronously (by ID).\n
		/// The group and game syncs are specified by ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason. (Most likely small allocation failure)
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			AkUInt32		in_GroupID,					///< The state group ID or the Switch Group ID.
			AkUInt32*		in_paGameSyncID,			///< Array of ID of the gamesyncs to either support or not support.
			AkUInt32		in_uNumGameSyncs			///< The number of game sync ID in the array.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Prepare or un-prepare game syncs asynchronously (by unicode string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			const wchar_t*		in_pszGroupName,		///< The state group Name or the Switch Group Name.
			const wchar_t**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support.
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync in the string array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);
#endif //AK_SUPPORT_WCHAR

		/// Prepare or un-prepare game syncs asynchronously.\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			const char*			in_pszGroupName,		///< The state group Name or the Switch Group Name.
			const char**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support.
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync in the string array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare game syncs asynchronously (by ID).\n
		/// The group and game syncs are specified by ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		AK_EXTERNAPIFUNC( AKRESULT, PrepareGameSyncs )(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			AkUInt32			in_GroupID,				///< The state group ID or the Switch Group ID.
			AkUInt32*			in_paGameSyncID,		///< Array of ID of the gamesyncs to either support or not support.
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync ID in the array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

	    //@}


		////////////////////////////////////////////////////////////////////////
		/// @name Listeners
		//@{

		/// Set a game object's active listeners.
		/// By default, all new game objects only have the listener 0 active.  Inactive listeners are not computed.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_multi_assignobjects
		AK_EXTERNAPIFUNC( AKRESULT, SetActiveListeners )(
			AkGameObjectID in_GameObjectID,				///< Game object identifier
			AkUInt32 in_uListenerMask					///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
			);

		/// Set a listener's position.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_settingpos
        AK_EXTERNAPIFUNC( AKRESULT, SetListenerPosition )( 
			const AkListenerPosition & in_Position,		///< Position to set
			AkUInt32 in_uIndex = 0 						///< Listener index (0: first listener, 7: 8th listener)
		    );

		/// Set a listener's spatialization parameters. This let you define listener-specific 
		/// volume offsets for each audio channel.
		/// If in_bSpatialized is false, only in_pVolumeOffsets is used for this listener (3D positions 
		/// have no effect on the speaker distribution). Otherwise, in_pVolumeOffsets is added to the speaker
		/// distribution computed for this listener.
		/// Use helper functions of AK::SpeakerVolumes to manipulate the vector of volume offsets in_pVolumeOffsets.
		/// 
		/// If a sound is mixed into a bus that has a different speaker configuration than in_channelConfig,
		/// standard up/downmix rules apply.
		/// \return AK_Success if message was successfully posted to sound engine queue, AK_Fail otherwise.
		/// \sa 
		/// - \ref soundengine_listeners_spatial
		AK_EXTERNAPIFUNC( AKRESULT, SetListenerSpatialization )(
			AkUInt32 in_uIndex,							///< Listener index (0: first listener, 7: 8th listener)
			bool in_bSpatialized,						///< Spatialization toggle (True : enable spatialization, False : disable spatialization)
			AkChannelConfig in_channelConfig,			///< Channel configuration associated with volumes in_pVolumeOffsets. Ignored if in_pVolumeOffsets is NULL.
			AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets = NULL	///< Per-speaker volume offset, in dB. See AkSpeakerVolumes.h for how to manipulate this vector.
			);

		/// Set a listener's ability to listen to audio and motion events.
		/// By default, all listeners are enabled for audio and disabled for motion.  
		/// \aknote If your game doesn't use Motion, you should NOT need to use this function. 
		/// This function isn't a global "activate" switch on the listeners.  Use SetActiveListeners properly to
		/// control which listeners are used in your game. \endaknote
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_multi_assignobjects
		/// - AK::SoundEngine::SetActiveListeners
		AK_EXTERNAPIFUNC( AKRESULT, SetListenerPipeline )(
			AkUInt32 in_uIndex,						///< Listener index (0: first listener, 7: 8th listener)
			bool in_bAudio,							///< True=Listens to audio events (by default it is true)
			bool in_bMotion							///< True=Listens to motion events (by default it is false)
			);


	    //@}


		////////////////////////////////////////////////////////////////////////
		/// @name Game Syncs
		//@{

		/// Set the value of a real-time parameter control (by ID).
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
        AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValue )( 
			AkRtpcID in_rtpcID, 									///< ID of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
		    );

#ifdef AK_SUPPORT_WCHAR
		/// Set the value of a real-time parameter control (by unicode string name).
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszRtpcName is NULL.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
        AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValue )( 
			const wchar_t* in_pszRtpcName,							///< Name of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
		    );
#endif //AK_SUPPORT_WCHAR

		/// Set the value of a real-time parameter control.
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszRtpcName is NULL.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
        AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValue )( 
			const char* in_pszRtpcName,								///< Name of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
		    );

		/// Set the value of a real-time parameter control (by ID).
		/// With this function, you may set a game parameter value on playing id scope. 
		/// Playing id scope supersedes both game object scope and global scope. 
		/// Note that busses ignore RTPCs when they are applied on playing id scope. Thus, you may only change bus 
		/// or bus plugins properties by calling SetRTPCValue() with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValueByPlayingID() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
		AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValueByPlayingID )( 
			AkRtpcID in_rtpcID, 									///< ID of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkPlayingID in_playingID,								///< Associated playing ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Set the value of a real-time parameter control (by unicode string name).
		/// With this function, you may set a game parameter value on playing id scope. 
		/// Playing id scope supersedes both game object scope and global scope. 
		/// Note that busses ignore RTPCs when they are applied on playing id scope. Thus, you may only change bus 
		/// or bus plugins properties by calling SetRTPCValue() with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValueByPlayingID() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
		AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValueByPlayingID )( 
			const wchar_t* in_pszRtpcName,							///< Name of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkPlayingID in_playingID,								///< Associated playing ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);
#endif //AK_SUPPORT_WCHAR
		
		/// Set the value of a real-time parameter control (by string name).
		/// With this function, you may set a game parameter value on playing id scope. 
		/// Playing id scope supersedes both game object scope and global scope. 
		/// Note that busses ignore RTPCs when they are applied on playing id scope. Thus, you may only change bus 
		/// or bus plugins properties by calling SetRTPCValue() with AK_INVALID_GAME_OBJECT.
		/// With this function, you may also change the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValueByPlayingID() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. Thus, if you call this 
		/// function at every game frame, you should not use in_uValueChangeDuration, as it would have no effect and it is less efficient.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
		AK_EXTERNAPIFUNC( AKRESULT, SetRTPCValueByPlayingID )( 
			const char* in_pszRtpcName,								///< Name of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkPlayingID in_playingID,								///< Associated playing ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);

		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// With this function, you may also reset the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() or ResetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::SetRTPCValue()
		AK_EXTERNAPIFUNC( AKRESULT, ResetRTPCValue )(
			AkRtpcID in_rtpcID, 									///< ID of the game parameter
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards its default value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// With this function, you may also reset the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() or ResetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszParamName is NULL.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::SetRTPCValue()
		AK_EXTERNAPIFUNC( AKRESULT, ResetRTPCValue )(
			const wchar_t* in_pszRtpcName,							///< Name of the game parameter
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards its default value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);
#endif //AK_SUPPORT_WCHAR

		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// With this function, you may also reset the value of a game parameter over time. To do so, specify a non-zero 
		/// value for in_uValueChangeDuration. At each audio frame, the game parameter value will be updated internally 
		/// according to the interpolation curve. If you call SetRTPCValue() or ResetRTPCValue() with in_uValueChangeDuration = 0 in the 
		/// middle of an interpolation, the interpolation stops and the new value is set directly. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszParamName is NULL.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::SetRTPCValue()
		AK_EXTERNAPIFUNC( AKRESULT, ResetRTPCValue )(
			const char* in_pszRtpcName,								///< Name of the game parameter
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
			AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards its default value
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
			bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
			);

		/// Set the state of a switch group (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_switch
		/// - AK::SoundEngine::GetIDFromString()
        AK_EXTERNAPIFUNC( AKRESULT, SetSwitch )( 
			AkSwitchGroupID in_switchGroup, 			///< ID of the switch group
			AkSwitchStateID in_switchState, 			///< ID of the switch
			AkGameObjectID in_gameObjectID				///< Associated game object ID
		    );

#ifdef AK_SUPPORT_WCHAR
		/// Set the state of a switch group (by unicode string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the switch or switch group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_switch
        AK_EXTERNAPIFUNC( AKRESULT, SetSwitch )( 
			const wchar_t* in_pszSwitchGroup,			///< Name of the switch group
			const wchar_t* in_pszSwitchState, 			///< Name of the switch
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);
#endif //AK_SUPPORT_WCHAR

		/// Set the state of a switch group.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the switch or switch group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_switch
        AK_EXTERNAPIFUNC( AKRESULT, SetSwitch )( 
			const char* in_pszSwitchGroup,				///< Name of the switch group
			const char* in_pszSwitchState, 				///< Name of the switch
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Post the specified trigger (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_triggers
		/// - AK::SoundEngine::GetIDFromString()
		AK_EXTERNAPIFUNC( AKRESULT, PostTrigger )( 
			AkTriggerID 	in_triggerID, 				///< ID of the trigger
			AkGameObjectID 	in_gameObjectID				///< Associated game object ID
		    );

#ifdef AK_SUPPORT_WCHAR
		/// Post the specified trigger (by unicode string name).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the trigger name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_triggers
        AK_EXTERNAPIFUNC( AKRESULT, PostTrigger )( 
			const wchar_t* in_pszTrigger,				///< Name of the trigger
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);
#endif //AK_SUPPORT_WCHAR

		/// Post the specified trigger.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the trigger name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_triggers
        AK_EXTERNAPIFUNC( AKRESULT, PostTrigger )( 
			const char* in_pszTrigger,			 	    ///< Name of the trigger
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Set the state of a state group (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        AK_EXTERNAPIFUNC( AKRESULT, SetState )( 
			AkStateGroupID in_stateGroup, 				///< ID of the state group
			AkStateID in_state 							///< ID of the state
		    );

#ifdef AK_SUPPORT_WCHAR
		/// Set the state of a state group (by unicode string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the state or state group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        AK_EXTERNAPIFUNC( AKRESULT, SetState )( 
			const wchar_t* in_pszStateGroup,				///< Name of the state group
			const wchar_t* in_pszState 						///< Name of the state
			);
#endif //AK_SUPPORT_WCHAR

		/// Set the state of a state group.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the state or state group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-insensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        AK_EXTERNAPIFUNC( AKRESULT, SetState )( 
			const char* in_pszStateGroup,					///< Name of the state group
			const char* in_pszState 						///< Name of the state
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Environments
		//@{

		/// Set the auxiliary busses to route the specified game object
		/// The array size cannot exceed AK_MAX_AUX_PER_OBJ.
		/// To clear the game object's auxiliary sends, in_uNumSendValues must be 0.
		/// \aknote The actual maximum number of aux sends in which a game object can be is AK_MAX_AUX_PER_OBJ. \endaknote
		/// \aknote If an send control value is set to 0%, the effect won't be inserted at all, as if it wasn't part of the array.
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_dynamic_aux_bus_routing
		/// - \ref soundengine_environments_id_vs_string
		/// - AK::SoundEngine::GetIDFromString()
		/// \return 
		/// - AK_Success if successful
		///	- AK_InvalidParameter if the array size exceeds AK_MAX_AUX_PER_OBJ, or if a duplicated environment is found in the array
		AK_EXTERNAPIFUNC( AKRESULT, SetGameObjectAuxSendValues )( 
			AkGameObjectID		in_gameObjectID,		///< Associated game object ID
			AkAuxSendValue*		in_aAuxSendValues,		///< Variable-size array of AkAuxSendValue structures
														///< (it may be NULL if no environment must be set, and its size cannot exceed AK_MAX_AUX_PER_OBJ)
			AkUInt32			in_uNumSendValues		///< The number of auxiliary busses at the pointer's address
														///< (it must be 0 if no environment is set, and can not exceed AK_MAX_AUX_PER_OBJ)
			);

		/// Register a callback to be called to allow the game to modify or override the volume to be applied at the output of an audio bus.
		/// The callback must be registered once per bus ID to override.
		/// Call with in_pfnCallback = NULL to unregister.
		/// \sa 
		/// - \ref soundengine_environments
		/// - AkSpeakerVolumeMatrixCallbackInfo
		/// - AK::IAkMixerInputContext
		/// - AK::IAkMixerPluginContext
		AK_EXTERNAPIFUNC( AKRESULT, RegisterBusVolumeCallback )( 
			AkUniqueID in_busID,						///< Bus ID, as obtained by GetIDFromString( bus_name ).
			AkBusCallbackFunc in_pfnCallback			///< Callback function.
			);

		/// Register a callback to be called to allow the game to modify or override the volume to be applied at the output of an audio bus.
		/// You may also use this callback to obtain additional information from the bus, such as metering.
		/// The callback must be registered once per bus ID.
		/// Call with in_pfnCallback = NULL to unregister.
		/// \sa 
		/// - AK::IAkMetering
		AK_EXTERNAPIFUNC( AKRESULT, RegisterBusMeteringCallback )( 
			AkUniqueID in_busID,						///< Bus ID, as obtained by GetIDFromString( bus_name ).
			AkBusMeteringCallbackFunc in_pfnCallback,	///< Callback function.
			AkMeteringFlags in_eMeteringFlags			///< Metering flags.
			);

		/// Set the output bus volume (direct) to be used for the specified game object.
		/// The control value is a number ranging from 0.0f to 1.0f.
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_setting_dry_environment
		/// - \ref soundengine_environments_id_vs_string
		/// \return Always returns AK_Success
		AK_EXTERNAPIFUNC( AKRESULT, SetGameObjectOutputBusVolume )( 
			AkGameObjectID		in_gameObjectID,		///< Associated game object ID
			AkReal32			in_fControlValue		///< Dry level control value, ranging from 0.0f to 1.0f
														///< (0.0f stands for 0% dry, while 1.0f stands for 100% dry)
			);

		/// Set an effect shareset at the specified audio node and effect slot index.
		/// The target node cannot be a Bus, to set effects on a bus, use SetBusEffect() instead.
		/// \aknote The option "Override Parent" in 
		/// the Effect section in Wwise must be enabled for this node, otherwise the parent's effect will 
		/// still be the one in use and the call to SetActorMixerEffect will have no impact.
		/// 
		/// \return Always returns AK_Success
		AK_EXTERNAPIFUNC( AKRESULT, SetActorMixerEffect )( 
			AkUniqueID in_audioNodeID,					///< Can be a member of the actor-mixer or interactive music hierarchy (not a bus)
			AkUInt32 in_uFXIndex,						///< Effect slot index (0-3)
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to clear the effect slot
			);

		/// Set an effect shareset at the specified bus and effect slot index.
		/// The bus can either be an audio bus or an auxiliary bus.
		/// This adds a reference on the audio node to an existing ShareSet.
		/// \aknote This function has unspecified behavior when adding an effect to a currently playing
		/// bus which does not have any effects, or removing the last effect on a currently playing bus.
		/// \aknote This function will replace existing effects on the node. If the target node is not at 
		/// the top of the hierarchy and is in the actor-mixer hierarchy, the option "Override Parent" in 
		/// the Effect section in Wwise must be enabled for this node, otherwise the parent's effect will 
		/// still be the one in use and the call to SetBusEffect will have no impact.
		/// 
		/// \return Always returns AK_Success
		AK_EXTERNAPIFUNC( AKRESULT, SetBusEffect )( 
			AkUniqueID in_audioNodeID,					///< Bus Short ID.
			AkUInt32 in_uFXIndex,						///< Effect slot index (0-3)
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to clear the effect slot
			);

#ifdef AK_SUPPORT_WCHAR
		/// Set an effect shareset at the specified bus and effect slot index.
		/// The bus can either be an audio bus or an auxiliary bus.
		/// This adds a reference on the audio node to an existing ShareSet.
		/// \aknote This function has unspecified behavior when adding an effect to a currently playing
		/// bus which does not have any effects, or removing the last effect on a currently playing bus.
		/// \aknote This function will replace existing effects on the node. If the target node is not at 
		/// the top of the hierarchy and is in the actor-mixer hierarchy, the option "Override Parent" in 
		/// the Effect section in Wwise must be enabled for this node, otherwise the parent's effect will 
		/// still be the one in use and the call to SetBusEffect will have no impact.
		/// 
		/// \returns AK_IDNotFound is name not resolved, returns AK_Success otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, SetBusEffect )( 
			const wchar_t* in_pszBusName,				///< Bus name
			AkUInt32 in_uFXIndex,						///< Effect slot index (0-3)
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to clear the effect slot
			);
#endif //AK_SUPPORT_WCHAR

		/// Set an effect shareset at the specified bus and effect slot index.
		/// The bus can either be an audio bus or an auxiliary bus.
		/// This adds a reference on the audio node to an existing ShareSet.
		/// \aknote This function has unspecified behavior when adding an effect to a currently playing
		/// bus which does not have any effects, or removing the last effect on a currently playing bus.
		/// \aknote This function will replace existing effects on the node. If the target node is not at 
		/// the top of the hierarchy and is in the actor-mixer hierarchy, the option "Override Parent" in 
		/// the Effect section in Wwise must be enabled for this node, otherwise the parent's effect will 
		/// still be the one in use and the call to SetBusEffect will have no impact.
		/// 
		/// \returns AK_IDNotFound is name not resolved, returns AK_Success otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, SetBusEffect )( 
			const char* in_pszBusName,		///< Bus name
			AkUInt32 in_uFXIndex,			///< Effect slot index (0-3)
			AkUniqueID in_shareSetID		///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to clear the effect slot
			);

		/// Set a Mixer shareset at the specified bus.
		/// \aknote This function has unspecified behavior when adding a mixer to a currently playing
		/// bus which does not have any effects nor mixer, or removing the last mixer on a currently playing bus.
		/// \aknote This function will replace existing mixers on the node. 
		/// 
		/// \return Always returns AK_Success
		AK_EXTERNAPIFUNC( AKRESULT, SetMixer )( 
			AkUniqueID in_audioNodeID,					///< Bus Short ID.
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to remove.
			);

#ifdef AK_SUPPORT_WCHAR
		/// Set a Mixer shareset at the specified bus.
		/// \aknote This function has unspecified behavior when adding a mixer to a currently playing
		/// bus which does not have any effects nor mixer, or removing the last mixer on a currently playing bus.
		/// \aknote This function will replace existing mixers on the node. 
		/// 
		/// \returns AK_IDNotFound is name not resolved, returns AK_Success otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, SetMixer )( 
			const wchar_t* in_pszBusName,				///< Bus name
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to remove.
			);
#endif //AK_SUPPORT_WCHAR

		/// Set a Mixer shareset at the specified bus.
		/// \aknote This function has unspecified behavior when adding a mixer to a currently playing
		/// bus which does not have any effects nor mixer, or removing the last mixer on a currently playing bus.
		/// \aknote This function will replace existing mixers on the node. 
		/// 
		/// \returns AK_IDNotFound is name not resolved, returns AK_Success otherwise.
		AK_EXTERNAPIFUNC( AKRESULT, SetMixer )( 
			const char* in_pszBusName,		///< Bus name
			AkUniqueID in_shareSetID		///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to remove.
			);

		/// Set a game object's obstruction and occlusion levels.
		/// This method is used to affect how an object should be heard by a specific listener.
		/// \sa 
		/// - \ref soundengine_obsocc
		/// - \ref soundengine_environments
		/// \return Always returns AK_Success
		AK_EXTERNAPIFUNC( AKRESULT, SetObjectObstructionAndOcclusion )(  
			AkGameObjectID in_ObjectID,			///< Associated game object ID
			AkUInt32 in_uListener,				///< Listener index (0: first listener, 7: 8th listener)
			AkReal32 in_fObstructionLevel,		///< ObstructionLevel: [0.0f..1.0f]
			AkReal32 in_fOcclusionLevel			///< OcclusionLevel: [0.0f..1.0f]
			);

		//@}
        
        ////////////////////////////////////////////////////////////////////////
		/// @name Capture
		//@{

		/// Start recording the sound engine audio output. 
		/// StartOutputCapture outputs a wav file in the 5.1 configuration with the channels in this order:
		/// <ol>
		///		<li>Front Left</li>
		///		<li>Front Right</li>
		///		<li>Center</li>
		///		<li>LFE</li>
		///		<li>Surround Left</li>
		///		<li>Surround Right</li>
		/// </ol>
		/// \return AK_Success if successful, AK_Fail if there was a problem starting the output capture.
		/// \remark
		///		- The sound engine opens a stream for writing using AK::IAkStreamMgr::CreateStd(). If you are using the
		///			default implementation of the Stream Manager, file opening is executed in your implementation of 
		///			the Low-Level IO interface AK::StreamMgr::IAkFileLocationResolver::Open(). The following 
		///			AkFileSystemFlags are passed: uCompanyID = AKCOMPANYID_AUDIOKINETIC and uCodecID = AKCODECID_PCM,
		///			and the AkOpenMode is AK_OpenModeWriteOvrwr. Refer to \ref streamingmanager_lowlevel_location for
		///			more details on managing the deployement of your Wwise generated data.
		/// \sa 
		/// - AK::SoundEngine::StopOutputCapture()
		/// - AK::StreamMgr::SetFileLocationResolver()
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel_location
		AK_EXTERNAPIFUNC( AKRESULT, StartOutputCapture )( 
			const AkOSChar* in_CaptureFileName				///< Name of the output capture file
			);

		/// Stop recording the sound engine audio output. 
		/// \return AK_Success if successful, AK_Fail if there was a problem stopping the output capture.
		/// \sa 
		/// - AK::SoundEngine::StartOutputCapture()
		AK_EXTERNAPIFUNC( AKRESULT, StopOutputCapture )();

		/// Add text marker in audio output file. 
		/// \return AK_Success if successful, AK_Fail if there was a problem adding the output marker.
		/// \sa 
		/// - AK::SoundEngine::StartOutputCapture()
		AK_EXTERNAPIFUNC( AKRESULT, AddOutputCaptureMarker )(
			const char* in_MarkerText					///< Text of the marker
			);
			
		/// Start recording the sound engine profiling information into a file. This file can be read
		/// by Wwise Authoring.
		/// \remark This function is provided as a utility tool only. It does nothing if it is 
		///			called in the release configuration and returns AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, StartProfilerCapture )( 
			const AkOSChar* in_CaptureFileName				///< Name of the output profiler file (.prof extension recommended)
			);

		/// Stop recording the sound engine profiling information. 
		/// \remark This function is provided as a utility tool only. It does nothing if it is 
		///			called in the release configuration and returns AK_NotCompatible.
		AK_EXTERNAPIFUNC( AKRESULT, StopProfilerCapture )();

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Secondary Outputs
		//@{

		/// Adds a secondary output to the system.  Use this to add controller-attached headphones or speakers.  You can attach multiple devices to the same "player" if needed.		
		/// Secondary output feature is supported only on WiiU (Controller-speaker), PS4 (Controller-speaker & BGM), XboxOne (BGM).  Other platforms will return Ak_NotImplemented.
		/// \sa integrating_secondary_outputs
		/// \return 
		/// - AK_NotImplemented: Feature not supported on this platforms (all platforms except WiiU, PS4 and XBoxOne)
		/// - AK_InvalidParameter: Out of range parameters or unsupported parameter combinations (see parameter list below).
		/// - AK_Success: Parameters are valid.
		AK_EXTERNAPIFUNC( AKRESULT, AddSecondaryOutput )(
			AkUInt32 in_iOutputID,			///< Device identifier, when multiple devices of the same type are possible.
											///< - WiiU Controller-Speakers: 0 to 3 for Remotes, 0 for DRC.
											///< - PS4 Controller-Speakers: UserID as returned from sceUserServiceGetLoginUserIdList
											///< - XBoxOne & PS4 BGM outputs: use 0.
			AkAudioOutputType in_iDeviceType,	///< Device Type, must be one of the currently supported devices types.  See AkAudioOutputType.
											///< - WiiU (Controller-speaker): Use AkSink_DRC or AkSink_Remote
											///< - PS4 (Controller-speaker & BGM): Use AkSink_PAD, AkSink_Personal, AkSink_BGM, AkSink_BGM_NonRecordable
											///< - XboxOne (BGM): Use AkSink_BGM, AkSink_BGM_NonRecordable
			AkUInt32 in_uListenerMask		///< Listener(s) to attach to this device.  Everything heard by these listeners will be sent to this output.  This is a bitmask.  Avoid using listener 0, usually reserved for the main TV output.
			);

		AK_EXTERNAPIFUNC( AKRESULT, RemoveSecondaryOutput )(
			AkUInt32 in_iOutputID,				///< Player number (almost all platforms) or device-unique identifier (UserID on PS4).
			AkAudioOutputType in_iDeviceType	///< Device Type, must be one of the currently supported devices types. 
			);
		
		/// Set the volume of a secondary output device.
		AK_EXTERNAPIFUNC( AKRESULT, SetSecondaryOutputVolume )(
			AkUInt32 in_iOutputID,			///< Player number (almost all platforms) or device-unique identifier (UserID on PS4).			
			AkAudioOutputType in_iDeviceType, ///< Device Type, must be one of the currently supported devices types.  See AkAudioOutputType
			AkReal32 in_fVolume				///< Volume (0.0 = Muted, 1.0 = Volume max)
			);
		//@}

		/// This function should be called to put the sound engine in background mode, where audio isn't processed anymore.  This needs to be called if the console has a background mode or some suspended state.
		/// Call WakeupFromSuspend when your application receives the message from the OS that the process is back in foreground.
		/// When suspended, the sound engine will process API messages (like PostEvent, SetSwitch, etc) only when \ref RenderAudio() is called. 
		/// It is recommended to match the <b>in_bRenderAnyway</b> parameter with the behavior of the rest of your game: 
		/// if your game still runs in background and you must keep some kind of coherent state between the audio engine and game, then allow rendering.
		/// If you want to minimize CPU when in background, then don't allow rendering and never call RenderAudio from the game.
		///
		/// - Android: Call for APP_CMD_PAUSE
		/// - iOS: On iOS only, notify the sound engine that the device is about to be suspended.
		/// - XBoxOne: Use when entering constrained mode or suspended mode.		
		/// \sa \ref WakeupFromSuspend
		AK_EXTERNAPIFUNC( AKRESULT, Suspend )(
			bool in_bRenderAnyway = false /// If set to true, audio processing will still occur, but not outputted.  When set to false, no audio will be processed at all, even upon reception of RenderAudio().
			);

		/// This function should be called to wakeup the sound engine and start processing audio again.  This needs to be called if the console has a background mode or some suspended state.
		/// - Android: Call for APP_CMD_RESUME
		/// - iOS: On iOS only, reinitialize the sound engine components after performing a suspend (lock) and wakeup (unlock) sequence. This API must be called in the application delegate's 
		/// applicationDidBecomeActive: method. See the IntegrationDemo's application delegate for an example. It is designed to handle several interruptions incurred by the suspend and
		/// wakeup sequence: An interrupted sound engine initialization, preventing the CoreAudio audio session from being activated. The interrupted audio session initialization is then 
		/// completed by this API later when the app is brought back to the foreground.
		/// When the audio session category is other than AkAudioSessionCategoryAmbient, this function will check if ananother app's audio is playing. If true, then the host game's background music (routed to a secondary bus) will be muted; otherwise the background music will be reenabled. This means that the mutual exclusion between the host game's background music and another app's audio can only be executed when user perform a lock/unlock cycle of the device or a foreground/background cycle of the app.
		/// When the audio session category is AkAudioSessionCategoryAmbient, the mutual exclusion is managed by the sound engine automatically at any time.
		/// When this function fails, e.g. on certain iOS8 devices, call Suspend() agian with in_bRenderAnyway set to true, so that the sound engine can retry waking up later automatically. However, avoid posting events after calling the remedy Suspend(true) so that no accmulated events burst out instantly after waking up.
		AK_EXTERNAPIFUNC( AKRESULT, WakeupFromSuspend )(); 
	}
}

#endif // _AK_SOUNDENGINE_H_
