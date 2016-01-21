//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSoundSeedWindFactory.h

/// \file 
///! Plug-in unique ID and creation functions (hooks) necessary to register the SoundSeed Wind plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  SoundSeed Wind
/// <br><b>Library file:</b> AkSoundSeedWind.lib

#ifndef _AK_SOUNDSEEDWINDFACTORY_H_
#define _AK_SOUNDSEEDWINDFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files. 
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const AkUInt32 AKSOURCEID_SOUNDSEEDWIND = 119;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateSoundSeedWindParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateSoundSeedWind )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Information shared between UI and SoundEngine plug-ins. Parameter ID for change notifications
static const AkPluginParamID AKSSWIND_DEFLECTORS_DATA_ID		= 200;	///< Number of deflectors changed
static const AkPluginParamID AKSSWIND_DEFLECTORS_POSITIONS_ID	= 225;	///< Only deflector positions changed
static const AkPluginParamID AKSSWIND_DEFLECTORS_PROPERTIES_ID	= 250;	///< Only deflector properties changed
static const AkPluginParamID AKSSWIND_DEFLECTORS_MAXDISTANCE_ID	= 275;	///< Only max distance changed
static const AkPluginParamID AKSSWIND_CURVES_DATA_ID			= 300;	///< Curve properties changed

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_SOUNDSEEDWIND,
								 CreateSoundSeedWind,
								 CreateSoundSeedWindParams );
*/

#endif // _AK_SOUNDSEEDWINDFACTORY_H_
