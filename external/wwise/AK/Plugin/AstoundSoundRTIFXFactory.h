//////////////////////////////////////////////////////////////////////
//
// © 2013 GenAudio
//
//////////////////////////////////////////////////////////////////////

// AstoundSoundRTIFXFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the GenAudio RTI plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  AstoundSound RTI
/// <br><b>Library file:</b> AstoundsoundRTIFX.lib

#ifndef _ASTOUNDSOUND_RTIFXFACTORY_H_
#define _ASTOUNDSOUND_RTIFXFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_GENAUDIO company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKEFFECTID_GENAUDIORTI = 0;// Legacy RTI out of place effect
const unsigned long AKEFFECTID_GENAUDIORTI_MIXER = 3;
const unsigned long AKEFFECTID_GENAUDIORTI_MIXERATTACHABLE = 4;

typedef enum 
{
  AstoundSoundRTI_CullingType_ByVolume,
  AstoundSoundRTI_CullingType_ByDistance
}AstoundSoundRTI_CullingType;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAstoundSoundRTIFXParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAstoundSoundRTIFX )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Set the number of simultaneous RTI effects that can be simultaneously spawned.
/// Returns true if there's an error.
AK_FUNC( bool, AstoundSoundRTI_UpdateAvailableSlots )(
	AkInt32 in_availableslotsrank0					///< Must be greater than 0.
	);

/// Set the type of algorithm being used for Full3D culling.
/// Returns true if there's an error.
AK_FUNC( void, AstoundSoundRTI_SetFull3DCullingType )(
	AstoundSoundRTI_CullingType in_cullingtype
	);

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAstoundSoundRTIMixerParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Static creation function that returns an instance of the sound engine plug-in to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateAstoundSoundRTIMixer )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);


/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine's plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateAstoundSoundRTIMixerAttachmentParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_GENAUDIO, 
								 AKEFFECTID_GENAUDIORTI,
								 CreateAstoundSoundRTIFX,
								 CreateAstoundSoundRTIFXParams );

AK::SoundEngine::RegisterPlugin( AkPluginTypeMixer, 
								 AKCOMPANYID_GENAUDIO, 
								 AKEFFECTID_GENAUDIORTI_MIXER,
								 CreateAstoundSoundRTIMixer,
								 CreateAstoundSoundRTIMixerParams );

AK::SoundEngine::RegisterPlugin( AkPluginTypeEffect, 
								 AKCOMPANYID_GENAUDIO, 
								 AKEFFECTID_GENAUDIORTI_MIXERATTACHABLE,
								 NULL,
								 CreateAstoundSoundRTIMixerAttachmentParams );
*/

#endif // _ASTOUNDSOUND_RTIFXFACTORY_H_

