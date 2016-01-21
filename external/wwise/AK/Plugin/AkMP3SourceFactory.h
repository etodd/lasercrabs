//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
// AkMP3SourceFactory.h

/// \file 
///! Plug-in unique ID and creation functions (hooks) necessary to register the audio input plug-in to the sound engine.
/// <br><b>Wwise source name:</b>  MP3
/// <br><b>Library file:</b> AkMP3Source.lib

#ifndef _AK_MP3SOURCEFACTORY_H_
#define _AK_MP3SOURCEFACTORY_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

///
/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const AkUInt32 AKSOURCEID_MP3 = 201;

/// Static creation function that returns an instance of the sound engine plug-in parameter node to be hooked by the sound engine plug-in manager.
AK_FUNC( AK::IAkPluginParam *, CreateMP3SourceParams )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/// Plugin mechanism. Source create function and register its address to the plug-in manager.
AK_FUNC( AK::IAkPlugin*, CreateMP3Source )(
	AK::IAkPluginMemAlloc * in_pAllocator			///< Memory allocator interface
	);

/*
Use the following code to register your plug-in

AK::SoundEngine::RegisterPlugin( AkPluginTypeSource, 
								 AKCOMPANYID_AUDIOKINETIC, 
								 AKSOURCEID_MP3,
								 CreateMP3Source,
								 CreateMP3SourceParams );
*/

#endif // _AK_MP3SOURCEFACTORY_H_
