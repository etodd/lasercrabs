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
// AkKeyDef.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _KEYDEF_H_
#define _KEYDEF_H_

#include <AK/Tools/Common/AkArray.h> //For ArrayPoolDefault

template <class T_KEY, class T_ITEM> 
struct MapStruct
{
	T_KEY	key; 
	T_ITEM	item;
	bool operator ==(const MapStruct& in_Op) const
	{
		return ( (key == in_Op.key) /*&& (item == in_Op.item)*/ );
	}
};


// A helper struct that can be used as a T_ITEM in an CAkKeyArray, when you need to only preform a shallow copy
//	 on the data that you are referencing.  For example if you wanted to to use an AkArray type.
// NOTE: AllocData() and FreeData() must be explicitly called, or else pData can be manually Alloc'd/Free'd.
template < typename T_KEY, typename T_DATA, class T_ALLOC = ArrayPoolDefault >
struct AkKeyDataPtrStruct
{
	AkKeyDataPtrStruct(): pData(NULL) {}
	AkKeyDataPtrStruct(T_KEY in_key): key(in_key), pData(NULL) {}

	T_KEY	key;
	T_DATA* pData;
	bool operator ==(const AkKeyDataPtrStruct<T_KEY,T_DATA>& in_Op) const
	{
		return ( key == in_Op.key );
	}

	bool AllocData()
	{
		AKASSERT( !pData );
		pData = (T_DATA*) T_ALLOC::Alloc( sizeof( T_DATA ));
		if (pData)
		{
			AkPlacementNew( pData ) T_DATA();
			return true;
		}
		return false;
	}

	void FreeData()
	{
		if( pData )
		{
			pData->~T_DATA();
			T_ALLOC::Free(pData);
			pData = NULL;
		}
	}

	static AkForceInline T_KEY& Get( AkKeyDataPtrStruct<T_KEY, T_DATA>& in_val ) { return in_val.key; }
};

#endif //_KEYDEF_H_
