//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSoundSeedWooshFactory.h

/// \file 
///! Plug-in unique ID and creation functions (hooks) necessary to register the SoundSeed Woosh plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  SoundSeed Woosh
/// <br><b>Library file:</b> AkSoundSeedWoosh.lib

#ifndef _AK_SOUNDSEEDWOOSHFACTORY_H_
#define _AK_SOUNDSEEDWOOSHFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files. 
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const AkUInt32 AKSOURCEID_SOUNDSEEDWOOSH = 120;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateSoundSeedWooshParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateSoundSeedWoosh )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Information shared between UI and SoundEngine plug-ins. Parameter ID for change notifications
static const AkPluginParamID AKSSWOOSH_DEFLECTORS_DATA_ID		= 200;	///< Number of deflectors changed
static const AkPluginParamID AKSSWOOSH_DEFLECTORS_PROPERTIES_ID	= 250;	///< Only deflector properties changed
static const AkPluginParamID AKSSWOOSH_CURVES_DATA_ID			= 300;	///< Curves changed
static const AkPluginParamID AKSSWOOSH_PATH_DATA_ID				= 400;	///< Path changed


/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_SOUNDSEEDWOOSH,
								 CreateSoundSeedWoosh,
								 CreateSoundSeedWooshParams );
*/

#endif // _AK_SOUNDSEEDWOOSHFACTORY_H_
