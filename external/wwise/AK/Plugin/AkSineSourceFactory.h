//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSineSourceFactory.h

/// \file 
///! Plug-in unique ID and creation functions (hooks) necessary to register the Sine plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  Sine
/// <br><b>Library file:</b> AkSineTone.lib

#ifndef _AK_SINESOURCEFACTORY_H_
#define _AK_SINESOURCEFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files. 
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const AkUInt32 AKSOURCEID_SINE = 100;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateSineSourceParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateSineSource )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_SINE,
								 CreateSineSource,
								 CreateSineSourceParams );
*/

#endif // _AK_SINESOURCEFACTORY_H_
