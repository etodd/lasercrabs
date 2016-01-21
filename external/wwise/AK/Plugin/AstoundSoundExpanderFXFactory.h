//////////////////////////////////////////////////////////////////////
//
// © 2013 GenAudio
//
//////////////////////////////////////////////////////////////////////

// AstoundSoundExpanderFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the GenAudio Expander plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  AstoundSound Expander
/// <br><b>Library file:</b> AstoundsoundExpanderFX.lib

#ifndef _ASTOUNDSOUND_EXPANDERFXFACTORY_H_
#define _ASTOUNDSOUND_EXPANDERFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_GENAUDIO company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_GENAUDIOEXPANDER = 1;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAstoundSoundExpanderFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAstoundSoundExpanderFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_GENAUDIO, 
								 AKEFFECTID_GENAUDIOEXPANDER,
								 CreateAstoundSoundExpanderFX,
								 CreateAstoundSoundExpanderFXParams );
*/

#endif // _ASTOUNDSOUND_EXPANDERFXFACTORY_H_

