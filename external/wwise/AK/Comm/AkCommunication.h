//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// The main communication interface (between the in-game sound engine and
/// authoring tool).
/// \sa
/// - \ref initialization_comm
/// - \ref termination_comm

#ifndef _AK_COMMUNICATION_H
#define _AK_COMMUNICATION_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#define AK_COMM_SETTINGS_MAX_STRING_SIZE 64

/// Platform-independent initialization settings of communication module between the Wwise sound engine
/// and authoring tool.
/// \sa 
/// - AK::Comm::Init()
struct AkCommSettings
{
	AkCommSettings()
	{
		szAppNetworkName[0] = 0;
	}
	AkUInt32	uPoolSize;		///< Size of the communication pool, in bytes. 
#ifdef AK_3DS
	AkThreadProperties threadProperties; ///< Communication & Connection threading properties (its default priority is AK_THREAD_PRIORITY_ABOVENORMAL)
#endif

	/// Ports used for communication between the Wwise authoring application and your game.
	/// All of these ports are opened in the game when Wwise communication is enabled.
	/// When using HIO type communication, the ports are in fact channels and they must be 3
	/// consecutives channels in the order they are defined in the Port structure.
	///
	/// \sa
	/// - \ref initialization_comm_ports
	/// - AK::Comm::GetDefaultInitSettings()
	/// - AK::Comm::Init()
	struct Ports
	{
		/// Constructor
		Ports()
			: uDiscoveryBroadcast( AK_COMM_DEFAULT_DISCOVERY_PORT )
#if defined( AK_COMM_NO_DYNAMIC_PORTS )
			, uCommand( AK_COMM_DEFAULT_DISCOVERY_PORT + 1 )
			, uNotification( AK_COMM_DEFAULT_DISCOVERY_PORT + 2 )
#else
			, uCommand( 0 )
			, uNotification( 0 )
#endif
		{
		}

		/// This is where the authoring application broadcasts "Game Discovery" requests
		/// to discover games running on the network. Default value: 24024.
		///
		/// \warning Unlike the other ports in this structure, this port cannot be dynamic
		///          (cannot be set to 0). Refer to \ref initialization_comm_ports_discovery_broadcast
		///          for more details.
		AkUInt16 uDiscoveryBroadcast;

		/// Used by the "command" channel.
		/// \remark Set to 0 to request a dynamic/ephemeral port.
		AkUInt16 uCommand;

		/// Used by the "notification" channel.
		/// \remark Set to 0 to request a dynamic/ephemeral port.
		AkUInt16 uNotification;
	};

	/// Ports used for communication between the Wwise authoring application and your game.
	/// \sa
	/// - \ref initialization_comm
	/// - AkCommSettings::Ports
	/// - AK::Comm::Init()
	Ports ports;	

	/// Tells if the base console communication library should be initialized.  
	/// If set to false, the game should load/initialize the console's communication library prior to calling this function.
	/// Set to false only if your game already use sockets before the sound engine initialization.
	/// Some consoles have critical requirements for initialization, see \ref initialization_comm_console_lib
	bool bInitSystemLib;

	/// Optional name that will be displayed over network remote connection of Wwise.
	/// It must be a NULL terminated string.
	char szAppNetworkName[AK_COMM_SETTINGS_MAX_STRING_SIZE];
};

namespace AK
{
	namespace Comm
	{
		///////////////////////////////////////////////////////////////////////
		/// @name Initialization
		//@{

		/// Initializes the communication module. When this is called, and AK::SoundEngine::RenderAudio()
		/// is called periodically, you may use the authoring tool to connect to the sound engine.
		///
		/// \warning This function must be called after the sound engine and memory manager have
		///          been properly initialized.
		///
		///
		/// \remark The AkCommSettings structure should be initialized with
		///         AK::Comm::GetDefaultInitSettings(). You can then change some of the parameters
		///			before calling this function.
		///
		/// \return
		///      - AK_Success if initialization was successful.
		///      - AK_InvalidParameter if one of the settings is invalid.
		///      - AK_InsufficientMemory if the specified pool size is too small for initialization.
		///      - AK_Fail for other errors.
		///		
		/// \sa
		/// - \ref initialization_comm
		/// - AK::Comm::GetDefaultInitSettings()
		/// - AkCommSettings::Ports
        AK_EXTERNAPIFUNC( AKRESULT, Init )(
			const AkCommSettings &	in_settings///< Initialization settings.			
			);

		/// Gets the last error from the OS-specific communication library.
		/// \return The system error code.  Check the code in the platform manufacturer documentation for details about the error.
		AK_EXTERNAPIFUNC(AkInt32, GetLastError());

		/// Gets the communication module's default initialization settings values.
		/// \sa
		/// - \ref initialization_comm 
		/// - AK::Comm::Init()
		AK_EXTERNAPIFUNC( void, GetDefaultInitSettings )(
            AkCommSettings &	out_settings	///< Returned default initialization settings.
		    );
		
		/// Terminates the communication module.
		/// \warning This function must be called before the memory manager is terminated.		
		/// \sa
		/// - \ref termination_comm 
        AK_EXTERNAPIFUNC( void, Term )();

		/// Terminates and reinitialize the communication module using current settings.
		///
		/// \return
		///      - AK_Success if initialization was successful.
		///      - AK_InvalidParameter if one of the settings is invalid.
		///      - AK_InsufficientMemory if the specified pool size is too small for initialization.
		///      - AK_Fail for other errors.
		///
		/// \sa
		/// - \ref AK::SoundEngine::iOS::WakeupFromSuspend()
        AK_EXTERNAPIFUNC( AKRESULT, Reset )();
        

		/// Get the initialization settings currently in use by the CommunicationSystem
		///
		/// \return
		///      - AK_Success if initialization was successful.
		AK_EXTERNAPIFUNC( const AkCommSettings&, GetCurrentSettings )();

		//@}
	}
}

#endif // _AK_COMMUNICATION_H
