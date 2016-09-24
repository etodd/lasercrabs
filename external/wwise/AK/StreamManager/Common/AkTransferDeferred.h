//////////////////////////////////////////////////////////////////////
//
// AkTransferDeferred.h
//
// Transfer object used by the deferred lined-up device.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_TRANSFER_DEFERRED_H_
#define _AK_TRANSFER_DEFERRED_H_

#include "AkIOMemMgr.h"
#include "AkStmMemView.h"

namespace AK
{
namespace StreamMgr
{
	class CAkStmTask;

	//-----------------------------------------------------------------------------
    // Name: CAkStmMemViewDeferred
    // Desc: Extends CAkStmMemView specifically for deferred devices in order to be 
	// able to be linked in an observers list (observed by a low-level transfer).
	//-----------------------------------------------------------------------------
	class CAkStmMemViewDeferred : public CAkStmMemView
	{
	public:
		CAkStmMemViewDeferred( bool in_bCachedAlloc ) 
			: CAkStmMemView( in_bCachedAlloc )
			, pNextObserver( NULL )
			, m_pOwner( NULL ) {}

		// Update a memory view when transfer is complete (updates its owner task).
		void Update( AKRESULT in_eResult, bool in_bRequiredLowLevelXfer );

		// Cancel a pending memory view. Handshaking is performed with low-level transfer
		// in order to determine whether IAkIOHookDeferred::Cancel() will be called.
		// If the decision is taken that Cancel() should be called, the view's memory block is untagged 
		// atomically to avoid having other tasks trying to attach an observer to it.
		// Sync: Owner task must be locked prior to callign this function, device is locked inside.
		void Cancel(
			IAkIOHookDeferred * in_pLowLevelHook, 
			bool in_bCallLowLevelIO, 
			bool in_bAllCancelled 
			);

		CAkStmTask * GetOwner() { return m_pOwner; }
		void SetOwner( CAkStmTask * in_pOwner ) { m_pOwner = in_pOwner; }
	
	public:
		CAkStmMemViewDeferred * pNextObserver;	// List bare light policy for observers list.

	protected:
		CAkStmTask * m_pOwner;	// Task that owns this memory view. Required in order to resolve transfers.
	};

	// List bare light policy for observers list.
	template <class T>
	struct AkListBareNextObserver
	{
		static AkForceInline T *& Get( T * in_pItem ) 
		{
			return in_pItem->pNextObserver;
		}
	};
	typedef AkListBareLight<CAkStmMemViewDeferred,AkListBareNextObserver>	AkStmMemObserversList;

	
	//-----------------------------------------------------------------------------
    // Name: CAkLowLevelTransferDeferred
	// Desc: Deferred device specific implementation of a low-level transfer. 
	// Carries the AkIOTransferAsync sent to the Low-Level IO. 
	// Also contains and notifies a list of observers, and handles one-time cancellation notifications in the Low-Level IO.
	// Sync: Factory (Prepare()), and access to observers must always be done inside device lock.
	//-----------------------------------------------------------------------------
	class CAkLowLevelTransferDeferred : public CAkLowLevelTransfer
	{
	public:

		CAkLowLevelTransferDeferred()
			:pNextTransfer( NULL )
		{}

		// Entry point for Low-Level IO transfer completion.
		static void LLIOCallback( 
			AkAsyncIOTransferInfo * in_pTransferInfo,	// Pointer to the AkAsyncIOTransferInfo structure that was passed to corresponding Read() or Write() call.
			AKRESULT		in_eResult			// Result of transfer: AK_Success or AK_Fail (stream becomes invalid).
			);

		
		// Sync: device must be locked when calling this function.		
		inline void Prepare( 
			CAkStmTask * in_pOwner, 
			void * in_pBuffer,
			const AkUInt64 in_uPosition,
			AkUInt32 in_uBufferSize,
			AkUInt32 in_uRequestedSize
			)
		{
			info.pBuffer		= in_pBuffer;
			info.uFilePosition	= in_uPosition;
			info.uBufferSize	= in_uBufferSize;
			info.uRequestedSize = in_uRequestedSize;
			info.pCallback		= LLIOCallback;
			info.pCookie		= this;	// Keep transfer object in cookie.
			info.pUserData		= NULL;

			m_pOwner			= in_pOwner;
			m_bWasLLIOCancelCalled = false;
			m_bWasSentToLLIO	= false;

			AKASSERT( m_observers.First() == NULL );
		}

		// Sync: Device must be locked when calling this function.
		inline bool IsValid() { return (m_pOwner != NULL); }
		
		// Sync: Device must be locked when calling this function.
		inline void Clear() { m_pOwner = NULL; }

		// Sync: Device must be locked when calling this function.
		void AddObserver( CAkStmMemViewDeferred * in_pObserver )
		{
			m_observers.AddFirst( in_pObserver );
		}
		inline bool HasObservers()
		{
			return ( m_observers.First() != NULL );
		}

		// Execute transfer (read or write). Low-level call is skipped if it is not the first time.
		// Returns result from low-level IO, AK_Success if was skipped.
		AKRESULT Execute(
			IAkIOHookDeferred * in_pLowLevelHook,
			AkFileDesc *		in_pFileDesc, 
			AkIoHeuristics &	in_heuristics,
			bool				in_bWriteOp
			);

		// Cancel a transfer: calls Cancel() on the Low-Level IO if required to do so.
		// Observers who call this function must set their status to cancelled first.
		// Sync: The owner task must be locked.
		void Cancel( 
			IAkIOHookDeferred * in_pLowLevelHook, 
			bool in_bCallLowLevelIO, 
			bool & io_bAllCancelled 
			);

		// Ask the low-level transfer if it can be cancelled. It can if and only if 
		// there is only one observer to this transfer, and it is the caller.
		// Sync: Owner task must be locked, device must be locked.
		bool CanCancel();

	protected:
		// Called from LL IO completion static callback.
		// Pops all observers, destroys itself, then notifies all cached observers that transfer is complete.
		// Sync: None. Locks device to safely pop observers.
		void Update(
			AKRESULT		in_eResult			// Result of transfer: AK_Success or AK_Fail (stream becomes invalid).
			);

	public:
		CAkLowLevelTransferDeferred *	pNextTransfer;	// Pointer to next transfer for pooling.
		AkAsyncIOTransferInfo	info;			// Asynchronous transfer info.
	private:
		AkStmMemObserversList	m_observers;	// List of observers. They are notified when this transfer completes.
		CAkStmTask *	m_pOwner;					// Owner task (in order to call Low-Level Cancel() with proper file desc).
		AkUInt32		m_bWasLLIOCancelCalled	:1;	// This bit is set when IAkIOHookDeferred::Cancel() is called (to avoid calling it more than once).
		AkUInt32		m_bWasSentToLLIO		:1;	// This bit is set when IAkIOHookDeferred::Read/Write() is called (to avoid calling it more than once).
	};

}
}
#endif // _AK_TRANSFER_DEFERRED_H_

