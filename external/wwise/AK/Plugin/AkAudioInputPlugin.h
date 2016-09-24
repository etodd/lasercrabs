//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
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
