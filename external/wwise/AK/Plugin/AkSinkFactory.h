//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSinkFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Sink
/// <br><b>Library file:</b> AkSink.lib

#ifndef _AK_SINK_FACTORY_H_
#define _AK_SINK_FACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_SINK = 152;

/// Static creation function that returns an instance of the sound engine plug-in.
AK_FUNC( AK::IAkPlugin*, CreateSink )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/* Pass CreateSceAudio3dSink to AkInitSettings::settingsMainOutput::pfSinkPluginFactory when initializing the sound engine.

AkInitSettings settings;
AK::SoundEngine::GetDefaultInitSettings( settings );
settings.settingsMainOutput.pfSinkPluginFactory = CreateSink;
AK::SoundEngine::Init( &settings, &platformSettings );

*/

#endif // _AK_SINK_FACTORY_H_

