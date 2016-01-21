//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkFlangerFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the Flanger plug-in to the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise Flanger
/// <br><b>Library file:</b> AkFlangerFX.lib

#ifndef _AK_FLANGERFXFACTORY_H_
#define _AK_FLANGERFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files.
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const unsigned long AKEFFECTID_FLANGER = 125;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateFlangerFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateFlangerFX )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/*
Use the following code to register your plug-in


AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKEFFECTID_FLANGER,
								 CreateFlangerFX,
								 CreateFlangerFXParams );
*/

#endif // _AK_FLANGERFXFACTORY_H_

