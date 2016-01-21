//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AuroHeadphoneFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Auro Headphone
/// <br><b>Library file:</b> AuroHeadphoneFX.lib

#ifndef _AHPFXFACTORY_H_
#define _AHPFXFACTORY_H_
#define HAS_AKFACTORY_HEADER

#include <AK/SoundEngine/Common/IAkPlugin.h>

const unsigned long AKEFFECTID_AUROHEADPHONE = 1100;


AK_FUNC( AK::IAkPluginParam *, CreateAuroHeadphoneFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAuroHeadphoneFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUROTECHNOLOGIES, 
								 AKEFFECTID_AUROHEADPHONE,
								 CreateAuroHeadphoneFX,
								 CreateAuroHeadphoneFXParams );
*/

#endif //_AHPFXFACTORY_H_
