/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: v2017.2.1  Build: 6524
  Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/
//////////////////////////////////////////////////////////////////////
//
// AkDefaultIOHookDeferred.cpp
//
// Default deferred low level IO hook (AK::StreamMgr::IAkIOHookDeferred) 
// and file system (AK::StreamMgr::IAkFileLocationResolver) implementation 
// on OS X.
// 
// AK::StreamMgr::IAkFileLocationResolver: 
// Resolves file location using simple path concatenation logic 
// (implemented in ../Common/CAkFileLocationBase). It can be used as a 
// standalone Low-Level IO system, or as part of a multi device system. 
// In the latter case, you should manage multiple devices by implementing 
// AK::StreamMgr::IAkFileLocationResolver elsewhere (you may take a look 
// at class CAkDefaultLowLevelIODispatcher).
//
// AK::StreamMgr::IAkIOHookDeferred: 
// Uses standard C IO functions, into seperate thread for async IO.
// AioFuncRead and AioFuncWrite handle the IO operation asynchronously. 
// The AK::StreamMgr::IAkIOHookDeferred interface is meant to be used with
// AK_SCHEDULER_DEFERRED_LINED_UP streaming devices. 
//
// Init() creates a streaming device (by calling AK::StreamMgr::CreateDevice()).
// AkDeviceSettings::uSchedulerTypeFlags is set inside to AK_SCHEDULER_DEFERRED_LINED_UP.
// If there was no AK::StreamMgr::IAkFileLocationResolver previously registered 
// to the Stream Manager, this object registers itself as the File Location Resolver.
//
//////////////////////////////////////////////////////////////////////

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkDefaultIOHookDeferred.h"
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include "AkFileHelpers.h"


// Device info.
#define POSIX_DEFERRED_DEVICE_NAME		("POSIX Deferred")	// Default deferred device name.

CAkDefaultIOHookDeferred::CAkDefaultIOHookDeferred()
: m_deviceID( AK_INVALID_DEVICE_ID )
, m_bAsyncOpen( false )
{
}

CAkDefaultIOHookDeferred::~CAkDefaultIOHookDeferred()
{
}

// Initialization/termination. Init() registers this object as the one and 
// only File Location Resolver if none were registered before. Then 
// it creates a streaming device with scheduler type AK_SCHEDULER_DEFERRED_LINED_UP.
AKRESULT CAkDefaultIOHookDeferred::Init(
	const AkDeviceSettings &	in_deviceSettings,			// Device settings.
	bool						in_bAsyncOpen/*=true*/,		// If true, files are opened asynchronously when possible.
	AkOSChar * 					in_pszMountPoint/*=NULL*/	// Mount point (optional, can be NULL).
	)
{
	if ( in_deviceSettings.uSchedulerTypeFlags != AK_SCHEDULER_DEFERRED_LINED_UP )
	{
		AKASSERT( !"CAkDefaultIOHookDeferred I/O hook only works with AK_SCHEDULER_DEFERRED_LINED_UP devices" );
		return AK_Fail;
	}
	
	m_bAsyncOpen = in_bAsyncOpen;
	
	// If the Stream Manager's File Location Resolver was not set yet, set this object as the 
	// File Location Resolver (this I/O hook is also able to resolve file location).
	if ( !AK::StreamMgr::GetFileLocationResolver() )
		AK::StreamMgr::SetFileLocationResolver( this );

	// Create a device in the Stream Manager, specifying this as the hook.
	m_deviceID = AK::StreamMgr::CreateDevice( in_deviceSettings, this );
	if ( m_deviceID != AK_INVALID_DEVICE_ID )
	{
		return AK_Success;
	}

	return AK_Fail;
}

void CAkDefaultIOHookDeferred::Term()
{
	CAkMultipleFileLoc::Term();

	if ( AK::StreamMgr::GetFileLocationResolver() == this )
		AK::StreamMgr::SetFileLocationResolver( NULL );
	
	AK::StreamMgr::DestroyDevice( m_deviceID );
}

//
// IAkFileLocationAware implementation.
//-----------------------------------------------------------------------------

// Returns a file descriptor for a given file name (string).
AKRESULT CAkDefaultIOHookDeferred::Open( 
	const AkOSChar* in_pszFileName,     // File name.
	AkOpenMode      in_eOpenMode,       // Open mode.
	AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
	bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	AkFileDesc &    out_fileDesc        // Returned file descriptor.
	)
{
	// We normally consider that calls to ::CreateFile() on a hard drive are fast enough to execute in the
	// client thread. If you want files to be opened asynchronously when it is possible, this device should 
	// be initialized with the flag in_bAsyncOpen set to true.
	out_fileDesc.deviceID = m_deviceID;
	if ( io_bSyncOpen || !m_bAsyncOpen )
	{
		io_bSyncOpen = true;
		return CAkMultipleFileLoc::Open(in_pszFileName, in_eOpenMode, in_pFlags, true, out_fileDesc);
	}

	// The client allows us to perform asynchronous opening.
	// We only need to specify the deviceID, and leave the boolean to false.
	return AK_Success;
}

// Returns a file descriptor for a given file ID.
AKRESULT CAkDefaultIOHookDeferred::Open( 
	AkFileID        in_fileID,          // File ID.
	AkOpenMode      in_eOpenMode,       // Open mode.
	AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
	bool &			io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	AkFileDesc &    out_fileDesc        // Returned file descriptor.
	)
{
	// We normally consider that calls to ::CreateFile() on a hard drive are fast enough to execute in the
	// client thread. If you want files to be opened asynchronously when it is possible, this device should 
	// be initialized with the flag in_bAsyncOpen set to true.
	out_fileDesc.deviceID = m_deviceID;
	if ( io_bSyncOpen || !m_bAsyncOpen )
	{
		io_bSyncOpen = true;
		return CAkMultipleFileLoc::Open(in_fileID, in_eOpenMode, in_pFlags, true, out_fileDesc);
	}

	// The client allows us to perform asynchronous opening.
	// We only need to specify the deviceID, and leave the boolean to false.
	return AK_Success;
}

//
// IAkIOHookDeferred implementation.
//-----------------------------------------------------------------------------
// Async Read
void AioFuncRead(AkAsyncIOTransferInfo& transferInfo)
{
	
	AkFileHandle fileHandle = ((AkFileDesc*)transferInfo.pUserData)->hFile;

	// Lock the file so no other thread tries to do it too
	flockfile(fileHandle);

	// Seek in the file
	if( lseek( fileno(fileHandle), transferInfo.uFilePosition, SEEK_SET ) == transferInfo.uFilePosition )
	{
		// Read the data
		AkUInt32 byteTransferred = (AkUInt32) read( fileno(fileHandle), transferInfo.pBuffer, transferInfo.uRequestedSize );
		
		if( byteTransferred == transferInfo.uRequestedSize )
		{
			transferInfo.pCallback( &transferInfo, AK_Success );
			funlockfile(fileHandle);
			return;
		}
	}
	
	funlockfile(fileHandle);
	transferInfo.pCallback( &transferInfo, AK_Fail );

	return;
}

// Async Write
void AioFuncWrite(AkAsyncIOTransferInfo& transferInfo)
{
	AkFileHandle fileHandle = ((AkFileDesc*)transferInfo.pUserData)->hFile;
	
	// Lock the file so no other thread tries to do it too
	flockfile(fileHandle);
	
	// Seek in the file
	if( lseek( fileno(fileHandle), transferInfo.uFilePosition, SEEK_SET ) == transferInfo.uFilePosition )
	{
		// Read the data
		AkUInt32 byteTransferred = (AkUInt32) write( fileno(fileHandle), transferInfo.pBuffer, transferInfo.uRequestedSize );
		
		if( byteTransferred == transferInfo.uRequestedSize )
		{
			transferInfo.pCallback( &transferInfo, AK_Success );
			fflush(fileHandle);
			funlockfile(fileHandle);
			return;
		}
	}
	
	funlockfile(fileHandle);
	transferInfo.pCallback( &transferInfo, AK_Fail );
	
	return;
}

// Reads data from a file (asynchronous overload).
AKRESULT CAkDefaultIOHookDeferred::Read(
	AkFileDesc &			in_fileDesc,        // File descriptor.
	const AkIoHeuristics & /*in_heuristics*/,	// Heuristics for this data transfer (not used in this implementation).
	AkAsyncIOTransferInfo & io_transferInfo		// Asynchronous data transfer info.
	)
{
	io_transferInfo.pUserData = (void*)&in_fileDesc;
	
	//Call the low level read which takes care of error handling with the callback 
	AioFuncRead(io_transferInfo);

	return AK_Success;
}

// Writes data to a file (asynchronous overload).
AKRESULT CAkDefaultIOHookDeferred::Write(
	AkFileDesc &			in_fileDesc,        // File descriptor.
	const AkIoHeuristics & /*in_heuristics*/,	// Heuristics for this data transfer (not used in this implementation).
	AkAsyncIOTransferInfo & io_transferInfo		// Platform-specific asynchronous IO operation info.
	)
{
	io_transferInfo.pUserData = (void*)&in_fileDesc;
	
	//Call the low level read which takes care of error handling with the callback 
	AioFuncWrite(io_transferInfo);
	
	return AK_Success;
}

// Cancel transfer(s).
void CAkDefaultIOHookDeferred::Cancel(
	AkFileDesc &			/*in_fileDesc*/,	// File descriptor.
	AkAsyncIOTransferInfo & io_transferInfo,	// Transfer info to cancel.
	bool & io_bCancelAllTransfersForThisFile	// Flag indicating whether all transfers should be cancelled for this file (see notes in function description).
	)
{
	// not implemented
}

// Close a file.
AKRESULT CAkDefaultIOHookDeferred::Close(
	AkFileDesc &	in_fileDesc      // File descriptor.
    )
{
	return CAkFileHelpers::CloseFile( in_fileDesc.hFile );
}

// Returns the block size for the file or its storage device. 
AkUInt32 CAkDefaultIOHookDeferred::GetBlockSize(
    AkFileDesc &  /*in_fileDesc*/     // File descriptor.
    )
{
	// There is no limitation nor performance degradation with unaligned
	// seeking on any mount point with asynchronous cell fs API.
    return 1;
}

// Returns a description for the streaming device above this low-level hook.
void CAkDefaultIOHookDeferred::GetDeviceDesc(
    AkDeviceDesc & 
#ifndef AK_OPTIMIZED
	out_deviceDesc      // Description of associated low-level I/O device.
#endif
    )
{
#ifndef AK_OPTIMIZED
	// Deferred scheduler.
	out_deviceDesc.deviceID       = m_deviceID;
	out_deviceDesc.bCanRead       = true;
	out_deviceDesc.bCanWrite      = true;
	AK_CHAR_TO_UTF16( out_deviceDesc.szDeviceName, POSIX_DEFERRED_DEVICE_NAME, AK_MONITOR_DEVICENAME_MAXLENGTH );
	out_deviceDesc.uStringSize   = (AkUInt32)AKPLATFORM::AkUtf16StrLen( out_deviceDesc.szDeviceName ) + 1;
#endif
}

// Returns custom profiling data: 1 if file opens are asynchronous, 0 otherwise.
AkUInt32 CAkDefaultIOHookDeferred::GetDeviceData()
{
	return ( m_bAsyncOpen ) ? 1 : 0;
}

