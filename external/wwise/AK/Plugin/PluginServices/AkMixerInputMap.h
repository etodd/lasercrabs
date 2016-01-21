//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
 
#ifndef _AK_MIXERINPUTMAP_H_
#define _AK_MIXERINPUTMAP_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/Tools/Common/AkArray.h>

/// Collection class to manage inputs in mixer plugins.
/// The inputs are identified by their context AK::IAkMixerInputContext. The type of data attached to it is the template argument USER_DATA.
/// The collection performs allocation/deallocation of user data via AK_PLUGIN_NEW/DELETE().
/// Usage
/// 
/// // Init 
/// AkMixerInputMap<MyStruct> m_mapInputs;
/// m_mapInputs.Init( in_pAllocator );	// in_pAllocator passed at plugin init.
///
/// // Add an input.
/// m_mapInputs.AddInput( in_pInput );	// AK::IAkMixerInputContext * in_pInput passed to OnInputConnected()
/// 
/// // Find an input
/// MyStruct * pInput = m_mapInputs.Exists( in_pInputContext );	// AK::IAkMixerInputContext * in_pInputContext passed to ConsumeInput()
///
/// // Iterate through inputs.
///	AkMixerInputMap<MyStruct>::Iterator it = m_mapInputs.End();
///	while ( it != m_mapInputs.End() )
///	{
///		MyStruct * pInput = (*it).pUserData;
///		...
///		++it;
///	}

/// Structure of an entry in the AkMixerInputMap map.
template <class USER_DATA>
struct AkInputMapSlot
{
	AK::IAkMixerInputContext *	pContext;
	USER_DATA *					pUserData;	/// User data. Here we have a buffer. Other relevant info would be the game object position and input parameters of the previous frame.

	AkInputMapSlot() : pContext( NULL ), pUserData( NULL ) {}
	bool operator ==(const AkInputMapSlot& in_Op) const { return ( pContext == in_Op.pContext ); }
};

/// Allocator for plugin-friendly arrays.
/// TODO Replace by a sorted array.
class AkPluginArrayAllocator
{
public:
	AkForceInline AkPluginArrayAllocator() : m_pAllocator( NULL ) {}
	AkForceInline void Init( AK::IAkPluginMemAlloc * in_pAllocator ) { m_pAllocator = in_pAllocator; }
protected:
	AkForceInline void * Alloc( size_t in_uSize ) { AKASSERT( m_pAllocator || !"Allocator not set. Did you forget to call AkMixerInputMap::Init()?" ); return AK_PLUGIN_ALLOC( m_pAllocator, in_uSize ); }
	AkForceInline void Free( void * in_pAddress ) { AKASSERT( m_pAllocator || !"Allocator not set. Did you forget to call AkMixerInputMap::Init()?" ); AK_PLUGIN_FREE( m_pAllocator, in_pAddress ); }
	AkForceInline AK::IAkPluginMemAlloc * GetAllocator() { return m_pAllocator; }
private:
	AK::IAkPluginMemAlloc *		m_pAllocator;
};

/// AkMixerInputMap: Map of inputs (identified with AK::IAkMixerInputContext *) to user-defined blocks of data.
template <class USER_DATA>
class AkMixerInputMap : public AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>
{
public:
	typedef AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1> BaseClass;

	/// Returns the user data associated with given input context. Returns NULL if none found.
	USER_DATA * Exists( AK::IAkMixerInputContext * in_pInput )
	{
		typename AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::Iterator it = FindEx( in_pInput );
		return ( it != BaseClass::End() ) ? (*it).pUserData : NULL;
	}

	/// Adds an input with new user data.
	USER_DATA * AddInput( AK::IAkMixerInputContext * in_pInput )
	{
		typename AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::Iterator it = FindEx( in_pInput );
		if ( it != BaseClass::End() )
			return (*it).pUserData;
		else
		{
			AkInputMapSlot<USER_DATA> * pSlot = AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::AddLast();
			if ( pSlot )
			{
				pSlot->pUserData = AK_PLUGIN_NEW( AkPluginArrayAllocator::GetAllocator(), USER_DATA );
				if ( pSlot->pUserData )
				{
					pSlot->pContext = in_pInput;
					return pSlot->pUserData;
				}
				BaseClass::RemoveLast();
			}
		}
		return NULL;
	}

	/// Removes an input and destroys its associated user data.
	bool RemoveInput( AK::IAkMixerInputContext * in_pInput )
	{
		typename AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::Iterator it = FindEx( in_pInput );
		if ( it != BaseClass::End() )
		{
			AKASSERT( (*it).pUserData );
			AK_PLUGIN_DELETE( AkPluginArrayAllocator::GetAllocator(), (*it).pUserData );
			BaseClass::EraseSwap( it );
			return true;
		}
		return false;
	}

	/// Terminate array.
	void Term()
	{
		if ( BaseClass::m_pItems )
		{
			RemoveAll();
			AkPluginArrayAllocator::Free( BaseClass::m_pItems );
			BaseClass::m_pItems = 0;
			BaseClass::m_ulReserved = 0;
		}
	}

	/// Finds an item in the array.
	typename AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::Iterator FindEx( AK::IAkMixerInputContext * in_pInput ) const
	{
		AkInputMapSlot<USER_DATA> mapSlot;
		mapSlot.pContext = in_pInput;
		return BaseClass::FindEx( mapSlot );
	}

	/// Removes and destroys all items in the array.
	void RemoveAll()
	{
		for ( typename AkArray<AkInputMapSlot<USER_DATA>, const AkInputMapSlot<USER_DATA>&, AkPluginArrayAllocator, 1>::Iterator it = BaseClass::Begin(), itEnd = BaseClass::End(); it != itEnd; ++it )
		{
			AKASSERT( (*it).pUserData );
			AK_PLUGIN_DELETE( AkPluginArrayAllocator::GetAllocator(), (*it).pUserData );
			(*it).~AkInputMapSlot();
		}
		BaseClass::m_uLength = 0;
	}
};

#endif // _AK_MIXERINPUTMAP_H_
