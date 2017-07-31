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

// AkWinSoundEngine.h

/// \file 
/// Main Sound Engine interface, specific WIN32.

#ifndef _AK_WIN_SOUND_ENGINE_H_
#define _AK_WIN_SOUND_ENGINE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

///< API used for audio output
///< Use with AkPlatformInitSettings to select the API used for audio output.  
///< Use AkAPI_Default, it will select the more appropriate API depending on the computer's capabilities.  Other values should be used for testing purposes.
///< \sa AK::SoundEngine::Init
enum AkAudioAPI
{
	AkAPI_Wasapi = 1 << 0,								///< Use Wasapi 
	AkAPI_XAudio2 = 1 << 1,								///< Use XAudio2 (this is the preferred API on Windows)
	AkAPI_DirectSound = 1 << 2,							///< Use DirectSound
	AkAPI_Default = AkAPI_Wasapi | AkAPI_XAudio2 | AkAPI_DirectSound,	///< Default value, will select the more appropriate API (XAudio2 is the default)	
};

///< Used with \ref AK::GetWindowsDeviceName to specify the device state mask.
enum AkAudioDeviceState
{
	AkDeviceState_Active = 1 << 0,	   ///< The audio device is active That is, the audio adapter that connects to the endpont device is present and enabled.
	AkDeviceState_Disabled = 1 << 1,   ///< The audio device is disabled.
	AkDeviceState_NotPresent = 1 << 2, ///< The audio device is not present because the audio adapter that connects to the endpoint device has been removed from the system.
	AkDeviceState_Unplugged = 1 << 3,  ///< The audio device is unplugged.
	AkDeviceState_All = AkDeviceState_Active | AkDeviceState_Disabled | AkDeviceState_NotPresent | AkDeviceState_Unplugged, ///< Includes audio devices in all states.
};

///< Used with \ref AK::SoundEngine::AddSecondaryOutput to specify the type of secondary output.
enum AkAudioOutputType
{
	AkOutput_None = 0,		///< Used for uninitialized type, do not use.
	AkOutput_Dummy,			///< Dummy output, simply eats the audio stream and outputs nothing.
	AkOutput_Main,			///< Main output.  This cannot be used with AddSecondaryOutput, but can be used to query information about the main output (GetSpeakerConfiguration for example).	
	AkOutput_Secondary,		///< Adds an output linked to the hardware device specified  (See AddSecondaryOutput).
	AkOutput_NumBuiltInOutputs,		///< Do not use.
	AkOutput_Plugin			///< Specify if using Audio Device Plugin Sink.
};

struct IXAudio2;

/// Platform specific initialization settings
/// \sa AK::SoundEngine::Init
/// \sa AK::SoundEngine::GetDefaultPlatformInitSettings
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold

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

	AkUInt32			uSampleRate;			///< Sampling Rate. Default is 48000 Hz. Use 24000hz for low quality. Any positive reasonable sample rate is supported. However be careful setting a custom value. Using an odd or really low sample rate may result in malfunctionning sound engine.


	AkAudioAPI			eAudioAPI;				///< Main audio API to use. Leave to AkAPI_Default for the default sink (default value).
												///< If a valid audioDeviceShareset plug-in is provided, the AkAudioAPI will be Ignored.
												///< \ref AkAudioAPI
	
	bool				bGlobalFocus;			///< Corresponding to DSBCAPS_GLOBALFOCUS. If using the AkAPI_DirectSound AkAudioAPI type, sounds will be muted if set to false when the game loses the focus.
												///< This setting is ignored when using other AkAudioAPI types.

	IXAudio2*			pXAudio2;				///< XAudio2 instance to use for the Wwise sound engine.  If NULL (default) Wwise will initialize its own instance.  Used only if the sink type is XAudio2 in AkInitSettings.outputType.

	AkUInt32			idAudioDevice;			///< Device ID to use for the main audio output, as returned from AK::GetDeviceID or AK::GetDeviceIDFromName.  
												 												
};

struct IDirectSound8;
struct IXAudio2;
struct IMMDevice;
struct IUnknown;

namespace AK
{
	/// Get instance of XAudio2 created by the sound engine at initialization.
	/// \return Non-addref'd pointer to XAudio2 interface. NULL if sound engine is not initialized or XAudio2 is not used.
	/// The returned pointer can be of either XAudio 2.7, XAudio 2.8, Xaudio 2.9 depending on the Windows version the game is running on.  Use QueryInterface to identify which one and cast appropriately
	AK_EXTERNAPIFUNC( IUnknown *, GetWwiseXAudio2Interface)();

	/// Get instance of DirectSound created by the sound engine at initialization.
	/// \return Non-addref'd pointer to DirectSound interface. NULL if sound engine is not initialized or DirectSound is not used.
	AK_EXTERNAPIFUNC( IDirectSound8 *, GetDirectSoundInstance )();

	/// Finds the device ID for particular Audio Endpoint. 
	/// \note CoInitialize must have been called for the calling thread.  See Microsoft's documentation about CoInitialize for more details.
	/// \return A device ID to use with AddSecondaryOutput or AkPlatformInitSettings.idAudioDevice
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceID ) (IMMDevice* in_pDevice);

	/// Finds an audio endpoint that matches the token in the device name or device ID and returns and ID compatible with AddSecondaryOutput.  
	/// This is a helper function that searches in the device ID (as returned by IMMDevice->GetId) and 
	/// in the property PKEY_Device_FriendlyName.  The token parameter is case-sensitive.  If you need to do matching on different conditions, use IMMDeviceEnumerator directly and AK::GetDeviceID.
	/// \note CoInitialize must have been called for the calling thread.  See Microsoft's documentation about CoInitialize for more details.
	/// \return An ID to use with AddSecondaryOutput.  The ID returned is the device ID as returned by IMMDevice->GetId, hashed by AK::SoundEngine::GetIDFromName()
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceIDFromName )(wchar_t* in_szToken);

	/// Get the user-friendly name for the specified device.  Call repeatedly with index starting at 0 and increasing to get all available devices, including disabled and unplugged devices, until the returned string is null.
	/// You can also get the default device information by specifying index=-1.  The default device is the one with a green checkmark in the Audio Playback Device panel in Windows.
	/// The returned out_uDeviceID parameter is the Device ID to use with Wwise.  Use it to specify the main device in AkPlatformInitSettings.idAudioDevice or in AK::SoundEngine::AddSecondaryOutput. 
	/// \note CoInitialize must have been called for the calling thread.  See Microsoft's documentation about CoInitialize for more details.
	/// \return The name of the device at the "index" specified.  The pointer is valid until the next call to GetWindowsDeviceName.
	AK_EXTERNAPIFUNC(const wchar_t*, GetWindowsDeviceName) (
		AkInt32 index,			 ///< Index of the device in the array.  -1 to get information on the default device.
		AkUInt32 &out_uDeviceID, ///< Device ID for Wwise.  This is the same as what is returned from AK::GetDeviceID and AK::GetDeviceIDFromName.  Use it to specify the main device in AkPlatformInitSettings.idAudioDevice or in AK::SoundEngine::AddSecondaryOutput. 
		AkAudioDeviceState uDeviceStateMask = AkDeviceState_All ///< Optional bitmask used to filter the device based on their state.
		);
};

#endif //_AK_WIN_SOUND_ENGINE_H_
