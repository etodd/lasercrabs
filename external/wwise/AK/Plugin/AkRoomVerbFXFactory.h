//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkRoomVerbFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the RoomVerb reverb plug-in to the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise RoomVerb
/// <br><b>Library file:</b> AkRoomVerbFX.lib

#ifndef _AK_ROOMVERBFXFACTORY_H_
#define _AK_ROOMVERBFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files.
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const unsigned long AKEFFECTID_ROOMVERB = 118;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateRoomVerbFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateRoomVerbFX )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/*
Use the following code to register your plug-in


AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKEFFECTID_ROOMVERB,
								 CreateRoomVerbFX,
								 CreateRoomVerbFXParams );
*/

#endif // _AK_ROOMVERBFXFACTORY_H_

