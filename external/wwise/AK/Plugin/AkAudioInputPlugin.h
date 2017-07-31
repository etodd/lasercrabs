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

/// \file 
///! Definition of callbacks used for the Audio Input Plugin
/// <br><b>Wwise source name:</b>  AudioInput
/// <br><b>Library file:</b> AkAudioInputSource.lib

#pragma once
#define AKSOURCEID_AUDIOINPUT 200
////////////////////////////////////////////////////////////////////////////////////////////
// API external to the plug-in, to be used by the game.

/// Callback requesting for the AkAudioFormat to use for the plug-in instance.
/// Refer to the Source Input plugin documentation to learn more about the valid formats.
/// \sa \ref soundengine_plugins_source
AK_CALLBACK(void, AkAudioInputPluginGetFormatCallbackFunc)(
	AkPlayingID		in_playingID,   ///< Playing ID (same that was returned from the PostEvent call).
	AkAudioFormat&  io_AudioFormat  ///< Already filled format, modify it if required.
	);

/// Function that returns the Gain to be applied to the Input Plugin.
/// [0..1] range where 1 is maximum volume.
AK_CALLBACK(AkReal32, AkAudioInputPluginGetGainCallbackFunc)(
	AkPlayingID		in_playingID    ///< Playing ID (same that was returned from the PostEvent call).
	);

/// \typedef void( *AkAudioInputPluginExecuteCallbackFunc )( AkPlayingID in_playingID, AkAudioBuffer* io_pBufferOut )
/// Callback requesting for new data for playback.
/// \param in_playingID Playing ID (same that was returned from the PostEvent call)
/// \param io_pBufferOut Buffer to fill
/// \remarks See IntegrationDemo sample for a sample on how to implement it.
AK_CALLBACK(void, AkAudioInputPluginExecuteCallbackFunc)(
	AkPlayingID		in_playingID,
	AkAudioBuffer*	io_pBufferOut
	);

/// This function should be called at the same place the AudioInput plug-in is being registered.
AK_EXTERNAPIFUNC(void, SetAudioInputCallbacks)(
	AkAudioInputPluginExecuteCallbackFunc in_pfnExecCallback,
	AkAudioInputPluginGetFormatCallbackFunc in_pfnGetFormatCallback = NULL, // Optional
	AkAudioInputPluginGetGainCallbackFunc in_pfnGetGainCallback = NULL      // Optional
	);
////////////////////////////////////////////////////////////////////////////////////////////
