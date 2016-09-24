//////////////////////////////////////////////////////////////////////
//
// AkStmDeferredLinedUpBase.inl
//
// Template layer that adds pending transfer handling services to 
// stream objects that should be used in deferred devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include <AK/Tools/Common/AkAutoLock.h>

using namespace AK;


// CAkDeviceDeferredLinedUpBase
//-----------------------------------------------------------------------------

template <class TStmBase>
CAkStmDeferredLinedUpBase<TStmBase>::CAkStmDeferredLinedUpBase()
{
}

template <class TStmBase>
CAkStmDeferredLinedUpBase<TStmBase>::~CAkStmDeferredLinedUpBase()
{
	AKASSERT(m_listPendingXfers.IsEmpty());
	AKASSERT(m_listCancelledXfers.IsEmpty());
	m_listPendingXfers.Term();
	m_listCancelledXfers.Term();
}

// Override CanBeDestroyed: only if no transfer is pending.
// Sync: Ensure that another thread is not accessing the pending transfers array at the same time.
template <class TStmBase>
bool CAkStmDeferredLinedUpBase<TStmBase>::CanBeDestroyed()
{
	AkAutoLock<CAkLock> pendingXfers( TStmBase::m_lockStatus );

	AKASSERT( m_listPendingXfers.IsEmpty() );	// Should have been cancelled already.

	// Wait until the Low-Level I/O has finished cancelling all I/O transfers.
	return ( m_listCancelledXfers.IsEmpty() );
}

// Update stream object after I/O.
// Sync: Locks stream's status.
// Override Update() to handle completed requests that were not received in the order they were sent.
template <class TStmBase>
bool CAkStmDeferredLinedUpBase<TStmBase>::Update(
	CAkStmMemView *	in_pTransfer,	// Logical transfer object.
	AKRESULT		in_eIOResult,	// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	bool			in_bRequiredLowLevelXfer	// True if this transfer required a call to low-level.
	)
{
	bool bBufferAdded = false;

	// Lock status.
	AkAutoLock<CAkLock> update( TStmBase::m_lockStatus );

	// If I/O was successful and this transfer should not be flushed, check if it is the first PENDING
	// one in the queue. If it isn't, tag it as "completed" but let it there without updating.
	// After updating, always check if the first transfer was tagged as completed in order to resolve it.

	bool bStoreData = (in_pTransfer 
					&& AK_Success == in_eIOResult
					&& in_pTransfer->DoStoreData());

	if ( bStoreData
		&& !IsOldestPendingTransfer( in_pTransfer  ) )
	{
		// Tag it as completed if did not use cache, ready if it did. Resolve later.
		if ( in_bRequiredLowLevelXfer )
			in_pTransfer->TagAsCompleted();
		else 
			in_pTransfer->TagAsReady();

		// Note: do not decrement IO count for transfers completed out-of-order. It still counts 
		// as a transfer that is pending in the Low-Level IO (the latter should have completed them
		// in the correct order anyway). Using this policy, we are able to bound the number of pending
		// transfer (thus to pre-allocate them).
		// IO count is decremented in UpdateCompletedTransfers().
	}
	else
	{
		if ( in_pTransfer )
		{
			AKASSERT( in_pTransfer->Status() != CAkStmMemView::TransferStatus_Completed 
				&& in_pTransfer->Status() != CAkStmMemView::TransferStatus_Ready );

			// Remove transfer object from pending transfers list before updating position (and enqueuing in buffer list).
			PopTransferRequest( in_pTransfer, bStoreData );

			// Transfer may be pending, or cancelled. If it was pending but did not require a low-level transfer,
			// set it to ready now.
			if ( !in_bRequiredLowLevelXfer && (in_pTransfer->Status() != CAkStmMemView::TransferStatus_Cancelled) )
				in_pTransfer->TagAsReady();

			// Enqueue data ref in buffer list.
			TStmBase::AddMemView( in_pTransfer, bStoreData );
	
			// Update all "completed" transfers whose updating was deferred because they did not arrive in the order in which they were sent.
			// Even if in_pTransfer was a canceled transfer, there could be out of order, non-canceled completed transfers in the pending list.
			UpdateCompletedTransfers();

			bBufferAdded = true;
		}

		TStmBase::UpdateTaskStatus( in_eIOResult );

		// Decrement IO count here. See comment in if() above.
		TStmBase::m_pDevice->DecrementIOCount();

#ifndef AK_OPTIMIZED
		// Tell profiler that it can reset the "active" bit if we don't require scheduling anymore and we
		// are not waiting for IO.
		TStmBase::m_bCanClearActiveProfile = !TStmBase::m_bRequiresScheduling && m_listPendingXfers.IsEmpty() && m_listCancelledXfers.IsEmpty();
#endif
	}

	return bBufferAdded;
}

template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::UpdateCompletedTransfers()
{
	// Update while old transfers were completed but not processed yet.
	CAkStmMemView * pOldestTransfer = GetOldestCompletedTransfer();
	while ( pOldestTransfer )
	{
		// Remove transfer object from pending transfers list before updating position (and enqueuing in buffer list).
		PopTransferRequest( pOldestTransfer, true );
		
		// Enqueue in buffer list.
		TStmBase::AddMemView( pOldestTransfer, true );

		// Decrement IO count here. See comment in Update() above when tagging transfers as "completed".
		TStmBase::m_pDevice->DecrementIOCount();

		pOldestTransfer = GetOldestCompletedTransfer();
	}
}

// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
// required transfer size.
template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::PushTransferRequest(
	CAkStmMemViewDeferred *	in_pTransfer	// Logical transfer object.
	)
{
	m_listPendingXfers.AddLast( in_pTransfer );
	in_pTransfer->SetOwner( this );
}

template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::PopTransferRequest(
	CAkStmMemView *	in_pTransfer,	// Logical transfer object to remove.
	bool			in_bStoreData	// True if there was no error.
	)
{
	// Search the cancelled list if it was cancelled. Otherwise, it MUST be the oldest transfer (unless there was an IO error).
	if ( in_pTransfer->Status() != CAkStmMemView::TransferStatus_Cancelled )
	{
		if ( in_bStoreData 
			|| m_listPendingXfers.First() == in_pTransfer )
		{
			AKASSERT( m_listPendingXfers.First() == in_pTransfer );
			m_listPendingXfers.RemoveFirst();
		}
		else
		{
#ifdef AK_ENABLE_ASSERTS
			bool bFound = false;
#endif
			// If there was an error, this transfer could be in any order. Search it.
			AkStmMemViewList::IteratorEx it = m_listPendingXfers.BeginEx();
			while ( it != m_listPendingXfers.End() )
			{
				if ( (*it) == in_pTransfer )
				{
					m_listPendingXfers.Erase( it );
#ifdef AK_ENABLE_ASSERTS
					bFound = true;
#endif
					break;
				}
				++it;
			}
			AKASSERT( bFound || !"Could not find transfer object to dequeue" );
		}
	}
	else
	{
#ifdef AK_ENABLE_ASSERTS
		bool bFound = false;
#endif
		AkStmMemViewListLight::IteratorEx it = m_listCancelledXfers.BeginEx();
		while ( it != m_listCancelledXfers.End() )
		{
			if ( (*it) == in_pTransfer )
			{
				m_listCancelledXfers.Erase( it );
#ifdef AK_ENABLE_ASSERTS
				bFound = true;
#endif
				break;
			}
			++it;
		}
		AKASSERT( bFound || !"Could not find transfer object to dequeue" );
	}
}

// Mark given transfers as cancelled and notify Low-Level IO.
// Stream must be locked.
template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::CancelTransfers(
	AkStmMemViewList & in_listToCancel, // List of transfers to cancel. WARNING: all items will be removed.
	bool in_bNotifyAllCancelled	// "All cancelled" flag to pass to low-level IO. True only if all low-level transfer are truly cancelled.
	)
{
	// Start by moving all transfers in the cancelled list, tagging them as cancelled, 
	// then call Cancel() on the Low-Level IO (the latter could decide to cancel them all at once, 
	// so we must have them tagged appropriately before calling the Low-Level IO).
	// If the transfer was already completed, update now.
	{
		AkStmMemViewList::IteratorEx it = in_listToCancel.BeginEx();
		while ( it != in_listToCancel.End() )
		{
			CAkStmMemView * pTransfer = *it;
			AKASSERT( pTransfer->Status() != CAkStmMemView::TransferStatus_Cancelled
				|| !"Transfer already cancelled" );
			it = in_listToCancel.Erase( it );

			if ( pTransfer->Status() == CAkStmMemView::TransferStatus_Pending )
			{
				AddToCancelledList( pTransfer );
			}
			else
			{
				CancelCompleted( pTransfer );
			}
			
		}
	}

	//In case there were some remaining transfers in the pending list that have been completed out of order.
	UpdateCompletedTransfers();

	{
		bool bCallLowLevelIO = true;
		AkStmMemViewListLight::Iterator it = m_listCancelledXfers.Begin();
		while ( it != m_listCancelledXfers.End() )
		{
			// IMPORTANT: Cache reference before calling CancelTransfer because it could be dequeued 
			// inside this function: The Low-Level IO can call Update() directly from this thread.
			CAkStmMemViewDeferred * pTransfer = (CAkStmMemViewDeferred*)(*it);
			++it;
			// This notifies the Low-Level IO or calls Update directly if transfer was already completed.
			pTransfer->Cancel( 
				static_cast<IAkIOHookDeferred*>( TStmBase::m_pDevice->GetLowLevelHook() ),
				bCallLowLevelIO, 
				in_bNotifyAllCancelled );
			bCallLowLevelIO = !in_bNotifyAllCancelled;
		}
	}
}

// Cancel and destroy a transfer that was already completed (already returned from the Low-Level IO).
// IMPORTANT: the transfer must be dequeued from any list before calling this function.
// IMPORTANT: the transfer will be destroyed therein.
template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::CancelCompleted( 
	CAkStmMemView * in_pTransfer	// Logical transfer object.
	)
{
	AKASSERT( in_pTransfer->Status() == CAkStmMemView::TransferStatus_Completed 
			|| in_pTransfer->Status() == CAkStmMemView::TransferStatus_Ready );

	// Update position with bStoreData = false (resources will be freed if applicable).
	TStmBase::AddMemView( 
		in_pTransfer,
		false );

	TStmBase::m_pDevice->DecrementIOCount();
}

// Returns true if the given transfer object is the oldest PENDING transfer of the queue.
template <class TStmBase>
bool CAkStmDeferredLinedUpBase<TStmBase>::IsOldestPendingTransfer( 
	CAkStmMemView * in_pTransfer	// Logical transfer object.
	)
{
	AKASSERT( in_pTransfer->Status() == CAkStmMemView::TransferStatus_Pending );

	// The oldest transfer is the first transfer in the queue. It cannot be cancelled
	// (would not be in this list) nor completed or ready (would have been processed already).
	AKASSERT( m_listPendingXfers.First()->Status() == CAkStmMemView::TransferStatus_Pending );
	return ( m_listPendingXfers.First() == in_pTransfer );
}

template <class TStmBase>
CAkStmMemView * CAkStmDeferredLinedUpBase<TStmBase>::GetOldestCompletedTransfer()
{
	if ( !m_listPendingXfers.IsEmpty() )
	{
		// The oldest transfer is the first transfer in the queue. 
		// Return it if it is completed (or ready), NULL if it is still pending.
		AKASSERT( m_listPendingXfers.First()->Status() != CAkStmMemView::TransferStatus_Cancelled );
		if ( m_listPendingXfers.First()->Status() != CAkStmMemView::TransferStatus_Pending )
			return m_listPendingXfers.First();
	}
	return NULL;
}

