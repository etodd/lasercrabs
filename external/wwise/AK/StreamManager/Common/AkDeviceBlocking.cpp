//////////////////////////////////////////////////////////////////////
//
// AkDeviceBlocking.h
//
// Win32 Blocking Scheduler Device implementation.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkDeviceBlocking.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
using namespace AK;
using namespace AK::StreamMgr;

CAkDeviceBlocking::CAkDeviceBlocking(
	IAkLowLevelIOHook *	in_pLowLevelHook
	)
: CAkDeviceBase( in_pLowLevelHook )
{
}

CAkDeviceBlocking::~CAkDeviceBlocking( )
{
}

AKRESULT CAkDeviceBlocking::Init( 
    const AkDeviceSettings &	in_settings,
    AkDeviceID					in_deviceID 
    )
{
	AKRESULT eResult = CAkDeviceBase::Init( in_settings, in_deviceID );
	return eResult;
}

// Stream creation interface.
// --------------------------------------------------------

// Standard stream.
CAkStmTask * CAkDeviceBlocking::_CreateStd(
    AkFileDesc *		in_pFileDesc,		// Application defined ID.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream         // Returned interface to a standard stream.    
    )
{
    out_pStream = NULL;
    
    AKRESULT eResult = AK_Fail;

	CAkStdStmBlocking * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmBlocking() );
    
    // If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, AK_MAX_PRIORITY );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmBlocking() );
	}

    if ( pNewStm != NULL )
    {
        eResult = pNewStm->Init( 
			this, 
			in_pFileDesc, 
			in_eOpenMode );
    }
    else
	{
        eResult = AK_InsufficientMemory;
	}

    if ( AK_Success == eResult )
	{
		out_pStream = pNewStm;
		return pNewStm;
	}
	else
    {
		// --------------------------------------------------------
		// Failed. Clean up.
		// --------------------------------------------------------
    	if ( pNewStm != NULL )
       		pNewStm->InstantDestroy();
    }
	return NULL;
}

// Automatic stream
CAkStmTask * CAkDeviceBlocking::_CreateAuto(
    AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
    IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
    )
{
    AKASSERT( in_heuristics.fThroughput >= 0 &&
              in_heuristics.priority >= AK_MIN_PRIORITY &&
              in_heuristics.priority <= AK_MAX_PRIORITY );

    out_pStream = NULL;
    
#ifndef AK_OPTIMIZED
	if ( !m_mgrMemIO.HasPool() )
    {
		AKASSERT( !"Streaming pool does not exist: cannot create automatic stream" );
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_CannotStartStreamNoMemory );
        return NULL;
    }
#endif

	AKRESULT eResult;
    CAkAutoStmBlocking * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmBlocking() );
	
	// If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, in_heuristics.priority );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmBlocking() );
	}

	if ( pNewStm != NULL )
	{
		eResult = pNewStm->Init( 
			this,
			in_pFileDesc,
			in_fileID, 
			in_heuristics,
			in_pBufferSettings,
			m_uGranularity
			);
	}
	else
	{
		eResult = AK_InsufficientMemory;
	}
	
    if ( AK_Success == eResult )
    {
		out_pStream = pNewStm;
		return pNewStm;
	}
	else
	{
		// --------------------------------------------------------
		// Failed. Clean up.
		// --------------------------------------------------------
        if ( pNewStm != NULL )
            pNewStm->InstantDestroy();
        out_pStream = NULL;
    }

    return NULL;
}

// Scheduler.

// Finds the next task to be executed,
// posts the request to Low-Level IO and blocks until it is completed,
// updates the task.
void CAkDeviceBlocking::PerformIO()
{
    AkReal32 fOpDeadline;
    CAkStmTask * pTask = SchedulerFindNextTask( fOpDeadline );

    if ( pTask )
    {
        // Execute.
        ExecuteTask( pTask, fOpDeadline );
    }
}

// Execute task that was chosen by scheduler.
void CAkDeviceBlocking::ExecuteTask( 
    CAkStmTask *	in_pTask,
	AkReal32		in_fOpDeadline
    )
{
	AKASSERT( in_pTask != NULL );

	// Handle deferred opening.
	AKRESULT eResult = in_pTask->EnsureFileIsOpen();
	if ( eResult != AK_Success )
	{
		// Deferred open failed. Updade/Kill this task and bail out.
		in_pTask->Update( NULL,	AK_Fail, false );
		return;
	}

    // Get info for IO.
    AkFileDesc * pFileDesc;
	CAkLowLevelTransfer * pLowLevelXfer;
	bool bUnused;
	CAkStmMemView * pMemView = in_pTask->PrepareTransfer( 
		pFileDesc, 
		pLowLevelXfer,
		bUnused,
		false );
    if ( !pMemView )
	{
		// Transfer was cancelled at the last minute (for e.g. the client Destroy()ed the stream.
		// Update as "cancelled" and bail out.
		in_pTask->Update( NULL,	AK_Cancelled, false );
		return;
	}

	if ( pLowLevelXfer )
	{
		// Requires a low-level transfer.
		AkIoHeuristics heuristics;
		heuristics.priority = in_pTask->Priority();
		heuristics.fDeadline = in_fOpDeadline;

		CAkLowLevelTransferBlocking* pLLXferBlocking = (CAkLowLevelTransferBlocking*)pLowLevelXfer;
	    AKASSERT( pLLXferBlocking->info.uRequestedSize > 0 &&
				pLLXferBlocking->info.uRequestedSize <= m_uGranularity );

		// Read or write?
		bool bReadOp = !in_pTask->IsWriteOp();
		if ( bReadOp )
		{
			// Read.
			eResult = static_cast<IAkIOHookBlocking*>( m_pLowLevelHook )->Read( 
				*pFileDesc,
				heuristics,
				pLLXferBlocking->pAddress,
				pLLXferBlocking->info );
		}
		else
		{
			// Write.
			eResult = static_cast<IAkIOHookBlocking*>( m_pLowLevelHook )->Write( 
				*pFileDesc,
				heuristics,
				pLLXferBlocking->pAddress,
				pLLXferBlocking->info );
		}

		// Monitor errors.
#ifndef AK_OPTIMIZED
		if ( eResult != AK_Success )
			AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IODevice );
#endif

		// Dispose of low-level transfer now. 
		OnLowLevelTransferComplete( pMemView, eResult, bReadOp );
	}
	
    // Update task after transfer.
    in_pTask->Update( pMemView, eResult, ( pLowLevelXfer != NULL ) );
}


// IO memory access.
// -----------------------------------------------------

// Creates a view to the desired streaming memory, for standard streams.
// Accepts a memory block, which should map the user-provided memory.
// A view, pointing to this block, is created and returned. A new low-level transfer is created
// and attached to it, and is returned via out_pLowLevelXfer.
// If there was a failure (out of small object memory), the function returns NULL.
// Sync: 
//	- Thread safe. Internally locks device for memory access. 
//	- Client status should be locked prior to calling this function.
// Note: If the mem block is already busy, a new one is created temporarily (never happens with blocking device).
CAkStmMemView * CAkDeviceBlocking::CreateMemViewStd(
	CAkStmTask *	in_pOwner,			// Owner task. 
	AkMemBlock *	in_pMemBlock,		// Memory block for data view. 
	AkUInt32		in_uDataOffset,		// Data view offset from memory block start.
	AkUInt64		in_uPosition,		// Desired position in file.
	AkUInt32 		in_uBufferSize,		// Buffer size. Same as in_uRequestedSize except at EOF.
	AkUInt32 		in_uRequestedSize,	// Requested size.
	CAkLowLevelTransfer *& out_pLowLevelXfer	// Returned low-level transfer if a new one was created and it needs to be pushed to the Low-Level IO. Never NULL unless the function fails.
	)
{
	out_pLowLevelXfer = NULL;
	CAkStmMemView * pMemView;
	{
		AkAutoLock<CAkIOThread> deviceLock( *this );

		// If this fails, everything fails.
		pMemView = MemViewFactory();
		
	}

	if (pMemView)
	{
		AKASSERT( !in_pMemBlock->IsBusy() );

		// Get the one and only low-level transfer and attach it to the memblock.
		out_pLowLevelXfer = PrepareLowLevelTransfer( 
			in_pOwner,			// Owner task.
			(AkUInt8*)in_pMemBlock->pData + in_uDataOffset, // Address for transfer.
			in_uPosition,		// Position in file, relative to start of file.
			in_uBufferSize,		// Buffer size.
			in_uRequestedSize	// Requested transfer size.
			);
		AKASSERT( out_pLowLevelXfer );	// Cannot fail.
		in_pMemBlock->pTransfer = out_pLowLevelXfer;

		// Create a view to this memory block. The offset is the size that has been read already.
		pMemView->Attach( in_pMemBlock, in_uDataOffset );
	}
	
	return pMemView;
}


// Creates a view to the desired streaming memory, for automatic streams.
// Searches a memory block for IO. If available, tries to get a buffer with cached data. 
// Otherwise, returns a new buffer for IO. 
// If a block is found, a view, pointing to this block, is created and returned.
// If the block requires a transfer, a new low-level transfer is created and attached to it. 
// If it is already busy (never happens with blocking device), a low-level transfer is already attached to it.
// If the transfer needs to be pushed to the Low-Level IO, it is returned via out_pLowLevelXfer.
// If a block was not found, the function returns NULL.
// Sync: 
//	- Thread safe. Internally locks device for memory access. Notifies memory full if applicable, atomically.
//	- Client status should be locked prior to calling this function.
CAkStmMemView * CAkDeviceBlocking::CreateMemViewAuto(
	CAkStmTask *	in_pOwner,			// Owner task. 
	AkFileID		in_fileID,			// Block's associated file ID.
	AkUInt64		in_uPosition,		// Desired position in file.
	AkUInt32		in_uMinSize,		// Minimum data size acceptable (discard otherwise).
	AkUInt32		in_uRequiredAlign,	// Required data alignment.
	bool			in_bEof,			// True if desired block is last of file.
	bool			in_bCacheOnly,		// Get a view of cached data only, otherwise return NULL.
	AkUInt32 &		io_uRequestedSize,	// In: desired data size; Out: returned valid size.
	CAkLowLevelTransfer *& out_pLowLevelXfer	// Returned low-level transfer if a new one was created and it needs to be pushed to the Low-Level IO. NULL otherwise.
	)
{
	out_pLowLevelXfer = NULL;

	// I/O pool access must be enclosed in scheduler lock.
	AkAutoLock<CAkIOThread> deviceLock( *this );

	AkMemBlock * pMemBlock = NULL;

	// Try to get a block from the cache if in_fileID != AK_INVALID_FILE_ID. The minimum
	// buffer size acceptable is equal to m_uMinBufferSize (user-specified buffer constraint), 
	// unless io_uRequestedSize is smaller (which typically occurs at the end of file).
	AkUInt32 uOffset = ( m_mgrMemIO.UseCache() && in_fileID != AK_INVALID_FILE_ID ) ? m_mgrMemIO.GetCachedBlock(
							in_fileID,
							in_uPosition, 
							in_uMinSize,
							in_uRequiredAlign, 
							in_bEof,
							io_uRequestedSize, 
							pMemBlock ) : 0;

	if ( in_bCacheOnly ) //Requested a cache-only transfer. We got some special rules to follow here.
	{ 
		if ( pMemBlock == NULL )
		{
			return NULL;
		}
		else if ( pMemBlock->IsBusy() )
		{
			// If using cached data, we need to ensure that the block is not currently being filled up.
			m_mgrMemIO.ReleaseBlock( pMemBlock );
			return NULL;
		}
	}

	// Create a view to this memory block.
	// If this fails, everything fails.
	CAkStmMemView * pMemView = MemViewFactory();
	if (pMemView)
	{
		if (!pMemBlock)
		{
			// Nothing useful in cache. Acquire free buffer 
			m_mgrMemIO.GetOldestFreeBlock( io_uRequestedSize, in_uRequiredAlign, pMemBlock );
			if ( pMemBlock )
			{
				// Got a new block. Get one and only low-level transfer and tag block.
				out_pLowLevelXfer = PrepareLowLevelTransfer( 
					in_pOwner,				// Owner task.
					pMemBlock->pData,		// Address for transfer.
					in_uPosition,			// Position in file (relative to start of file).
					pMemBlock->uAllocSize,	// Buffer size: with automatic streams, even with smaller stream-specific buffer size, 
											// the maximum size in which the low-level IO may write is m_uGranularity.
					io_uRequestedSize		// Requested size.
					);
				AKASSERT( out_pLowLevelXfer );

				//NOTE:  Tagging the memory block can fail.  This will just mean that we can not re-use the block in cache.
				m_mgrMemIO.TagBlock( 
					pMemBlock, 
					out_pLowLevelXfer, 
					in_fileID, 
					in_uPosition,
					io_uRequestedSize
					);

				// Memory block obtained requires an IO transfer. Effective address points 
				// at the beginning of the block. io_uRequestedSize is unchanged. uOffset is 0.
			}
			else
			{
				// No memory. Get rid of mem view and notify out of memory.
				DestroyMemView(pMemView);
				return NULL;
			}
			
		}

		pMemView->Attach( pMemBlock, uOffset );
	}
	else
	{
		// Failed to allocate mem view, make sure to release the block
		if ( pMemBlock )
		{
			m_mgrMemIO.ReleaseBlock( pMemBlock );
			pMemBlock = NULL;
		}
	}

	AKASSERT( (pMemView && pMemBlock) || (!pMemView && !pMemBlock) ); //All or nothing

	return pMemView;
}

//-----------------------------------------------------------------------------
// Name: class CAkStdStmBlocking
// Desc: Standard stream implementation.
//-----------------------------------------------------------------------------
CAkStdStmBlocking::CAkStdStmBlocking()
: m_pCurrentTransfer( NULL )
, m_bTransferCancelling( false )
{
}

CAkStdStmBlocking::~CAkStdStmBlocking()
{
}

// Destruction. The object is destroyed and the interface becomes invalid.
// Sync: 
// Status lock. Released if we need to wait for transfers to complete.
void CAkStdStmBlocking::Destroy()
{
    // If an operation is pending, the scheduler might be executing it. This method must not return until it 
    // is complete: lock I/O for this task.
	m_lockStatus.Lock();

    // Allow destruction.
	SetToBeDestroyed();

	// Stop asking to be scheduled.
	SetStatus( AK_StmStatusCancelled );
	
	if ( m_pCurrentTransfer )
	{
		m_bTransferCancelling = true;

		// Wait for current transfer completion.
		SetBlockedStatus();
		m_lockStatus.Unlock();
		m_pDevice->WaitForIOCompletion( this );

		// After current transfer completes, m_bTransferCancelling is still true. This prevents this task
		// from being destroyed. Re-obtain the lock in order to clear it.
		m_bTransferCancelling = false;
		m_lockStatus.Lock();
	}
	
	m_lockStatus.Unlock();
}

// Cancel. If Pending, sets its status to Cancelled. Otherwise it returns right away.
// Sync: Status lock.
void CAkStdStmBlocking::Cancel()
{
	m_lockStatus.Lock();

	// Stop asking for IO.
	SetStatus( AK_StmStatusCancelled );
	
	if ( m_pCurrentTransfer )
    {
		m_bTransferCancelling = true;

		// Wait for current transfer completion.
		SetBlockedStatus();
		m_lockStatus.Unlock();
		m_pDevice->WaitForIOCompletion( this );

		// After current transfer completes, m_bTransferCancelling is still true. 
		// Re-obtain the lock in order to clear it.
		m_lockStatus.Lock();
		m_bTransferCancelling = false;
	}

	m_lockStatus.Unlock();
}

// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
// Blocking standard streams may be destroyed if and only if they are not currently waiting for the I/O thread
// to complete a transfer, cancelled or not.
bool CAkStdStmBlocking::CanBeDestroyed()
{
	AkAutoLock<CAkLock> stmLock( m_lockStatus );
	return ( ( m_pCurrentTransfer == NULL ) && !m_bTransferCancelling );
}

// Get information for data transfer.
// Returns the (device-specific) streaming memory view containing logical transfer information. 
// NULL if preparing transfer has aborted.
// out_pLowLevelXfer is set if and only if the transfer requires a transfer in the low-level IO.
// Sync: Locks stream's status.
CAkStmMemView * CAkStdStmBlocking::PrepareTransfer( 
	AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
	CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
	bool &					out_bExistingLowLevelXfer, // Always false on the blocking device.
	bool					
#ifdef AK_ENABLE_ASSERTS 
		in_bCacheOnly		// NOT Supported.
#endif
	)
{
	AKASSERT( !in_bCacheOnly || !"Not supported" );

	out_pLowLevelXfer = NULL;
	out_bExistingLowLevelXfer = false;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() )
		return NULL;

	out_pFileDesc = m_pFileDesc;

	AkUInt64 uFilePosition = GetCurUserPosition() + m_uTotalScheduledSize;

	// Required transfer size is the buffer size for this stream.
    // Slice request to granularity.
	// Cannot overshoot client buffer.
	// NOTE: m_memBlock.uAvailableSize is the original client request size.
	AKASSERT( m_uTotalScheduledSize <= m_memBlock.uAvailableSize );
	AkUInt32 uMaxTransferSize = ( m_memBlock.uAvailableSize - m_uTotalScheduledSize );
	if ( uMaxTransferSize > m_pDevice->GetGranularity() )
        uMaxTransferSize = m_pDevice->GetGranularity();

	// Requested size and buffer size is always identical with standard streams, except at the end of file.
	bool bEof;	// unused
	AkUInt32 uRequestedSize = ( !m_bIsWriteOp ) ? ClampRequestSizeToEof( uFilePosition, uMaxTransferSize, bEof ) : uMaxTransferSize;

	// Create a new memory view for transfer, and attach a low-level transfer to our mem block.
	CAkStmMemView * pMemView = ((CAkDeviceBlocking*)m_pDevice)->CreateMemViewStd(
		this,						// Owner.
		&m_memBlock,				// Memory block (base address).
		m_uTotalScheduledSize,		// Offset.
		uFilePosition,				// Position in file.
		uMaxTransferSize,			// Buffer size. 
		uRequestedSize,				// Requested size.
		out_pLowLevelXfer			// Returned low level transfer (always set if successful: standard stream cannot use cached data).
		);
	if ( pMemView )
	{
		// Set transferred size upfront.
		m_uTotalScheduledSize += uRequestedSize;

		m_pCurrentTransfer = pMemView;

		// Reset timer. Time count since last transfer starts now.
		m_iIOStartTime = m_pDevice->GetTime();
	}
	return pMemView;
}

// Update stream object after I/O.
// Sync: Locks stream's status.
bool CAkStdStmBlocking::Update(
	CAkStmMemView *	in_pTransfer,	// Logical transfer object.
	AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
    )
{
	bool bBufferAdded = false;

	// Lock status.
    AkAutoLock<CAkLock> update( m_lockStatus );

	if ( in_pTransfer )
	{
		bool bStoreData = ( AK_Success == in_eIOResult 
						&& in_pTransfer->DoStoreData() );

		AKASSERT( in_bRequiredLowLevelXfer );	// Standard stream cannot use cache data.
		AddMemView( in_pTransfer, bStoreData );

		m_pCurrentTransfer = NULL;

		bBufferAdded = true;
	}

	UpdateTaskStatus( in_eIOResult );

#ifndef AK_OPTIMIZED
	// Tell profiler that it can reset the "active" bit if it doesn't require scheduling anymore.
	m_bCanClearActiveProfile = !m_bRequiresScheduling;
#endif

	return bBufferAdded;
}

//-----------------------------------------------------------------------------
// Name: class CAkAutoStmBlocking
// Desc: Automatic stream implementation.
//-----------------------------------------------------------------------------
CAkAutoStmBlocking::CAkAutoStmBlocking()
: m_pCurrentTransfer( NULL )
, m_bTransferCancelled( false )
{
}

CAkAutoStmBlocking::~CAkAutoStmBlocking()
{
}

// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
// Blocking automatic streams may be destroyed if and only if they are not currently waiting for the I/O thread
// to complete a transfer, cancelled or not.
bool CAkAutoStmBlocking::CanBeDestroyed()
{
	AkAutoLock<CAkLock> stmLock( m_lockStatus );
	return ( ( m_pCurrentTransfer == NULL ) && !m_bTransferCancelled );
}

// Get information for data transfer.
// Returns the (device-specific) streaming memory view containing logical transfer information. 
// NULL if preparing transfer has aborted.
// out_pLowLevelXfer is set if and only if the transfer requires a transfer in the low-level IO.
// Sync: Locks stream's status.
CAkStmMemView * CAkAutoStmBlocking::PrepareTransfer( 
    AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
	CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
	bool &					out_bExistingLowLevelXfer, // Always false on the blocking device.
	bool					in_bCacheOnly		// Prepare transfer only if data is found in cache. out_pLowLevelXfer will be NULL.
	)
{
	out_pFileDesc = m_pFileDesc;
	out_pLowLevelXfer = NULL;
	out_bExistingLowLevelXfer = false;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() 
		|| m_pCurrentTransfer )	// Note: Need to test for current transfer to avoid reading cache block in progress.
		return NULL;

	// Get position for next transfer.
	AkUInt64 uFilePosition;
	AkUInt32 uRequestedSize;
	bool bEof;
	GetPositionForNextTransfer( uFilePosition, uRequestedSize, bEof );

	//Is is possible for caching streams to be exactly at the end of their prefetch buffer.
	if (uRequestedSize == 0)
		return NULL;

	// Get IO buffer.
	CAkStmMemView * pMemView = ((CAkDeviceBlocking*)m_pDevice)->CreateMemViewAuto(
		this,					// Observer.
		m_fileID,				// File ID (for cache)
		uFilePosition,			// Desired position in file.
		AkMin( m_uMinBufferSize, uRequestedSize ), // Minimum data size acceptable is min between desired size and buffer constraint.
		m_uBufferAlignment,		// Required data alignment.
		bEof,					// True if desired block is last of file.
		in_bCacheOnly,			// Get a view of cached data only, otherwise return NULL.
		uRequestedSize,			// In: Desired data size. Out: Valid data size (may be smaller than input if using cache).
		out_pLowLevelXfer		// Returned low-level transfer if a new one was created and it needs to be pushed to the Low-Level IO.
		);
	if ( !pMemView )
		return NULL; 
	
	// Update status.
	m_pCurrentTransfer = pMemView;

	// m_uVirtualBufferingSize takes looping heuristics into account. Clamp uRequestedSize if applicable.
	// When looping heuristics change, m_uVirtualBufferingSize is recomputed.
	if ( uFilePosition < m_uLoopEnd && ( uFilePosition + uRequestedSize ) > m_uLoopEnd )
		uRequestedSize = m_uLoopEnd - (AkUInt32)uFilePosition;	// Note: Currently, looping heuristics don't work with positions larger than 2GB.
	m_uVirtualBufferingSize += uRequestedSize;

	UpdateSchedulingStatus();

	// Reset timer. Time count since last transfer starts now.
    m_iIOStartTime = m_pDevice->GetTime();
	
	return pMemView;
}

// Update stream object after I/O.
// Sync: Locks stream's status.
bool CAkAutoStmBlocking::Update(
	CAkStmMemView *	in_pTransfer,	// Logical transfer object.
	AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
    )
{
	bool bBufferAdded = false;

	// Lock status.
    AkAutoLock<CAkLock> update( m_lockStatus );

	if ( in_pTransfer )
	{
		bool bStoreData = ( AK_Success == in_eIOResult 
							&& m_pCurrentTransfer
							&& in_pTransfer->DoStoreData() );

		// Using cache: tag transfer as "ready" now if did not require a low-level tranfer.
		if ( !in_bRequiredLowLevelXfer && bStoreData )
				in_pTransfer->TagAsReady();

		// "Remove" data ref from current transfer and enqueue it in buffer list.
		AddMemView( in_pTransfer, bStoreData );

		m_pCurrentTransfer = NULL;
		m_bTransferCancelled = false;

		bBufferAdded = true;
	}
	
	UpdateTaskStatus( in_eIOResult );

#ifndef AK_OPTIMIZED
	// Tell profiler that it can reset the "active" bit if it doesn't require scheduling anymore.
	m_bCanClearActiveProfile = !m_bRequiresScheduling;
#endif

	return bBufferAdded;
}

// Automatic streams must implement a method that returns the file position after the last
// valid (non cancelled) pending transfer. If there is no transfer pending, then it is the position
// at the end of buffering.
AkUInt64 CAkAutoStmBlocking::GetVirtualFilePosition()
{
	// Must be locked.
	if ( m_pCurrentTransfer )
		return m_pCurrentTransfer->EndPosition();
	else if ( m_listBuffers.Length() > m_uNextToGrant )
		return m_listBuffers.Last()->EndPosition();
	else
		return m_uNextExpectedUserPosition;	
}

// Cancel all pending transfers.
void CAkAutoStmBlocking::CancelAllPendingTransfers()
{
	if ( m_pCurrentTransfer )
		CancelCurrentTransfer();
}

// Cancel all pending transfers that are inconsistent with the next expected position (argument) 
// and looping heuristics.
void CAkAutoStmBlocking::CancelInconsistentPendingTransfers(
	AkUInt64 in_uNextExpectedPosition	// Expected file position of next transfer.
	)
{
	if ( m_pCurrentTransfer 
		&& m_pCurrentTransfer->StartPosition() != in_uNextExpectedPosition )
	{
		CancelCurrentTransfer();
	}
}

void CAkAutoStmBlocking::FlushSmallBuffersAndPendingTransfers( 
	AkUInt32 in_uMinBufferSize 
	)
{
	bool bFlush = false;

	if ( m_listBuffers.Length() > m_uNextToGrant )
    {
		AkUInt32 uIdx = 0;
		AkBufferList::IteratorEx it = m_listBuffers.BeginEx();
		while ( uIdx < m_uNextToGrant )
	    {
			++uIdx;
			++it;
		}

		// Lock scheduler for memory change.
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
			
			while ( it != m_listBuffers.End() )
			{
				if ( bFlush 
					|| (*it)->Size() < in_uMinBufferSize )
				{
					bFlush = true;	// From now on, flush everything.

					CAkStmMemView * pMemView = (*it);
					it = m_listBuffers.Erase( it );
					DestroyBuffer( pMemView );
				}
				else
					++it;
			}
		}
	}

	if ( m_pCurrentTransfer )
	{
		if ( bFlush 
			|| m_pCurrentTransfer->Size() < in_uMinBufferSize )
		{
			CancelCurrentTransfer();
		}
	}
}

AkUInt32 CAkAutoStmBlocking::ReleaseCachingBuffers(AkUInt32 in_uTargetMemToRecover)
{
	AkUInt32 uMemFreed = 0;
	if ( uMemFreed < in_uTargetMemToRecover && m_pCurrentTransfer )
	{
		uMemFreed += m_pCurrentTransfer->Size();
		CancelCurrentTransfer();
	}

	// Try to release more memory from the list of buffers by using the base class implementation
	uMemFreed += CAkAutoStmBase::ReleaseCachingBuffers(in_uTargetMemToRecover-uMemFreed);

	return uMemFreed;
}

// Change loop end heuristic. Use this function instead of setting m_uLoopEnd directly because
// m_uLoopEnd has an impact on the computation of the effective data size (see GetEffectiveViewSize()).
// Implemented in derived classes because virtual buffering has to be recomputed.
void CAkAutoStmBlocking::SetLoopEnd( 
	AkUInt32 in_uLoopEnd	// New loop end value.
	)
{
	m_uLoopEnd = in_uLoopEnd;
	m_uVirtualBufferingSize = ComputeVirtualBuffering();
}

// Helper: compute virtual buffering from scratch.
AkUInt32 CAkAutoStmBlocking::ComputeVirtualBuffering()
{
	AkUInt32 uVirtualBuffering = 0;
	AkUInt32 uNumGranted = m_uNextToGrant;
	{
		AkBufferList::Iterator it = m_listBuffers.Begin();
		while ( it != m_listBuffers.End() )
		{
			if ( uNumGranted == 0 )
				break;
			uNumGranted--;
			++it;
		}
		while ( it != m_listBuffers.End() )
		{
			uVirtualBuffering += GetEffectiveViewSize( (*it) );
			++it;
		}
	}
	if ( m_pCurrentTransfer )
		uVirtualBuffering += GetEffectiveViewSize( m_pCurrentTransfer );
	
	return uVirtualBuffering;
}

// Helper: cancel current transfer.
void CAkAutoStmBlocking::CancelCurrentTransfer()
{
	AKASSERT( m_pCurrentTransfer );
	m_pCurrentTransfer->TagAsCancelled();
	CorrectVirtualBufferingAfterCancel( m_pCurrentTransfer );
	m_pCurrentTransfer = NULL;
	m_bTransferCancelled = true;
}

#ifdef _DEBUG
void CAkAutoStmBlocking::CheckVirtualBufferingConsistency()
{
	AKASSERT( ComputeVirtualBuffering() == m_uVirtualBufferingSize );
}
#endif
