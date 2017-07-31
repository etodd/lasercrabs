/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
 ***********************************************************************/

#ifndef _AKHASHLIST_H
#define _AKHASHLIST_H

#include <AK/Tools/Common/AkKeyDef.h>// for MapStruct
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkArray.h>

// NOTE: when using this template, a hashing function of the following form must be available: 
//
// AkHashType AkHash( T_KEY );

typedef AkUInt32 AkHashType;

template < class T_KEY >
AkForceInline AkHashType AkHash(T_KEY in_key) { return (AkHashType)in_key; }

#define AK_HASH_SIZE_VERY_SMALL 11
static const AkHashType kHashSizes[] = { 29, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741 };
static const size_t kNumHashSizes = sizeof(kHashSizes) / sizeof(kHashSizes[0]);
static const AkReal32 kHashTableGrowthFactor = 0.9f; 

template < class T_KEY, class T_ITEM, typename T_ALLOC = ArrayPoolDefault >
class AkHashList: public T_ALLOC
{
public:
	
    struct Item
	{
		Item * pNextItem;               // our next one
		MapStruct<T_KEY, T_ITEM> Assoc;	// key-item association
	};

	typedef AkArray<Item*, Item*, T_ALLOC, 0 > HashTableArray;

	struct Iterator
	{
		typename AkHashList<T_KEY,T_ITEM,T_ALLOC>::HashTableArray* pTable;
		AkHashType uiTable;
		Item* pItem;

		inline Iterator& operator++()
		{
			AKASSERT( pItem );
			pItem = pItem->pNextItem;
			
			while ( ( pItem == NULL ) && ( ++uiTable < pTable->Length() ) )
				pItem = (*pTable)[ uiTable ];

			return *this;
		}

		inline MapStruct<T_KEY, T_ITEM>& operator*()
		{
			AKASSERT( pItem );
			return pItem->Assoc;
		}

		bool operator !=( const Iterator& in_rOp ) const
		{
			return ( pItem != in_rOp.pItem );
		}		
	};

	// The IteratorEx iterator is intended for usage when a possible erase may occurs
	// when simply iterating trough a list, use the simple Iterator, it is faster and lighter.
	struct IteratorEx : public Iterator
	{
		Item* pPrevItem;

		IteratorEx& operator++()
		{
			AKASSERT( this->pItem );
			
			pPrevItem = this->pItem;
			this->pItem = this->pItem->pNextItem;
			
			while ((this->pItem == NULL) && (++this->uiTable < this->pTable->Length()))
			{
				pPrevItem = NULL;
				this->pItem = (*this->pTable)[ this->uiTable ];
			}

			return *this;
		}
	};

	Iterator Begin()
	{
		Iterator returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = 0;
			returnedIt.pItem = m_table[0];

			while ((returnedIt.pItem == NULL) && (++returnedIt.uiTable < HashSize()))
				returnedIt.pItem = m_table[returnedIt.uiTable];
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
		}

		return returnedIt;
	}

	IteratorEx BeginEx()
	{
		IteratorEx returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = 0;
			returnedIt.pItem = *(m_table.Begin());
			returnedIt.pPrevItem = NULL;

			while ((returnedIt.pItem == NULL) && (++returnedIt.uiTable < HashSize()))
				returnedIt.pItem = m_table[returnedIt.uiTable];
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
		}

		return returnedIt;
	}

	inline Iterator End()
	{
		Iterator returnedIt;
		returnedIt.pItem = NULL;
		return returnedIt;
	}

	IteratorEx FindEx( T_KEY in_Key )
	{
		IteratorEx returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = AkHash(in_Key) % HashSize();
			returnedIt.pItem = m_table.Length() > 0 ? m_table[returnedIt.uiTable] : NULL;
			returnedIt.pPrevItem = NULL;

			while (returnedIt.pItem != NULL)
			{
				if (returnedIt.pItem->Assoc.key == in_Key)
					break;

				returnedIt.pPrevItem = returnedIt.pItem;
				returnedIt.pItem = returnedIt.pItem->pNextItem;
			}
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
			returnedIt.pPrevItem = NULL;
		}

		return returnedIt;
	}

	AkHashList(): m_uiSize( 0 )
	{
	}

	~AkHashList()
	{
		AKASSERT(m_uiSize == 0);
		m_table.Term();
	}

	void Term()
	{
		RemoveAll();
		m_table.Term();
	}

	void RemoveAll()
	{
		for (AkHashType i = 0; i < HashSize(); ++i)
		{
			Item * pItem = m_table[ i ];
			while ( pItem != NULL )
			{
				Item * pNextItem = pItem->pNextItem;
				pItem->Assoc.item.~T_ITEM();
				T_ALLOC::Free( pItem );
				pItem = pNextItem;
			}
			
			m_table[ i ] = NULL;
		}

		m_uiSize = 0;
	}

	T_ITEM * Exists( T_KEY in_Key )
	{
		if (HashSize() > 0)
		{
			AkUIntPtr uiTable = AkHash(in_Key) % HashSize();
			return ExistsInList(in_Key, uiTable);
		}
		return NULL;
	}

	// Set using an externally preallocated Item -- Hash list takes ownership of the Item.
	T_ITEM * Set( Item * in_pItem )
	{
		if (CheckSize())
		{
			AkHashType uiTable = AkHash(in_pItem->Assoc.key) % HashSize();

			AKASSERT(!ExistsInList(in_pItem->Assoc.key, uiTable)); // Item must not exist in list !

			// Add new entry

			in_pItem->pNextItem = m_table[uiTable];
			m_table[uiTable] = in_pItem;

			++m_uiSize;

			return &(in_pItem->Assoc.item);
		}

		return NULL;
	}

	T_ITEM * Set( T_KEY in_Key )
	{
		if ( CheckSize() )
		{
			AkUIntPtr uiTable = AkHash(in_Key) % HashSize();
			T_ITEM * pItem = ExistsInList(in_Key, uiTable);
			if (pItem)
				return pItem;

			return CreateEntry(in_Key, uiTable);
		}

		return NULL;
	}

	T_ITEM * Set( T_KEY in_Key, bool& out_bWasAlreadyThere )
	{
		if (CheckSize())
		{
			AkHashType uiTable = AkHash(in_Key) % HashSize();
			T_ITEM * pItem = ExistsInList(in_Key, uiTable);
			if (pItem)
			{
				out_bWasAlreadyThere = true;
				return pItem;
			}
			else
			{
				out_bWasAlreadyThere = false;
			}

			return CreateEntry(in_Key, uiTable);
		}

		return NULL;
	}

	void Unset( T_KEY in_Key )
	{
		if (HashSize() > 0)
		{
			AkHashType uiTable = AkHash(in_Key) % HashSize();
			Item * pItem = m_table[uiTable];
			Item * pPrevItem = NULL;
			while (pItem != NULL)
			{
				if (pItem->Assoc.key == in_Key)
					break;

				pPrevItem = pItem;
				pItem = pItem->pNextItem;
			}

			if (pItem)
				RemoveItem(uiTable, pItem, pPrevItem);
		}
	}

	IteratorEx Erase( const IteratorEx& in_rIter )
	{
		IteratorEx returnedIt;
		returnedIt.pTable = in_rIter.pTable;
		returnedIt.uiTable = in_rIter.uiTable;
		returnedIt.pItem = in_rIter.pItem->pNextItem;
		returnedIt.pPrevItem = in_rIter.pPrevItem;
		
		while ( ( returnedIt.pItem == NULL ) && ( ++returnedIt.uiTable < HashSize() ) )
		{
			returnedIt.pPrevItem = NULL;
			returnedIt.pItem = (*returnedIt.pTable)[returnedIt.uiTable];
		}
		
		RemoveItem( in_rIter.uiTable, in_rIter.pItem, in_rIter.pPrevItem );

		return returnedIt;
	}

	void RemoveItem( AkHashType in_uiTable, Item* in_pItem, Item* in_pPrevItem )
	{
		if( in_pPrevItem ) 
			in_pPrevItem->pNextItem = in_pItem->pNextItem;
		else
			m_table[ in_uiTable ] = in_pItem->pNextItem;

		in_pItem->Assoc.item.~T_ITEM();
		T_ALLOC::Free(in_pItem);

		--m_uiSize;
	}

	AkUInt32 Length() const
	{
		return m_uiSize;
	}

	AKRESULT Reserve(AkUInt32 in_uNumberOfEntires)
	{
		if ((HashSize() == 0) || (AkReal32)in_uNumberOfEntires / (AkReal32)HashSize() > kHashTableGrowthFactor)
			return Resize((AkUInt32)((AkReal32)in_uNumberOfEntires / kHashTableGrowthFactor));

		return AK_Success;
	}

	AKRESULT Resize(AkUInt32 in_uExpectedNumberOfEntires)
	{
		AKRESULT res = AK_Fail;

		AkUInt32 uNewSize = 0;
		for (AkUInt32 i = 0; i < kNumHashSizes; ++i)
		{
			if (kHashSizes[i] > in_uExpectedNumberOfEntires)
			{
				uNewSize = kHashSizes[i];
				break;
			}
		}

		if (uNewSize > 0)
		{
			HashTableArray oldArray;
			oldArray.Transfer(m_table);

			if ( m_table.GrowArray(uNewSize) )
			{
				for (AkUInt32 i = 0; i < uNewSize; i++)
					m_table.AddLast(NULL);

				for (AkUInt32 i = 0; i < oldArray.Length(); i++)
				{
					Item * pItem = oldArray[i];
					while (pItem != NULL)
					{
						Item * pNextItem = pItem->pNextItem;
						{
							AkHashType uiTable = AkHash(pItem->Assoc.key) % HashSize();
							pItem->pNextItem = m_table[uiTable];
							m_table[uiTable] = pItem;
						}
						pItem = pNextItem;
					}
				}

				oldArray.Term();

				res = AK_Success;
			}
			else
			{
				//Backpedal..
				m_table.Transfer(oldArray);
			}
		}

		return res;
	}

	inline AkUInt32 HashSize() const
	{
		return m_table.Length();
	}

	inline bool CheckSize() 
	{
		if ( HashSize() == 0 || (AkReal32)m_uiSize / (AkReal32)HashSize() > kHashTableGrowthFactor )
			Resize(HashSize());

		return (HashSize() > 0);
	}

protected:
	T_ITEM * ExistsInList( T_KEY in_Key, AkUIntPtr in_uiTable )
	{
		AKASSERT(HashSize() > 0);
		
		Item * pItem = m_table[(AkUInt32)in_uiTable];
		while (pItem != NULL)
		{
			if (pItem->Assoc.key == in_Key)
				return &(pItem->Assoc.item); // found

			pItem = pItem->pNextItem;
		}

		return NULL; // not found
	}

	T_ITEM * CreateEntry( T_KEY in_Key, AkUIntPtr in_uiTable )
	{
		Item * pNewItem = (Item *)T_ALLOC::Alloc(sizeof(Item));
		if ( pNewItem == NULL )
			return NULL;

		pNewItem->pNextItem = m_table[ (AkUInt32)in_uiTable ];
		pNewItem->Assoc.key = in_Key;

		AkPlacementNew( &(pNewItem->Assoc.item) ) T_ITEM; 

		m_table[(AkUInt32)in_uiTable] = pNewItem;

		++m_uiSize;

		return &(pNewItem->Assoc.item);
	}

	HashTableArray m_table;
	AkUInt32 m_uiSize;
};

// this one lets you define the structure
// only requirement is that T_MAPSTRUCT must have members pNextItem and key.
// client is responsible for allocation/deallocation of T_MAPSTRUCTS.
template <class T_KEY, class T_MAPSTRUCT>
struct AkDefaultHashListBarePolicy
{
	static const T_KEY& Key(const T_MAPSTRUCT* in_pItem) {return in_pItem->key;}
};

template < class T_KEY, class T_MAPSTRUCT, typename T_ALLOC = ArrayPoolDefault, class KEY_POLICY = AkDefaultHashListBarePolicy<T_KEY, T_MAPSTRUCT> > 
class AkHashListBare
{
	typedef AkArray<T_MAPSTRUCT*, T_MAPSTRUCT*, T_ALLOC, 0 > HashTableArray;
public:
	struct Iterator
	{
		typename AkHashListBare<T_KEY,T_MAPSTRUCT,T_ALLOC,KEY_POLICY>::HashTableArray* pTable;
		AkHashType uiTable;
		T_MAPSTRUCT* pItem;

		inline Iterator& operator++()
		{
			AKASSERT( pItem );
			pItem = pItem->pNextItem;
			
			while ((pItem == NULL) && (++uiTable < pTable->Length()))
				pItem = (*pTable)[ uiTable ];

			return *this;
		}

		inline T_MAPSTRUCT * operator*()
		{
			AKASSERT( pItem );
			return pItem;
		}

		bool operator !=( const Iterator& in_rOp ) const
		{
			return ( pItem != in_rOp.pItem );
		}		
	};

	// The IteratorEx iterator is intended for usage when a possible erase may occurs
	// when simply iterating trough a list, use the simple Iterator, it is faster and lighter.
	struct IteratorEx : public Iterator
	{
		T_MAPSTRUCT* pPrevItem;

		IteratorEx& operator++()
		{
			AKASSERT( this->pItem );
			
			pPrevItem = this->pItem;
			this->pItem = this->pItem->pNextItem;
			
			while ( ( this->pItem == NULL ) && ( ++this->uiTable < this->pTable->Length() ) )
			{
				pPrevItem = NULL;
				this->pItem = (*this->pTable)[ this->uiTable ];
			}

			return *this;
		}
	};

	Iterator Begin()
	{
		Iterator returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = 0;
			returnedIt.pItem = m_table[0];

			while ((returnedIt.pItem == NULL) && (++returnedIt.uiTable < HashSize()))
				returnedIt.pItem = m_table[returnedIt.uiTable];
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
		}

		return returnedIt;
	}

	IteratorEx BeginEx()
	{
		IteratorEx returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = 0;
			returnedIt.pItem = m_table[0];
			returnedIt.pPrevItem = NULL;
		
			while ( ( returnedIt.pItem == NULL ) && ( ++returnedIt.uiTable < HashSize() ) )
				returnedIt.pItem = m_table[ returnedIt.uiTable ];
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
		}

		return returnedIt;
	}

	inline Iterator End()
	{
		Iterator returnedIt;
		returnedIt.pItem = NULL;
		return returnedIt;
	}

	IteratorEx FindEx( T_KEY in_Key )
	{
		IteratorEx returnedIt;

		if (HashSize() > 0)
		{
			returnedIt.pTable = &m_table;
			returnedIt.uiTable = AkHash(in_Key) % HashSize();
			returnedIt.pItem = m_table[returnedIt.uiTable];
			returnedIt.pPrevItem = NULL;

			while (returnedIt.pItem != NULL)
			{
				if (KEY_POLICY::Key(returnedIt.pItem) == in_Key)
					break;

				returnedIt.pPrevItem = returnedIt.pItem;
				returnedIt.pItem = returnedIt.pItem->pNextItem;
			}
		}
		else
		{
			returnedIt.pTable = NULL;
			returnedIt.uiTable = 0;
			returnedIt.pItem = NULL;
			returnedIt.pPrevItem = NULL;
		}

		return returnedIt;
	}

	AkHashListBare()
		: m_uiSize( 0 )
	{
	}

	~AkHashListBare()
	{
	}

	//If you set 0 as the starting size, you *must* check all returns of Set() calls.
	//If you initialize with anything else, you can ignore return codes of Set(), they will always succeed.
	bool Init(AkUInt32 in_iStartingSize)
	{
		m_uiSize = 0;

		if(!m_table.Resize(in_iStartingSize))
			return false;

		for ( AkHashType i = 0; i < HashSize(); ++i )
			m_table[ i ] = NULL;
		
		return true;
	}

	void Term()
	{
		AKASSERT( m_uiSize == 0 );
		m_table.Term();
	}
/*
	void RemoveAll()
	{
		for ( AkHashType i = 0; i < HashSize(); ++i )
		{
			T_MAPSTRUCT * pItem = m_table[ i ];
			while ( pItem != NULL )
			{
				T_MAPSTRUCT * pNextItem = pItem->pNextItem;
				pItem->~T_MAPSTRUCT();
				T_ALLOD::Free( pItem );
				pItem = pNextItem;
			}
			
			m_table[ i ] = NULL;
		}

		m_uiSize = 0;
	}
*/
	T_MAPSTRUCT * Exists( T_KEY in_Key ) const
	{
		if (HashSize() > 0)
		{
			AkHashType uiTable = AkHash(in_Key) % HashSize();
			return ExistsInList(in_Key, uiTable);
		}
		return NULL;
	}

	// Set using an externally preallocated T_MAPSTRUCT -- Hash list takes ownership of the T_MAPSTRUCT.
	bool Set( T_MAPSTRUCT * in_pItem )
	{
		if (CheckSize())
		{
			AkHashType uiTable = AkHash(KEY_POLICY::Key(in_pItem)) % HashSize();
			AKASSERT(!ExistsInList(KEY_POLICY::Key(in_pItem), uiTable)); // T_MAPSTRUCT must not exist in list !

			// Add new entry

			in_pItem->pNextItem = m_table[uiTable];
			m_table[uiTable] = in_pItem;

			++m_uiSize;
			return true;
		}
		//This can only happen if the initial size of the map was 0.
		return false;
	}

	T_MAPSTRUCT * Unset( const T_KEY &in_Key )
	{
		T_MAPSTRUCT * pItem = NULL;

		if (HashSize() > 0)
		{
			AkHashType uiTable = AkHash(in_Key) % HashSize();
			pItem = m_table[uiTable];
			T_MAPSTRUCT * pPrevItem = NULL;
			while (pItem != NULL)
			{
				if (KEY_POLICY::Key(pItem) == in_Key)
					break;

				pPrevItem = pItem;
				pItem = pItem->pNextItem;
			}

			if (pItem)
				RemoveItem(uiTable, pItem, pPrevItem);
		}

		return pItem;
	}

	IteratorEx Erase( const IteratorEx& in_rIter )
	{
		IteratorEx returnedIt;
		returnedIt.pTable = in_rIter.pTable;
		returnedIt.uiTable = in_rIter.uiTable;
		returnedIt.pItem = in_rIter.pItem->pNextItem;
		returnedIt.pPrevItem = in_rIter.pPrevItem;
		
		while ((returnedIt.pItem == NULL) && (++returnedIt.uiTable < returnedIt.pTable->Length()))
		{
			returnedIt.pPrevItem = NULL;
			returnedIt.pItem = (*returnedIt.pTable)[ returnedIt.uiTable ];
		}
		
		RemoveItem( in_rIter.uiTable, in_rIter.pItem, in_rIter.pPrevItem );

		return returnedIt;
	}

	AkUInt32 Length() const
	{
		return m_uiSize;
	}

	AKRESULT Reserve(AkUInt32 in_uNumberOfEntires)
	{
		if ((HashSize() == 0) || (AkReal32)in_uNumberOfEntires / (AkReal32)HashSize() > kHashTableGrowthFactor)
			return Resize((AkUInt32)((AkReal32)in_uNumberOfEntires / kHashTableGrowthFactor));

		return AK_Success;
	}

	AKRESULT Resize(AkUInt32 in_uExpectedNumberOfEntires)
	{
		AKRESULT res = AK_Fail;

		AkHashType uNewSize = 0;
		for (AkUInt32 i = 0; i < kNumHashSizes; ++i)
		{
			if (kHashSizes[i] > in_uExpectedNumberOfEntires)
			{
				uNewSize = kHashSizes[i];
				break;
			}
		}

		if (uNewSize > 0)
		{
			HashTableArray oldArray;
			oldArray.Transfer(m_table);

			if (m_table.GrowArray(uNewSize))
			{
				for (AkUInt32 i = 0; i < uNewSize; i++)
					m_table.AddLast(NULL);

				for (AkUInt32 i = 0; i < oldArray.Length(); i++)
				{
					T_MAPSTRUCT* pItem = oldArray[i];
					while (pItem != NULL)
					{
						T_MAPSTRUCT* pNextItem = pItem->pNextItem;
						{
							AkHashType uiTable = AkHash(KEY_POLICY::Key(pItem)) % uNewSize;
							pItem->pNextItem = m_table[uiTable];
							m_table[uiTable] = pItem;
						}
						pItem = pNextItem;
					}
				}

				oldArray.Term();

				res = AK_Success;
			}
			else
			{
				//Backpedal..
				m_table.Transfer(oldArray);
			}
		}

		return res;
	}

	inline AkHashType HashSize() const
	{
		return m_table.Length();
	}


	inline bool CheckSize()
	{
		if ( (HashSize() == 0) || (AkReal32)m_uiSize / (AkReal32)HashSize() > kHashTableGrowthFactor )
			Resize(HashSize());

		return (HashSize() > 0);
	}

protected:
	void RemoveItem( AkHashType in_uiTable, T_MAPSTRUCT* in_pItem, T_MAPSTRUCT* in_pPrevItem )
	{
		if( in_pPrevItem ) 
			in_pPrevItem->pNextItem = in_pItem->pNextItem;
		else
			m_table[ in_uiTable ] = in_pItem->pNextItem;

		--m_uiSize;
	}

	T_MAPSTRUCT * ExistsInList( T_KEY in_Key, AkHashType in_uiTable ) const
	{
		T_MAPSTRUCT * pItem = m_table[in_uiTable];
		while (pItem != NULL)
		{
			if (KEY_POLICY::Key(pItem) == in_Key)
				return pItem; // found
			
			pItem = pItem->pNextItem;
		}

		return NULL; // not found
	}

	HashTableArray m_table;

	AkUInt32 m_uiSize;
};


#endif // _AKHASHLIST_H
