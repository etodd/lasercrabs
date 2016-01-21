// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
// AkSilenceSourceFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the Silence plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  Wwise Silence
/// <br><b>Library file:</b> AkSilenceGen.lib

#ifndef _AK_SILENCESOURCEFACTORY_H_
#define _AK_SILENCESOURCEFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is PERSISTED in project files. 
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const AkUInt32 AKSOURCEID_SILENCE = 101;

/// - This is the Plug-in ID for the silence for motion FX (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is PERSISTED in project files. 
/// \aknote If the same class is used to generate motion data, it needs a separate ID
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const AkUInt32 AKSOURCEID_MOTIONSILENCE = 404;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateSilenceSourceParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateSilenceSource )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_SILENCE,
								 CreateSilenceSource,
								 CreateSilenceSourceParams );
*/

#endif // _AK_SILENCESOURCEFACTORY_H_

