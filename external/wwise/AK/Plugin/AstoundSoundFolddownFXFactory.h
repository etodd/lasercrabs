//////////////////////////////////////////////////////////////////////
//
// © 2013 GenAudio
//
//////////////////////////////////////////////////////////////////////

// AstoundSoundFolddownFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the GenAudio Folddown plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  AstoundSound Folddown
/// <br><b>Library file:</b> AstoundsoundFolddownFX.lib

#ifndef _ASTOUNDSOUND_FOLDDOWNFXFACTORY_H_
#define _ASTOUNDSOUND_FOLDDOWNFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_GENAUDIO company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_GENAUDIOFOLDDOWN = 2;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAstoundSoundFolddownFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAstoundSoundFolddownFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_GENAUDIO, 
								 AKEFFECTID_GENAUDIOFOLDDOWN,
								 CreateAstoundSoundFolddownFX,
								 CreateAstoundSoundFolddownFXParams );
*/

#endif // _ASTOUNDSOUND_FOLDDOWNFXFACTORY_H_

