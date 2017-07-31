/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

#define AKMOTIONDEVICEID_RUMBLE 406

/// Audiokinetic namespace
namespace AK
{

namespace MotionEngine
{
	/// Connects a motion device to a player.  Call this function from your game to tell the motion engine that
	/// a player is using the specified device.
	/// \return 
	/// - AK_Success if the initialization was successful
	/// - AK_Fail if the device could not be initialized.  Usually this means the drivers are not installed.
	/// \sa
	/// - \ref integrating_elements_motion
	AK_EXTERNAPIFUNC(AKRESULT, AddPlayerMotionDevice)(
		AkUInt8 in_iPlayerID,			///< Player number, must be between 0 and 3.  See platform-specific documentation for more details.
		AkUInt32 in_iCompanyID,			///< Company ID providing support for the device
		AkUInt32 in_iDeviceID,			///< Device ID, must be one of the currently supported devices. 
		void* in_pDevice = NULL,		///< PS4: PS4 Device handle, returned by scePadOpen. 
										///< Windows: Windows Direct Input Device reference for DirectInput. NULL to use XInput. 
										///< XboxOne: Use IGamepad::Id. 
										///< Motion plugins: see plugin vendor documentation.
										///< Keep NULL for all other device types.										
		AkUInt32 in_uSize = 0			///< Reserved for plugins. Keep to zero unless plugin usage mandates it.
		);

	/// Disconnects a motion device from a player port.  Call this function from your game to tell the motion engine that
	/// a player is not using the specified device anymore.
	/// \sa
	/// - \ref integrating_elements_motion
	AK_EXTERNAPIFUNC( void, RemovePlayerMotionDevice )(
		AkUInt8 in_iPlayerID,			///< Player number, must be between 0 and 3.  See platform-specific documentation for more details.
		AkUInt32 in_iCompanyID,			///< Company ID providing support for the device
		AkUInt32 in_iDeviceID			///< Device ID, must be one of the currently supported devices. 
		);

	/// Attaches a player to a listener.  This is necessary for the player to receive motion through the connected
	/// devices.  
	/// \sa
	/// - \ref integrating_elements_motion
	/// - \ref soundengine_listeners 
	AK_EXTERNAPIFUNC( void, SetPlayerListener )(
		AkUInt8 in_iPlayerID,				///< Player ID, between 0 and 3
		AkGameObjectID in_uListenerID		///< Listener game object ID
		);

	/// Set the master volume for a player.  All devices assigned to this player will be affected by this volume.
	/// \sa
	/// - \ref integrating_elements_motion
	AK_EXTERNAPIFUNC( void, SetPlayerVolume )(
		AkUInt8 in_iPlayerID,					///< Player ID, between 0 and 3
		AkReal32 in_fVolume						///< Master volume for the given player, in decibels (-96.3 to 96.3).
		);
}
}
