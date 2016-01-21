//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AuroPannerMixerFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Auro Panner
/// <br><b>Library file:</b> AuroPanner.lib

#ifndef _AUROPANNERMIXERFACTORY_H_
#define _AUROPANNERMIXERFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
//const unsigned long AKCOMPANYID_AUROTECHNOLOGIES = 263;
const unsigned long AKEFFECTID_AUROPANNER = 1101;
const unsigned long AKEFFECTID_AUROPANNER_ATTACHMENT = 1102;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAuroPannerMixerParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAuroPannerMixer )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAuroPannerMixerAttachmentParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeMixer, 
								 AKCOMPANYID_AUROTECHNOLOGIES, 
								 AKEFFECTID_AUROPANNER,
								 CreateAuroPannerMixer,
								 CreateAuroPannerMixerParams );

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUROTECHNOLOGIES,
								 AKEFFECTID_AUROPANNER_ATTACHMENT, 
								 NULL,
								 CreateAuroPannerMixerAttachmentParams );
*/

#endif // _AUROPANNERMIXERFACTORY_H_

