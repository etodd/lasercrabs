//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009 McDSP, All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// McDSPFutzBoxFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the McDSP FutzBox plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  McDSP FutzBox
/// <br><b>Library file:</b> McDSPFutzBoxFX.lib

#ifndef _MCDSP_FUTZBOXFXFACTORY_H_
#define _MCDSP_FUTZBOXFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_MCDSP company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_MCDSPFUTZBOX = 110;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateMcDSPFutzBoxFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateMcDSPFutzBoxFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_MCDSP, 
								 AKEFFECTID_MCDSPFUTZBOX,
								 CreateMcDSPFutzBoxFX,
								 CreateMcDSPFutzBoxFXParams );
*/

#endif // _MCDSP_FUTZBOXFXFACTORY_H_

