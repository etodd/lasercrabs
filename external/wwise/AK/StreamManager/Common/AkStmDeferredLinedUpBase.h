//////////////////////////////////////////////////////////////////////
//
// AkStmDeferredLinedUpBase.h
//
// Template layer that adds pending transfer handling services to 
// stream objects that should be used in deferred devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_STM_DEFERRED_LINEDUP_H_
#define _AK_STM_DEFERRED_LINEDUP_H_

#include "AkDeviceBase.h"
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include "AkTransferDeferred.h"

namespace AK
{
namespace StreamMgr
{
	//-----------------------------------------------------------------------------
    // Name: CAkDeviceDeferredLinedUpBase
    // Desc: Base implementation of the deferred lined-up scheduler.
    //-----------------------------------------------------------------------------
    class CAkDeviceDeferredLinedUpBase : public CAkDeviceBase
    {
    public:

		CAkDeviceDeferredLinedUpBase( IAkLowLevelIOHook * in_pLowLevelHook )
			:CAkDeviceBase( in_pLowLevelHook ) {}
		virtual ~CAkDeviceDeferredLinedUpBase() {}
	};


	//-----------------------------------------------------------------------------
    // Name: CAkStmDeferredLinedUpBase
    // Desc: Deferred device specific services for all kinds of stream objects.
    //-----------------------------------------------------------------------------
	template <class TStmBase>
	class CAkStmDeferredLinedUpBase : public TStmBase
	{
	public:

		// Override CanBeDestroyed: only if no transfer is pending.
		virtual bool CanBeDestroyed();

		// Update stream object after I/O.
		// Sync: Locks stream's status.
		// Handles completed requests that were not received in the order they were sent.
        virtual bool Update(
			CAkStmMemView *	in_pTransfer,	// Logical transfer object.
			AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
			bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
			);
		
		// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
		// required transfer size.
		// Called by the device to ensure proper synchronization.
		void PushTransferRequest(
			CAkStmMemViewDeferred *	in_pTransfer	// Logical transfer object.
			);

	protected:

		CAkStmDeferredLinedUpBase();
		virtual ~CAkStmDeferredLinedUpBase();

		// Updates all "completed" transfers whose updating was deferred because they did not arrive in
		// the order in which they were sent.
		void UpdateCompletedTransfers();

		// Removes transfer request that just completed. Need to pass the position and size of the 
		// transfer (after processing and validation) in order to correct the virtual position.
		void PopTransferRequest(
			CAkStmMemView *	in_pTransfer,			// Logical transfer object to remove.
			bool			in_bStoreData			// True if there was no error.
            );
		
		// Mark given transfers as cancelled and notify Low-Level IO.
		// Stream must be locked.
		void CancelTransfers(
			AkStmMemViewList &	in_listToCancel,		// List of transfers to cancel. WARNING: all items will be removed.
			bool				in_bNotifyAllCancelled	// "All cancelled" flag to pass to low-level IO. True only if all low-level transfer are truly cancelled.
			);

		// Cancel and destroy a transfer that was already completed (already returned from the Low-Level IO).
		// IMPORTANT: the transfer must be dequeued from any list before calling this function.
		// IMPORTANT: the transfer will be destroyed therein.
		void CancelCompleted( 
			CAkStmMemView * in_pTransfer	// Logical transfer object.
			);
			
		// Returns true if the given transfer object is the first PENDING transfer of the queue.
		bool IsOldestPendingTransfer( 
			CAkStmMemView * in_pTransfer	// Logical transfer object.
			);
		// Returns the oldest completed transfer in the queue.
		// Cancelled transfers are skipped. If the first transfer that is not cancelled is still pending,
		// or if there are no completed transfers, it returns NULL.
		CAkStmMemView * GetOldestCompletedTransfer();

		// Push a transfer in the list of cancelled transfers.
		// Sets transfer's status to 'cancelled'.
		inline void AddToCancelledList(
			CAkStmMemView * in_pTransfer	// Logical transfer object
			)
		{
			in_pTransfer->TagAsCancelled();
			TStmBase::CorrectVirtualBufferingAfterCancel( in_pTransfer );
			m_listCancelledXfers.AddFirst( in_pTransfer );
		}

	protected:
		// IMPORTANT: Transfers are enqueued by the head: the latest transfer is the first of the queue.
        AkStmMemViewList	m_listPendingXfers;	// List of pending transfers.

		// IMPORTANT: Both lists use the same "next transfer" member, because a transfer can only be 
		// in one of these lists at a time. The same is true with CAkAutoStmBase::m_listBuffers.
		AkStmMemViewListLight m_listCancelledXfers;	// List of cancelled transfers.
	};

#include "AkStmDeferredLinedUpBase.inl"
}
}
#endif	//_AK_STM_DEFERRED_LINEDUP_H_
