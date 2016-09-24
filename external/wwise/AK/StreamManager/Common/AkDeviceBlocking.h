//////////////////////////////////////////////////////////////////////
//
// AkDeviceBlocking.h
//
// Win32 specific Blocking Scheduler Device implementation.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_DEVICE_BLOCKING_H_
#define _AK_DEVICE_BLOCKING_H_

#include "AkDeviceBase.h"
#include <AK/Tools/Common/AkAutoLock.h>

namespace AK
{
namespace StreamMgr
{
	// Deferred device specific implementation of a low-level transfer. 
	// Carries the AkIOTransferAsync sent to the Low-Level IO. 
	// Also contains and notifies a list of observers, and handles one-time cancellation notifications in the Low-Level IO.
	class CAkLowLevelTransferBlocking : public CAkLowLevelTransfer
	{
	public:
		
		void Prepare( 
			void * in_pBuffer,
			const AkUInt64 in_uPosition,
			AkUInt32 in_uBufferSize,
			AkUInt32 in_uRequestedSize
			)
		{
			pAddress			= in_pBuffer;
			info.uFilePosition	= in_uPosition;
			info.uBufferSize	= in_uBufferSize;
			info.uRequestedSize = in_uRequestedSize;
		}
		
	public:
		AkIOTransferInfo	info;
		void *				pAddress;
	};



    //-----------------------------------------------------------------------------
    // Name: CAkDeviceBlocking
    // Desc: Implementation of the Blocking Scheduler device.
    //-----------------------------------------------------------------------------
    class CAkDeviceBlocking : public CAkDeviceBase
    {
    public:

        CAkDeviceBlocking( 
			IAkLowLevelIOHook *	in_pLowLevelHook
			);
        virtual ~CAkDeviceBlocking();

		virtual AKRESULT	Init( 
            const AkDeviceSettings &	in_settings,
            AkDeviceID					in_deviceID 
            );

		// IO memory access.
		// -----------------------------------------------------

		// Creates a view to the desired streaming memory, for standard streams.
		// Accepts a memory block, which should map the user-provided memory.
		// A view, pointing to this block, is created and returned. A new low-level transfer is created
		// and attached to it, and is returned via out_pLowLevelXfer.
		// If there was a failure (out of small objects memory), the function returns NULL.
		// Sync: 
		//	- Thread safe. Internally locks device for memory access. 
		//	- Client status should be locked prior to calling this function.
		// Note: If the mem block is already busy, a new one is created temporarily (never happens with blocking device).
		CAkStmMemView * CreateMemViewStd(
			CAkStmTask *	in_pOwner,			// Owner task. 
			AkMemBlock *	in_pMemBlock,		// Memory block for data view. 
			AkUInt32		in_uDataOffset,		// Data view offset from memory block start.
			AkUInt64		in_uPosition,		// Desired position in file.
			AkUInt32 		in_uBufferSize,		// Buffer size. Same as in_uRequestedSize except at EOF.
			AkUInt32 		in_uRequestedSize,	// Requested size.
			CAkLowLevelTransfer *& out_pLowLevelXfer	// Returned low-level transfer if a new one was created and it needs to be pushed to the Low-Level IO. Never NULL unless the function fails.
			);

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
		CAkStmMemView * CreateMemViewAuto(
			CAkStmTask *	in_pOwner,			// Owner task. 
			AkFileID		in_fileID,			// Block's associated file ID.
			AkUInt64		in_uPosition,		// Desired position in file.
			AkUInt32		in_uMinSize,		// Minimum data size acceptable (discard otherwise).
			AkUInt32		in_uRequiredAlign,	// Required data alignment.
			bool			in_bEof,			// True if desired block is last of file.
			bool			in_bCacheOnly,		// Get a view of cached data only, otherwise return NULL.
			AkUInt32 &		io_uRequestedSize,	// In: desired data size; Out: returned valid size.
			CAkLowLevelTransfer *& out_pLowLevelXfer	// Returned low-level transfer if a new one was created and it needs to be pushed to the Low-Level IO. NULL otherwise.
			);

    protected:

		// Stream creation interface.
		// --------------------------------------------------------

		// Standard stream.
		virtual CAkStmTask * _CreateStd(
			AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
			AkOpenMode					in_eOpenMode,       // Open mode (read, write, ...).
			IAkStdStream *&				out_pStream         // Returned interface to a standard stream.
			);

		// Automatic stream
		virtual CAkStmTask * _CreateAuto(
			AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
			AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
			const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
			AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
			IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
			);

		virtual CAkStmMemView * MemViewFactory()
		{
			return AkNew( CAkStreamMgr::GetObjPoolID(), CAkStmMemView(true) );
		}

        // This device's implementation of PerformIO().
        virtual void PerformIO();

        // Execute task chosen by scheduler.
        void ExecuteTask( 
            CAkStmTask *	in_pTask,				// Task selected for execution.
			AkReal32		in_fOpDeadline			// Operation deadline.
            );

		// Sets members of the low-level transfer object for data transfer into the low-level IO.
		// Converts position (relative to start of file) into low-level position (absolute).
		// Sync: None required since only the IO thread prepares and disposes of the one and only low-level transfer.
		inline CAkLowLevelTransfer * PrepareLowLevelTransfer( 
			CAkStmTask * in_pOwner,		// Owner task.
			void * in_pBuffer,			// Address for transfer.
			AkUInt64 in_uPosition,		// Position in file (relative to start of file).
			AkUInt32 in_uBufferSize,	// Buffer size.
			AkUInt32 in_uRequestedSize	// Requested transfer size.
			)
		{
			m_currentTransfer.Prepare( 
				in_pBuffer,
				in_uPosition + in_pOwner->GetFileOffset(),
				in_uBufferSize,
				in_uRequestedSize
				);
			return &m_currentTransfer;
		}

		// Notify device when a low-level transfer completes.
		// Pass in (one of) the views that owns the block that owns it. 
		// The block is cleared out of its low-level transfer,
		// and is untagged if there has been an error.
		// Sync: Device lock is required because both the IO thread and client threads
		// may access the mem block. 
		inline void OnLowLevelTransferComplete( 
			CAkStmMemView * in_pOwnerView,
			AKRESULT in_eResult,
			bool 
#ifndef AK_OPTIMIZED
				in_bAddToStats
#endif
			)
		{
			AkAutoLock<CAkIOThread> lock( *this );

			AKASSERT( in_pOwnerView );
			AkMemBlock * pMemBlock = in_pOwnerView->Block();
			AKASSERT( pMemBlock->pTransfer );
			// Clear block's low-level transfer.
			pMemBlock->pTransfer = NULL;
			if ( in_eResult != AK_Success )
			{
				// Failed. Block metadata needs to be invalidated.
				// Avoid calling the memory manager if the file ID was not even set. 
				// Standard streams don't set it, and their mem block does not belong to the memory manager.
				if ( pMemBlock->IsTagged() )
					m_mgrMemIO.UntagBlock( pMemBlock );
			}
		}

	protected:
		CAkLowLevelTransferBlocking		m_currentTransfer;	// The one and only current transfer.
    };

	//-----------------------------------------------------------------------------
    // Name: class CAkStdStmBlocking
    // Desc: Standard stream implementation.
    //-----------------------------------------------------------------------------
    class CAkStdStmBlocking : public CAkStdStmBase
    {
	public:
		 CAkStdStmBlocking();
		 virtual ~CAkStdStmBlocking();

	protected:

		// User interface.
		// ---------------------------------

		// Closes stream. The object is destroyed and the interface becomes invalid.
        virtual void Destroy();

		// Cancel.
        virtual void Cancel();


		// Task interface.
		// ---------------------------------

		// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
		// Blocking standard streams may be destroyed if and only if they are not currently waiting for the I/O thread
		// to complete a transfer, cancelled or not.
		virtual bool CanBeDestroyed();

		// Get information for data transfer.
		// Returns the (device-specific) streaming memory view containing logical transfer information. 
		// NULL if preparing transfer has aborted.
		// out_pLowLevelXfer is set if and only if the transfer requires a transfer in the low-level IO.
		// Sync: Locks stream's status.
        virtual CAkStmMemView * PrepareTransfer( 
			AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
			CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
			bool &					out_bExistingLowLevelXfer, // Always false on the blocking device.
			bool					in_bCacheOnly		// Prepare transfer only if data is found in cache. out_pLowLevelXfer will be NULL.
			);

		// Update stream object after I/O.
		// Sync: Locks stream's status.
		virtual bool Update(
			CAkStmMemView *	in_pTransfer,	// Logical transfer object.
			AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
			bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
			);

	protected:
		
		// Current logical transfer. Reference to a memory view on which the (one and only) transfer is occurring.
		// NULL when the device's current transfer does not concern this stream.
		CAkStmMemView *		m_pCurrentTransfer;

		// Cancelled transfer management: m_bTransferCancelling is true while waiting for current transfer to complete
		// after a call to Cancel() or Destroy(). This is required in order to prevent this stream from being
		// destroyed (see CanBeDestroyed()).
		bool				m_bTransferCancelling;
	};
		
		
	//-----------------------------------------------------------------------------
    // Name: class CAkAutoStmBlocking
    // Desc: Automatic stream implementation.
    //-----------------------------------------------------------------------------
    class CAkAutoStmBlocking : public CAkAutoStmBase
    {
	public:
		CAkAutoStmBlocking();
		virtual ~CAkAutoStmBlocking();

		// Task interface.
		// ---------------------------------
		
		// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
		// Blocking automatic streams may be destroyed if and only if they are not currently waiting for the I/O thread
		// to complete a transfer, cancelled or not.
		virtual bool CanBeDestroyed();

		// Get information for data transfer.
		// Returns the (device-specific) streaming memory view containing logical transfer information. 
		// NULL if preparing transfer has aborted.
		// out_pLowLevelXfer is set if and only if the transfer requires a transfer in the low-level IO.
		// Sync: Locks stream's status.
        virtual CAkStmMemView * PrepareTransfer( 
			AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
			CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
			bool &					out_bExistingLowLevelXfer, // Always false for std streams.
			bool					in_bCacheOnly		// Prepare transfer only if data is found in cache. out_pLowLevelXfer will be NULL.
			);

		// Update stream object after I/O.
		// Sync: Locks stream's status.
		virtual bool Update(
			CAkStmMemView *	in_pTransfer,	// Logical transfer object.
			AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
			bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
			);

	protected:
		
        // Returns the file position after the one and only valid (non cancelled) pending transfer. 
		// If there is no transfer pending, then it is the position at the end of buffering.
		virtual AkUInt64 GetVirtualFilePosition();
		
		// Cancel all pending transfers.
		virtual void CancelAllPendingTransfers();

		// For use with caching streams.  Try to recover up to in_uTargetMemToRecover bytes by freeing data starting
		//	with the most recent pending transfer and working backwards to the beginning of the file.
		virtual AkUInt32 ReleaseCachingBuffers(AkUInt32 in_uTargetMemToRecover);

		// Cancel all pending transfers that are inconsistent with the next expected position (argument) 
		// and looping heuristics.
		virtual void CancelInconsistentPendingTransfers(
			AkUInt64 in_uNextExpectedPosition	// Expected file position of next transfer.
			);

		// Run through buffers (completed and pending) and check their data size. If it is smaller than 
		// in_uMinBufferSize, flush it and flush everything that comes after.
		virtual void FlushSmallBuffersAndPendingTransfers( 
			AkUInt32 in_uMinBufferSize // Minimum buffer size.
			);

		// Change loop end heuristic. Use this function instead of setting m_uLoopEnd directly because
		// m_uLoopEnd has an impact on the computation of the effective data size (see GetEffectiveViewSize()).
		// Implemented in derived classes because virtual buffering has to be recomputed.
		virtual void SetLoopEnd( 
			AkUInt32 in_uLoopEnd	// New loop end value.
			);

		// Helper: compute virtual buffering from scratch.
		AkUInt32 ComputeVirtualBuffering();

		// Helper: cancel current transfer.
		void CancelCurrentTransfer();

#ifdef _DEBUG
		virtual void CheckVirtualBufferingConsistency();
#endif

	protected:

		// Current logical transfer. Reference to a memory view on which the (one and only) transfer is occurring.
		// NULL when the device's current transfer does not concern this stream, or if it has been cancelled.
		CAkStmMemView *		m_pCurrentTransfer;

		// Cancelled transfer management: m_bTransferCancelled is true when current transfer was cancelled. 
		// m_pCurrentTransfer has been NULL. m_bTransferCancelled is reset in Update().
		bool				m_bTransferCancelled;
    };
}
}
#endif //_AK_DEVICE_BLOCKING_H_
