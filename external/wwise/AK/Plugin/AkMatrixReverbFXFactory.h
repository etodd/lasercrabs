//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkMatrixReverbFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the matrix reverb plug-in to the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise Matrix Reverb
/// <br><b>Library file:</b> AkMatrixReverbFX.lib

#ifndef _AK_MATRIXREVERBFXFACTORY_H_
#define _AK_MATRIXREVERBFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files.
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const unsigned long AKEFFECTID_MATRIXREVERB = 115;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateMatrixReverbFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateMatrixReverbFX )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// Delay length mode
enum AkDelayLengthsMode
{
	AKDELAYLENGTHSMODE_DEFAULT	=  0,	///< Default settings
	AKDELAYLENGTHSMODE_CUSTOM	=  1	///< Custom settings
};

/// Delay times used when in default mode
static const float g_fDefaultDelayLengths[16] = { 13.62f, 15.66f, 17.52f, 19.02f, 20.83f, 22.60f, 24.05f, 24.78f, 25.60f, 26.09f, 26.55f, 26.91f, 28.04f, 29.09f, 29.90f, 30.86f };

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKEFFECTID_MATRIXREVERB,
								 CreateMatrixReverbFX,
								 CreateMatrixReverbFXParams );
*/

#endif // _AK_MATRIXREVERBFXFACTORY_H_

