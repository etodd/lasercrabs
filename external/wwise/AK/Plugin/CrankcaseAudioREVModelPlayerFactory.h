#ifndef _AK_REVMODELPLAYERSOURCEFACTORY_H_
#define _AK_REVMODELPLAYERSOURCEFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

// Crankcase REV Model Player
const unsigned long AKSOURCEID_REV_MODEL_PLAYER = 416;
const unsigned long AKSOURCEID_REV_MODEL_ENGINECONTROL = 417;
const unsigned long AKSOURCEID_REV_MODEL_MODELCONTROL = 418;

/// - This is the Plug-in unique ID (when combined with Company ID AKCOMPANYID_AUDIOKINETIC)
/// - This ID must be the same as the PluginID in the Plug-in's XML definition file, and is persisted in project files.
/// \aknote Don't change the ID or existing projects will not recognize this plug-in anymore.

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateREVModelPlayerSourceParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateREVModelPlayerSource )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource,
								 AKCOMPANYID_CRANKCASEAUDIO,
								 AKSOURCEID_REV_MODEL_PLAYER,
								 CreateREVModelPlayerSource,
								 CreateREVModelPlayerSourceParams );
*/

#endif
