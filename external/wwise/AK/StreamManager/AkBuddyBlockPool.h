//////////////////////////////////////////////////////////////////////
//
// AkBuddyBlockPool.h
//
// Copyright (c) 2015 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_BUDDY_BLOCK_POOL_H_
#define _AK_BUDDY_BLOCK_POOL_H_

#include <math.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkListBareLight.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

#ifdef _DEBUG
#define AK_BUDDY_POOL_STATS
//#define AK_CHECK_POOL
#endif

#ifdef AK_CHECK_POOL
#define AK_CHECK_POOL
#define CHECK_POOL() CheckPool()
#else
#define CHECK_POOL()
#endif

class AkBuddyBlockPool
{
private:
	struct MemBlock
	{
		MemBlock* pNextItem;

#ifdef CHECK_POOL
		MemBlock() : pNextItem(NULL), uSentinel(0xBBBBBBBB) {}
		AkUInt32 uSentinel;
#endif
	};

public:
 	enum{ kSmallestLevel = 0 };
	enum{ kMaxNumLevels = 32 };

	AkBuddyBlockPool() : m_pMem(NULL), m_uPoolSize(0), m_poolId(AK_INVALID_POOL_ID), m_uMaxBlockSize(0), m_uMinBlockSize(0), m_uShiftForMinBlockSize(0), m_uNumLevels(0)
	{
	}

	AKRESULT Init(AkUInt32 in_uPoolSize, AkUInt32 in_uMaxBlockSize, AkUInt32 in_uMinBlockSizeAndAlign, AkMemPoolAttributes in_eAttributes, void* in_pUserAllocatedMem = NULL)
	{
		if ( (in_uMaxBlockSize == 0) || (in_uMaxBlockSize & (in_uMaxBlockSize - 1)) ||
			(in_uMinBlockSizeAndAlign == 0) || (in_uMinBlockSizeAndAlign & (in_uMinBlockSizeAndAlign - 1)))
		{
			return AK_Fail;
		}

		m_uMaxBlockSize = in_uMaxBlockSize;
		m_uMinBlockSize = in_uMinBlockSizeAndAlign;

		m_uShiftForMinBlockSize = Log2ofPow2(m_uMinBlockSize);
		m_uNumLevels = Log2ofPow2(m_uMaxBlockSize) - m_uShiftForMinBlockSize + 1;

		if (m_uNumLevels > kMaxNumLevels)
			return AK_Fail;

		m_poolId = AK::MemoryMgr::CreatePool(
			in_pUserAllocatedMem,
			in_uPoolSize,
			in_uPoolSize,
			(in_eAttributes | AkFixedSizeBlocksMode),
			in_uMinBlockSizeAndAlign);

		if (m_poolId != AK_INVALID_POOL_ID)
		{
 			AK::MemoryMgr::SetMonitoring(m_poolId, false);

			m_pMem = AK::MemoryMgr::GetBlock(m_poolId);
			AKASSERT(m_pMem);

			m_uPoolSize = (in_uPoolSize / m_uMaxBlockSize) * m_uMaxBlockSize;

			if (m_uPoolSize > 0)
			{
				AkUInt32 uAddress = (m_uPoolSize - m_uMaxBlockSize);

				while (true)
				{
					MemBlock * pBlockInfo = NewMemBlock(uAddress);

					m_FreeLists[LargestLevel()].AddFirst(pBlockInfo);

					if (uAddress != 0)
						uAddress -= m_uMaxBlockSize;
					else
						break;
				}

#ifdef AK_BUDDY_POOL_STATS
				m_uInternalFrag = 0;
				m_Stats.uReserved = in_uPoolSize;
				m_Stats.uAllocs = 0;
				m_Stats.uFrees = 0;
				m_Stats.uMaxFreeBlock = 0;
				m_Stats.uPeakUsed = 0;
				m_Stats.uUsed = 0;
#endif

				return AK_Success;
			}
		}
		CHECK_POOL();
		return AK_Fail;
	}
	
	void Term()
	{
		CHECK_POOL();
		if (m_poolId != AK_INVALID_POOL_ID)
		{
			AK::MemoryMgr::ReleaseBlock(m_poolId, m_pMem);
			m_pMem = NULL;
			AK::MemoryMgr::DestroyPool(m_poolId);
		}
	}

	void* Alloc(AkUInt32 in_uNumBytes)
	{
		CHECK_POOL();
		void * pMem = NULL;

		if (in_uNumBytes <= m_uMaxBlockSize && in_uNumBytes > 0 )
		{
			pMem = SubdivideMem(in_uNumBytes);

#ifdef AK_BUDDY_POOL_STATS
			if (pMem)
			{
				m_uInternalFrag += RemainderFreeSize(kSmallestLevel,in_uNumBytes);
				m_Stats.uAllocs++;
				m_Stats.uUsed += in_uNumBytes;
				m_Stats.uPeakUsed = AkMax(m_Stats.uUsed, m_Stats.uPeakUsed);
				UpdateMaxFreeBlock();
			}
#endif

		}
		CHECK_POOL();
		return pMem;
	}

	void Free(void * in_pMem, AkUInt32 in_uNumBytes)
	{
		CHECK_POOL();
		AKASSERT(in_uNumBytes <= m_uMaxBlockSize);
		AKASSERT( in_pMem >= m_pMem && (AkUInt8*)in_pMem < (AkUInt8*)m_pMem + m_uPoolSize);

		CoalesceMem((AkUInt32)((AkUInt8*)in_pMem - (AkUInt8*)m_pMem), in_uNumBytes);

#ifdef AK_BUDDY_POOL_STATS
		m_uInternalFrag -= RemainderFreeSize(kSmallestLevel, in_uNumBytes);
		m_Stats.uFrees++;
		m_Stats.uUsed -= in_uNumBytes;
		UpdateMaxFreeBlock();
#endif
		CHECK_POOL();
	}

	bool IsInitialized() { return m_poolId != AK_INVALID_POOL_ID;  }
	AkMemPoolId GetPoolId() { return m_poolId; }

private:

	AkUInt32 Log2ofPow2(AkUInt32 in_uNum)
	{
		AKASSERT(in_uNum != 0 && (in_uNum & (in_uNum - 1)) == 0);//must be pow 2
		AkUInt32 uRes = 0U;
		AkUInt32 uOnes = (in_uNum - 1);
		while (uOnes != 0)
		{
			uOnes = uOnes >> 1U;
			uRes++;
		}
		return uRes;
	}

	inline AkUInt32 LargestLevel(){ return m_uNumLevels - 1; }

	inline AkUInt32 BlockAddress(MemBlock* in_pBlock)
	{
#ifdef CHECK_POOL
		AKASSERT(in_pBlock->uSentinel == 0xBBBBBBBB);
#endif
		return (AkUInt32)((AkUInt8*)in_pBlock - (AkUInt8*)m_pMem);
	}

	MemBlock* NewMemBlock(AkUInt32 uAddress)
	{
		AKASSERT(uAddress < m_uPoolSize);
		AKASSERT(uAddress % m_uMinBlockSize == 0);
		MemBlock* pBlk = (MemBlock*)((AkUInt8*)m_pMem + uAddress);
		pBlk = AkPlacementNew(pBlk) MemBlock();
		return pBlk;
	}

	inline bool AreBlocksAdjacent(MemBlock* in_pLeft, MemBlock* in_pRight, AkUInt32 in_uLevel)
	{
		return ((AkUInt8*)in_pRight - (AkUInt8*)in_pLeft) == BlockSizeForLevel(in_uLevel);
	}

	inline AkUInt32 BlockSizeForLevel(AkUInt32 in_uLevel) const
	{
		AKASSERT(in_uLevel < m_uNumLevels);
		return (1u << (m_uShiftForMinBlockSize + in_uLevel));
	}

	inline AkUInt32 AllocBlockOffset(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return (in_uAllocSize & ~((1U << (m_uShiftForMinBlockSize + in_uLevel + 1)) - 1));
	}

	inline AkUInt32 FreeBlockOffset(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return m_uMaxBlockSize - ((m_uMaxBlockSize - in_uAllocSize) & ~((1U << (m_uShiftForMinBlockSize + in_uLevel)) - 1));
	}

	inline AkUInt32 AllocSubBlockSize(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return (in_uAllocSize & (1U << (m_uShiftForMinBlockSize + in_uLevel)));
	}

	inline AkUInt32 FreeSubBlockSize(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return ((m_uMaxBlockSize - in_uAllocSize) & (1U << (m_uShiftForMinBlockSize + in_uLevel)));
	}

	inline AkUInt32 RemainderAllocSize(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return (in_uAllocSize & ((1U << (m_uShiftForMinBlockSize + in_uLevel))) - 1);
	}

	inline AkUInt32 RemainderFreeSize(AkUInt32 in_uLevel, AkInt32 in_uAllocSize)
	{
		return ((m_uMaxBlockSize - in_uAllocSize) & (1U << m_uShiftForMinBlockSize) - 1);
	}

	//
	void* SubdivideMem(AkUInt32 in_uSizeNeeded)
	{
		void* pMem = NULL;

		MemBlock* pBlock = NULL;
		AkInt32 iLevel = kSmallestLevel;

		for (; iLevel < (AkInt32)m_uNumLevels; ++iLevel)
		{
			if (BlockSizeForLevel(iLevel) >= in_uSizeNeeded && !m_FreeLists[iLevel].IsEmpty())
			{
				pBlock = m_FreeLists[iLevel].First();
				m_FreeLists[iLevel].RemoveFirst();
				break;
			}
		}

		if (pBlock != NULL)
		{
			AkUInt32 uAddress = BlockAddress(pBlock);
			pMem = (void*)pBlock;

			--iLevel;

			for (; iLevel >= kSmallestLevel; --iLevel)
			{
				if (FreeSubBlockSize(iLevel, in_uSizeNeeded))
				{
					MemBlock* pNewFreeBlk = NewMemBlock(uAddress + FreeBlockOffset(iLevel, in_uSizeNeeded));

					BlockInfoList& freeList = m_FreeLists[iLevel];

					BlockInfoList::IteratorEx it = freeList.BeginEx();
					BlockInfoList::Iterator itEnd = freeList.End();
					while (it != itEnd && it.pItem < pNewFreeBlk)
						++it;

					freeList.Insert(it, pNewFreeBlk);
				}
			}
		}

		return pMem;
	}

	//
	void CoalesceBlock(AkUInt32 in_uLevel, MemBlock* in_pFloatingBlock)
	{
		AKASSERT(in_pFloatingBlock != NULL);

		BlockInfoList& list = m_FreeLists[in_uLevel];

		BlockInfoList::IteratorEx it = list.BeginEx();
		for (; it != list.End(); ++it)
		{
			if (*it > in_pFloatingBlock)
				break;
		}

		bool bDoInsert = true;
		if (in_uLevel < LargestLevel())
		{
			AkUInt32 uBlkSizeAbove = BlockSizeForLevel(in_uLevel+1);
			
			if ( (it.pPrevItem != NULL) &&
				((BlockAddress(it.pPrevItem) & (uBlkSizeAbove - 1)) == 0) &&
				AreBlocksAdjacent(it.pPrevItem, in_pFloatingBlock, in_uLevel))
			{
				//Combine prev and current.
				list.Remove(it.pPrevItem); // <- iterates through list again... can be avoided?
				CoalesceBlock(in_uLevel + 1, it.pPrevItem);
				bDoInsert = false;
			}
			else if ( (it.pItem != NULL) && 
				((BlockAddress(in_pFloatingBlock) & (uBlkSizeAbove - 1)) == 0) &&
				AreBlocksAdjacent(in_pFloatingBlock, it.pItem, in_uLevel))
			{
				//Combine and current and next.
				list.RemoveItem(it.pItem, it.pPrevItem);
				CoalesceBlock(in_uLevel + 1, in_pFloatingBlock);
				bDoInsert = false;
			}
		}

		if (bDoInsert)
			list.Insert(it, in_pFloatingBlock);
	}

	MemBlock* JoinAllocdBlock(AkUInt32 in_uLevel, MemBlock* in_pBlkRight, AkUInt32 in_uMemAddress, AkUInt32 in_uMemSize)
	{
		MemBlock* pBlkLeft = NewMemBlock(in_uMemAddress + AllocBlockOffset(in_uLevel, in_uMemSize));

		if (in_pBlkRight == NULL)
		{
			if (in_uLevel < LargestLevel() && BlockSizeForLevel(in_uLevel + 1) < in_uMemSize)
			{	
				pBlkLeft = JoinFreeBlock(in_uLevel, pBlkLeft);
			}
			else
			{
				// If we are at the highest level, do a fully recursive coalesce
				CoalesceBlock(in_uLevel, pBlkLeft);
				pBlkLeft = NULL;
			}
		}
		
		return pBlkLeft;
	}

	MemBlock* JoinFreeBlock(AkUInt32 in_uLevel, MemBlock* in_pBlk)
	{
		AKASSERT(in_pBlk != NULL);

		BlockInfoList& list = m_FreeLists[in_uLevel];
		BlockInfoList::IteratorEx it = list.BeginEx();
		for (; it != list.End(); ++it)
		{
			if (*it > in_pBlk)
				break;
		}

		if (in_uLevel < LargestLevel() && 
			(it.pItem != NULL) &&
			AreBlocksAdjacent(in_pBlk,it.pItem, in_uLevel))
		{
			//Combine and current and next.
			list.RemoveItem(it.pItem, it.pPrevItem);
		}
		else
		{
			// Buddy block is in use.  Can't join.
			list.Insert(it, in_pBlk);
			in_pBlk = NULL;
		}

		return in_pBlk;
	}

	void CoalesceMem(AkUInt32 uAddress, AkUInt32 in_uBytesToFree)
	{	
		AkUInt32 uLvl = kSmallestLevel;
		while (RemainderAllocSize(uLvl+1, in_uBytesToFree) == 0)
			++uLvl;

		MemBlock* pBlk = NULL;
		if (AllocSubBlockSize(uLvl, in_uBytesToFree) != 0 && RemainderAllocSize(uLvl, in_uBytesToFree) != 0)
		{
			// If we had to fill two of the smallest size blocks
			pBlk = NewMemBlock(uAddress + AllocBlockOffset(uLvl, in_uBytesToFree) + BlockSizeForLevel(uLvl));
		}

		pBlk = JoinAllocdBlock(uLvl, pBlk, uAddress, in_uBytesToFree);

		uLvl++;

		for (; uLvl < LargestLevel() && BlockSizeForLevel(uLvl) < in_uBytesToFree; ++uLvl)
		{
			if (AllocSubBlockSize(uLvl, in_uBytesToFree))
				pBlk = JoinAllocdBlock(uLvl, pBlk, uAddress, in_uBytesToFree);
			else if (pBlk != NULL && FreeSubBlockSize(uLvl, in_uBytesToFree))
				pBlk = JoinFreeBlock(uLvl, pBlk);
		}
		
		if (pBlk != NULL)
			CoalesceBlock(uLvl, pBlk);
	}

private:

	typedef AkListBare<MemBlock, AkListBareNextItem, AkCountPolicyWithCount> BlockInfoList;

	BlockInfoList m_FreeLists[kMaxNumLevels];
	
	void* m_pMem;

	AkUInt32 m_uPoolSize;
	AkMemPoolId m_poolId;

	AkUInt32 m_uMaxBlockSize;
	AkUInt32 m_uMinBlockSize;
	AkUInt32 m_uShiftForMinBlockSize;
	AkUInt32 m_uNumLevels;

public:

#ifdef AK_BUDDY_POOL_STATS

	AkUInt32 m_uInternalFrag;
	AK::MemoryMgr::PoolStats m_Stats;

	void PrintStats()
	{
#if 0
		printf("Pool Size: %u bytes\n", m_uPoolSize);
		printf("Internal Fragmentation: %u bytes, %f%%\n", m_uInternalFrag, ((m_Stats.uUsed + m_uInternalFrag) == 0) ? 0.f : ((float)m_uInternalFrag / (float)(m_Stats.uUsed + m_uInternalFrag)) * 100.f);

		AkUInt32 uTotalFreeBytes = 0;
		AkUInt32 uTotalFreeBlocks = 0;
		for (int i = 0; i < m_uNumLevels; ++i)
		{
			uTotalFreeBlocks += m_FreeLists[i].Length();
			uTotalFreeBytes += m_FreeLists[i].Length() * BlockSizeForLevel(i);
		}

		AKASSERT(m_Stats.uUsed + m_uInternalFrag + uTotalFreeBytes == m_uPoolSize);

		printf("Free Memory: %u bytes, %f%%\n", uTotalFreeBytes, ((float)uTotalFreeBytes / (float)m_uPoolSize) * 100.f);
		printf("Free Block Distribution:\n");
		for (int i = 0; i < m_uNumLevels; ++i)
		{
			AkUInt32 uFreeBlocks  = m_FreeLists[i].Length();
			AkReal32 fPct = (AkReal32)uFreeBlocks / (AkReal32)uTotalFreeBlocks;
			printf("L%i %6u |", i, BlockSizeForLevel(i));
			for (int j = 0; j < (int)(fPct * 64.f); ++j)
				printf("-");
			printf("[%u]\n", uFreeBlocks);
		}
#endif
	}

	void CheckPool()
	{
		AkUInt32 uTotalFreeBytes = 0;
		AkUInt32 uTotalFreeBlocks = 0;
		for (unsigned int i = 0; i < m_uNumLevels; ++i)
		{
			uTotalFreeBlocks += m_FreeLists[i].Length();
			uTotalFreeBytes += m_FreeLists[i].Length() * BlockSizeForLevel(i);

			BlockInfoList::IteratorEx it = m_FreeLists[i].BeginEx();
			while (it != m_FreeLists[i].End())
			{
				AKASSERT(it.pItem->uSentinel == 0xBBBBBBBB);

				AKASSERT(it.pPrevItem == NULL || it.pPrevItem < it.pItem);
				
				AKASSERT(	(it.pPrevItem == NULL) || 
							(i == LargestLevel()) ||
							(BlockAddress(it.pPrevItem) % BlockSizeForLevel(i+1) != 0) || 
							!AreBlocksAdjacent(it.pPrevItem, it.pItem, i) );

				AKASSERT(BlockAddress(it.pItem) % BlockSizeForLevel(i) == 0);
				++it;
			}
		}

		AKASSERT(uTotalFreeBytes <= m_uPoolSize);
		AKASSERT(m_Stats.uUsed + m_uInternalFrag + uTotalFreeBytes == m_uPoolSize);
	}

	void UpdateMaxFreeBlock()
	{
		for (AkInt32 i = LargestLevel(); i >= kSmallestLevel; --i)
		{
			if (!m_FreeLists[i].IsEmpty())
			{
				m_Stats.uMaxFreeBlock = BlockSizeForLevel(i);
				break;
			}
		}
	}


#endif
};

#endif
