//////////////////////////////////////////////////////////////////////
//
// AkDeviceDeferredLinedUp.h
//
// Win32 Deferred Scheduler Device implementation.
// Requests to low-level are sent in a lined-up fashion.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_DEVICE_DEFERRED_LINEDUP_H_
#define _AK_DEVICE_DEFERRED_LINEDUP_H_

#include "AkDeviceBase.h"
#include "AkStmDeferredLinedUpBase.h"

namespace AK
{
namespace StreamMgr
{
	class CAkStdStmDeferredLinedUp;
	class CAkAutoStmDeferredLinedUp;

    //-----------------------------------------------------------------------------
    // Name: CAkDeviceDeferredLinedUp
    // Desc: Implementation of the deferred lined-up scheduler.
    //-----------------------------------------------------------------------------
    class CAkDeviceDeferredLinedUp : public CAkDeviceDeferredLinedUpBase
    {
    public:

        CAkDeviceDeferredLinedUp( 
			IAkLowLevelIOHook *	in_pLowLevelHook 
			);
        virtual ~CAkDeviceDeferredLinedUp();

		virtual AKRESULT	Init( 
            const AkDeviceSettings &	in_settings,
            AkDeviceID					in_deviceID 
            );
		virtual void		Destroy();

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
		// Note: If the mem block is already busy, a new one is created temporarily.
		CAkStmMemViewDeferred * CreateMemViewStd(
			CAkStdStmDeferredLinedUp *	in_pOwner,	// Owner task (standard stream).
			AkMemBlock *	in_pMemBlock,		// Memory block for data view. 
			AkUInt32		in_uDataOffset,		// Data view offset from memory block start.
			AkUInt64		in_uPosition,		// Desired position in file.
			AkUInt32 		in_uBufferSize,		// Buffer size. Same as in_uRequestedSize except at EOF.
			AkUInt32 		in_uRequestedSize,	// Requested size.
			CAkLowLevelTransferDeferred *& out_pLowLevelXfer	// Returned low-level transfer.
			);

		// Creates a view to the desired streaming memory, for automatic streams.
		// Searches a memory block for IO. If available, tries to get a buffer with cached data. 
		// Otherwise, returns a new buffer for IO. 
		// If a block is found, a view, pointing to this block, is created and returned.
		// If the block requires a transfer, a new low-level transfer is created and attached to it. 
		// If it is already busy, a low-level transfer is already attached to it. The returned memory view
		// is added as an observer of that transfer.
		// If the transfer needs to be pushed to the Low-Level IO, it is returned via out_pLowLevelXfer.
		// If a block was not found, the function returns NULL.
		// Sync: 
		//	- Thread safe. Internally locks device for memory access. Notifies memory full if applicable, atomically.
		//	- Client status should be locked prior to calling this function.
		CAkStmMemViewDeferred * CreateMemViewAuto(
			CAkAutoStmDeferredLinedUp *	in_pOwner,	// Owner task (automatic stream).
			AkFileID		in_fileID,			// Block's associated file ID.
			AkUInt64		in_uPosition,		// Desired position in file.
			AkUInt32		in_uMinSize,		// Minimum data size acceptable (discard otherwise).
			AkUInt32		in_uRequiredAlign,	// Required data alignment.
			bool			in_bEof,			// True if desired block is last of file.
			bool			in_bCacheOnly,		// Get a view of cached data only, otherwise return NULL.
			AkUInt32 &		io_uRequestedSize,	// In: desired data size; Out: returned valid size.
			CAkLowLevelTransferDeferred *& out_pNewLowLevelXfer,	// Returned low-level transfer if it was created. Device must push it to the Low-Level IO. NULL otherwise.
			bool & out_bExistingLowLevelXfer // Set to true if a low level transfer already exists for the memory block that is referenced by the returned view.
			);


		// Notify device when a low-level transfer completes.
		// Pass in (one of) the views that owns the block that owns it. The block is cleared out of its 
		// low-level transfer and the latter is disposed of.
		// The block is untagged if there has been an error.
		// Sync: Device must be locked prior to calling this function.
		inline void OnLowLevelTransferComplete( 
			CAkStmMemView * in_pOwnerView,
			AKRESULT in_eResult,
			bool 
#ifndef AK_OPTIMIZED
				in_bAddToStats
#endif
			)
		{
			AKASSERT( in_pOwnerView );
			AkMemBlock * pMemBlock = in_pOwnerView->Block();
			AKASSERT( pMemBlock->pTransfer );
			DestroyLowLevelTransfer( (CAkLowLevelTransferDeferred*)pMemBlock->pTransfer );
			pMemBlock->pTransfer = NULL;
			if ( in_eResult != AK_Success )
			{
				// Failed. Block metadata needs to be invalidated.
				// Avoid calling the memory manager if the block was not tagged.
				// - Standard streams don't set it, and their mem block does not belong to the memory manager.
				// - Mem blocks are untagged immediately when their low-level transfer is cancelled.
				if ( pMemBlock->IsTagged() )
					m_mgrMemIO.UntagBlock( pMemBlock );
			}
		}

		// Notify device when a low-level transfer is cancelled.
		// Pass in the block that owns it. The block is cleared out of its low-level transfer,
		// and it is untagged immediately.
		// Sync: Device must be locked prior to calling this function.
		inline void OnLowLevelTransferCancelled( 
			AkMemBlock * in_pMemBlock
			)
		{
			AKASSERT( in_pMemBlock->pTransfer );
			// Avoid calling the memory manager if the block was not tagged. 
			// - Standard streams don't set it, and their mem block does not belong to the memory manager.
			if ( in_pMemBlock->IsTagged() )
				m_mgrMemIO.UntagBlock( in_pMemBlock );

			// Profiling.
#ifndef AK_OPTIMIZED
			m_uNumLowLevelRequestsCancelled++;
#endif
		}

    protected:
        
        // This device's implementation of PerformIO().
        virtual void PerformIO( );

        // Execute task chosen by scheduler.
        void ExecuteTask( 
            CAkStmTask *	in_pTask,				// Task selected for execution.
			AkReal32		in_fOpDeadline			// Operation deadline.
            );

		// Creates a low-level transfer object and sets its fields for data transfer into the low-level IO.
		// Converts position (relative to start of file) into low-level position (absolute).
		inline CAkLowLevelTransferDeferred * CreateLowLevelTransfer( 
			CAkStmTask * in_pOwner,		// Owner task.
			void * pBuffer,				// Address for transfer.
			AkUInt64 in_uPosition,		// Position in file, relative to start of file.
			AkUInt32 in_uBufferSize,	// Buffer size.
			AkUInt32 in_uRequestedSize	// Requested transfer size.
			)
		{
			CAkLowLevelTransferDeferred * pXfer = (CAkLowLevelTransferDeferred*)m_poolLowLevelTransfers.First();
			AKASSERT( pXfer || !"Not enough cached transfer objects" );
			m_poolLowLevelTransfers.RemoveFirst();
			pXfer->Prepare( 
				in_pOwner, 
				pBuffer, 
				in_uPosition + in_pOwner->GetFileOffset(), 
				in_uBufferSize, 
				in_uRequestedSize 
				);
			return pXfer;
		}

		// Sync: Device must be locked when calling this function.
		inline void DestroyLowLevelTransfer( CAkLowLevelTransferDeferred * in_pTransfer )
		{
			AKASSERT( !in_pTransfer->HasObservers() );
			in_pTransfer->Clear();	// Clear owner, to improve chances of crashing when low-level IO calls back twice.
			m_poolLowLevelTransfers.AddLast( in_pTransfer );
		}

	protected:

		// Stream creation interface override
		// (because we need to initialize specialized stream objects).
		// ---------------------------------------------------------------
		// Standard stream.
		virtual CAkStmTask * _CreateStd(
			AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
			AkOpenMode					in_eOpenMode,       // Open mode (read, write, ...).
			IAkStdStream *&				out_pStream         // Returned interface to a standard stream.
			);
		// Automatic stream.
		virtual CAkStmTask * _CreateAuto(
			AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
			AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
			const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
			AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
			IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
			);

		virtual CAkStmMemView * MemViewFactory()
		{
			return AkNew( CAkStreamMgr::GetObjPoolID(), CAkStmMemViewDeferred(true) );
		}

	private:

		// Low-Level transfers pool.
		// Next item policy for bare lists.
		template <class T>
		struct AkListBareNextLowLevelTransfer
		{
			static AkForceInline T *& Get( T * in_pItem ) 
			{
				return in_pItem->pNextTransfer;
			}
		};
		typedef AkListBare<CAkLowLevelTransferDeferred, AkListBareNextLowLevelTransfer,AkCountPolicyWithCount,AkLastPolicyWithLast> AkLowLevelTransfersPool;
		AkLowLevelTransfersPool			m_poolLowLevelTransfers;
		CAkLowLevelTransferDeferred	*	m_pLowLevelTransfersMem;
    };

    //-----------------------------------------------------------------------------
    // Stream objects specificity.
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    // Name: class CAkStdStmDeferredLinedUp
    // Desc: Overrides methods for deferred lined-up device specificities.
    //-----------------------------------------------------------------------------
    class CAkStdStmDeferredLinedUp : public CAkStmDeferredLinedUpBase<CAkStdStmBase>
    {
    public:

        CAkStdStmDeferredLinedUp();
        virtual ~CAkStdStmDeferredLinedUp();

		// Cancel.
        virtual void Cancel();

		// Override GetStatus(): return "pending" if we are in fact Idle, but transfers are pending.
        virtual AkStmStatus GetStatus();        // Get operation status.

		// Closes stream. The object is destroyed and the interface becomes invalid.
        virtual void	Destroy();
		
		// Get information for data transfer.
		// Returns the (device-specific) streaming memory view containing logical transfer information. 
		// NULL if preparing transfer has aborted.
		// out_pLowLevelXfer is set if and only if the transfer requires a transfer in the low-level IO.
		// Sync: Locks stream's status.
		virtual CAkStmMemView * PrepareTransfer( 
			AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
			CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
			bool &					out_bExistingLowLevelXfer, // Indicates a low level transfer already exists that references the memory block returned.
			bool					in_bCacheOnly		// Prepare transfer only if data is found in cache. out_pLowLevelXfer will be NULL.
			);

		// Helper: Cancel all pending transfers.
		// Stream must be locked.
		void _CancelAllPendingTransfers();
    };

    //-----------------------------------------------------------------------------
    // Name: class CAkAutoStmDeferredLinedUp
    // Desc: Base automatic stream implementation.
    //-----------------------------------------------------------------------------
    class CAkAutoStmDeferredLinedUp : public CAkStmDeferredLinedUpBase<CAkAutoStmBase>
    {
    public:

        CAkAutoStmDeferredLinedUp();
        virtual ~CAkAutoStmDeferredLinedUp();
		
		// Get information for data transfer.
		// Returns the (device-specific) transfer object containing transfer information. 
		// NULL if the transfer should be cancelled.
		virtual CAkStmMemView * PrepareTransfer( 
			AkFileDesc *&			out_pFileDesc,		// Stream's associated file descriptor.
			CAkLowLevelTransfer *&	out_pLowLevelXfer,	// Low-level transfer. Set to NULL if it doesn't need to be pushed to the Low-Level IO.
			bool				&	out_bExistingLowLevelXfer, // Indicates a low level transfer already exists that references the memory block returned.
			bool					in_bCacheOnly		// Prepare transfer only if data is found in cache. out_pLowLevelXfer will be NULL.
			);

	protected:

		// CAkAutoStm-specific implementation.
		// -------------------------------------

		// Automatic streams must implement a method that returns the file position after the last
		// valid (non cancelled) pending transfer. If there is no transfer pending, then it is the position
		// at the end of buffering.
		virtual AkUInt64 GetVirtualFilePosition();

		// Cancel all pending transfers.
		// Stream must be locked.
		virtual void CancelAllPendingTransfers();

		// For use with caching streams.  Try to recover up to in_uTargetMemToRecover bytes by freeing data starting
		//	with the most recent pending transfer and working backwards to the beginning of the file.
		virtual AkUInt32 ReleaseCachingBuffers(AkUInt32 in_uTargetMemToRecover);

		// Cancel all pending transfers that are inconsistent with the next expected position (argument) 
		// and looping heuristics. 
		// Stream must be locked.
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

#ifdef _DEBUG
		virtual void CheckVirtualBufferingConsistency();
#endif
    };
}
}
#endif //_AK_DEVICE_DEFERRED_LINEDUP_H_
