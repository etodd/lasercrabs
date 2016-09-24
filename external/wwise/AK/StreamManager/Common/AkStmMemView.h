//////////////////////////////////////////////////////////////////////
//
// AkStmMemView.h
//
// IO memory management.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_STM_MEM_VIEW_H_
#define _AK_STM_MEM_VIEW_H_

#include "AkIOMemMgr.h"

// ------------------------------------------------------------------------------
// Defines.
// ------------------------------------------------------------------------------


namespace AK
{
namespace StreamMgr
{
	// ------------------------------------------------------------------------------
    // Stream memory view. 
	// It is a view to a memory block (IO pool or user-provided).
	// May also be considered as a logical IO transfer, as well as a holder for memory 
	// blocks inside streams.
    // ------------------------------------------------------------------------------
	class CAkStmMemView 
    {
		friend class CAkDeviceBase;
		friend class CAkDeviceBlocking;
		friend class CAkDeviceDeferredLinedUp;
	public:

		// Mem view status. When it is not "ready", a mem view is a logical data transfer, from the point
		// of view of stream objects.
		enum TransferStatusType
		{
			TransferStatus_Pending,
			TransferStatus_Completed,
			TransferStatus_Cancelled,
			TransferStatus_Ready
		};

		CAkStmMemView( bool in_bCachedAlloc )
			: pNextView( NULL )
			, m_pBlock( NULL )
			, m_uOffsetInBlock( 0 )
			, m_eStatus( TransferStatus_Ready )
			, m_bCachedAlloc( in_bCachedAlloc ) 
		{
		}
		~CAkStmMemView() {}

		// Access to data.
		//
		inline void * Buffer() { return (AkUInt8*)(m_pBlock->pData) + m_uOffsetInBlock; }
		inline AkUInt32 Size() { AKASSERT( m_pBlock->uAvailableSize >= m_uOffsetInBlock ); return m_pBlock->uAvailableSize - m_uOffsetInBlock; }
		inline AkUInt32 AllocSize() { return m_pBlock->uAllocSize; }
		inline AkUInt64 StartPosition() { return m_pBlock->uPosition + m_uOffsetInBlock; }
		inline AkUInt64 EndPosition() { return m_pBlock->uPosition + m_pBlock->uAvailableSize; }

		inline void ClearSize() { AKASSERT( m_pBlock ); m_uOffsetInBlock = m_pBlock->uAvailableSize; }

		// Status management.
		// 
		// Returns true if the transfer's data can be added to the stream, in CAkStmTask::Update(). 
		// If it returns false, the data is flushed. 
		inline bool DoStoreData() { return m_eStatus != TransferStatus_Cancelled; }
		inline TransferStatusType Status() { return m_eStatus; }
		// Set transfer status to 'cancelled'. Call this method before calling Cancel().
		inline void TagAsCancelled() { AKASSERT ( TransferStatus_Pending == m_eStatus ); m_eStatus = TransferStatus_Cancelled; }
		// Tag pending transfers as completed if they completed out of order.
		// Do not tag "ready" transfers as "completed". "Ready" transfer are already completed.
		inline void TagAsCompleted() { AKASSERT( TransferStatus_Pending == m_eStatus ); m_eStatus = TransferStatus_Completed; }
		inline void TagAsReady() { AKASSERT( m_eStatus != TransferStatus_Cancelled ); m_eStatus = TransferStatus_Ready; }

		inline bool IsCachedAlloc() { return m_bCachedAlloc; }
		
		// Attach/detach a memory block to this view. The caller is also responsible for incrementing 
		// the memory block refcount.
		// Sync: must be called within device lock, atomically with block acquisition.
		void Attach( AkMemBlock * in_pBlock, AkUInt32 in_uOffsetInBlock )
		{
			m_pBlock = in_pBlock;
			m_uOffsetInBlock = in_uOffsetInBlock;
			m_eStatus = TransferStatus_Pending;
		}
		// Returns and loses ownership of its memory block.
		// Sync: must be called within device lock.
		AkMemBlock * Detach()
		{
			AkMemBlock * pBlock = m_pBlock;
			m_pBlock = NULL;
			return pBlock;
		}

	protected:
		AkMemBlock * Block() { return m_pBlock; }
		CAkLowLevelTransfer * GetLowLevelTransfer() { AKASSERT( m_pBlock ); return m_pBlock->pTransfer; }

	public:
		CAkStmMemView *	pNextView;				// List bare sibling.

	protected:
        AkMemBlock *		m_pBlock;			// Memory block pointed by this view. Memblocks' refcount should be in sync with this object.
		AkUInt32			m_uOffsetInBlock;	// Byte offset of this view into the block.
		TransferStatusType	m_eStatus		:3;	// Status. Starts as Pending, may become completed or cancelled, and ends as Ready in Update().
		AkUInt32			m_bCachedAlloc	:1;	// Set to false if this was allocated on the fly.
    };

	// Next item policy for bare lists.
	template <class T>
	struct AkListBareNextMemView
	{
		static AkForceInline T *& Get( T * in_pItem ) 
		{
			return in_pItem->pNextView;
		}
	};
	typedef AkListBareLight<CAkStmMemView,AkListBareNextMemView>	AkStmMemViewListLight;
	typedef AkListBare<CAkStmMemView,AkListBareNextMemView>			AkStmMemViewList;

}
}

#endif // _AK_STM_MEM_VIEW_H_
