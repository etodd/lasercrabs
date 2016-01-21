//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Plug-in unique ID and creation functions necessary to register the Rumble plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Wwise Rumble Plugin
/// <br><b>Library file:</b> AkRumble.lib

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>

/// - This is the plug-in's unique ID (combined with the AKCOMPANYID_AUDIOKINETIC company ID)
/// - This ID must be the same as the plug-in ID in the plug-in's XML definition file, and is persisted in project files. 
/// \akwarning
/// Changing this ID will cause existing projects not to recognize the plug-in anymore.
/// \endakwarning
const unsigned long AKMOTIONDEVICEID_RUMBLE = 406;

/// Static creation function that returns an instance of the sound engine plug-in motion bus for game controllers.
AK_EXTERNFUNC( AK::IAkPlugin*, AkCreateRumblePlugin )( AK::IAkPluginMemAlloc * in_pAllocator );
