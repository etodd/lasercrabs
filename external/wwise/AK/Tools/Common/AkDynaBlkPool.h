//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKBLOCKPOOL_H
#define _AKBLOCKPOOL_H

#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkListBareLight.h>

//
//  AkDynaBlkPool	- A dynamic block pool allocator which will grow (and shrink) in size in contiguous chunks of 'uPoolChunkSize' objects.
//					- Fragmentation in the pool will prevent it from shrinking, but in many use cases this is acceptable.
//

#ifdef _DEBUG
//#define AK_DYNA_BLK_STATS
#define AK_DYNA_BLK_SCRUB_MEM
#endif

#ifdef AK_DYNA_BLK_STATS
#define STATS_ALLOC() Stats_Alloc()
#define STATS_NEWCHUNK() Stats_NewChunk()
#define STATS_FREE() Stats_Free()
#define STATS_DELCHUNK() Stats_DelChunk()
#else
#define STATS_ALLOC()
#define STATS_NEWCHUNK()
#define STATS_FREE()
#define STATS_DELCHUNK()
#endif

#ifdef AK_DYNA_BLK_SCRUB_MEM
#define SCRUB_NEW_CHUNK() memset(&memory, 0xCC, sizeof(T)*uPoolChunkSize)
#define SCRUB_NEW_ALLOC(pItem) memset(pItem, 0xAA, sizeof(T))
#define SCRUB_FREE_BLOCK(pObj) memset(pObj, 0xDD, sizeof(T))
#else
#define SCRUB_NEW_CHUNK()
#define SCRUB_NEW_ALLOC(pItem)
#define SCRUB_FREE_BLOCK(pObj)
#endif

template  < typename T, AkUInt32 uPoolChunkSize, class TAlloc = ArrayPoolDefault>
class AkDynaBlkPool
{
	enum { kChunkMemoryBytes = sizeof(T)*uPoolChunkSize };

	struct FreeBlock
	{
		FreeBlock* pNextItem;
		char padding[ sizeof(T) - sizeof(FreeBlock*) ];
	};
	typedef AkListBare< FreeBlock, AkListBareNextItem, AkCountPolicyWithCount, AkLastPolicyNoLast > tFreeList;

	struct PoolChunk
	{
		PoolChunk() : pNextLightItem(NULL)
		{
			SCRUB_NEW_CHUNK();
			for( AkUInt32 i=0; i<uPoolChunkSize; ++i )
			{
				FreeBlock* pBlk = reinterpret_cast<FreeBlock*>( &memory ) + i;
				freeList.AddFirst(pBlk);
			}
		}

		inline bool BelongsTo( FreeBlock* pMem ) const { return (AkUInt8*)pMem >= memory && (AkUInt8*)pMem < (memory+kChunkMemoryBytes); }
		inline bool AllFree() const { return freeList.Length() == uPoolChunkSize; }
		inline bool AllAllocd() const {	return freeList.IsEmpty();	}

		AkUInt8 memory[ kChunkMemoryBytes ];
		PoolChunk* pNextLightItem;
		tFreeList freeList;
	};
	typedef AkListBareLight< PoolChunk > tChunkList;

public:

	T* New()
	{
		T* ptr = Alloc();
		if (ptr)
			AkPlacementNew(ptr) T;
		return ptr;
	}

	void Delete(T* ptr)
	{
		ptr->~T();
		Free(ptr);
	}

	T* Alloc()
	{
		FreeBlock* pItem = NULL;
		PoolChunk* pChunk = NULL;
		
		pChunk = m_chunkList.First();
		while (pChunk != NULL && pChunk->AllAllocd())
			pChunk = pChunk->pNextLightItem;

		if (pChunk == NULL)
		{
			pChunk = (PoolChunk *) TAlloc::Alloc( sizeof( PoolChunk ) );
			AkPlacementNew(pChunk) PoolChunk();
			STATS_NEWCHUNK();
			if (pChunk != NULL)
				m_chunkList.AddFirst(pChunk);
		}

		if (pChunk != NULL)
		{
			pItem = pChunk->freeList.First();
			AKASSERT(pItem != NULL);
			pChunk->freeList.RemoveFirst();
			SCRUB_NEW_ALLOC(pItem);
			STATS_ALLOC();
		}

		return reinterpret_cast<T*>(pItem);
	}

	void Free( T* pObj )
	{
		SCRUB_FREE_BLOCK((void*)pObj);
		
		FreeBlock* pItem = reinterpret_cast<FreeBlock*>(pObj);

		PoolChunk* pPrevChunk = NULL;
		PoolChunk* pChunk = m_chunkList.First();
		while (pChunk != NULL && !pChunk->BelongsTo(pItem))
		{
			pPrevChunk = pChunk;
			pChunk = pChunk->pNextLightItem;
		}

		AKASSERT(pChunk != NULL);
		pChunk->freeList.AddFirst(pItem);
		STATS_FREE();

		if (pChunk->AllFree())
		{
			m_chunkList.RemoveItem(pChunk, pPrevChunk);
			pChunk->~PoolChunk();
			TAlloc::Free( pChunk );
			STATS_DELCHUNK();
		}
	}

private:
	tChunkList m_chunkList;

#ifdef AK_DYNA_BLK_STATS
	void Stats_Alloc()
	{
		uCurrentUsedBytes += sizeof(T);
		uPeakUsedBytes = AkMax(uCurrentUsedBytes, uPeakUsedBytes);
	}
	void Stats_NewChunk()
	{
		uCurrentAllocdBytes += sizeof(PoolChunk);
		uPeakAllocdBytes = AkMax(uCurrentAllocdBytes, uPeakAllocdBytes);
	}
	void Stats_Free()
	{
		uCurrentUsedBytes -= sizeof(T);
	}
	void Stats_DelChunk()
	{
		uCurrentAllocdBytes -= sizeof(PoolChunk);
	}
public:
	AkUInt32 uPeakUsedBytes;
	AkUInt32 uPeakAllocdBytes;
	AkUInt32 uCurrentAllocdBytes;
	AkUInt32 uCurrentUsedBytes;
#endif
};


#endif
