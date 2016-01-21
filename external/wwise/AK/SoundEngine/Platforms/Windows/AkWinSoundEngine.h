//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkWinSoundEngine.h

/// \file 
/// Main Sound Engine interface, specific WIN32.

#ifndef _AK_WIN_SOUND_ENGINE_H_
#define _AK_WIN_SOUND_ENGINE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

/// Sound quality option
/// (available in the PC version only)
enum AkSoundQuality
{
	AkSoundQuality_High,	///< High quality (48 kHz sampling rate, default value)
	AkSoundQuality_Low,		///< Reduced quality (24 kHz sampling rate)
};

///< API used for audio output
///< Use with AkInitSettings to select the API used for audio output.  
///< Use AkAPI_Default, it will select the more appropriate API depending on the computer's capabilities.  Other values should be used for testing purposes.
///< \sa AK::SoundEngine::Init
enum AkAudioAPI
{
	AkAPI_XAudio2 = 1 << 0,								///< Use XAudio2 (this is the preferred API on Windows)
	AkAPI_DirectSound = 1 << 1,							///< Use DirectSound
	AkAPI_Wasapi = 1 << 2,
	AkAPI_Default = AkAPI_Wasapi | AkAPI_XAudio2 | AkAPI_DirectSound,	///< Default value, will select the more appropriate API.	
	AkAPI_Dummy = 1 << 3,								///< Dummy output, outputs nothing.
};

///< Used with \ref AK::SoundEngine::AddSecondaryOutput to specify the type of secondary output.
enum AkAudioOutputType
{
	AkOutput_Dummy = 1 << 3,		///< Dummy output, simply eats the audio stream and outputs nothing.
	AkOutput_MergeToMain = 1 << 4,	///< This output will mix back its content to the main output, after the master mix.
	AkOutput_Main = 1 << 5,			///< Main output.  This cannot be used with AddSecondaryOutput, but can be used to query information about the main output (GetSpeakerConfiguration for example).	
	AkOutput_Secondary = 1 << 6,	///< Adds an output linked to the hardware device specified  (See AddSecondaryOutput).  Supported ONLY with a WASAPI AkAudioAPI (see above).
	AkOutput_NumOutputs = 1 << 7,	///< Do not use.
};

/// Platform specific initialization settings
/// \sa AK::SoundEngine::Init
/// \sa AK::SoundEngine::GetDefaultPlatformInitSettings
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold

struct IXAudio2;
struct AkPlatformInitSettings
{
    // Direct sound.
    HWND			    hWnd;					///< Handle to the window associated to the audio.
												///< Each game must specify the HWND that will be passed to DirectSound initialization.
												///< The value returned by GetDefaultPlatformInitSettings is the foreground HWND at 
												///< the moment of the initialization of the sound engine and may not be the correct one for your game.
												///< It is required that each game provides the correct HWND to be used.
									

    // Threading model.
    AkThreadProperties  threadLEngine;			///< Lower engine threading properties
	AkThreadProperties  threadBankManager;		///< Bank manager threading properties (its default priority is AK_THREAD_PRIORITY_NORMAL)
	AkThreadProperties  threadMonitor;			///< Monitor threading properties (its default priority is AK_THREAD_PRIORITY_ABOVENORMAL). This parameter is not used in Release build.

    // Memory.
    AkUInt32            uLEngineDefaultPoolSize;///< Lower Engine default memory pool size
	AkReal32            fLEngineDefaultPoolRatioThreshold;	///< 0.0f to 1.0f value: The percentage of occupied memory where the sound engine should enter in Low memory mode. \ref soundengine_initialization_advanced_soundengine_using_memory_threshold

	// Voices.
	AkUInt16            uNumRefillsInVoice;		///< Number of refill buffers in voice buffer. 2 == double-buffered, defaults to 4.
	AkSoundQuality		eAudioQuality;			///< Quality of audio processing, default = AkSoundQuality_High.
	
	bool				bGlobalFocus;			///< Corresponding to DSBCAPS_GLOBALFOCUS. If using the DirectSound sink type, sounds will be muted if set to false when the game loses the focus.
												///< This setting is ignored when using other sink types.

	IXAudio2*			pXAudio2;				///< XAudio2 instance to use for the Wwise sound engine.  If NULL (default) Wwise will initialize its own instance.  Used only if the sink type is XAudio2 in AkInitSettings.eSinkType.
};

struct IDirectSound8;
struct IXAudio2;
struct IMMDevice;

namespace AK
{
	/// Get instance of XAudio2 created by the sound engine at initialization.
	/// \return Non-addref'd pointer to XAudio2 interface. NULL if sound engine is not initialized or XAudio2 is not used.
	AK_EXTERNAPIFUNC( IXAudio2 *, GetWwiseXAudio2Interface )();

	/// Get instance of DirectSound created by the sound engine at initialization.
	/// \return Non-addref'd pointer to DirectSound interface. NULL if sound engine is not initialized or DirectSound is not used.
	AK_EXTERNAPIFUNC( IDirectSound8 *, GetDirectSoundInstance )();

	/// Finds the device ID for particular Audio Endpoint.  
	/// \return A device ID to use with AddSecondaryOutput
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceID ) (IMMDevice* in_pDevice);

	/// Finds an audio endpoint that matches the token in the device name or device ID and returns and ID compatible with AddSecondaryOutput.  
	/// This is a helper function that searches in the device ID (as returned by IMMDevice->GetId) and 
	/// in the property PKEY_Device_FriendlyName.  If you need to do matching on different conditions, use IMMDeviceEnumerator directly.
	/// \return An ID to use with AddSecondaryOutput.  The ID returned is the device ID as returned by IMMDevice->GetId, hashed by AK::SoundEngine::GetIDFromName()
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceIDFromName )(wchar_t* in_szToken);
};

#endif //_AK_WIN_SOUND_ENGINE_H_
