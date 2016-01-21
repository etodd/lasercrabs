//////////////////////////////////////////////////////////////////////
//
// AkIOMemMgr.cpp
//
// IO memory management.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "AkIOMemMgr.h"
#include "AkIOThread.h"
#include <stdio.h>

using namespace AK;
using namespace AK::StreamMgr;

inline AkUInt32 RoundToBlockSize(AkUInt32 in_uRequestedSize, AkUInt32 in_uAlignment)
{
	return (((in_uRequestedSize - 1) / in_uAlignment) + 1 ) * in_uAlignment;
}

CAkIOMemMgr::CAkIOMemMgr()
: m_totalCachedMem(0)
, m_totalAllocedMem(0)
, m_bUseCache( false )
#ifndef AK_OPTIMIZED
, m_streamIOPoolSize( 0 )
, m_uAllocs( 0 )
, m_uFrees( 0 )
, m_uPeakUsed( 0 )
#endif
{
}

CAkIOMemMgr::~CAkIOMemMgr()
{
}

AKRESULT CAkIOMemMgr::Init( 
	const AkDeviceSettings &	in_settings ,
	CAkIOThread* in_pIoThread
	)
{
	AKASSERT(in_pIoThread != NULL);
	m_pIoThread = in_pIoThread;

	// Number of I/O buffers and effective pool size:
	AkUInt32 uNumBuffers = in_settings.uIOMemorySize / in_settings.uGranularity;
	AkUInt32 uMemorySize = uNumBuffers * in_settings.uGranularity;

	m_totalAllocedMem = 0;
	m_totalCachedMem = 0;

	// Create stream memory pool.
    if ( uMemorySize > 0 )
    {		
		static const AkUInt32 kAbsoluteMinBlockSize = 512; //Review: This could be smaller to decrease fragmentation but will effect performance.

		AkUInt32 uMinBlockSize = kAbsoluteMinBlockSize;
		while (uMinBlockSize < in_settings.uIOMemoryAlignment)
			uMinBlockSize = uMinBlockSize << 1;

		AkUInt32 uMaxBlockSize = uMinBlockSize;
		while (uMaxBlockSize < in_settings.uGranularity)
			uMaxBlockSize = uMaxBlockSize << 1;

		m_StreamPool.Init(uMemorySize, uMaxBlockSize, uMinBlockSize, in_settings.ePoolAttributes, in_settings.pIOMemory);
    }

	if (m_StreamPool.IsInitialized())
	{
		AK_SETPOOLNAME(m_StreamPool.GetPoolId(), AKTEXT("Stream I/O"));

#ifndef AK_OPTIMIZED
		m_streamIOPoolSize = uMemorySize;
#endif

		// Create cached memory dictionary.
		// This array is growable but we will preallocate an estimate of the max number of blocks
		if ( m_arTaggedBlocks.Reserve( uNumBuffers ) != AK_Success )
		{
			AKASSERT( !"Not enough memory in the stream manager pool to create cache repository" );
			return AK_Fail;
		}

		// Compute number of views available in order to respect the maximum cache ratio.
		m_bUseCache = in_settings.bUseStreamCache;
	}
    else if ( in_settings.uIOMemorySize > 0 )
    {
        AKASSERT( !"Cannot create stream pool, or IO memory size is smaller than granularity" );
		return AK_Fail;
    }
    // otherwise, device does not support automatic streams.

	return AK_Success;
}

AkMemBlock* CAkIOMemMgr::AllocMemBlock( AkUInt32 in_uAllocSize, AkUInt32 in_uRequestedSize, AkUInt32 in_uAlignment )
{
	AkUInt8 * pIOMemory = (AkUInt8*)m_StreamPool.Alloc( in_uAllocSize );
	if (pIOMemory)
	{
		AKASSERT((AkUIntPtr)pIOMemory % in_uAlignment == 0);

		AkMemBlock* pBlk = AkNew( CAkStreamMgr::GetObjPoolID(), AkMemBlock( pIOMemory ) );
		if (pBlk)
		{
			pBlk->uAllocSize = in_uAllocSize;
			pBlk->uAvailableSize = in_uRequestedSize;
			m_totalAllocedMem += in_uAllocSize;
			UpdatePeakMemUsed();
			return pBlk;
		}
		else
		{
			m_StreamPool.Free(pIOMemory, in_uAllocSize);
		}
	}
	
	return NULL;
}

void CAkIOMemMgr::FreeMemBlock( AkMemBlock* in_pBlk )
{
	//Block must be untagged first
	AKASSERT(!in_pBlk->IsTagged());
	
	m_totalAllocedMem -= in_pBlk->uAllocSize;

	m_StreamPool.Free(in_pBlk->pData, in_pBlk->uAllocSize);

	AkDelete(CAkStreamMgr::GetObjPoolID(), in_pBlk );

	m_pIoThread->NotifyMemChange();
}

void CAkIOMemMgr::Term()
{
	FlushCache();
	m_listCachedBuffers.Term();
	m_arTaggedBlocks.Term();

    // Destroy IO pool.
	m_StreamPool.Term();

	m_pIoThread = NULL;
}

// IO memory access.
// IMPORTANT: These methods are not thread safe. 
//

// Release memory block.
// Returns refcount after releasing.
AkUInt32 CAkIOMemMgr::ReleaseBlock(
	AkMemBlock *	in_pMemBlock	// Memory block to release.
	)
{
	AKASSERT( in_pMemBlock->uRefCount > 0 );
	AkUInt32 uRefCount = --in_pMemBlock->uRefCount;
	if ( 0 == in_pMemBlock->uRefCount )
	{
		AKASSERT( !in_pMemBlock->pTransfer || !"Freeing block that has transfer" );

		// Add on top of the FIFO if the block is not tagged, at the end otherwise.
		if ( in_pMemBlock->IsTagged() )
		{
			m_totalCachedMem += in_pMemBlock->uAvailableSize;
			m_listCachedBuffers.AddLast( in_pMemBlock );

			// Adding the memory bloc to the list of cached buffers means that it is 
			//	up for grabs by some other stream. Notify the io thread.
			m_pIoThread->NotifyMemChange();
		}
		else
		{
			FreeMemBlock(in_pMemBlock);
		}

#ifndef AK_OPTIMIZED
		++m_uFrees;
#endif
	}

	return uRefCount;
}

// Get a free memory block.
// Returned memory block is addref'd (to 1), NULL if none found.
void CAkIOMemMgr::GetOldestFreeBlock(
	AkUInt32	in_uRequestedBufferSize,
	AkUInt32	in_uBlockAlign,
	AkMemBlock *&	out_pMemBlock
	)
{
	CHECK_CACHE_CONSISTENCY( NULL );

	AkUInt32 uAllocationSize = RoundToBlockSize(in_uRequestedBufferSize, in_uBlockAlign);
	do
	{
		out_pMemBlock = AllocMemBlock(uAllocationSize, in_uRequestedBufferSize, in_uBlockAlign );
		if (!out_pMemBlock)
		{
			out_pMemBlock = m_listCachedBuffers.First();
			if (out_pMemBlock == NULL)
			{
				//No available memory.  Bail out!
				m_pIoThread->NotifyMemIdle();
				return;
			}

			AKASSERT( out_pMemBlock->uRefCount == 0 );
			m_totalCachedMem -= out_pMemBlock->uAvailableSize;
			UpdatePeakMemUsed();
			m_listCachedBuffers.RemoveFirst();

			// pNextBlock is not used now that the block was dequeued, but it is an union with pTransfer, 
			// so we ought to clear it now.
			out_pMemBlock->pTransfer = NULL;

			if ( out_pMemBlock->uAllocSize != uAllocationSize )
			{
				//Too small, or too big. Ditch it an try to scape up some more memory from the allocator
				if (out_pMemBlock->IsTagged())
					UntagBlock(out_pMemBlock);

				FreeMemBlock(out_pMemBlock);
				out_pMemBlock = NULL;
			}
		}
	}
	while ( out_pMemBlock == NULL );

	//Should have a valid block at this point, otherwise we would have bailed out above.
	AKASSERT ( out_pMemBlock != NULL);
	
	CHECK_CACHE_CONSISTENCY( out_pMemBlock );

	++out_pMemBlock->uRefCount;

#ifndef AK_OPTIMIZED
	++m_uAllocs;
#endif

}

#ifdef _DEBUG
// Debugging: Verify that cache is consistent.
void CAkIOMemMgr::CheckCacheConsistency( AkMemBlock * in_pMustFindBlock )
{
	AkMemBlock * pPrevBlock = NULL;
	bool bFound = ( in_pMustFindBlock == NULL );
	AkMemBlocksDictionnary::Iterator it = m_arTaggedBlocks.Begin();
	while ( it != m_arTaggedBlocks.End() )
	{
		AkMemBlock * pThisBlock = (*it);
		if ( pPrevBlock 
			&& ( pPrevBlock->fileID > pThisBlock->fileID
				|| ( pPrevBlock->fileID == pThisBlock->fileID && pPrevBlock->uPosition < pThisBlock->uPosition )
				|| ( pPrevBlock->fileID == pThisBlock->fileID && pPrevBlock->uPosition == pThisBlock->uPosition && pPrevBlock->pData > pThisBlock->pData )
				) )
		{
			AKASSERT( false );
		}
		if ( in_pMustFindBlock == pThisBlock )
			bFound = true;

		pPrevBlock = pThisBlock;
		++it;
	}
	AKASSERT( bFound || in_pMustFindBlock->fileID == AK_INVALID_FILE_ID );
}
#endif

#ifdef AK_STREAM_MGR_INSTRUMENTATION
void CAkIOMemMgr::PrintCache()
{
	AKPLATFORM::OutputDebugMsg( "Index FileID Position Size\n" );
	AkMemBlocksDictionnary::Iterator it = m_arTaggedBlocks.Begin();
	while ( it != m_arTaggedBlocks.End() )
	{
		AkMemBlock * pMemBlock = (*it);
		char msg[64];
		sprintf( msg, "%u\t%u\t%lu\t%u\n", (*it), pMemBlock->fileID, (AkUInt32)pMemBlock->uPosition, pMemBlock->uAvailableSize );
		AKPLATFORM::OutputDebugMsg( msg );
		++it;
	}
}
#endif

// Comparison function for cache info binary search, using the first 2 keys: file ID and end position.
// Favors going towards the entry having the largest end position: Use this to converge to the first 
// element that matches the two criteria.
// Returns -1 if a better match _could_ be found before, 1 if a better match would be found after,
// 0 if a perfect match is found.
inline int Compare_FileID_Position( const AkMemBlock * in_pSorted, AkFileID in_fileID, AkUInt64 in_uPosition )
{
	// 1st key: file ID.
	if ( in_fileID < in_pSorted->fileID )
		return -1;
	else if ( in_fileID > in_pSorted->fileID )
		return 1;

	// 2nd key: position.
	else if ( in_uPosition > in_pSorted->uPosition )
		return -1;
	else if ( in_uPosition < in_pSorted->uPosition )
		return 1;

	AKASSERT( in_uPosition == in_pSorted->uPosition && in_fileID == in_pSorted->fileID );
	return 0;
}

// Finds a cached memory block if available.
// If a match was found, out_pMemBlock is set and addref'd. NULL otherwise.
// Returns the offset that is useable for the client within the cache buffer. 
// The actual data size io_uRequestedSize is updated, and is always between in_uMinSize and 
// io_uRequestedSize, except if it is the last memory region of the file.
// Returns the useable address within the cache buffer if a match was found, NULL otherwise.
// Notes: 
// - Returned memory block is addref'd.
// - Alignment and minimum data size constraint applicable to cache are handled herein.
AkUInt32 CAkIOMemMgr::GetCachedBlock(
	AkFileID		in_fileID,			// Block's associated file ID.
	AkUInt64		in_uPosition,		// Desired position in file.
	AkUInt32		in_uMinSize,		// Minimum data size acceptable (discard otherwise).
	AkUInt32		in_uRequiredAlign,	// Required data alignment.
	bool			in_bEof,			// True if last buffer in file
	AkUInt32 &		io_uRequestedSize,	// In: desired data size; Out: returned valid size. Modified only if a block is found.
	AkMemBlock *&	out_pMemBlock		// Returned memory block.
	)
{
	AKASSERT( UseCache() );
	AKASSERT( in_fileID != AK_INVALID_FILE_ID 
			&& in_uRequiredAlign >= 1 
			&& io_uRequestedSize >= in_uMinSize );

	out_pMemBlock = NULL;

	if (m_arTaggedBlocks.IsEmpty())
	{
		return 0;
	}

	AkMemBlock * pMemBlock = NULL;

	PRINT_CACHE();
	
	// 1) Perform a binary search to bring us just before or right on the block that matches the file ID and 
	// data position.
	int iTop = 0, iBottom = m_arTaggedBlocks.Length()-1;
	int iThis = ( iBottom - iTop ) / 2 + iTop; 
	do
	{
		iThis = ( iBottom - iTop ) / 2 + iTop; 
		int iCmp = Compare_FileID_Position( m_arTaggedBlocks[iThis], in_fileID, in_uPosition );
		if ( 0 == iCmp )
		{
			pMemBlock = m_arTaggedBlocks[iThis];
			break;
		}
		else if ( iCmp < 0 )
			iBottom = iThis - 1;
		else
			iTop = iThis + 1;
	}
	while ( iTop <= iBottom );

	// if (pMemBlock), iThis now points the block with same file ID and position.
	// Otherwise, iTop OR iBottom (depending on the direction of last bisection) points to the item just 
	// before (which may or may not have the correct file ID, and if it does, has the next larger position). 
	// This item may be useable. Whether a perfect match was found or not, we still need to ensure that it is.
	if ( !pMemBlock 
		|| ( in_uPosition > pMemBlock->uPosition + io_uRequestedSize - in_uMinSize ) )	// Perfect match, but is it valid?
	{
		// No perfect match, or perfect match not valid.
		// The current pick may be usable.
		AkMemBlock * pCurrentPick = m_arTaggedBlocks[iThis];
		if ( in_fileID == pCurrentPick->fileID
			&& in_uPosition >= pCurrentPick->uPosition
			&& ((AkInt32)in_uPosition) <= (AkInt32)(pCurrentPick->uPosition + pCurrentPick->uAvailableSize - in_uMinSize) )
		{
			pMemBlock = pCurrentPick;
		}
		else
		{
			// Otherwise the next block may also be usable.
			int iNextBlock = iThis + 1;
			if( iNextBlock < (AkInt32)m_arTaggedBlocks.Length() )
			{
				AkMemBlock * pSecondBestBlock = m_arTaggedBlocks[iNextBlock];
				if ( in_fileID == pSecondBestBlock->fileID
					&& in_uPosition >= pSecondBestBlock->uPosition
					&& ((AkInt32)in_uPosition) <= (AkInt32)(pSecondBestBlock->uPosition + pSecondBestBlock->uAvailableSize - in_uMinSize ) )
				{
					pMemBlock = pSecondBestBlock;
				}
				else
				{
					// No.
					return 0;
				}
			}
			else
			{
				// No.
				return 0;
			}
		}
	}

	AKASSERT( pMemBlock );

	// The best candidate has been chosen: it has the correct file, and its position 
	// is pretty much what we are looking for.
	// Compute effective position and data size.
	AKASSERT( in_uPosition >= pMemBlock->uPosition );
	AkUInt32 uPositionOffset = (AkUInt32)( in_uPosition - pMemBlock->uPosition );
	AkUInt32 uAvailableValidSize = pMemBlock->uAvailableSize - uPositionOffset;

	// Verify effective size and data address against alignment. Discard otherwise.
	/// NOTE: Suboptimal for last buffer. 
	if (   ( uAvailableValidSize <= io_uRequestedSize )				// Cannot overshoot desired size.
		// Alignment constraints. Relax requirements on size if last buffer.
		&& ( ( ( uAvailableValidSize % in_uRequiredAlign ) == 0 ) || ( in_bEof && uAvailableValidSize == io_uRequestedSize ) )
		&& ( ( ((AkUIntPtr)((AkUInt8*)pMemBlock->pData + uPositionOffset)) % in_uRequiredAlign ) == 0 )
		// ...or minimum size constraint.
		&& ( uAvailableValidSize >= in_uMinSize ) ) 
	{
		// Use this cache block.
		io_uRequestedSize = uAvailableValidSize;

		if ( pMemBlock->uRefCount == 0 )
		{
			// Free block. Pop it out of the free list.
			/// REVIEW Consider having an option to avoid searching cached data into free blocks. Would avoid this linear search.
			AKVERIFY( m_listCachedBuffers.Remove( pMemBlock ) == AK_Success );

			m_totalCachedMem -= pMemBlock->uAvailableSize;
			UpdatePeakMemUsed();

			// pNextBlock is not used now that the block was dequeued, but it is an union with pTransfer, 
			// so we ought to clear it now.
			pMemBlock->pTransfer = NULL;
#ifndef AK_OPTIMIZED
			++m_uAllocs;
#endif
		}
		++pMemBlock->uRefCount;

		out_pMemBlock = pMemBlock;
		return uPositionOffset;
	}
	
	return 0;
}

// Comparison function for cache info binary search, using all keys. Returns 0 if match is perfect,
// -1 if a better match would be found before, 1 if a better match would be found after.
inline int Compare_AllKeys( const AkMemBlock * in_pSorted, const AkMemBlock * in_pSearched )
{
	// 1st key: file ID.
	if ( in_pSearched->fileID < in_pSorted->fileID )
		return -1;
	else if ( in_pSearched->fileID > in_pSorted->fileID )
		return 1;

	// 2nd key: position.
	else if ( in_pSearched->uPosition > in_pSorted->uPosition )
		return -1;
	else if ( in_pSearched->uPosition < in_pSorted->uPosition )
		return 1;

	// 3rd key: address
	else if ( (AkUIntPtr)in_pSearched->pData < (AkUIntPtr)in_pSorted->pData )
		return -1;
	else if ( (AkUIntPtr)in_pSearched->pData > (AkUIntPtr)in_pSorted->pData )
		return 1;

	// Perfect match.
	AKASSERT( in_pSearched->uPosition == in_pSorted->uPosition 
			&& in_pSearched->fileID == in_pSorted->fileID 
			&& in_pSearched->pData == in_pSorted->pData );
	return 0;
}

// Untag a block after a cancelled or failed IO transfer.
void CAkIOMemMgr::UntagBlock(
	AkMemBlock *	in_pMemBlock		// Memory block to tag with caching info.
	)
{
	AKASSERT( in_pMemBlock->IsTagged() );

	// Find block, remove, untag, reinsert.
	CHECK_CACHE_CONSISTENCY( in_pMemBlock );

	AKASSERT(!m_arTaggedBlocks.IsEmpty());
	
	// Find the index of the block in the list, using all three keys.
	// Binary search: blocks are always sorted (file ID first, position second, buffer address third).
	AkInt32 iTop = 0, iBottom = m_arTaggedBlocks.Length()-1;
	AkInt32 iThis;
	do
	{
		iThis = ( iBottom - iTop ) / 2 + iTop; 
		int iCmp = Compare_AllKeys( m_arTaggedBlocks[iThis], in_pMemBlock );
		if ( 0 == iCmp )
		{
			m_arTaggedBlocks.Erase( iThis );
			break;
		}
		else if ( iCmp < 0 )
			iBottom = iThis - 1;
		else
			iTop = iThis + 1;
	}
	while ( iTop <= iBottom );

	// Set info on actual block.
	in_pMemBlock->fileID			= AK_INVALID_FILE_ID;
	
	CHECK_CACHE_CONSISTENCY( in_pMemBlock );
}

// Untag all blocks (cache flush).
void CAkIOMemMgr::FlushCache()
{
	if ( m_bUseCache )
	{
		CHECK_CACHE_CONSISTENCY( NULL );

		m_listCachedBuffers.RemoveAll();

		AkUInt32 uNewArrayLen = m_arTaggedBlocks.Length();
		for ( AkMemBlocksDictionnary::Iterator dictIt = m_arTaggedBlocks.Begin(); dictIt != m_arTaggedBlocks.End(); ++ dictIt )
		{
			AkMemBlock *& pMemBlk = (*dictIt);
			pMemBlk->fileID = AK_INVALID_FILE_ID;
			if (pMemBlk->uRefCount == 0)
			{
				FreeMemBlock(pMemBlk);
			}
		}

		m_arTaggedBlocks.RemoveAll();

		CHECK_CACHE_CONSISTENCY( NULL );
	}
}

// Tag a block with caching info before IO transfer.
AKRESULT CAkIOMemMgr::TagBlock(
	AkMemBlock *	in_pMemBlock,		// Memory block to tag with caching info.
	CAkLowLevelTransfer * in_pTransfer,	// Associated transfer.
	AkFileID		in_fileID,			// File ID.
	AkUInt64		in_uPosition,		// Absolute position in file.
	AkUInt32		in_uDataSize		// Size of valid data fetched from Low-Level IO.
	)
{
	AKRESULT res = AK_Fail;

	AKASSERT( in_pMemBlock->uRefCount == 1 );

	if ( !UseCache() || in_fileID == AK_INVALID_FILE_ID )
	{
		// Not using cache. Blocks are never kept reordered. Just set data and leave.
		in_pMemBlock->uPosition			= in_uPosition;
		in_pMemBlock->uAvailableSize	= in_uDataSize;	
		in_pMemBlock->pTransfer			= in_pTransfer;
		return AK_Success;
	}

	AKASSERT( !in_pMemBlock->pTransfer || !"Block already has transfer" );	// If you intend to tag this block, it should have been free.
	
	CHECK_CACHE_CONSISTENCY( in_pMemBlock );

	bool bFound = false;
	AkUInt32 uOriginalLocation = m_arTaggedBlocks.Length();

	if (!m_arTaggedBlocks.IsEmpty())
	{
		// Find the index of the block in the list, using all three keys.
		// Binary search: blocks are always sorted (file ID first, position second, buffer address third).
		AkInt32 iTop = 0, iBottom = m_arTaggedBlocks.Length()-1;
		AkInt32 iThis;
		do
		{
			iThis = ( iBottom - iTop ) / 2 + iTop; 
			int iCmp = Compare_AllKeys( m_arTaggedBlocks[iThis], in_pMemBlock );
			if ( 0 == iCmp )
			{
				bFound = true;
				uOriginalLocation = iThis;
				break;
			}
			else if ( iCmp < 0 )
				iBottom = iThis - 1;
			else
				iTop = iThis + 1;
		}
		while ( iTop <= iBottom );
	}
	
	// If the block was not found in the list, then we have to add it.
	if (bFound || m_arTaggedBlocks.AddLast(in_pMemBlock) != NULL )
	{
		AKASSERT(!m_arTaggedBlocks.IsEmpty());

		// Find the location where our block should be inserted.
		// Need to use a temporary block with such info.
		AkMemBlock newBlockData = *in_pMemBlock;
		newBlockData.fileID				= in_fileID;
		newBlockData.uPosition			= in_uPosition;

		AkInt32 iTop = 0, iBottom = m_arTaggedBlocks.Length()-1;
		AkInt32 iThis;
		do
		{
			iThis = ( iBottom - iTop ) / 2 + iTop; 
			int iCmp = Compare_AllKeys( m_arTaggedBlocks[iThis], &newBlockData );
			if ( 0 == iCmp )
			{
				iBottom = iTop = iThis;
				break;
			}
			else if ( iCmp < 0 )
				iBottom = iThis - 1;
			else
				iTop = iThis + 1;
		}
		while ( iTop <= iBottom );

		// Insert at index either iBottom or iTop, depending on whether the location was found from above or below.
		AkUInt32 uNewLocation = AkMax( iBottom, iTop );

		m_arTaggedBlocks.Move( uOriginalLocation, uNewLocation );

		in_pMemBlock->fileID			= in_fileID;
		
		res = AK_Success;
	}

	in_pMemBlock->uPosition			= in_uPosition;
	in_pMemBlock->uAvailableSize	= in_uDataSize;	
	in_pMemBlock->pTransfer			= in_pTransfer;

	CHECK_CACHE_CONSISTENCY( in_pMemBlock );

	return res;
}

// Temporary blocks.
// -----------------------------------
// Clone a memory block in order to have simultaneous low-level transfers on the 
// same mapped memory (needed by standard streams in deferred device). 
void CAkIOMemMgr::CloneTempBlock( 
	AkMemBlock * in_pMemBlockBase,
	AkMemBlock *& out_pMemBlock 
	)
{
	// Allocate a new block with values of in_pMemBlockBase.
	out_pMemBlock = (AkMemBlock*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( AkMemBlock ) );
	if ( !out_pMemBlock )
		return;

	AkPlacementNew( out_pMemBlock ) AkMemBlock( in_pMemBlockBase->pData );
	out_pMemBlock->uAvailableSize = in_pMemBlockBase->uAvailableSize;
	out_pMemBlock->uPosition = in_pMemBlockBase->uPosition;
}

void CAkIOMemMgr::DestroyTempBlock( 
	AkMemBlock * in_pMemBlockBase,
	AkMemBlock * in_pMemBlock 
	)
{
	// Release block only if it is temporary.
	if ( in_pMemBlock != in_pMemBlockBase )
	{
		AkFree( CAkStreamMgr::GetObjPoolID(), in_pMemBlock );
	}
}

void CAkIOMemMgr::GetProfilingData(
	AkUInt32 in_uBlockSize,
	AkDeviceData &  out_deviceData
	)
{
#ifndef AK_OPTIMIZED
	out_deviceData.uMemSize = m_streamIOPoolSize;
	out_deviceData.uMemUsed = m_totalAllocedMem;
	out_deviceData.uAllocs	= m_uAllocs;
	out_deviceData.uFrees	= m_uFrees;
	out_deviceData.uPeakRefdMemUsed = m_uPeakUsed;
	out_deviceData.uUnreferencedCachedBytes = m_totalCachedMem;
#endif
}

void CAkIOMemMgr::UpdatePeakMemUsed()
{
#ifndef AK_OPTIMIZED
	AkUInt32 uRefdMemUsed = (m_totalAllocedMem - m_totalCachedMem);
	if (uRefdMemUsed > m_uPeakUsed)
		m_uPeakUsed = uRefdMemUsed;
#endif
}

