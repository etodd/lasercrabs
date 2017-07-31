/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

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
class AkDynaBlkPool: public TAlloc
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
		if (ptr) AkPlacementNew(ptr) T;
		return ptr;
	}

	template<typename A1>
	T* New(A1 a1)
	{
		T* ptr = Alloc();
		if (ptr) AkPlacementNew(ptr) T(a1);
		return ptr;
	}

	template<typename A1, typename A2>
	T* New(A1 a1, A2 a2)
	{
		T* ptr = Alloc();
		if (ptr) AkPlacementNew(ptr) T(a1, a2);
		return ptr;
	}

	template<typename A1, typename A2, typename A3>
	T* New(A1 a1, A2 a2, A3 a3)
	{
		T* ptr = Alloc();
		if (ptr) AkPlacementNew(ptr) T(a1, a2, a3);
		return ptr;
	}

	template<typename A1, typename A2, typename A3, typename A4>
	T* New(A1 a1, A2 a2, A3 a3, A4 a4)
	{
		T* ptr = Alloc();
		if (ptr) AkPlacementNew(ptr) T(a1,a2,a3,a4);
		return ptr;
	}

	void Delete(T* ptr)
	{
		ptr->~T();
		Free(ptr);
	}

private:
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
