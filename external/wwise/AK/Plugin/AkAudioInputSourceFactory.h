//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkAudioInputSourceFactory.h

/// \file 
///! Plug-in unique ID and creation functions (hooks) necessary to register the audio input plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  AudioInput
/// <br><b>Library file:</b> AkAudioInputSource.lib

#ifndef _AK_AUDIOINPUTSOURCEFACTORY_H_
#define _AK_AUDIOINPUTSOURCEFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const AkUInt32 AKSOURCEID_AUDIOINPUT = 200;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_EXTERNAPIFUNC( AK::IAkPluginParam *, CreateAudioInputSourceParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_EXTERNAPIFUNC( AK::IAkPlugin*, CreateAudioInputSource )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

////////////////////////////////////////////////////////////////////////////////////////////
// API external to the plug-in, to be used by the game.

/// Callback requesting for the AkAudioFormat to use for the plug-in instance.
/// Refer to the Source Input plugin documentation to learn more about the valid formats.
/// \sa \ref soundengine_plugins_source
AK_CALLBACK( void, AkAudioInputPluginGetFormatCallbackFunc )(
    AkPlayingID		in_playingID,   ///< Playing ID (same that was returned from the PostEvent call).
    AkAudioFormat&  io_AudioFormat  ///< Already filled format, modify it if required.
    );

/// Function that returns the Gain to be applied to the Input Plugin.
/// [0..1] range where 1 is maximum volume.
AK_CALLBACK( AkReal32, AkAudioInputPluginGetGainCallbackFunc )(
    AkPlayingID		in_playingID    ///< Playing ID (same that was returned from the PostEvent call).
    );

/// \typedef void( *AkAudioInputPluginExecuteCallbackFunc )( AkPlayingID in_playingID, AkAudioBuffer* io_pBufferOut )
/// Callback requesting for new data for playback.
/// \param in_playingID Playing ID (same that was returned from the PostEvent call)
/// \param io_pBufferOut Buffer to fill
/// \remarks See IntegrationDemo sample for a sample on how to implement it.
AK_CALLBACK( void, AkAudioInputPluginExecuteCallbackFunc )(
    AkPlayingID		in_playingID,
    AkAudioBuffer*	io_pBufferOut
    );

/// This function should be called at the same place the AudioInput plug-in is being registered.
AK_EXTERNAPIFUNC( void, SetAudioInputCallbacks )(
                AkAudioInputPluginExecuteCallbackFunc in_pfnExecCallback, 
                AkAudioInputPluginGetFormatCallbackFunc in_pfnGetFormatCallback = NULL, // Optional
                AkAudioInputPluginGetGainCallbackFunc in_pfnGetGainCallback = NULL      // Optional
                );
////////////////////////////////////////////////////////////////////////////////////////////

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_AUDIOINPUT,
								 CreateAudioInputSource,
								 CreateAudioInputSourceParams );
*/

#endif // _AK_AUDIOINPUTSOURCEFACTORY_H_
