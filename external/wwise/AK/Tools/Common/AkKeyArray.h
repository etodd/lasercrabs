/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version: <VERSION>  Build: <BUILDNUMBER>
  Copyright (c) <COPYRIGHTYEAR> Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkKeyArray.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _KEYARRAY_H_
#define _KEYARRAY_H_

#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkKeyDef.h>

// The Key list is simply a list that may be referenced using a key
// NOTE : 
template <class T_KEY, class T_ITEM, class U_POOL = ArrayPoolDefault, AkUInt32 TGrowBy = 1, class TMovePolicy = AkAssignmentMovePolicy<MapStruct<T_KEY, T_ITEM> > > 
class CAkKeyArray : public AkArray< MapStruct<T_KEY, T_ITEM>, const MapStruct<T_KEY, T_ITEM>&, U_POOL, TGrowBy, TMovePolicy>
{
public:
//====================================================================================================
// Return NULL if the Key does not exisis
// Return T_ITEM* otherwise
//====================================================================================================
	T_ITEM* Exists( T_KEY in_Key )
	{
		typename CAkKeyArray< T_KEY, T_ITEM, U_POOL, TGrowBy, TMovePolicy >::Iterator it = this->FindEx( in_Key );
		return ( it != this->End() ) ? &(it.pItem->item) : NULL;
	}

public:
//====================================================================================================
// Sets the item referenced by the specified key and item
// Return AK_Fail if the list max size was exceeded
//====================================================================================================
	T_ITEM * Set( T_KEY in_Key, T_ITEM in_Item )
	{
		T_ITEM* pSearchedItem = Exists( in_Key );
		if( pSearchedItem )
		{
			*pSearchedItem = in_Item;
		}
		else
		{
			MapStruct<T_KEY, T_ITEM> * pStruct = this->AddLast();
			if ( pStruct )
			{
				pStruct->key = in_Key;
				pStruct->item = in_Item;
				pSearchedItem = &( pStruct->item );
			}
		}
		return pSearchedItem;
	}

	T_ITEM * SetFirst( T_KEY in_Key, T_ITEM in_Item )
	{
		T_ITEM* pSearchedItem = Exists( in_Key );
		if( pSearchedItem )
		{
			*pSearchedItem = in_Item;
		}
		else
		{
			MapStruct<T_KEY, T_ITEM> * pStruct = this->Insert( 0 ); //insert at index 0 is AddFirst.
			if ( pStruct )
			{
				pStruct->key = in_Key;
				pStruct->item = in_Item;
				pSearchedItem = &( pStruct->item );
			}
		}
		return pSearchedItem;
	}

	T_ITEM * Set( T_KEY in_Key )
	{
		T_ITEM* pSearchedItem = Exists( in_Key );
		if( !pSearchedItem )
		{
			MapStruct<T_KEY, T_ITEM> * pStruct = this->AddLast();
			if ( pStruct )
			{
				pStruct->key = in_Key;
				pSearchedItem = &( pStruct->item );
			}
		}
		return pSearchedItem;
	}

	// NOTE: The real definition should be 
	// typename CAkKeyArray<T_KEY,T_ITEM,TGrowBy, TMovePolicy>::Iterator FindEx( T_KEY in_Item ) const
	// Typenaming the base class is a workaround for bug MTWX33123 in the new Freescale CodeWarrior.
	typename AkArray< MapStruct<T_KEY, T_ITEM>, const MapStruct<T_KEY, T_ITEM>&, U_POOL, TGrowBy, TMovePolicy>::Iterator FindEx( T_KEY in_Item ) const
	{
		typename CAkKeyArray< T_KEY, T_ITEM, U_POOL, TGrowBy, TMovePolicy >::Iterator it = this->Begin();

		typename CAkKeyArray< T_KEY, T_ITEM, U_POOL, TGrowBy, TMovePolicy >::Iterator itEnd = this->End();
		for ( ; it != itEnd; ++it )
		{
			if ( (*it).key == in_Item )
				break;
		}

		return it;
	}

//====================================================================================================
//	Remove the item referenced by the specified key
//====================================================================================================

	void Unset( T_KEY in_Key )
	{
		typename CAkKeyArray< T_KEY, T_ITEM, U_POOL, TGrowBy, TMovePolicy >::Iterator it = FindEx( in_Key );
		if( it != this->End() )
		{
			this->Erase( it );
		}
	}

//====================================================================================================
//	More efficient version of Unset when order is unimportant
//====================================================================================================

	void UnsetSwap( T_KEY in_Key )
	{
		typename CAkKeyArray< T_KEY, T_ITEM, U_POOL, TGrowBy, TMovePolicy >::Iterator it = FindEx( in_Key );
		if( it != this->End() )
		{
			this->EraseSwap( it );
		}
	}
};

/// Key policy for AkSortedKeyArray.
template <class T_KEY, class T_ITEM> struct AkGetArrayKey
{
	/// Default policy.
	static AkForceInline T_KEY & Get( T_ITEM & in_item ) 
	{
		return in_item.key;
	}
};

/// Array of items, sorted by key. Uses binary search for lookups. BEWARE WHEN
/// MODIFYING THE ARRAY USING BASE CLASS METHODS.
template <class T_KEY, class T_ITEM, class U_POOL, class U_KEY = AkGetArrayKey< T_KEY, T_ITEM >, unsigned long TGrowBy = 1, class TMovePolicy = AkAssignmentMovePolicy<T_ITEM> > 
class AkSortedKeyArray : public AkArray< T_ITEM, const T_ITEM &, U_POOL, TGrowBy, TMovePolicy >
{
public:
	template< class P_KEY>
	T_ITEM* Exists( P_KEY in_key )
	{
		bool bFound;
		T_ITEM * pItem = BinarySearch( in_key, bFound );
		return bFound ? pItem : NULL;
	}

	// Add an item to the list (allowing duplicate keys)
	template< class P_KEY>
	T_ITEM * Add( P_KEY in_key )
	{
		T_ITEM * pItem = AddNoSetKey( in_key );

		// Then set the key
		if ( pItem )
			U_KEY::Get( *pItem ) = in_key;

		return pItem;
	}

	// Add an item to the list (allowing duplicate keys)
	template< class P_KEY>
	T_ITEM * AddNoSetKey( P_KEY in_key )
	{
		bool bFound;
		T_ITEM * pItem = BinarySearch( in_key, bFound );
		if ( pItem )
		{
			unsigned int uIdx = (unsigned int) ( pItem - this->m_pItems );
			pItem = this->Insert( uIdx );
		}
		else
		{
			pItem = this->AddLast();
		}

		return pItem;
	}

	// Set an item in the list (returning existing item if present)
	template< class P_KEY>
	T_ITEM * Set( P_KEY in_key )
	{
		bool bFound;
		T_ITEM * pItem = BinarySearch( in_key, bFound );
		if ( !bFound )
		{
			if ( pItem )
			{
				unsigned int uIdx = (unsigned int) ( pItem - this->m_pItems );
				pItem = this->Insert( uIdx );
			}
			else
			{
				pItem = this->AddLast();
			}

			if ( pItem )
				U_KEY::Get( *pItem ) = in_key;
		}

		return pItem;
	}

	template< class P_KEY>
	void Unset( P_KEY in_key )
	{
		bool bFound;
		T_ITEM * pItem = BinarySearch( in_key, bFound );
		if ( bFound )
		{
			typename AkArray< T_ITEM, const T_ITEM &, U_POOL, TGrowBy, TMovePolicy >::Iterator it;
			it.pItem = pItem;
			this->Erase( it );
		}
	}

	// WARNING: Do not use on types that need constructors or destructor called on Item objects at each creation.
	template< class P_KEY>
	void Reorder( P_KEY in_OldKey, P_KEY in_NewKey, T_ITEM in_item )
	{	
		bool bFound;
		T_ITEM * pItem = BinarySearch( in_OldKey, bFound );

		//AKASSERT( bFound );
		if( !bFound ) return;// cannot be an assert for now.(WG-19496)

		unsigned int uIdx = (unsigned int) ( pItem - this->m_pItems );
		unsigned int uLastIdx = this->Length()-1;

		AKASSERT( *pItem == in_item );

		bool bNeedReordering = false;
		if( uIdx > 0 ) // if not first
		{
			T_ITEM * pPrevItem = this->m_pItems + (uIdx - 1);
			if( in_NewKey < U_KEY::Get( *pPrevItem ) )
			{
				// Check one step further
				if( uIdx > 1 )
				{
					T_ITEM * pSecondPrevItem = this->m_pItems + (uIdx - 2);
					if( in_NewKey > U_KEY::Get( *pSecondPrevItem ) )
					{
						return Swap( pPrevItem, pItem );
					}
					else
					{
						bNeedReordering = true;
					}
				}
				else
				{
					return Swap( pPrevItem, pItem );
				}
			}
		}
		if( !bNeedReordering && uIdx < uLastIdx )
		{
			T_ITEM * pNextItem = this->m_pItems + (uIdx + 1);
			if( in_NewKey > U_KEY::Get( *pNextItem ) )
			{
				// Check one step further
				if( uIdx < (uLastIdx-1) )
				{
					T_ITEM * pSecondNextItem = this->m_pItems + (uIdx + 2);
					if( in_NewKey < U_KEY::Get( *pSecondNextItem ) )
					{
						return Swap( pNextItem, pItem );
					}
					else
					{
						bNeedReordering = true;
					}
				}
				else
				{
					return Swap( pNextItem, pItem );
				}
			}
		}

		if( bNeedReordering )
		{
			/////////////////////////////////////////////////////////
			// Faster implementation, moving only what is required.
			/////////////////////////////////////////////////////////
			unsigned int uIdxToInsert; // non initialized
			T_ITEM * pTargetItem = BinarySearch( in_NewKey, bFound );
			if ( pTargetItem )
			{
				uIdxToInsert = (unsigned int) ( pTargetItem - this->m_pItems );
				if( uIdxToInsert > uIdx )
				{
					--uIdxToInsert;// we are still in the list, don't count the item to be moved.
				}
			}
			else
			{
				uIdxToInsert = uLastIdx;
			}

			T_ITEM * pStartItem = this->m_pItems + uIdx;
			T_ITEM * pEndItem = this->m_pItems + uIdxToInsert;
			if( uIdxToInsert < uIdx )
			{
				// Slide backward.
				while( pStartItem != pEndItem )
				{
					--pStartItem;
					pStartItem[ 1 ] = pStartItem[ 0 ];
				}		
			}
			else
			{
				// Slide forward.
				while( pStartItem != pEndItem )
				{
					pStartItem[ 0 ] = pStartItem[ 1 ];
					++pStartItem;
				}
			}
			pEndItem[0] = in_item;
			///////////////////////////////////////////////
		}
	}

	// WARNING: Do not use on types that need constructors or destructor called on Item objects at each creation.
	template<class P_KEY>
	void ReSortArray() //To be used when the < > operator changed meaning.
	{
		AkInt32 NumItemsToReInsert = this->Length();
		if( NumItemsToReInsert != 0 )
		{
			// Do a re-insertion sort.
			// Fool the table by faking it is empty, then re-insert one by one.
			T_ITEM * pReinsertionItem = this->m_pItems;
			this->m_uLength = 0; // Faking the Array Is Empty.
			for( AkInt32 idx = 0; idx < NumItemsToReInsert; ++idx )
			{
				T_ITEM ItemtoReinsert = pReinsertionItem[idx]; // make a copy as the source is about to be overriden.
				
				T_KEY keyToReinsert = U_KEY::Get(ItemtoReinsert);

				T_ITEM* pInsertionEmplacement = AddNoSetKey( *reinterpret_cast<P_KEY*>(&keyToReinsert) );
						
				AKASSERT( pInsertionEmplacement );
				*pInsertionEmplacement = ItemtoReinsert;
			}
		}
	}

	template< class P_KEY>
	T_ITEM * BinarySearch( P_KEY in_key, bool & out_bFound )
	{
		AkInt32 uTop = 0, uBottom = this->Length()-1;

		// binary search for key
		while ( uTop <= uBottom )
		{
			AkInt32 uThis = ( uBottom - uTop ) / 2 + uTop; 
			if( in_key < U_KEY::Get( this->m_pItems[ uThis ] ) )
				uBottom = uThis - 1;
			else if ( in_key > U_KEY::Get( this->m_pItems[ uThis ] ) ) 
				uTop = uThis + 1;
			else
			{
				out_bFound = true;
				return this->m_pItems + uThis;
			}
		}

		out_bFound = false;
		return this->m_pItems ? this->m_pItems + uTop : NULL;
	}

	AkForceInline void Swap( T_ITEM * in_ItemA, T_ITEM * in_ItemB )
	{
		T_ITEM ItemTemp = *in_ItemA;
		*in_ItemA = *in_ItemB;
		*in_ItemB = ItemTemp;
	}
};

#endif //_KEYARRAY_H_
