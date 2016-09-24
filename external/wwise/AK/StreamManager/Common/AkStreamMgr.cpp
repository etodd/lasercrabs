//////////////////////////////////////////////////////////////////////
//
// AkStreamMgr.cpp
//
// Stream manager Windows-specific implementation:
// Device factory.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkStreamMgr.h"
#include <AK/Tools/Common/AkMonitorError.h>
#include "AkStreamingDefaults.h"

// Factory products.
#include "AkDeviceBlocking.h"
#include "AkDeviceDeferredLinedUp.h"
#ifdef AK_SUPPORT_WCHAR
#include <wchar.h>
#endif //AK_SUPPORT_WCHAR
#include <stdio.h>

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Declaration of the one and only global pointer to the stream manager.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AKSOUNDENGINE_API AK::IAkStreamMgr * AK::IAkStreamMgr::m_pStreamMgr = NULL;

//-----------------------------------------------------------------------
// Global variables.
//-----------------------------------------------------------------------
AK::StreamMgr::IAkFileLocationResolver * AK::StreamMgr::CAkStreamMgr::m_pFileLocationResolver = NULL;
AK::StreamMgr::CAkStreamMgr::AkDeviceArray AK::StreamMgr::CAkStreamMgr::m_arDevices;
AkMemPoolId AK::StreamMgr::CAkStreamMgr::m_streamMgrPoolId = AK_INVALID_POOL_ID;
#ifndef AK_OPTIMIZED
	AkInt32 AK::StreamMgr::CAkStreamMgr::m_iNextStreamID = 0;
#endif
AK::StreamMgr::CAkStreamMgr::ArrayLangChgObservers	AK::StreamMgr::CAkStreamMgr::m_arLangChgObserver;

//
// Language management.
// ---------------------------------------------
namespace AK
{
	namespace StreamMgr
	{
		// The one and only language static buffer.
		static AkOSChar m_szCurrentLanguage[AK_MAX_LANGUAGE_NAME_SIZE];
	}
}
// ---------------------------------------------

//
// Helpers.
// ---------------------------------------------
namespace AK
{
	namespace StreamMgr
	{
		inline void MonitorFileOpenError(AKRESULT in_eRes, AkFileID in_fileID)
		{
#ifndef AK_OPTIMIZED
			// Monitor error.
			AkOSChar szMsg[64];
			if (AK_Success == in_eRes)
			{
				AKASSERT(!"Invalid file size: fileDesc.iFileSize <= 0");
				OS_PRINTF(szMsg, AKTEXT("Invalid file size: %u"), in_fileID);
			}
			else if (AK_FileNotFound == in_eRes)
			{
				OS_PRINTF(szMsg, AKTEXT("File not found: %u"), in_fileID);
			}
			else
			{
				OS_PRINTF(szMsg, AKTEXT("Cannot open file: %u"), in_fileID);
			}
			AK::Monitor::PostString(szMsg, AK::Monitor::ErrorLevel_Error);
#endif
		}

		inline void MonitorFileOpenError(AKRESULT in_eRes, const AkOSChar * in_pszFileName)
		{
#ifndef AK_OPTIMIZED
			// Monitor error.
			AkOSChar szMsg[AK_MAX_PATH + 64];
			if (AK_Success == in_eRes)
			{
				AKASSERT(!"Invalid file size: fileDesc.iFileSize <= 0");	// Low-level IO implementation error.
				OS_PRINTF(szMsg, AKTEXT("Invalid file size: %s"), in_pszFileName);
			}
			else if (AK_FileNotFound == in_eRes)
			{
				OS_PRINTF(szMsg, AKTEXT("File not found: %s"), in_pszFileName);
			}
			else
			{
				OS_PRINTF(szMsg, AKTEXT("Cannot open file: %s"), in_pszFileName);
			}
			AK::Monitor::PostString(szMsg, AK::Monitor::ErrorLevel_Error);
#endif
		}
	}
}
// ---------------------------------------------

//-----------------------------------------------------------------------------
// Factory.
//-----------------------------------------------------------------------------
AK::IAkStreamMgr * AK::StreamMgr::Create( 
    const AkStreamMgrSettings &	in_settings		// Stream manager initialization settings.
    )
{
    // Check memory manager.
    if ( !AK::MemoryMgr::IsInitialized() )
    {
        AKASSERT( !"Memory manager does not exist" );
        return NULL;
    }

    // Factory.
    AKASSERT( AK::IAkStreamMgr::Get() == NULL || !"CreateStreamMgr( ) should be called only once" );
    if ( AK::IAkStreamMgr::Get() == NULL )
    {
		// Clear current language.
		m_szCurrentLanguage[0] = 0;

        // Create stream manager.
        if ( CAkStreamMgr::m_streamMgrPoolId == AK_INVALID_POOL_ID )
        {
            // Create stream manager objects pool.
            CAkStreamMgr::m_streamMgrPoolId = AK::MemoryMgr::CreatePool( NULL,
                                                    in_settings.uMemorySize,
                                                    AK_STM_OBJ_POOL_BLOCK_SIZE,
                                                    AkMalloc );
        }
	    if ( CAkStreamMgr::m_streamMgrPoolId == AK_INVALID_POOL_ID )
		{
            AKASSERT( !"Stream manager pool creation failed" );
			return NULL;
		}
		AK_SETPOOLNAME(CAkStreamMgr::m_streamMgrPoolId,AKTEXT("Stream Manager"));
        
        // Instantiate stream manager.
        CAkStreamMgr * pStreamMgr = AkNew( CAkStreamMgr::m_streamMgrPoolId, CAkStreamMgr() );
        
        // Initialize.
        if ( pStreamMgr != NULL )
        {
            if ( pStreamMgr->Init( in_settings ) != AK_Success )
            {
                // Failed. Clean up.
                AKASSERT( !"Failed initializing stream manager" );
                pStreamMgr->Destroy();
                pStreamMgr = NULL;
            }
        }

		// If instantiation failed, need to destroy stm mgr pool. 
        if ( pStreamMgr == NULL )
        {
            AKVERIFY( AK::MemoryMgr::DestroyPool( CAkStreamMgr::m_streamMgrPoolId ) == AK_Success );
        }
    }
        
	AKASSERT( AK::IAkStreamMgr::Get() != NULL );
    return AK::IAkStreamMgr::Get();
}

void AK::StreamMgr::GetDefaultSettings(
	AkStreamMgrSettings &		out_settings
	)
{
	out_settings.uMemorySize            = AK_DEFAULT_STM_OBJ_POOL_SIZE;
}

void AK::StreamMgr::GetDefaultDeviceSettings(
	AkDeviceSettings &			out_settings
	)
{
	out_settings.pIOMemory				= NULL;
	out_settings.uIOMemorySize			= AK_DEFAULT_DEVICE_IO_POOL_SIZE;
	out_settings.uIOMemoryAlignment		= AK_REQUIRED_IO_POOL_ALIGNMENT;
	out_settings.ePoolAttributes		= AK_DEFAULT_BLOCK_ALLOCATION_TYPE;

	out_settings.uGranularity			= AK_DEFAULT_DEVICE_GRANULARITY;
	out_settings.uSchedulerTypeFlags	= AK_DEFAULT_DEVICE_SCHEDULER;
	
	AKPLATFORM::AkGetDefaultThreadProperties( out_settings.threadProperties );
	
	// I/O thread uses a thread priority above normal.
	out_settings.threadProperties.nPriority	= AK_DEFAULT_DEVICE_THREAD_PRIORITY;

	out_settings.fTargetAutoStmBufferLength = AK_DEFAULT_DEVICE_BUFFERING_LENGTH;
	out_settings.uMaxConcurrentIO			= AK_DEFAULT_MAX_CONCURRENT_IO;

	out_settings.bUseStreamCache			= AK_DEFAULT_DEVICE_CACHE_ENABLED;

	out_settings.uMaxCachePinnedBytes	= (AkUInt32)-1; //Unlimited;
}

AK::StreamMgr::IAkFileLocationResolver * AK::StreamMgr::GetFileLocationResolver()
{
	AKASSERT( AK::IAkStreamMgr::Get() 
			|| !"Trying to get file location resolver before StreamManager was created" );
	return CAkStreamMgr::m_pFileLocationResolver;
}

void AK::StreamMgr::SetFileLocationResolver(
	AK::StreamMgr::IAkFileLocationResolver *	in_pFileLocationResolver
	)
{
	AKASSERT( AK::IAkStreamMgr::Get()
			|| !"Trying to set file location handler before StreamManager was created" );
	CAkStreamMgr::m_pFileLocationResolver = in_pFileLocationResolver;
}

AkMemPoolId AK::StreamMgr::GetPoolID()
{
	return CAkStreamMgr::GetObjPoolID();
}

// Device creation.
AkDeviceID AK::StreamMgr::CreateDevice(
    const AkDeviceSettings &	in_settings,		// Device settings.
	IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
    )
{
    return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->CreateDevice( in_settings, in_pLowLevelHook );
}
AKRESULT AK::StreamMgr::DestroyDevice(
    AkDeviceID                  in_deviceID         // Device ID.
    )
{
    return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->DestroyDevice( in_deviceID );
}

// Language management.
AKRESULT AK::StreamMgr::SetCurrentLanguage(
	const AkOSChar *	in_pszLanguageName			///< Language name.
	)
{
	return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->SetCurrentLanguage( in_pszLanguageName );
}

// Get current language. See AkStreamMgrModule.h.
const AkOSChar * AK::StreamMgr::GetCurrentLanguage()
{
	return m_szCurrentLanguage;
}

AKRESULT AK::StreamMgr::AddLanguageChangeObserver(
	AkLanguageChangeHandler in_handler,	///< Callback function.
	void * in_pCookie					///< Cookie, passed back to AkLanguageChangeHandler.
	)
{
	return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->AddLanguageChangeObserver( in_handler, in_pCookie );
}

void AK::StreamMgr::RemoveLanguageChangeObserver(
	void * in_pCookie					///< Cookie, passed back to AkLanguageChangeHandler.
	)
{
	return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->RemoveLanguageChangeObserver( in_pCookie );
}

// Cache management.
void AK::StreamMgr::FlushAllCaches()
{
	static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->FlushAllCaches();
}


using namespace AK::StreamMgr;


//--------------------------------------------------------------------
// Deferred open data.
//--------------------------------------------------------------------

// Create by string.
AkDeferredOpenData * AkDeferredOpenData::Create( 
	const AkOSChar*				in_pszFileName,		// File name.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AkDeferredOpenData * pDeferredOpenData = (AkDeferredOpenData*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkDeferredOpenData ) );
	if ( pDeferredOpenData )
	{
		if ( pDeferredOpenData->Init( in_pszFileName, in_pFSFlags, in_eOpenMode ) != AK_Success )
		{
			pDeferredOpenData->Destroy();
			pDeferredOpenData = NULL;
		}
	}
	return pDeferredOpenData;
}

// Create by ID
AkDeferredOpenData * AkDeferredOpenData::Create( 
	AkFileID					in_fileID,			// File ID.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AkDeferredOpenData * pDeferredOpenData = (AkDeferredOpenData*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkDeferredOpenData ) );
	if ( pDeferredOpenData )
		pDeferredOpenData->Init( in_fileID, in_pFSFlags, in_eOpenMode );
	return pDeferredOpenData;
}

void AkDeferredOpenData::Destroy()
{
	Term();
	AkFree( CAkStreamMgr::GetObjPoolID(), this );
}

AKRESULT AkDeferredOpenData::Execute( AkFileDesc & io_fileDesc )
{
	AKRESULT eResult;
	bool bSyncOpen = true;	// Force Low-Level IO to open synchronously.
	AkFileSystemFlags * pFlags = ( bUseFlags ) ? &flags : NULL;
	if ( bByString )
	{
		eResult = CAkStreamMgr::GetFileLocationResolver()->Open(
			pszFileName, 
			eOpenMode,
			pFlags,
			bSyncOpen,
			io_fileDesc );
	}
	else
	{
		eResult = CAkStreamMgr::GetFileLocationResolver()->Open(
			fileID, 
			eOpenMode,
			pFlags,
			bSyncOpen,
			io_fileDesc );
	}

	if (eResult != AK_Success 
		|| (io_fileDesc.iFileSize <= 0 && eOpenMode == AK_OpenModeRead)
		|| !bSyncOpen)
	{
		// Debug check sync flag.
		AKASSERT(bSyncOpen || !"Cannot defer open when asked for synchronous");

		// Monitor error.
		if ( bByString )
			MonitorFileOpenError(eResult, pszFileName);
		else
			MonitorFileOpenError(eResult, fileID);

		eResult = AK_Fail;
	}
	
	return eResult;
}

// Init by string.
AKRESULT AkDeferredOpenData::Init( 
	const AkOSChar*				in_pszFileName,		// File name.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	// Important: Need to set this first for proper freeing in case of error.
	bByString = true;

	eOpenMode = in_eOpenMode;
	
	if ( in_pFSFlags )
	{
		bUseFlags = true;
		flags = *in_pFSFlags;
	}
	else
		bUseFlags = false;

	// Allocate string buffer for user defined stream name.
	size_t uStrLen = AKPLATFORM::OsStrLen( in_pszFileName ) + 1;
    pszFileName = (AkOSChar*)AkAlloc( CAkStreamMgr::GetObjPoolID(), (AkUInt32)sizeof(AkOSChar)*( uStrLen ) );
    if ( !pszFileName )
        return AK_Fail;

    // Copy.
	AKPLATFORM::SafeStrCpy( pszFileName, in_pszFileName, uStrLen );
	return AK_Success;
}

// Init by ID.
void AkDeferredOpenData::Init( 
	AkFileID					in_fileID,			// File ID.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	// Important: Need to set this first for proper freeing in case of error.
	bByString = false;

	if ( in_pFSFlags )
	{
		bUseFlags = true;
		flags = *in_pFSFlags;
	}
	else
		bUseFlags = false;

	fileID = in_fileID;
	eOpenMode = in_eOpenMode;
}

void AkDeferredOpenData::Term()
{
	if ( bByString 
		&& pszFileName )
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pszFileName );
	}
}

//--------------------------------------------------------------------
// class CAkStreamMgr
//--------------------------------------------------------------------

void CAkStreamMgr::Destroy()
{
    Term();

    // Destroy singleton.
    AKASSERT( AK::MemoryMgr::IsInitialized() &&
              m_streamMgrPoolId != AK_INVALID_POOL_ID );
    if ( AK::MemoryMgr::IsInitialized() &&
         m_streamMgrPoolId != AK_INVALID_POOL_ID )
    {
        AkDelete( m_streamMgrPoolId, this );
    }

    // Destroy stream manager pool.
    AKVERIFY( AK::MemoryMgr::DestroyPool( m_streamMgrPoolId ) == AK_Success );
    m_streamMgrPoolId = AK_INVALID_POOL_ID;
}

CAkStreamMgr::CAkStreamMgr()
{
    // Assign global pointer.
    m_pStreamMgr = this;
}

CAkStreamMgr::~CAkStreamMgr()
{
	for (CachedFileStreamDataMap::Iterator it = m_cachedFileStreams.Begin(); it !=m_cachedFileStreams.End(); ++it)
	{
		CachedFileStreamDataStruct& stct = (*it).item;
		AKASSERT (stct.pData && stct.pData->pStream);
		stct.pData->pStream->Destroy();
		stct.pData->pStream = NULL;
		stct.FreeData();
	}
	m_cachedFileStreams.Term();

    // Reset global pointer.
    m_pStreamMgr = NULL;
}


// Initialise/Terminate.
//-------------------------------------
AKRESULT CAkStreamMgr::Init(
    const AkStreamMgrSettings & /*in_settings*/
    )
{
	return AK_Success;
}

void CAkStreamMgr::Term()
{
	CAkStreamMgr::m_pFileLocationResolver = NULL;
	
    // Destroy devices remaining.
    AkDeviceArray::Iterator it = m_arDevices.Begin();
    while ( it != m_arDevices.End() )
    {
		if ( (*it) )
			(*it)->Destroy();
        ++it;
    }
    m_arDevices.Term();

	CAkStreamMgr::m_arLangChgObserver.Term();
}

//-----------------------------------------------------------------------------
// Device management.
// Warning: These functions are not thread safe.
//-----------------------------------------------------------------------------
// Device creation.
AkDeviceID CAkStreamMgr::CreateDevice(
    const AkDeviceSettings &	in_settings,		// Device settings.
	IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
    )
{
	AkDeviceID newDeviceID = AK_INVALID_DEVICE_ID;

	// Find first available slot.
	for ( AkUInt32 uSlot = 0; uSlot < m_arDevices.Length(); ++uSlot )
	{
		if ( !m_arDevices[uSlot] )
		{
			newDeviceID = uSlot;
			break;
		}
	}

	if ( AK_INVALID_DEVICE_ID == newDeviceID )
	{
		// Create slot.
		if ( !m_arDevices.AddLast( NULL ) )
		{
			AKASSERT( !"Could not add new device to list" );
			return AK_INVALID_DEVICE_ID;
		}
		newDeviceID = (AkDeviceID)m_arDevices.Length() - 1;
		m_arDevices.Last() = NULL;
	}

    CAkDeviceBase * pNewDevice = NULL;
    AKRESULT eResult = AK_Fail;

    // Device factory.
    if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_BLOCKING )
    {
        // AK_SCHEDULER_BLOCKING
        pNewDevice = AkNew( m_streamMgrPoolId, CAkDeviceBlocking( in_pLowLevelHook ) );
        if ( pNewDevice != NULL )
        {
            eResult = pNewDevice->Init( 
				in_settings,
				newDeviceID );
        }
        AKASSERT( eResult == AK_Success || !"Cannot initialize IO device" );
    }
    else if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_DEFERRED_LINED_UP )
    {
        // AK_SCHEDULER_DEFERRED_LINED_UP.
        pNewDevice = AkNew( m_streamMgrPoolId, CAkDeviceDeferredLinedUp( in_pLowLevelHook ) );
        if ( pNewDevice != NULL )
        {
            eResult = pNewDevice->Init( 
				in_settings,
				newDeviceID );
        }
        AKASSERT( eResult == AK_Success || !"Cannot initialize IO device" );
    }
    else
    {
        AKASSERT( !"Invalid device type" );
        return AK_INVALID_DEVICE_ID;
    }

	if ( AK_Success == eResult )
		m_arDevices[newDeviceID] = pNewDevice;
	else
    {
		// Handle failure. At this point we have a valid device ID (index in array).
        if ( pNewDevice )
            pNewDevice->Destroy();
        return AK_INVALID_DEVICE_ID;
    }

    return newDeviceID;
}

// Warning: This function is not thread safe. No stream should exist for that device when it is destroyed.
AKRESULT   CAkStreamMgr::DestroyDevice(
    AkDeviceID          in_deviceID         // Device ID.
    )
{
    if ( (AkUInt32)in_deviceID >= m_arDevices.Length() 
		|| !m_arDevices[in_deviceID] )
    {
        return AK_InvalidParameter;
    }

    m_arDevices[in_deviceID]->Destroy();
	m_arDevices[in_deviceID] = NULL;

    return AK_Success;
}

// Global pool cleanup: dead streams.
// Since the StreamMgr's global pool is shared across all devices, they all need to perform
// dead handle clean up. The device that calls this method will also be asked to kill one of
// its tasks.
void CAkStreamMgr::ForceCleanup(
	CAkDeviceBase * in_pCallingDevice,		// Calling device: if specified, the task with the lowest priority for this device will be killed.
	AkPriority in_priority					// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
	)
{
	for ( AkUInt32 uDevice = 0; uDevice < m_arDevices.Length(); uDevice++ )
    {
		if ( m_arDevices[uDevice] )
		    m_arDevices[uDevice]->ForceCleanup( in_pCallingDevice == m_arDevices[uDevice], in_priority );
	}
}


// Stream creation interface.
// ------------------------------------------------------


// Standard stream open methods.
// -----------------------------

// String overload.
AKRESULT CAkStreamMgr::CreateStd(
    const AkOSChar*     in_pszFileName,     // Application defined string (title only, or full path, or code...).
    AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
	bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
    // Check parameters.
    if ( in_pszFileName == NULL )
    {
        AKASSERT( !"Invalid file name" );
        return AK_InvalidParameter;
    }

	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = false;

    AkFileDesc * pFileDesc = (AkFileDesc*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkFileDesc ) );
	if ( !pFileDesc )
		return AK_Fail;
	memset( pFileDesc, 0, sizeof( AkFileDesc ) );
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_pszFileName,
		in_eOpenMode,
		in_pFSFlags,
		bSyncOpen,
		*pFileDesc );
	if (eRes != AK_Success
		|| (pFileDesc->iFileSize <= 0 && bSyncOpen && in_eOpenMode == AK_OpenModeRead))
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );

#ifndef AK_OPTIMIZED
        // HACK: Hide monitoring errors for banks that are not found in bIsLanguageSpecific directory.
        if ( in_pFSFlags &&
			 in_pFSFlags->bIsLanguageSpecific &&
             in_pFSFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC &&
             ( in_pFSFlags->uCodecID == AKCODECID_BANK || 
				in_pFSFlags->uCodecID == AKCODECID_FILE_PACKAGE ) )
             return eRes;

		MonitorFileOpenError(eRes, in_pszFileName);
#endif
        return (eRes == AK_FileNotFound) ? AK_FileNotFound : AK_Fail;
    }

	CAkDeviceBase * pDevice = GetDevice( pFileDesc->deviceID );
    if ( !pDevice )
	{
		AkFree(CAkStreamMgr::GetObjPoolID(), pFileDesc);
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
		return AK_Fail;
	}
	
	IAkStdStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateStd( 
		pFileDesc,
        in_eOpenMode,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( *pFileDesc );
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen( pFileDesc );
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( pFileDesc, in_pszFileName, in_pFSFlags, in_eOpenMode ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->SetToBeDestroyed();
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// ID overload.
AKRESULT CAkStreamMgr::CreateStd(
    AkFileID            in_fileID,          // Application defined ID.
    AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
	bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = false;

    AkFileDesc * pFileDesc = (AkFileDesc*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkFileDesc ) );
	if ( !pFileDesc )
		return AK_Fail;
	memset( pFileDesc, 0, sizeof( AkFileDesc ) );
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_fileID,
		in_eOpenMode,
		in_pFSFlags,
		bSyncOpen,
		*pFileDesc );

	if (eRes != AK_Success
		|| (pFileDesc->iFileSize <= 0 && bSyncOpen && in_eOpenMode == AK_OpenModeRead))
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
    	
#ifndef AK_OPTIMIZED
        // HACK: Hide monitoring errors for banks that are not found in bIsLanguageSpecific directory.
        if ( in_pFSFlags &&
			 in_pFSFlags->bIsLanguageSpecific &&
             in_pFSFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC &&
             ( in_pFSFlags->uCodecID == AKCODECID_BANK 
				|| in_pFSFlags->uCodecID == AKCODECID_FILE_PACKAGE ) )
             return eRes;

		MonitorFileOpenError(eRes, in_fileID);
#endif
        return (eRes == AK_FileNotFound) ? AK_FileNotFound : AK_Fail;
    }

    CAkDeviceBase * pDevice = GetDevice( pFileDesc->deviceID );
    if ( !pDevice )
	{
		AkFree(CAkStreamMgr::GetObjPoolID(), pFileDesc);
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
		return AK_Fail;
	}

    IAkStdStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateStd( 
		pFileDesc,
        in_eOpenMode,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( *pFileDesc );
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen( pFileDesc );
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( pFileDesc, in_fileID, in_pFSFlags, in_eOpenMode ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->SetToBeDestroyed();
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}


// Automatic stream open methods.
// ------------------------------

// String overload.
AKRESULT CAkStreamMgr::CreateAuto(
    const AkOSChar*             in_pszFileName,     // Application defined string (title only, or full path, or code...).
    AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
    IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
	bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
    // Check parameters.
    if ( in_pszFileName == NULL )
    {
        AKASSERT( !"Invalid file name" );
        return AK_InvalidParameter;
    }
    if ( in_heuristics.fThroughput < 0 ||
         in_heuristics.priority < AK_MIN_PRIORITY ||
         in_heuristics.priority > AK_MAX_PRIORITY )
    {
        AKASSERT( !"Invalid automatic stream heuristic" );
        return AK_InvalidParameter;
    }

	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	AkFileID fileIDForCaching = AK_INVALID_FILE_ID;
	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
	{
		in_pFSFlags->bIsAutomaticStream = true;
		fileIDForCaching = in_pFSFlags->uCacheID;
	}

    AkFileDesc * pFileDesc = (AkFileDesc*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkFileDesc ) );
	if ( !pFileDesc )
		return AK_Fail;
	memset( pFileDesc, 0, sizeof( AkFileDesc ) );
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_pszFileName,
		AK_OpenModeRead,  // Always read from an autostream.
		in_pFSFlags,
		bSyncOpen,
		*pFileDesc );

    if (eRes != AK_Success
		|| (bSyncOpen && pFileDesc->iFileSize <= 0))
    {
    	AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
    	MonitorFileOpenError(eRes, in_pszFileName);
        return (eRes == AK_FileNotFound) ? AK_FileNotFound : AK_Fail;
    }

    CAkDeviceBase * pDevice = GetDevice( pFileDesc->deviceID );
    if ( !pDevice )
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
        return AK_Fail;
	}

    IAkAutoStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateAuto( 
		pFileDesc,
		fileIDForCaching,
        in_heuristics,
        in_pBufferSettings,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( *pFileDesc );
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen( pFileDesc );
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( pFileDesc, in_pszFileName, in_pFSFlags, AK_OpenModeRead ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->SetToBeDestroyed();
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// ID overload.
AKRESULT CAkStreamMgr::CreateAuto(
    AkFileID                    in_fileID,          // Application defined ID.
    AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
	IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
	bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
	// Check parameters.
	if ( in_heuristics.fThroughput < 0 ||
		in_heuristics.priority < AK_MIN_PRIORITY ||
		in_heuristics.priority > AK_MAX_PRIORITY )
	{
		AKASSERT( !"Invalid automatic stream heuristic" );
		return AK_InvalidParameter;
	}

	AKASSERT( m_pFileLocationResolver
		|| !"File location resolver was not set on the Stream Manager" );

	AkFileID fileIDForCaching = AK_INVALID_FILE_ID;
	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
	{
		in_pFSFlags->bIsAutomaticStream = true;
		// Take cache ID if specified. Otherwise use the file ID if called from Wwise sound engine.
		if ( in_pFSFlags->uCacheID != AK_INVALID_FILE_ID )
			fileIDForCaching = in_pFSFlags->uCacheID;
	}

	AkFileDesc * pFileDesc = (AkFileDesc*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkFileDesc ) );
	if ( !pFileDesc )
		return AK_Fail;
	memset( pFileDesc, 0, sizeof( AkFileDesc ) );
	bool bSyncOpen = in_bSyncOpen;
	AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_fileID,
		AK_OpenModeRead,  // Always read from an autostream.
		in_pFSFlags,
		bSyncOpen,
		*pFileDesc );

	if (eRes != AK_Success
		|| (bSyncOpen && pFileDesc->iFileSize <= 0))
	{
		AkFree(CAkStreamMgr::GetObjPoolID(), pFileDesc);
		MonitorFileOpenError(eRes, in_fileID);		
		return (eRes == AK_FileNotFound) ? AK_FileNotFound : AK_Fail;
	}

	CAkDeviceBase * pDevice = GetDevice( pFileDesc->deviceID );
	if ( !pDevice )
	{
		AkFree(CAkStreamMgr::GetObjPoolID(), pFileDesc);
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
		return AK_Fail;
	}

	IAkAutoStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateAuto( 
		pFileDesc,
		fileIDForCaching,
		in_heuristics,
		in_pBufferSettings,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( *pFileDesc );
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		return AK_Fail;
	}

	if ( bSyncOpen )
	{
		pTask->SetFileOpen( pFileDesc );
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( pFileDesc, in_fileID, in_pFSFlags, AK_OpenModeRead ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->SetToBeDestroyed();
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// Cache pinning mechanism.
// ------------------------------

AKRESULT CAkStreamMgr::PinFileInCache(
	AkFileID                    in_fileID,          // Application defined ID.
	AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
	AkPriority					in_uPriority
	)
{
	AKRESULT res = AK_Fail;
	CachedFileStreamDataStruct* pStruct = m_cachedFileStreams.Exists( in_fileID );
	if (pStruct)
	{
		CachedFileStreamData* pData = pStruct->pData;
		AKASSERT(pData);
		AKASSERT(pData->pStream);
		
		if ( pData->AddRef( in_uPriority ) )
		{
			CAkAutoStmBase* pAutoStream = static_cast<CAkAutoStmBase*>(pData->pStream);
			pAutoStream->GetDevice()->UpdateCachingPriority(pAutoStream, pData->GetPriority() );
			res =  AK_Success;
		}
		
	}
	else
	{
		pStruct = m_cachedFileStreams.Set( in_fileID );
		if (pStruct)
		{
			if ( pStruct->AllocData() ) 
		{
				CachedFileStreamData* pData = pStruct->pData;

			if ( AK_Success == CreateCachingStream( in_fileID, in_pFSFlags, in_uPriority, pData->pStream ) )
			{
					AKASSERT( pData->pStream );

					pData->AddRef(in_uPriority);

				const unsigned long MAX_NUMBER_STR_SIZE = 11;
				AkOSChar szName[MAX_NUMBER_STR_SIZE];
				AK_OSPRINTF( szName, MAX_NUMBER_STR_SIZE, AKTEXT("%u"), in_fileID );
				pData->pStream->SetStreamName( szName );
				pData->pStream->Start();

				res = AK_Success;
			}
			else
			{
				//Failed to create the stream.
				pStruct->FreeData();
				m_cachedFileStreams.Unset(in_fileID);
				pData = NULL;
				pStruct = NULL;
				res = AK_Fail;
			}
		}
			else
	{
				//Failed to allocate data
				m_cachedFileStreams.Unset(in_fileID);
				pStruct = NULL;
				res = AK_Fail;
			}
		}
	}

	return res;
}

AKRESULT CAkStreamMgr::UnpinFileInCache(
	AkFileID                    in_fileID,          // Application defined ID.
	AkPriority					in_uPriority
	)
{
	AKRESULT res = AK_Fail;

	CachedFileStreamDataStruct* pStruct = m_cachedFileStreams.Exists( in_fileID );
	if (pStruct)
	{
		CachedFileStreamData* pData = pStruct->pData;
		AKASSERT(pData);

		if ( pData->Release(in_uPriority) )
		{
			if ( pData->pStream != NULL )
			{
				pData->pStream->Destroy();
				pData->pStream = NULL;
			}
			pStruct->FreeData();
			m_cachedFileStreams.Unset(in_fileID);
			pStruct = NULL;
			res = AK_Success;
		}
		else
		{
			CAkAutoStmBase* pAutoStream = static_cast<CAkAutoStmBase*>(pData->pStream);
			pAutoStream->GetDevice()->UpdateCachingPriority(pAutoStream, pData->GetPriority() );
		}
	}

	return res;
}

AKRESULT CAkStreamMgr::UpdateCachingPriority(
										AkFileID                    in_fileID,          // Application defined ID.
										AkPriority					in_uPriority,
										AkPriority					in_uOldPriority
										)
{
	AKRESULT res = AK_Fail;
	CachedFileStreamDataStruct* pStruct = m_cachedFileStreams.Exists( in_fileID );
	if (pStruct)
	{
		CachedFileStreamData* pData = pStruct->pData;
		AKASSERT(pData);
		AKASSERT(pData->pStream);
		if ( pData->UpdatePriority(in_uPriority, in_uOldPriority) )
		{
		CAkAutoStmBase* pAutoStream = static_cast<CAkAutoStmBase*>(pData->pStream);
			pAutoStream->GetDevice()->UpdateCachingPriority(pAutoStream, pData->GetPriority() );
		res = AK_Success;
	}
	}
	return res;
}

AKRESULT CAkStreamMgr::GetBufferStatusForPinnedFile( AkFileID in_fileID , AkReal32& out_fPercentBuffered, bool& out_bCacheFull )
{
	out_fPercentBuffered = 0.f;
	out_bCacheFull = false;

	AKRESULT res = AK_Fail;
	CachedFileStreamDataStruct* pStruct = m_cachedFileStreams.Exists( in_fileID );
	if (pStruct)
	{
		CachedFileStreamData* pData = pStruct->pData;
		AKASSERT(pData);
		AKASSERT(pData->pStream);

		CAkAutoStmBase* pAutoStream = static_cast<CAkAutoStmBase*>(pData->pStream);
		
		AkUInt32 uCachingBufferSize = pAutoStream->GetNominalBuffering();
		AkStreamInfo stmInfo;
		pAutoStream->GetInfo(stmInfo);
		if (stmInfo.uSize != 0)
			uCachingBufferSize = AkMin(uCachingBufferSize, stmInfo.uSize);

		AkUInt32 uBufferedBytes = pAutoStream->GetVirtualBufferingSize();

		out_fPercentBuffered = ((AkReal32)uBufferedBytes / (AkReal32)uCachingBufferSize) * 100.f;
		out_bCacheFull = ( uBufferedBytes < uCachingBufferSize ) && 
						 ( (uCachingBufferSize - uBufferedBytes) > pAutoStream->GetDevice()->RemainingCachePinnedBytes() );
		res = AK_Success;
	}

	return res;
}

// ID overload.
AKRESULT CAkStreamMgr::CreateCachingStream(
								  AkFileID                    in_fileID,          // Application defined ID.
								  AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can NOT be NULL.
								  AkPriority				  in_uPriority,								 
								  IAkAutoStream*&            out_pStream		// Returned interface to an automatic stream.
								  )
{
	AKASSERT( m_pFileLocationResolver || !"File location resolver was not set on the Stream Manager" );

	AKASSERT ( in_pFSFlags );
	in_pFSFlags->bIsAutomaticStream = true;

	AkFileDesc * pFileDesc = (AkFileDesc*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkFileDesc ) );
	if ( !pFileDesc )
		return AK_Fail;
	memset( pFileDesc, 0, sizeof( AkFileDesc ) );
	bool bSyncOpen = false;
	AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_fileID,
		AK_OpenModeRead,  // Always read from an autostream.
		in_pFSFlags,
		bSyncOpen,
		*pFileDesc );

	if (in_pFSFlags->uNumBytesPrefetch == 0) // the resolver may have changed this value)
	{
		eRes = AK_Fail;
	}

	if (eRes != AK_Success
		|| (bSyncOpen && pFileDesc->iFileSize <= 0))
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		MonitorFileOpenError(eRes, in_fileID);
		return (eRes == AK_FileNotFound) ? AK_FileNotFound : AK_Fail;
	}

	CAkDeviceBase * pDevice = GetDevice( pFileDesc->deviceID );
	if ( !pDevice )
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
		return AK_Fail;
	}

	IAkAutoStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateCachingStream( 
		pFileDesc,
		in_fileID,
		in_pFSFlags->uNumBytesPrefetch,
		in_uPriority,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( *pFileDesc );
		AkFree( CAkStreamMgr::GetObjPoolID(), pFileDesc );
		return AK_Fail;
	}

	if ( bSyncOpen )
	{
		pTask->SetFileOpen( pFileDesc );
	}
	else
	{
		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( pFileDesc, in_fileID, in_pFSFlags, AK_OpenModeRead ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->SetToBeDestroyed();
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// Profiling interface.
// --------------------------------------------------------------------
IAkStreamMgrProfile * CAkStreamMgr::GetStreamMgrProfile()
{
#ifndef AK_OPTIMIZED
    return this;
#else
	return NULL;
#endif
}

#ifndef AK_OPTIMIZED
// Device enumeration.
AkUInt32 CAkStreamMgr::GetNumDevices()
{
	// Find real number of stream devices.
	AkUInt32 uNumDevices = 0;
	for ( AkUInt32 uSlot = 0; uSlot < m_arDevices.Length(); ++uSlot )
	{
		if ( m_arDevices[uSlot] )
			++uNumDevices;
	}

    return uNumDevices;
}

IAkDeviceProfile * CAkStreamMgr::GetDeviceProfile( 
    AkUInt32 in_uDeviceIndex    // [0,numStreams[
    )
{
    if ( in_uDeviceIndex >= m_arDevices.Length() )
    {
        AKASSERT( !"Invalid device index" );
        return NULL;
    }
    
	// Convert device index to device ID.
	for ( AkUInt32 uDeviceID = 0; uDeviceID < m_arDevices.Length(); ++uDeviceID )
	{
		if ( !m_arDevices[uDeviceID] )
			++in_uDeviceIndex;	// NULL. Skip.
		else if ( in_uDeviceIndex == uDeviceID )
			return m_arDevices[uDeviceID];
	}

	AKASSERT( !"Invalid device index" );
    return NULL;
}

AKRESULT CAkStreamMgr::StartMonitoring()
{
    for ( AkUInt32 u=0; u<m_arDevices.Length( ); u++ )
    {
		if ( m_arDevices[u] )
		{
        	AKVERIFY( m_arDevices[u]->StartMonitoring( ) == AK_Success );
		}
    }
    return AK_Success;
}

void CAkStreamMgr::StopMonitoring()
{
    for ( AkUInt32 u=0; u<m_arDevices.Length( ); u++ )
    {
        if ( m_arDevices[u] )
        	m_arDevices[u]->StopMonitoring( );
    }
}

#endif

// Language management.
AKRESULT CAkStreamMgr::SetCurrentLanguage(
	const AkOSChar *	in_pszLanguageName
	)
{
	if ( !in_pszLanguageName )
	{
		AKASSERT( !"Invalid language string" );
		return AK_Fail;
	}
	size_t uStrLen = AKPLATFORM::OsStrLen( in_pszLanguageName );
	if ( uStrLen >= AK_MAX_LANGUAGE_NAME_SIZE 
		|| ( uStrLen > 0 && ( in_pszLanguageName[uStrLen-1] == '/' || in_pszLanguageName[uStrLen-1] == '\\' ) ) )
	{
		AKASSERT( !"Invalid language name" );
		return AK_Fail;
	}
	AKPLATFORM::SafeStrCpy( m_szCurrentLanguage, in_pszLanguageName, AK_MAX_LANGUAGE_NAME_SIZE );

	// Notify observers, from last to first (to let them unregister from callback).
	AkUInt32 uCallback = m_arLangChgObserver.Length();
	while ( uCallback > 0 )
	{
		LangChgObserver & rCallbackData = m_arLangChgObserver[uCallback-1];
		rCallbackData.handler( m_szCurrentLanguage, rCallbackData.pCookie );
		--uCallback;
	}

	return AK_Success;
}

AKRESULT CAkStreamMgr::AddLanguageChangeObserver(
	AkLanguageChangeHandler in_handler,	
	void * in_pCookie					
	)
{
	LangChgObserver * pNewObserver = m_arLangChgObserver.AddLast();
	if ( pNewObserver )
	{
		pNewObserver->handler = in_handler;
		pNewObserver->pCookie = in_pCookie;
		return AK_Success;
	}
	return AK_Fail;
}

void CAkStreamMgr::RemoveLanguageChangeObserver(
	void * in_pCookie					
	)
{
	ArrayLangChgObservers::Iterator it = m_arLangChgObserver.Begin();
	while ( it != m_arLangChgObserver.End() )
	{
		if ( (*it).pCookie == in_pCookie )
			it = m_arLangChgObserver.Erase( it );
		else
			++it;
	}
}

void CAkStreamMgr::FlushAllCaches()
{
	for ( AkUInt32 uDevice = 0; uDevice < m_arDevices.Length(); uDevice++ )
    {
		if ( m_arDevices[uDevice] )
		    m_arDevices[uDevice]->FlushCache();
	}
}

