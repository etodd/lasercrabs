//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkRecorderFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the Recorder reverb plug-in to the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise Recorder
/// <br><b>Library file:</b> AkRecorderFX.lib

#ifndef _AK_RECORDERFXFACTORY_H_
#define _AK_RECORDERFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files.
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.
const unsigned long AKEFFECTID_RECORDER = 132;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateRecorderFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateRecorderFX )(
	AK::IAkPluginMemAlloc * in_pAllocator		///< Memory allocator interface.
	);

/// AkRecorderFX plug-in settings structure.
struct AkRecorderSettings
{
	AkUInt32 uIOMemorySize;		///< Size of I/O memory pool shared between instances of AkRecorderFX
	AkUInt32 uIOGranularity;	///< I/O granularity
	AkPriority ePriority;		///< I/O priority
};

/// Get the default settings for the AkRecorderFX plug-in.
AK_EXTERNFUNC( void, GetAkRecorderDefaultSettings )(
	AkRecorderSettings & out_settings		///< Returned default settings
	);

/// Set the AkRecorderFX plug-in settings.
/// \aknote Do not call this function during plug-in execution.
AK_EXTERNFUNC( void, SetAkRecorderSettings )(
	const AkRecorderSettings & in_settings	///< New settings to apply
	);

/*
Use the following code to register your plug-in:

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKEFFECTID_RECORDER,
								 CreateRecorderFX,
								 CreateRecorderFXParams );
*/

#endif // _AK_RECORDERFXFACTORY_H_

