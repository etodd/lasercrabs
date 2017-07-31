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

#ifndef _AKARRAY_H
#define _AKARRAY_H

#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#define AK_DEFINE_ARRAY_POOL( _name_, _poolID_ )	\
struct _name_										\
{													\
	static AkMemPoolId Get()						\
	{												\
		return _poolID_;							\
	}												\
};

AK_DEFINE_ARRAY_POOL( _ArrayPoolDefault, g_DefaultPoolId )
AK_DEFINE_ARRAY_POOL( _ArrayPoolLEngineDefault, g_LEngineDefaultPoolId )

template <class U_POOL>
struct AkArrayAllocatorNoAlign
{
	AkForceInline void * Alloc( size_t in_uSize )
	{
		return AK::MemoryMgr::Malloc( U_POOL::Get(), in_uSize );
	}

	AkForceInline void Free( void * in_pAddress )
	{
		AK::MemoryMgr::Free( U_POOL::Get(), in_pAddress );
	}

	AkForceInline void TransferMem(void *& io_pDest, AkArrayAllocatorNoAlign<U_POOL> in_srcAlloc, void * in_pSrc ) 
	{
		io_pDest = in_pSrc;
	}
};

template <class U_POOL>
struct AkArrayAllocatorAlignedSimd
{
	AkForceInline void * Alloc( size_t in_uSize )
	{
		return AK::MemoryMgr::Malign( U_POOL::Get(), in_uSize, AK_SIMD_ALIGNMENT );
	}

	AkForceInline void Free( void * in_pAddress )
	{
		AK::MemoryMgr::Falign( U_POOL::Get(), in_pAddress );
	}

	AkForceInline void TransferMem(void *& io_pDest, AkArrayAllocatorAlignedSimd<U_POOL> in_srcAlloc, void * in_pSrc ) 
	{
		io_pDest = in_pSrc;
	}

};

// AkHybridAllocator
//	Attempts to allocate from a small buffer of size uBufferSizeBytes, which is contained within the array type.  Useful if the array is expected to contain a small number of elements.
//	If the array grows to a larger size than uBufferSizeBytes, the the memory is allocated from the default memory pool.
//	NOTE: only use with types that are trivially copyable.
template< AkUInt32 uBufferSizeBytes, AkUInt8 uAlignmentSize = AK_OS_STRUCT_ALIGN>
struct AkHybridAllocator
{
	static const AkUInt32 _uBufferSizeBytes = uBufferSizeBytes;

	AkForceInline void * Alloc(size_t in_uSize)
	{
		if (in_uSize <= uBufferSizeBytes)
			return (void *)&m_buffer;
		else
			return AK::MemoryMgr::Malign(g_DefaultPoolId, in_uSize, uAlignmentSize);
	}

	AkForceInline void Free(void * in_pAddress)
	{
		if (&m_buffer != in_pAddress)
			AK::MemoryMgr::Falign(g_DefaultPoolId, in_pAddress);
	}

	AkForceInline void TransferMem(void *& io_pDest, AkHybridAllocator<uBufferSizeBytes, uAlignmentSize>& in_srcAlloc, void * in_pSrc)
	{
		if (&in_srcAlloc.m_buffer == in_pSrc)
		{
			AKPLATFORM::AkMemCpy(m_buffer, in_srcAlloc.m_buffer, uBufferSizeBytes);
			io_pDest = m_buffer;
		}
		else
		{
			io_pDest = in_pSrc;
		}
	}
	
	AK_ALIGN(char m_buffer[uBufferSizeBytes], uAlignmentSize);
};

template <class T>
struct AkAssignmentMovePolicy
{
	// By default the assignment operator is invoked to move elements of an array from slot to slot.  If desired,
	//	a custom 'Move' operation can be passed into TMovePolicy to transfer ownership of resources from in_Src to in_Dest.
	static AkForceInline void Move( T& in_Dest, T& in_Src )
	{
		in_Dest = in_Src;
	}
};

// Can be used as TMovePolicy to create arrays of arrays.
template <class T>
struct AkTransferMovePolicy
{
	static AkForceInline void Move( T& in_Dest, T& in_Src )
	{
		in_Dest.Transfer(in_Src); //transfer ownership of resources.
	}
};

// Common allocators:
typedef AkArrayAllocatorNoAlign<_ArrayPoolDefault> ArrayPoolDefault;
typedef AkArrayAllocatorNoAlign<_ArrayPoolLEngineDefault> ArrayPoolLEngineDefault;
typedef AkArrayAllocatorAlignedSimd<_ArrayPoolLEngineDefault> ArrayPoolLEngineDefaultAlignedSimd;

/// Specific implementation of array
template <class T, class ARG_T, class TAlloc = ArrayPoolDefault, unsigned long TGrowBy = 1, class TMovePolicy = AkAssignmentMovePolicy<T> > class AkArray : public TAlloc
{
public:
	/// Constructor
	AkArray()
		: m_pItems( 0 )
		, m_uLength( 0 )
		, m_ulReserved( 0 )
	{
	}

	/// Destructor
	~AkArray()
	{
		AKASSERT( m_pItems == 0 );
		AKASSERT( m_uLength == 0 );
		AKASSERT( m_ulReserved == 0 );
	}

// Workaround for SWIG to parse nested structure: 
// Bypass this inner struct and use a proxy in a separate header.
#ifndef SWIG
	/// Iterator
	struct Iterator
	{
		T* pItem;	///< Pointer to the item in the array.

		/// ++ operator
		Iterator& operator++()
		{
			AKASSERT( pItem );
			++pItem;
			return *this;
		}

		/// -- operator
        Iterator& operator--()
		{
			AKASSERT( pItem );
			--pItem;
			return *this;
		}

		/// * operator
		T& operator*()
		{
			AKASSERT( pItem );
			return *pItem;
		}

		/// == operator
		bool operator ==( const Iterator& in_rOp ) const
		{
			return ( pItem == in_rOp.pItem );
		}

		/// != operator
		bool operator !=( const Iterator& in_rOp ) const
		{
			return ( pItem != in_rOp.pItem );
		}
	};
#endif // #ifndef SWIG

	/// Returns the iterator to the first item of the array, will be End() if the array is empty.
	Iterator Begin() const
	{
		Iterator returnedIt;
		returnedIt.pItem = m_pItems;
		return returnedIt;
	}

	/// Returns the iterator to the end of the array
	Iterator End() const
	{
		Iterator returnedIt;
		returnedIt.pItem = m_pItems + m_uLength;
		return returnedIt;
	}

	/// Returns the iterator th the specified item, will be End() if the item is not found
	Iterator FindEx( ARG_T in_Item ) const
	{
		Iterator it = Begin();

		for ( Iterator itEnd = End(); it != itEnd; ++it )
		{
			if ( *it == in_Item )
				break;
		}

		return it;
	}

	/// Returns the iterator th the specified item, will be End() if the item is not found
	/// The array must be in ascending sorted order.
	Iterator BinarySearch( ARG_T in_Item ) const
	{
		Iterator itResult = End();
		if (m_pItems)
		{
			T * pTop = m_pItems, * pBottom = m_pItems + m_uLength;

			while ( pTop <= pBottom )
			{
				T* pThis = ( pBottom - pTop ) / 2 + pTop; 
				if( in_Item < *pThis )
					pBottom = pThis - 1;
				else if ( in_Item > *pThis ) 
					pTop = pThis + 1;
				else
				{
					itResult.pItem = pThis;
					break;
				}
			}
		}

		return itResult;
	}

	/// Erase the specified iterator from the array
	Iterator Erase( Iterator& in_rIter )
	{
		AKASSERT( m_pItems != 0 );

		// Move items by 1

		T * pItemLast = m_pItems + m_uLength - 1;

		for ( T * pItem = in_rIter.pItem; pItem < pItemLast; pItem++ )
			TMovePolicy::Move( pItem[ 0 ], pItem[ 1 ] );

		// Destroy the last item

		pItemLast->~T();

		m_uLength--;

		return in_rIter;
	}

	/// Erase the item at the specified index
    void Erase( unsigned int in_uIndex )
	{
		AKASSERT( m_pItems != 0 );

		// Move items by 1

		T * pItemLast = m_pItems + m_uLength - 1;

		for ( T * pItem = m_pItems+in_uIndex; pItem < pItemLast; pItem++ )
			TMovePolicy::Move( pItem[ 0 ], pItem[ 1 ] );

		// Destroy the last item

		pItemLast->~T();

		m_uLength--;
	}

	/// Erase the specified iterator in the array. but it dos not guarantee the ordering in the array.
	/// This version should be used only when the order in the array is not an issue.
	Iterator EraseSwap( Iterator& in_rIter )
	{
		AKASSERT( m_pItems != 0 );

		if ( Length( ) > 1 )
		{
			// Swap last item with this one.
			TMovePolicy::Move( *in_rIter.pItem, Last( ) );
		}

		// Destroy.
		AKASSERT( Length( ) > 0 );
		Last( ).~T();

		m_uLength--;

		return in_rIter;
	}

	/// Pre-Allocate a number of spaces in the array
	AKRESULT Reserve( AkUInt32 in_ulReserve )
	{
		AKASSERT( m_pItems == 0 && m_uLength == 0 );
		AKASSERT( in_ulReserve || TGrowBy );

		if ( in_ulReserve )
		{
			m_pItems = (T *) TAlloc::Alloc( sizeof( T ) * in_ulReserve );
			if ( m_pItems == 0 )
				return AK_InsufficientMemory;

			m_ulReserved = in_ulReserve;
		}

		return AK_Success;
	}

	AkUInt32 Reserved() const { return m_ulReserved; }

	/// Term the array. Must be called before destroying the object.
	void Term()
	{
		if ( m_pItems )
		{
			RemoveAll();
			TAlloc::Free( m_pItems );
			m_pItems = 0;
			m_ulReserved = 0;
		}
	}

	/// Returns the numbers of items in the array.
	AkForceInline AkUInt32 Length() const
	{
		return m_uLength;
	}

	/// Returns true if the number items in the array is 0, false otherwise.
	AkForceInline bool IsEmpty() const
	{
		return m_uLength == 0;
	}
	
	/// Returns a pointer to the specified item in the list if it exists, 0 if not found.
	T* Exists(ARG_T in_Item) const
	{
		Iterator it = FindEx( in_Item );
		return ( it != End() ) ? it.pItem : 0;
	}

	/// Add an item in the array, without filling it.
	/// Returns a pointer to the location to be filled.
	T * AddLast()
	{
		size_t cItems = Length();

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4127 )
#endif
		if ( ( cItems >= m_ulReserved ) && TGrowBy > 0 )
		{
			if ( !GrowArray() ) 
				return 0;
		}
#if defined(_MSC_VER)
#pragma warning( pop )
#endif

		// have we got space for a new one ?
		if(  cItems < m_ulReserved )
		{
			T * pEnd = m_pItems + m_uLength++;
			AkPlacementNew( pEnd ) T; 
			return pEnd;
		}

		return 0;
	}

	/// Add an item in the array, and fills it with the provided item.
	T * AddLast(ARG_T in_rItem)
	{
		T * pItem = AddLast();
		if ( pItem )
			*pItem = in_rItem;
		return pItem;
	}

	/// Returns a reference to the last item in the array.
	T& Last()
	{
		AKASSERT( m_uLength );

		return *( m_pItems + m_uLength - 1 );
	}

	/// Removes the last item from the array.
    void RemoveLast()
    {
        AKASSERT( m_uLength );
        ( m_pItems + m_uLength - 1 )->~T();
        m_uLength--;
    }

	/// Removes the specified item if found in the array.
	AKRESULT Remove(ARG_T in_rItem)
	{
		Iterator it = FindEx( in_rItem );
		if ( it != End() )
		{
			Erase( it );
			return AK_Success;
		}

		return AK_Fail;
	}

	/// Fast remove of the specified item in the array.
	/// This method do not guarantee keeping ordering of the array.
	AKRESULT RemoveSwap(ARG_T in_rItem)
	{
		Iterator it = FindEx( in_rItem );
		if ( it != End() )
		{
			EraseSwap( it );
			return AK_Success;
		}

		return AK_Fail;
	}

	/// Removes all items in the array
	void RemoveAll()
	{
		for ( Iterator it = Begin(), itEnd = End(); it != itEnd; ++it )
			(*it).~T();
		m_uLength = 0;
	}

	/// Operator [], return a reference to the specified index.
	AkForceInline T& operator[](unsigned int uiIndex) const
    {
        AKASSERT( m_pItems );
        AKASSERT( uiIndex < Length() );
        return m_pItems[uiIndex];
    }

	/// Insert an item at the specified position without filling it.
	/// Returns the pointer to the item to be filled.
	T * Insert(unsigned int in_uIndex)
	{
        AKASSERT( in_uIndex <= Length() );

		size_t cItems = Length();

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4127 )
#endif
		if ( ( cItems >= m_ulReserved ) && TGrowBy > 0 )
		{
			if ( !GrowArray() ) 
				return 0;
		}
#if defined(_MSC_VER)
#pragma warning( pop )
#endif

		// have we got space for a new one ?
		if(  cItems < m_ulReserved )
		{
			T * pItemLast = m_pItems + m_uLength++;
			AkPlacementNew( pItemLast ) T; 

			// Move items by 1

			for ( T * pItem = pItemLast; pItem > ( m_pItems + in_uIndex ); --pItem )
				TMovePolicy::Move( pItem[ 0 ], pItem[ -1 ] );

			// Reinitialize item at index

			( m_pItems + in_uIndex )->~T();
			AkPlacementNew( m_pItems + in_uIndex ) T; 

			return m_pItems + in_uIndex;
		}

		return 0;
	}

	/// Resize the array.
	bool GrowArray( AkUInt32 in_uGrowBy = TGrowBy )
	{
		AKASSERT( in_uGrowBy );
		
		AkUInt32 ulNewReserve = m_ulReserved + in_uGrowBy;
		T * pNewItems = (T *) TAlloc::Alloc( sizeof( T ) * ulNewReserve );
		if ( !pNewItems ) 
			return false;

		// Copy all elements in new array, destroy old ones

		size_t cItems = Length();

		if ( m_pItems && m_pItems != pNewItems /*AkHybridAllocator may serve up same memory*/ ) 
		{
			for ( size_t i = 0; i < cItems; ++i )
			{
				AkPlacementNew( pNewItems + i ) T; 

				TMovePolicy::Move( pNewItems[ i ], m_pItems[ i ] );
	            
				m_pItems[ i ].~T();
			}

			TAlloc::Free( m_pItems );
		}

		m_pItems = pNewItems;
		m_ulReserved = ulNewReserve;

		return true;
	}

	/// Resize the array to the specified size.
	bool Resize(AkUInt32 in_uiSize)
	{
		AkUInt32 cItems = Length();
		if (in_uiSize < cItems)
		{
			//Destroy superfluous elements
			for(AkUInt32 i = in_uiSize - 1 ; i < cItems; i++)
			{
				m_pItems[ i ].~T();
			}
			m_uLength = in_uiSize;
			return true;
		}

		if ( in_uiSize > m_ulReserved )
		{
			if ( !GrowArray(in_uiSize - cItems) ) 
				return false;
		}

		//Create the missing items.
		for(size_t i = cItems; i < in_uiSize; i++)
		{
			AkPlacementNew( m_pItems + i ) T; 
		}

		m_uLength = in_uiSize;
		return true;
	}

	void Transfer(AkArray<T,ARG_T,TAlloc,TGrowBy,TMovePolicy>& in_rSource)
	{
		Term();

		TAlloc::TransferMem( (void*&)m_pItems, in_rSource, (void*)in_rSource.m_pItems );
		m_uLength = in_rSource.m_uLength;
		m_ulReserved = in_rSource.m_ulReserved;

		in_rSource.m_pItems = NULL;
		in_rSource.m_uLength = 0;
		in_rSource.m_ulReserved = 0;
	}

	AKRESULT Copy(const AkArray<T, ARG_T, TAlloc, TGrowBy, TMovePolicy>& in_rSource)
	{
		Term();

		if (Resize(in_rSource.Length()))
		{
			for (AkUInt32 i = 0; i < in_rSource.Length(); ++i)
				m_pItems[i] = in_rSource.m_pItems[i];
			return AK_Success;
		}
		return AK_Fail;
	}

protected:

	T *         m_pItems;		///< pointer to the beginning of the array.
	AkUInt32    m_uLength;		///< number of items in the array.
	AkUInt32	m_ulReserved;	///< how many we can have at most (currently allocated).
};


#endif
