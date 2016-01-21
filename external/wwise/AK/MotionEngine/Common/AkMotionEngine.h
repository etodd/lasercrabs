//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

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
	AK_EXTERNAPIFUNC( AKRESULT, AddPlayerMotionDevice )(
		AkUInt8 in_iPlayerID,			///< Player number, must be between 0 and 3.  See platform-specific documentation for more details.
		AkUInt32 in_iCompanyID,			///< Company ID providing support for the device
		AkUInt32 in_iDeviceID,			///< Device ID, must be one of the currently supported devices. 
		void* in_pDevice = NULL			///< PS4: PS4 Device handle, returned by scePadOpen. 
										///< Windows: Windows Direct Input Device reference for DirectInput. NULL to use XInput. 
										///< WiiU: Use AK_MOTION_WIIMOTE_DEVICE to add a Wiimote or AK_MOTION_DRC_DEVICE to add a DRC device. 
										///< XboxOne: Use IGamepad::Id. 
										///> Keep NULL for all other platforms.
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

	/// Registers a motion device for use in the game.  
	/// \sa
	/// - \ref integrating_elements_motion
	AK_EXTERNAPIFUNC( void, RegisterMotionDevice )(
		AkUInt32 in_ulCompanyID,				///< Company ID providing support for the device
		AkUInt32 in_ulPluginID,					///< Device ID, must be one of the currently supported devices. 
		AkCreatePluginCallback in_pCreateFunc	///< Creation function.
		);

	/// Attaches a player to a listener.  This is necessary for the player to receive motion through the connected
	/// devices.  
	/// \sa
	/// - \ref integrating_elements_motion
	/// - \ref soundengine_listeners 
	AK_EXTERNAPIFUNC( void, SetPlayerListener )(
		AkUInt8 in_iPlayerID,					///< Player ID, between 0 and 3
		AkUInt8 in_iListener					///< Listener ID, between 0 and 7
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
