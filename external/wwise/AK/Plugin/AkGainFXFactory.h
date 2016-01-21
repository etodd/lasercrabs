//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkGainFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the parametric equalizer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise Gain
/// <br><b>Library file:</b> AkGainFX.lib

#ifndef _AK_GAINFXFACTORY_H_
#define _AK_GAINFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_GAIN = 139;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateGainFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateGainFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKEFFECTID_GAIN,
								 CreateGainFX,
								 CreateGainFXParams );
*/

#endif // _AK_GAINFXFACTORY_H_

