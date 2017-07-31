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
struct AkKeyDataPtrStruct: public T_ALLOC
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
